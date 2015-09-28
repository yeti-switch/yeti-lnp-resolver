#pragma once

#include "thread.h"
#include "singleton.h"

#include <string>
#include <list>
#include <utility>

#include <pqxx/pqxx>

#include "resolver.h"

using std::string;
//using std::list;
using std::pair;

struct cache_entry {
	int database_id;
    string dst,lrn, data, tag;
    cache_entry(int _database_id, const string &_dst, const resolver_driver::result &r):
		database_id(_database_id),
		dst(_dst),
        lrn(r.lrn),
        data(r.raw_data),
        tag(r.tag) {}
};

class _cache: public thread {
	typedef std::list<cache_entry *> queue_t;
	queue_t q;
	mutex q_m;
	condition<bool> q_run;
	condition<bool> stopped;
	bool gotostop;

	pqxx::connection *c;

	void prepare_queries(pqxx::connection *conn);
	int _connect_db(pqxx::connection **conn, string conn_str);
	int connect_db();

	bool update_cache(cache_entry *e);

  protected:
	void on_stop();
	void dispose() {}

  public:
	void run();
	void sync(cache_entry *e);

	_cache();
	~_cache();
};

typedef singleton<_cache> lnp_cache;

