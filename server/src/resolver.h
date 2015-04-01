#pragma once

#include "singleton.h"

#include <string>
using std::string;

#include <map>

#include "resolver_driver.h"

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
	void stop();

	bool configure();
	void resolve(int database_id, const string &in, string &out);
};

typedef singleton<_resolver> resolver;


