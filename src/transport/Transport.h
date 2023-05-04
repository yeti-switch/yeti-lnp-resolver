#pragma once

#include "dispatcher/EventHandler.h"
#include "singleton.h"

#include <stdlib.h>
#include <arpa/inet.h>
#include <string>
#include <utility>

using namespace std;

#define MSG_SZ 1024 * 2

class Transport;
using transport = singleton<Transport>;

typedef struct ClientInfo {
    struct sockaddr_in addr;
    socklen_t addr_size;
} ClientInfo;

typedef struct RecvData {
    ClientInfo client_info;
    char data[MSG_SZ];
    size_t length;
} RecvData;

class TransportHandler {
public:
    virtual void on_data_received(Transport *transport, const RecvData &recv_data) = 0;
};

class Transport: public EventHandler
{
public:
    Transport();
    virtual ~Transport();

    void set_handler(TransportHandler *transport_handler);
    int send_data(const string &data, const ClientInfo &client_info);
    int send_data(const void *buf, size_t size, const ClientInfo &client_info);

    /* EventHandler overrides */
    int handle_event(int fd, uint32_t events, bool &stop) override;

protected:
    int init_sock();
    int bind_sock();
    int bind_sock_to(const char *host, int port);
    int recv_data(RecvData &out);
    int shutdown_sock();

private:
    int sock_fd = -1;
    TransportHandler *handler;
};
