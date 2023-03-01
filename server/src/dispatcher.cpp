#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cfg.h"
#include "libs/uri_parser.h"
#include "dispatcher.h"
#include "Resolver.h"

#define TAGGED_REQ_VERSION 0
#define TAGGED_PDU_HDR_SIZE 3
#define TAGGED_ERR_RESPONSE_HDR_SIZE 2

#define CNAM_REQ_VERSION 1
#define CNAM_HDR_SIZE 6
#define CNAM_RESPONSE_HDR_SIZE 4

#define EPOLL_MAX_EVENTS 2048
#define MSG_SZ 1024 * 2

_dispatcher::_dispatcher()
{ }

void _dispatcher::loop()
{
    int ret;
    int epoll_fd;
    int nn_poll_fd;
    int nn_id;

    struct epoll_event ev;
    struct epoll_event events[EPOLL_MAX_EVENTS];

    socklen_t addr_size = 0;
    struct sockaddr_in s_addr, cl_s_addr;

    UriComponents uri_c;
    char msg[MSG_SZ];
    std::string reply;
    int rcv_l, snd_l;

    // create 'stop event'
    if(-1 == (stop_event_fd = eventfd(0, EFD_NONBLOCK)))
        throw std::string("eventfd call failed");

    if((epoll_fd = epoll_create(EPOLL_MAX_EVENTS)) == -1)
        throw std::string("epoll_create call failed");

    memset(&ev, 0, sizeof(epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = stop_event_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_event_fd, &ev) == -1)
        throw std::string("epoll_ctl call for stop_event_fd failed");

    // create 'nanomsg socket'
    nn_sock_fd = nn_socket(AF_SP,NN_REP);
    if(nn_sock_fd < 0){
        throw std::string("nn_socket()");
    }

    size_t vallen = sizeof(nn_poll_fd);
    if(0!=nn_getsockopt(nn_sock_fd, NN_SOL_SOCKET, NN_RCVFD, &nn_poll_fd, &vallen)) {
        throw std::string("nn_getsockopt(NN_SOL_SOCKET, NN_RCVFD)");
    }

    memset(&ev, 0, sizeof(epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = nn_poll_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, nn_poll_fd, &ev) == -1)
        throw std::string("epoll_ctl call for nn_poll_fd failed");

    // create 'udp socket'
    udp_sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if(udp_sock_fd < 0){
        throw std::string("socket()");
    }

    memset(&ev, 0, sizeof(epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = udp_sock_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sock_fd, &ev) == -1)
        throw std::string("epoll_ctl call for udp_sock_fd failed");

    // bind sockets
    bool binded = false;
    if(cfg.bind_urls.empty()){
        throw std::string("no listen endpoints specified. check your config");
    }

    for(const auto &i : cfg.bind_urls) {
        const char *url = i.c_str();
        uri_c = parseAddr(url);

        // if 'protocol' is empty use udp socket
        if (strlen(uri_c.proto) == 0) {
            // bind 'udp socket'
            memset(&s_addr, 0, sizeof(s_addr));
            s_addr.sin_family = AF_INET;
            s_addr.sin_port = htons(uri_c.port);
            s_addr.sin_addr.s_addr = inet_addr(uri_c.host);

            ret = bind(udp_sock_fd, (struct sockaddr*) &s_addr, sizeof(s_addr));
            if (ret < 0) {
                err("can't bind to url '%s': %d (%s)", url, errno, nn_strerror(errno));
                continue;
            }

        } else {
            // bind 'nanomsg socket'
            nn_id = nn_bind(nn_sock_fd, url);
            if (nn_id < 0) {
                err("can't bind to url '%s': %d (%s)",url, errno,nn_strerror(errno));
                continue;
            }
        }

        binded = binded || true;
        info("listen on %s",url);
    }

    if(!binded){
        err("there are no listened interfaces after bind. check log and your config");
        throw std::string("can't bind to any url");
    }

    pthread_setname_np(pthread_self(), "dispatcher");

    bool _stop = false;
    while(!_stop) {
        ret = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS,-1);
        if(ret == -1 && errno != EINTR)
            err("dispatcher: epoll_wait: %s",strerror(errno));

        if(ret < 1)
            continue;

        for (int n = 0; n < ret; ++n) {
            struct epoll_event &e = events[n];
            if (!(e.events & EPOLLIN))
                continue;

            if (e.data.fd == stop_event_fd) {
                dbg("got stop event. terminate");
                _stop = true;
                break;
            }

            if (e.data.fd == nn_poll_fd || e.data.fd == udp_sock_fd) {
                memset(&msg, 0, MSG_SZ);

                // receive msg by 'nanomsg'
                if (e.data.fd == nn_poll_fd) {
                    rcv_l = nn_recv(nn_sock_fd, msg, MSG_SZ, 0);

                    if (rcv_l < 0) {
                        if(errno==EINTR) continue;
                        if(errno==EBADF) {
                            err("nn_recv returned EBADF. terminate");
                            _stop = true;
                            break;
                        }
                        dbg("nn_recv() = %d, errno = %d(%s)", rcv_l, errno, nn_strerror(errno));
                        //!TODO: handle timeout, etc
                        continue;
                    }
                }

                // receive msg by 'udp socket'
                if (e.data.fd == udp_sock_fd) {
                    memset(&cl_s_addr, 0, sizeof(cl_s_addr));
                    addr_size = sizeof(cl_s_addr);  // addr_size is value/result

                    rcv_l = recvfrom(udp_sock_fd, msg, MSG_SZ, 0, (struct sockaddr*)&cl_s_addr, &addr_size);

                    if (rcv_l < 0) {
                        if(errno==EINTR) continue;
                        if(errno==EBADF) {
                            err("recvfrom returned EBADF. terminate");
                            _stop = true;
                            break;
                        }
                        dbg("recvfrom() = %d, errno = %d(%s)", rcv_l, errno, nn_strerror(errno));
                        //!TODO: handle timeout, etc
                        continue;
                    }
                }

                // msg did receive
                msg[rcv_l] = '\0';

                // make reply
                reply = create_reply_for_msg(msg, rcv_l);

                // send reply by 'nanomsg'
                if (e.data.fd == nn_poll_fd) {
                    snd_l = nn_send(nn_sock_fd, reply.data(), reply.length(), 0);

                    if (snd_l < 0)
                        err("nn_send(): %d(%s)", errno, nn_strerror(errno));
                }

                // send reply by 'udp socket'
                if (e.data.fd == udp_sock_fd) {
                    snd_l = sendto(udp_sock_fd, reply.data(), reply.length(), 0, (struct sockaddr*)&cl_s_addr, addr_size);

                    if (snd_l < 0)
                        err("sendto(): %d(%s)", errno, nn_strerror(errno));
                }
            }
        }
    }

    close(epoll_fd);

    nn_shutdown(nn_sock_fd, nn_id);
    nn_close(nn_sock_fd);

    shutdown(udp_sock_fd, SHUT_RDWR);
    close(udp_sock_fd);
}

