#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "cfg.h"
#include "dispatcher.h"
#include "Resolver.h"

#define PDU_HDR_SIZE 2
#define NEW_PDU_HDR_SIZE 3

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

    size_t vallen;
    if(0!=nn_getsockopt(s, NN_SOL_SOCKET, NN_RCVFD, &nn_poll_fd, &vallen)) {
        throw std::string("nn_setsockopt(NN_LINGER)");
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

    try {
        reply = create_reply(msg,len);
    } catch(const std::string & e){
        err("got string exception: %s",e.c_str());
        reply = create_error_reply(ECErrorId::GENERAL_ERROR,e);
    } catch(const CResolverError & e){
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());
        reply = create_error_reply(e.code(),e.what());
    }

    if(-1==nn_send(s, &reply, NN_MSG, 0)) {
        err("nn_send(): %d(%s)",errno,nn_strerror(errno));
    }

    return 0;
}

char *_dispatcher::str2reply(const ECErrorId code, const std::string &s)
{
    auto l = s.size();
    auto msg = static_cast<char *>(nn_allocmsg(l+PDU_HDR_SIZE, 0));

    msg[0]  = static_cast<char>(code);
    msg[1] = static_cast<char>(l);

    memcpy(msg+PDU_HDR_SIZE,s.c_str(),l);

    return msg;
}

char *_dispatcher::make_reply(const CDriver::SResult_t &r)
{
    auto lrn_len = r.localRoutingNumber.size();
    auto tag_len = r.localRoutingTag.size();
    auto data_len = lrn_len + tag_len;

    auto msg = static_cast<char *>(nn_allocmsg(data_len+NEW_PDU_HDR_SIZE, 0));

    msg[0] = static_cast<char> (ECErrorId::NO_ERROR); //code
    msg[1] = static_cast<char>(data_len);  //global_len
    msg[2] = static_cast<char>(lrn_len);   //lrn_len

    memcpy(msg+NEW_PDU_HDR_SIZE,r.localRoutingNumber.c_str(),lrn_len);
    memcpy(msg+NEW_PDU_HDR_SIZE+lrn_len,r.localRoutingTag.c_str(),tag_len);

    return msg;
}

char *_dispatcher::create_error_reply(const ECErrorId code, const std::string description)
{
   return str2reply(code,description);
}

char *_dispatcher::create_reply(const char *req, int req_len)
{
    if(req_len < PDU_HDR_SIZE) {
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
    }

    CDriver::SResult_t r;
    int data_len = req[1];
    int database_id = req[0];
    int version = 0;
    int lnp_offset = PDU_HDR_SIZE;
    bool old_format = data_len!=0;

    if(!old_format) {
        if(req_len < NEW_PDU_HDR_SIZE){
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
        }
        version = data_len;
        data_len = req[2];
        lnp_offset++;
    }

    (void) version; // compiling error suppression

    if((lnp_offset+data_len) > req_len) {
        err("malformed request: too big data_len");
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "malformed request");
    }

    std::string lnp(req+lnp_offset,static_cast<size_t>(data_len));
    dbg("process request: database: %d, lnp: %s, old_format: %d",
        database_id,lnp.c_str(),old_format);

    resolver::instance()->resolve(database_id,lnp,r);

    if(old_format) {
        return str2reply(ECErrorId::NO_ERROR,r.localRoutingNumber);
    } else {
        return make_reply(r);
    }
}
