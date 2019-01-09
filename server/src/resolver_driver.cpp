#include "resolver_driver.h"
#include "log.h"

#define DEFAULT_REPLY_TIMEOUT 4000

resolver_driver::driver_cfg::driver_cfg(const pqxx::result::tuple &r)
{
	database_id = r["o_id"].as<int>(),
	driver_id = r["o_driver_id"].as<int>(),
	port = r["o_port"].as<int>(0);
	name = r["o_name"].c_str(),
	host = r["o_host"].c_str();
	thinq_token = r["o_thinq_token"].c_str();
	thinq_username = r["o_thinq_username"].c_str();
    data_path = r["o_csv_file"].c_str();
	timeout = r["o_timeout"].as<unsigned int>(DEFAULT_REPLY_TIMEOUT);
    try {
        alkazar_key = r["o_alkazar_key"].c_str();
    } catch(...) {
        dbg("no field o_alkazar_key in DB response for driver: %d %s. leave alkazar_key empty",
             database_id,driver_id2name(driver_id));
    }
}

resolver_driver::resolver_driver(const resolver_driver::driver_cfg &dcfg):
	_cfg(dcfg)
{
	//dbg("%p",this);
}

resolver_driver::~resolver_driver()
{
	//dbg("%p",this);
}

void resolver_driver::show_info(int map_id)
{
    if(_cfg.port){
        info("id: %d, driver: %s, addr: <%s:%d>, timeout: %u",
             map_id,driver_id2name(_cfg.driver_id),
            _cfg.host.c_str(),_cfg.port,_cfg.timeout);
    } else {
        info("id: %d, driver: %s, addr: <%s>, timeout: %u",
             map_id,driver_id2name(_cfg.driver_id),
            _cfg.host.c_str(),_cfg.timeout);
    }
}