void _dispatcher::stop()
{
    eventfd_write(stop_event_fd, 1);
}

std::string _dispatcher::create_reply_for_msg(char *msg, int len)
{
    std::string reply;
    int type;

    try {
        reply = process_message(msg,len, type);
    } catch(const std::string & e){
        err("got string exception: %s",e.c_str());
        reply = create_error_reply(type, ECErrorId::GENERAL_ERROR,e);
    } catch(const CResolverError & e){
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());
        reply = create_error_reply(type, e.code(),e.what());
    }

    return reply;
}

std::string _dispatcher::process_message(const char *req, int req_len, int &request_type)
{
    request_type = TAGGED_REQ_VERSION;

    if(req_len < 2) {
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
    }

    CDriver::SResult_t r;

    /* common req layout:
    *    1 byte - database id
    *    1 byte - request type
    *      * 0: lnp_resolve_tagged
    *      * 1: lnp_resolve_cnam
    */

    if(req_len < 2) {
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
    }
    request_type = req[1];
    dbg("type: %d", request_type);

    int database_id = req[0];
    int data_offset, data_len;

    //check type
    switch(request_type) {
    case TAGGED_REQ_VERSION:
        /* tagged req layout:
        *    1 byte - database id
        *    1 byte - request type = 0
        *    1 byte - number length
        *    n bytes - number data
        */
        if(req_len < TAGGED_PDU_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
        }
        data_offset = TAGGED_PDU_HDR_SIZE;
        data_len = req[2];
        break;
    case CNAM_REQ_VERSION:
        /* cnam req layout:
        *    1 byte - database id
        *    1 byte - request type = 1
        *    4 bytes - json length
        *    n bytes - json data
        */
        if(req_len < CNAM_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
        }
        data_offset = CNAM_HDR_SIZE;
        data_len = *(int *)(req+2);
        break;
    default:
        request_type = 0; //force tagged reply
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "uknown request type");
    }

    if((data_offset+data_len) > req_len) {
        err("malformed request: too big data_len");
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "malformed request");
    }

    std::string data(req+data_offset,static_cast<size_t>(data_len));
    dbg("process request: database_id:%d, type:%d, data:%s",
        database_id, request_type, data.data());

    resolver::instance()->resolve(request_type, database_id,data,r);

    if(request_type == CNAM_REQ_VERSION)
        return create_json_reply(r);

    return create_tagged_reply(r);
}

