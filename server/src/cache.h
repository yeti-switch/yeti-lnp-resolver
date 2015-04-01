#pragma once

#include "thread.h"

#include "singleton.h"

#include <string>
#include <list>
#include <utility>

#include <pqxx/pqxx>

using std::string;
//using std::list;
using std::pair;

struct cache_entry {
	int database_id;
	string dst,lrn, data;
	cache_entry(int _database_id, string _dst, string _lrn, string _data):
		database_id(_database_id),
		dst(_dst),
		lrn(_lrn),
		data(_data) {}
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

  public:
	void run();
	void sync(cache_entry *e);

	_cache();
	~_cache();
};

typedef singleton<_cache> lnp_cache;

