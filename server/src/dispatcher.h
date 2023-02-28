#pragma once

#include "log.h"
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include <memory>
#include <string>

#include <cstring>

#include <confuse.h>

#include "ResolverException.h"
#include "Resolver.h"
#include "singleton.h"

class _dispatcher {
    int nn_sock_fd;
    int udp_sock_fd;
    int stop_event_fd;

    char* create_reply_for_msg(char *msg, int len);

    char *process_message(const char *req, int req_len, int &request_type);
    char *create_tagged_reply(const CDriver::SResult_t &r);
    char *create_json_reply(const CDriver::SResult_t &r);

    char *create_error_reply(int type, const ECErrorId code,const std::string &description);
    char *create_tagged_error_reply(const ECErrorId code,const std::string &s);
    char *create_json_error_reply(const ECErrorId code,const std::string &data);

  protected:
    void dispose() {}

  public:
    _dispatcher();

    void loop();
    void stop();
};

typedef singleton<_dispatcher> dispatcher;
