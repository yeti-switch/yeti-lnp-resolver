#include "cache.h"
#include "log.h"
#include "cfg.h"
#include <unistd.h>

#define RECONNECT_DELAY 5

#define CACHE_LNP_SQL \
	"SELECT * FROM cache_lnp_data($1,$2,$3,$4)"

#define CACHE_LNP_SQL_FMT \
	"SELECT * FROM cache_lnp_data(%d,'%s','%s',NULL)"

#define CACHE_LNP_STMT "cache_lnp"

const char *cache_lnp_args[] = {
	"smallint",
	"varchar",
	"varchar",
	"varchar",
	NULL
};

_cache::_cache():
	c(NULL),
	gotostop(false)
{}

_cache::~_cache() {}

#define RECONNECT_DELAY 5

void _cache::prepare_queries(pqxx::connection *conn)
{
	conn->set_variable("search_path",cfg.db.schema+", public");

	pqxx::prepare::declaration d = conn->prepare(CACHE_LNP_STMT, CACHE_LNP_SQL);

	const char **tp = cache_lnp_args;
	while(*tp){
		d(*tp,pqxx::prepare::treat_direct);
		tp++;
	}
	conn->prepare_now(CACHE_LNP_STMT);
}

int _cache::_connect_db(pqxx::connection **conn, string conn_str)
{
	pqxx::connection *c = NULL;
	int ret = 0;
	try{
		c = new pqxx::connection(conn_str);
		if (c->is_open()){
			prepare_queries(c);
			info("connected to database. backend pid: %d.",c->backendpid());
			ret = 1;
		}
	} catch(const pqxx::broken_connection &e){
			err("database connection exception: %s",e.what());
		delete c;
	} catch(const pqxx::undefined_function &e){
		err("database exception: undefined_function query: %s, what: %s",e.query().c_str(),e.what());
		c->disconnect();
		delete c;
		throw std::runtime_error("lnp cache exception");
	} catch(...){
	}
	*conn = c;
	return ret;
}

int _cache::connect_db()
{
	return _connect_db(&c,cfg.db.get_conn_string().c_str());
}

void _cache::run()
{
	set_name("db-cache-wr");
	if(!connect_db()){
		throw std::logic_error("can't connect to the database");
	}
	bool db_err = false;
	while(true) {
		bool qrun = q_run.wait_for_to(cfg.db.check_timeout);

		if(gotostop){
			stopped.set(true);
			return;
		}

		if(db_err || !qrun){ //check/renew connection
			if(NULL!=c){
				try {
					pqxx::work t(*c);
					t.commit();
					db_err = false;
				} catch (const pqxx::pqxx_exception &e) {
					db_err = true;
					dbg("database check error: %s",e.base().what());
					delete c;
					if(!_connect_db(&c,cfg.db.get_conn_string().c_str())){
						dbg("can't reconnect to the database");
					} else {
						db_err = false;
					}
				}
			} else {
				if(!_connect_db(&c,cfg.db.get_conn_string().c_str())){
					dbg("can't reconnect to the database");
				} else {
					db_err = false;
				}
			}
		}

		if(db_err) {
			dbg("wait %d seconds",RECONNECT_DELAY);
			sleep(RECONNECT_DELAY);
			continue;
		}

		//process queue
		q_m.lock();
		if(q.empty()){
			q_run.set(false);
			q_m.unlock();
			continue;
		}

		cache_entry *e = q.front();
		q.pop_front();

		if(q.empty())
			q_run.set(false);

		q_m.unlock();

		if(!update_cache(e)){
			err("cache update error");
			db_err = true;
		} else {
			dbg("entry %d:%s => %s was written to the database",
				e->database_id,e->dst.c_str(),e->lrn.c_str());
		}
		delete e;
	}
}

void _cache::on_stop()
{
	gotostop=true;
	q_run.set(true);
	stopped.wait_for();
	if(c) c->disconnect();
}

void _cache::sync(cache_entry *e)
{
	q_m.lock();
	q.push_back(e);
	q_m.unlock();
	q_run.set(true);
}

bool _cache::update_cache(cache_entry *e)
{
	try {
		pqxx::nontransaction tnx(*c);
		if(!tnx.prepared(CACHE_LNP_STMT).exists()){
			err("have no prepared SQL statement");
			return false;
		}

		pqxx::prepare::invocation invoc = tnx.prepared(CACHE_LNP_STMT);
		invoc(e->database_id);
		invoc(e->dst);
		invoc(e->lrn);
		invoc(); //data

		invoc.exec();

		return true;
	} catch(const pqxx::pqxx_exception &exc){
		dbg("cache update SQL exception: %s",exc.base().what());
		dbg("query: "CACHE_LNP_SQL_FMT,
			e->database_id,e->dst.c_str(),e->lrn.c_str());
		c->disconnect();
	}
	return false;
}