std::string _dispatcher::create_tagged_reply(const CDriver::SResult_t &r)
{
    // compose header
    char header[TAGGED_PDU_HDR_SIZE];
    memset(header, '\0', TAGGED_PDU_HDR_SIZE);
    if (TAGGED_PDU_HDR_SIZE > 0) header[0]= char(ECErrorId::NO_ERROR);
    if (TAGGED_PDU_HDR_SIZE > 1) header[1]= char(r.localRoutingNumber.size() + r.localRoutingTag.size());
    if (TAGGED_PDU_HDR_SIZE > 2) header[2]= char(r.localRoutingNumber.size());

    // compose message
    std::string msg;
    msg += std::string(header, TAGGED_PDU_HDR_SIZE);
    msg += r.localRoutingNumber;
    msg += r.localRoutingTag;

    return msg;
}

std::string _dispatcher::create_json_reply(const CDriver::SResult_t &r)
{
    // compose header
    char header[CNAM_RESPONSE_HDR_SIZE];
    memset(header, '\0', CNAM_RESPONSE_HDR_SIZE);
    if (CNAM_RESPONSE_HDR_SIZE > 0) header[0]= char(r.rawData.size());

    // compose message
    std::string msg;
    msg += std::string(header, CNAM_RESPONSE_HDR_SIZE);
    msg += r.rawData;

    return msg;
}

std::string _dispatcher::create_error_reply(int type, const ECErrorId code, const std::string &description)
{
    if(type == CNAM_REQ_VERSION)
        return create_json_error_reply(code, description);

    return create_tagged_error_reply(code,description);
}

std::string _dispatcher::create_tagged_error_reply(const ECErrorId code, const std::string &s)
{
    // compose header
    char header[TAGGED_ERR_RESPONSE_HDR_SIZE];
    memset(header, '\0', TAGGED_ERR_RESPONSE_HDR_SIZE);
    if (TAGGED_ERR_RESPONSE_HDR_SIZE > 0) header[0]= static_cast<char>(code);
    if (TAGGED_ERR_RESPONSE_HDR_SIZE > 1) header[1]= static_cast<char>(s.size());

    // compose message
    std::string msg;
    msg += std::string(header, TAGGED_ERR_RESPONSE_HDR_SIZE);
    msg += s;

    return msg;
}

std::string _dispatcher::create_json_error_reply(const ECErrorId code, const std::string &data)
{
    // compose json
    std::string json("{\"error\":{\"code\":");
    json += std::to_string((int)code);
    json += ",\"reason\":\"";
    json += data;
    json += "\"}}";

    // compose header
    char header[CNAM_RESPONSE_HDR_SIZE];
    memset(header, '\0', CNAM_RESPONSE_HDR_SIZE);
    if (CNAM_RESPONSE_HDR_SIZE > 0) header[0]= char(json.size());

    // compose message
    std::string msg;
    msg += std::string(header, CNAM_RESPONSE_HDR_SIZE);
    msg += json;

    return msg;
}
