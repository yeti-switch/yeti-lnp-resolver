#include "Transport.h"
#include "log.h"
#include "cfg.h"
#include "libs/uri_parser.h"

#include <sys/epoll.h>
#include <cstring>
#include <unistd.h>

Transport::Transport()
  : EventHandler()
{
    bind_endpoints();
}

Transport::~Transport()
{
    shutdown_endpoints();
}

void Transport::set_handler(TransportHandler *transport_handler)
{
    handler = transport_handler;
}

int Transport::init_sock()
{
    int fd = socket(PF_INET, SOCK_DGRAM, 0);

    if(fd < 0)
        return fd;

    if(0!=link(fd, EPOLLIN)) {
        close(fd);
        return -1;
    }

    return fd;
}

int Transport::bind_sock_to(int fd, const char *host, int port)
{
    if (fd < 0)
        return -1;

    struct sockaddr_in s_addr;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(host);
    s_addr.sin_port = htons(port);

    return bind(fd, (struct sockaddr*) &s_addr, sizeof(s_addr));
}

int Transport::bind_endpoints()
{
    int ret;
    bool binded = false;
    UriComponents uri_c;

    if(cfg.bind_urls.empty()) {
        throw string("no listen endpoints specified. check your config");
    }

    for(const auto &i : cfg.bind_urls) {
        const char *url = i.c_str();
        if (parseAddr(url, &uri_c) == -1)
            continue;

        // if 'protocol' is empty use udp transport
        if (strlen(uri_c.proto) == 0) {
            int fd = init_sock();
            if (fd < 0) {
                err("failed to create socket for url '%s': %d (%s)",
                    url, errno, strerror(errno));
                continue;
            }

            ret = bind_sock_to(fd, uri_c.host, uri_c.port);

            if (ret < 0) {
                err("can't bind to url '%s': %d (%s)",
                    url, errno, strerror(errno));
                continue;
            }
        } else {
            err("unsupported '%s' protocol for url '%s'",
                uri_c.proto, url);
            continue;
        }

        binded |= true;
        info("listen on %s",url);
    }

    if(!binded){
        err("there are no listened interfaces after bind. check log and your config");
        throw string("can't bind to any url");
    }

    return 0;
}

int Transport::shutdown_endpoints()
{
    iterate_events_map([](int fd, uint32_t) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    });
    return 0;
}

int Transport::recv_data(int fd, RecvData &out)
{
    if (fd < 0)
        return -1;

    memset(&out.data, 0, sizeof(char) * MSG_SZ);
    out.length = 0;

    out.client_info.recv_fd = fd;
    memset(&out.client_info.addr, 0, sizeof(out.client_info.addr));
    out.client_info.addr_size = sizeof(out.client_info.addr);

    int ret = recvfrom(fd,
                       out.data, MSG_SZ, 0,
                       (struct sockaddr*)&out.client_info.addr,
                       &out.client_info.addr_size);

    if (ret > 0) {
        out.data[ret] = '\0';
        out.length = ret;
    }

    return ret;
}

int Transport::send_data(const string &data, const ClientInfo &client_info)
{
    return send_data(data.c_str(), data.length(), client_info);
}

int Transport::send_data(const void *buf, size_t size, const ClientInfo &client_info)
{
    if (client_info.recv_fd < 0)
        return -1;

    int len;
    len = sendto(client_info.recv_fd, buf, size, 0,
                (struct sockaddr*)&client_info.addr,
                 client_info.addr_size);

    if (len < 0)
        err("send_data() error: %d(%s)", errno, strerror(errno));

    return len;
}

/* EventHandler overrides */

int Transport::handle_event(int fd, uint32_t events, bool &stop)
{
    RecvData data;
    int ret = recv_data(fd, data);

    if (ret < 0) {
        if (errno == EINTR) return -1;
        if (errno == EBADF) {
            err("transport::recv_data returned EBADF. terminate");
            stop = true;
            return -1;
        }

        dbg("transport::recv_data = %d, errno = %d(%s)", ret, errno, strerror(errno));
        //!TODO: handle timeout, etc
        return -1;
    }

    if (handler != nullptr)
        handler->on_data_received(this, data);

    return 0;
}
