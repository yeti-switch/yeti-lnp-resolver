#pragma once

#include "singleton.h"

#include <string>
using std::string;

#include <map>

struct resolve_exception {
	int code;
	std::string reason;
	resolve_exception(int code, std::string reason):
		code(code), reason(reason) {}
};

class resolver_driver {
	string host;
	unsigned short port;
	int id;
  public:
	resolver_driver(string host, unsigned short port, int id = -1);
	~resolver_driver();

	virtual void resolve(const string &in, string &out) = 0;
};

class resolver_driver_sip: public resolver_driver {
  public:
	resolver_driver_sip(string host, unsigned short port);
	void resolve(const string &in, string &out);
};

class _resolver {

  struct database_entry {
	  string name;
	  resolver_driver *driver;
	  database_entry(string name, resolver_driver *driver):
		  name(name), driver(driver) {}
	  ~database_entry(){ delete driver; }
  };
  typedef std::map<int,database_entry *> databases_t;
  databases_t databases;

  public:
	_resolver();
	~_resolver();

	bool configure();
	void resolve(int database_id, const string &in, string &out);
};

typedef singleton<_resolver> resolver;


