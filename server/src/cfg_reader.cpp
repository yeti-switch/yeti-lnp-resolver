#include "cfg_reader.h"

#include "log.h"
#include "cfg.h"
#include <errno.h>
#include <cstring>

cfg_opt_t db_opts[] = {
	CFG_STR("host","127.0.0.1",CFGF_NONE),
	CFG_INT("port",5432,CFGF_NONE),
	CFG_STR("username","auth_user",CFGF_NONE),
	CFG_STR("password","auth_pass",CFGF_NONE),
	CFG_STR("database","yeti",CFGF_NONE),
	CFG_STR("schema","switch",CFGF_NONE),
	CFG_INT("conn_timeout",0,CFGF_NONE),
	CFG_INT("check_interval",5000 /*5sec*/,CFGF_NONE),
	CFG_END()
};

cfg_opt_t opts[] = {
	CFG_SEC("db",db_opts,CFGF_NONE),
	CFG_END()
};

cfg_reader::cfg_reader()
{}

cfg_reader::~cfg_reader()
{}

#define LOG_BUF_SIZE 2048
void cfg_reader_error(cfg_t *c, const char *fmt, va_list ap)
{
	char buf[LOG_BUF_SIZE];
	int ret = vsnprintf(buf,LOG_BUF_SIZE,fmt,ap);
	fprintf(stderr,"config_error: %.*s\n",ret,buf);
	err("%.*s",ret,buf);
}

bool cfg_reader::load(const char *path)
{
	bool ret = false;

	c = cfg_init(opts, CFGF_NONE);
	cfg_set_error_function(c,cfg_reader_error);

	switch(cfg_parse(c, path)) {
	case CFG_SUCCESS:
		break;
	case CFG_FILE_ERROR:
		err("configuration file: '%s' could not be read: %s",
			path, strerror(errno));
		goto out;
	case CFG_PARSE_ERROR:
		err("configuration file '%s' parse error: %s", path);
		goto out;
	default:
		err("unexpected error on configuration file '%s' processing", path);
	}
	//cfg_print(c,stdout);
	ret = apply();
out:
	cfg_free(c);
	return ret;
}

bool cfg_reader::apply(){
	cfg_t *dbc;
	//database section
	dbc = cfg_getsec(c,"db");

	global_cfg_t::db_cfg &gdbc = cfg.db;

	gdbc.host = cfg_getstr(dbc,"host");
	gdbc.port = cfg_getint(dbc,"port");
	gdbc.user = cfg_getstr(dbc,"username");
	gdbc.pass = cfg_getstr(dbc,"password");
	gdbc.database = cfg_getstr(dbc,"database");
	gdbc.schema = cfg_getstr(dbc,"schema");
	gdbc.timeout = cfg_getint(dbc,"conn_timeout");
	gdbc.check_timeout = cfg_getint(dbc,"check_interval");

	return true;
}
