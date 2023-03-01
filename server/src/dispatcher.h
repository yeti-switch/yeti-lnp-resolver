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

    std::string create_reply_for_msg(char *msg, int len);

    std::string process_message(const char *req, int req_len, int &request_type);
    std::string create_tagged_reply(const CDriver::SResult_t &r);
    std::string create_json_reply(const CDriver::SResult_t &r);

    std::string create_error_reply(int type, const ECErrorId code,const std::string &description);
    std::string create_tagged_error_reply(const ECErrorId code,const std::string &s);
    std::string create_json_error_reply(const ECErrorId code,const std::string &data);

  protected:
    void dispose() {}

  public:
    _dispatcher();

    void loop();
    void stop();
};

typedef singleton<_dispatcher> dispatcher;
