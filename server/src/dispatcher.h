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
    int s;
    int stop_event_fd;

    int process_peer(char *msg, int len);

    char *str2reply(const ECErrorId code,const std::string &s);
    char *make_reply(const CDriver::SResult_t &r);
    char *create_reply(const char *req, int req_len);
    char *create_error_reply(const ECErrorId code,const std::string description);

  protected:
    void dispose() {}

  public:
    _dispatcher();

    void loop();
    void stop();
};

typedef singleton<_dispatcher> dispatcher;
