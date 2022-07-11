#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "cfg.h"
#include "dispatcher.h"
#include "Resolver.h"

#define TAGGED_REQ_VERSION 0
#define TAGGED_PDU_HDR_SIZE 3
#define TAGGED_ERR_RESPONSE_HDR_SIZE 2

#define CNAM_REQ_VERSION 1
#define CNAM_HDR_SIZE 6
#define CNAM_RESPONSE_HDR_SIZE 4

#define EPOLL_MAX_EVENTS 2048

_dispatcher::_dispatcher()
{ }

void _dispatcher::loop()
{
    int ret;
    int epoll_fd;
    int nn_poll_fd;

    struct epoll_event ev;
    struct epoll_event events[EPOLL_MAX_EVENTS];

    if(-1 == (stop_event_fd = eventfd(0, EFD_NONBLOCK)))
        throw std::string("eventfd call failed");

    if((epoll_fd = epoll_create(EPOLL_MAX_EVENTS)) == -1)
        throw std::string("epoll_create call failed");

    ev.events = EPOLLIN;

    ev.data.fd = stop_event_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_event_fd, &ev) == -1)
        throw std::string("epoll_ctl call for stop_event_fd failed");

    s = nn_socket(AF_SP,NN_REP);
    if(s < 0){
        throw std::string("nn_socket()");
    }

    size_t vallen = sizeof(nn_poll_fd);
    if(0!=nn_getsockopt(s, NN_SOL_SOCKET, NN_RCVFD, &nn_poll_fd, &vallen)) {
        throw std::string("nn_getsockopt(NN_SOL_SOCKET, NN_RCVFD)");
    }

    ev.data.fd = nn_poll_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, nn_poll_fd, &ev) == -1)
        throw std::string("epoll_ctl call for nn_poll_fd failed");

    bool binded = false;
    if(cfg.bind_urls.empty()){
        throw std::string("no listen endpoints specified. check your config");
    }

    for(const auto &i : cfg.bind_urls) {
        const char *url = i.c_str();
        ret = nn_bind(s, url);
        if(ret < 0){
            err("can't bind to url '%s': %d (%s)",url,
                errno,nn_strerror(errno));
            continue;
        }
        binded = true;
        info("listen on %s",url);
    }

    if(!binded){
        err("there are no listened interfaces after bind. check log and your config");
        throw std::string("can't bind to any url");
    }

    pthread_setname_np(pthread_self(), "dispatcher");

    bool _stop = false;
    while(!_stop) {
        ret = epoll_wait(epoll_fd,events,EPOLL_MAX_EVENTS,-1);
        if(ret == -1 && errno != EINTR){
            err("dispatcher: epoll_wait: %s",strerror(errno));
        }
        if(ret < 1)
            continue;

        for (int n = 0; n < ret; ++n) {
            struct epoll_event &e = events[n];
            if(!(e.events & EPOLLIN)){
                continue;
            }
            if(e.data.fd == stop_event_fd) {
                dbg("got stop event. terminate");
                _stop = true;
                break;
            }

            char *msg = nullptr;
            int l = nn_recv(s, &msg, NN_MSG, 0);
            if(l < 0) {
                if(errno==EINTR) continue;
                if(errno==EBADF) {
                    err("nn_recv returned EBADF. terminate");
                    _stop = true;
                    break;
                }
                dbg("nn_recv() = %d, errno = %d(%s)",l,errno,nn_strerror(errno));
                //!TODO: handle timeout, etc
                continue;
            }
            process_peer(msg,l);
            nn_freemsg(msg);
        }
    }
    nn_close(s);
}

void _dispatcher::stop()
{
    eventfd_write(stop_event_fd, 1);
}

int _dispatcher::process_peer(char *msg, int len)
{
    char *reply = nullptr;
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

    if(-1==nn_send(s, &reply, NN_MSG, 0)) {
        err("nn_send(): %d(%s)",errno,nn_strerror(errno));
    }

    return 0;
}

char *_dispatcher::process_message(const char *req, int req_len, int &request_type)
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

char *_dispatcher::create_tagged_reply(const CDriver::SResult_t &r)
{
    auto lrn_len = r.localRoutingNumber.size();
    auto tag_len = r.localRoutingTag.size();
    auto data_len = lrn_len + tag_len;

    auto msg = static_cast<char *>(nn_allocmsg(data_len+TAGGED_PDU_HDR_SIZE, 0));

    msg[0] = static_cast<char> (ECErrorId::NO_ERROR); //code
    msg[1] = static_cast<char>(data_len);  //global_len
    msg[2] = static_cast<char>(lrn_len);   //lrn_len

    memcpy(msg+TAGGED_PDU_HDR_SIZE,r.localRoutingNumber.c_str(),lrn_len);
    memcpy(msg+TAGGED_PDU_HDR_SIZE+lrn_len,r.localRoutingTag.c_str(),tag_len);

    return msg;
}

char *_dispatcher::create_json_reply(const CDriver::SResult_t &r)
{
    auto l = r.rawData.size();
    auto msg = static_cast<char *>(nn_allocmsg(l+CNAM_RESPONSE_HDR_SIZE, 0));
    *(int *)msg = l;
    memcpy(msg+CNAM_RESPONSE_HDR_SIZE,r.rawData.data(),l);

    return msg;
}

char *_dispatcher::create_error_reply(int type, const ECErrorId code, const std::string &description)
{
    if(type == CNAM_REQ_VERSION)
        return create_json_error_reply(code, description);
   return create_tagged_error_reply(code,description);
}

char *_dispatcher::create_tagged_error_reply(const ECErrorId code, const std::string &s)
{
    auto l = s.size();
    auto msg = static_cast<char *>(nn_allocmsg(l+TAGGED_ERR_RESPONSE_HDR_SIZE, 0));

    msg[0]  = static_cast<char>(code);
    msg[1] = static_cast<char>(l);

    memcpy(msg+TAGGED_ERR_RESPONSE_HDR_SIZE,s.c_str(),l);

    return msg;
}

char *_dispatcher::create_json_error_reply(const ECErrorId code, const std::string &data)
{
    std::string s("{\"error\":{\"code\":");
    s += std::to_string((int)code);

    s += ",\"reason\":\"";
    s += data;
    s += "\"}}";

    auto l = s.size();
    auto msg = static_cast<char *>(nn_allocmsg(l+CNAM_RESPONSE_HDR_SIZE, 0));
    *(int *)msg = l;
    memcpy(msg+CNAM_RESPONSE_HDR_SIZE,s.data(),l);

    return msg;
}
