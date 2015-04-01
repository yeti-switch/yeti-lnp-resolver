#pragma once

#include <string>
using std::string;

#define RESOLVE_EXCEPTION_DRIVER 7
#define RESOLVE_EXCEPTON_REASON_UNEXPECTED "unexpected reply"

struct resolve_exception {
	int code;
	std::string reason;
	resolve_exception(int code, std::string reason):
		code(code), reason(reason) {}
};

enum drivers_t {
	RESOLVER_DRIVER_SIP = 1
};
static const char *driver_id2name(int id)
{
	static const char *sip = "SIP/301-302";
	static const char *unknown = "unknown";
	switch(id){
		case RESOLVER_DRIVER_SIP: return sip; break;
		default: return unknown;
	}
}

class resolver_driver {
	string host;
	unsigned short port;
	int id;
  public:
	resolver_driver(string host, unsigned short port, int id = -1);
	~resolver_driver();
	virtual void on_stop() {}

	virtual void resolve(const string &in, string &out, string &data) = 0;

	const string& get_host() { return host; }
	unsigned short get_port() { return port; }
	int get_id() { return id; }
};
