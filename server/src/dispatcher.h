#pragma once

#include "log.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include <memory>
#include <string>

#include <cstring>

#include <confuse.h>

#include "singleton.h"

class _dispatcher {
	bool _stop;
	int s;

	int process_peer(char *msg, int len);
	void str2reply(char *&msg,int &len,int code,const std::string &s);
	void create_reply(char *&msg, int &len, const char *req, int req_len);
	void create_error_reply(char *&msg, int &len,int code, std::string description);

  public:
	_dispatcher();

	void loop();
	void stop() { _stop = true; }
};

typedef singleton<_dispatcher> dispatcher;
