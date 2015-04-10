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

  public:
	_resolver();
	~_resolver();
	void stop();

	bool configure();
	void resolve(int database_id, const string &in, string &out);
};

typedef singleton<_resolver> resolver;


