#pragma once

#include "thread.h"
#include "singleton.h"

#include <string>
#include <list>
#include <utility>

#include <pqxx/pqxx>

#include "Resolver.h"

using std::string;
//using std::list;
using std::pair;

struct cache_entry {
    CDriverCfg::CfgUniqId_t database_id;
    string dst,lrn, data, tag;
    cache_entry(CDriverCfg::CfgUniqId_t _database_id, const string &_dst, const CDriver::SResult_t & r):
		database_id(_database_id),
		dst(_dst),
        lrn(r.localNumberPortability),
        data(r.rawData),
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

