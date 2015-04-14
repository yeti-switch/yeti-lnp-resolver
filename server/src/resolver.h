#pragma once

#include "singleton.h"

#include <string>
using std::string;

#include <memory>
using std::auto_ptr;

#include <map>

#include "resolver_driver.h"

class _resolver {
	struct database_entry {
		string name;
		auto_ptr<resolver_driver> driver;
		database_entry(string name, resolver_driver *driver):
			name(name), driver(driver) { }
	};
	typedef std::map<int,std::unique_ptr<database_entry>> databases_t;
	databases_t databases;
	mutex databases_m;

protected:
  void dispose() {}

  public:
	_resolver();
	~_resolver();
	void launch();
	void stop();

	bool load_resolve_drivers(databases_t &db);
	bool configure();
	void resolve(int database_id, const string &in, string &out);
};

typedef singleton<_resolver> resolver;


