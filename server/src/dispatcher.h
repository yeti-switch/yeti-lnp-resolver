#pragma once

#include "log.h"
#include "resolver.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include <memory>
#include <string>

#include <cstring>

#include <confuse.h>

#include "singleton.h"

class _dispatcher {
	int s;
	int stop_event_fd;

	int process_peer(char *msg, int len);
	void str2reply(char *&msg,int &len,int code,const std::string &s);
	void make_reply(char *&msg,int &len,const resolver_driver::result &r);
	void create_reply(char *&msg, int &len, const char *req, int req_len);
	void create_error_reply(char *&msg, int &len,int code, std::string description);

  protected:
	void dispose() {}
  public:
	_dispatcher();

	void loop();
	void stop();
};

typedef singleton<_dispatcher> dispatcher;
