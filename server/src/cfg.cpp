#include "cfg.h"
#include "log.h"

#include <sstream>

struct global_cfg_t cfg;

#define DEFAULT_PID_FILE	"/var/run/yeti_mgmt.pid"

global_cfg_t::global_cfg_t():
	daemonize(true),
	pid(0),
	pid_file(0)
{}

bool global_cfg_t::validate_opts()
{
	return true;
}

string global_cfg_t::db_cfg::get_conn_string() {
	std::ostringstream ss;
	ss << "host = " << host <<
		  " port = " << std::dec << port <<
		  " user = " << user <<
		  " dbname = " << database <<
		  " password = " << pass <<
		  " connect_timeout  = " << timeout;
	return ss.str();
}

