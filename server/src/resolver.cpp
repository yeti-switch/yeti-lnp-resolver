#include "resolver.h"
#include <pqxx/pqxx>
#include "cfg.h"

#include "cache.h"

#include "resolver_driver_sip.h"

#define LOAD_LNP_STMT "SELECT * FROM load_lnp_databases()"

resolver_driver::resolver_driver(string host, unsigned short port, int id):
	host(host), port(port), id(id)
{}

resolver_driver::~resolver_driver()
{}

_resolver::_resolver()
{}

_resolver::~_resolver()
{
	for(databases_t::const_iterator i = databases.begin();
		i!=databases.end();++i) delete i->second;
	databases.clear();
}

void _resolver::stop()
{
	for(databases_t::const_iterator i = databases.begin();
		i!=databases.end();++i)
	{
		i->second->driver->on_stop();
	}
}

bool _resolver::configure()
{
	//load from database init resolvers map
	bool ret = false;

	try {
		pqxx::result r;
		pqxx::connection c(cfg.db.get_conn_string());
		c.set_variable("search_path",cfg.db.schema+", public");

		pqxx::work t(c);
		r = t.exec(LOAD_LNP_STMT);
		for(pqxx::result::size_type i = 0; i < r.size();++i){
			const pqxx::result::tuple &row = r[i];
			int database_id = row["o_id"].as<int>(),
				driver_id = row["o_driver_id"].as<int>(),
				port = row["o_port"].as<int>(0);
			string name = row["o_driver_id"].c_str(),
					host = row["o_host"].c_str();

			resolver_driver *d = NULL;
			switch(driver_id){
			case RESOLVER_DRIVER_SIP:
				d = new resolver_driver_sip(host,port);
				break;
			default: {
				err("unsupported driver_id: %d",driver_id);
				continue;
			}}

			if(port){
				dbg("add database %d:%s %s <%s:%d>",
					database_id,name.c_str(),driver_id2name(driver_id),
					host.c_str(),port);
			} else {
				dbg("add database %d:%s %s <%s>",
					database_id,name.c_str(),driver_id2name(driver_id),
					host.c_str());
			}

			databases.insert(std::make_pair<int,database_entry *>(
						 database_id,
						 new database_entry(name,d)
						 ));
		}
		ret = true;
	} catch(const pqxx::pqxx_exception &e){
		err("pqxx_exception: %s ",e.base().what());
	} catch(...){
		err("unexpected exception");
	}
	return ret;
}

void _resolver::resolve(int database_id, const string &in, string &out)
{
	databases_t::iterator i = databases.find(database_id);
	if(i==databases.end()){
		throw resolve_exception(1,"uknown database id");
	}
	string data;

	try {
		i->second->driver->resolve(in,out,data);
		dbg("resolved: %s -> %s using database %d",
			in.c_str(),out.c_str(),database_id);
	} catch(resolve_exception &e){
		throw e;
	} catch(...){
		dbg("unknown resolve exeption");
		throw resolve_exception(1,"internal error");
	}
	lnp_cache::instance()->sync(new cache_entry(database_id,in,out,data));
}

