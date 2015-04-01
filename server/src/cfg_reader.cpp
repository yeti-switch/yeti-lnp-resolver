#include "cfg_reader.h"

#include "log.h"
#include "cfg.h"
#include <errno.h>
#include <cstring>

cfg_opt_t daemon_opts[] = {
	CFG_STR("listen_uri","tcp://127.0.0.1:4444",CFGF_NONE),
	CFG_END()
};

cfg_opt_t sip_opts[] = {
	CFG_STR("contact_user","yeti-lnp-resolver",CFGF_NONE),
	CFG_STR("from_uri","sip:yeti-lnp-resolver@localhost",CFGF_NONE),
	CFG_STR("from_name","yeti-lnp-resolver",CFGF_NONE),
	CFG_END()
};

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
	CFG_SEC("daemon",daemon_opts,CFGF_NONE),
	CFG_SEC("db",db_opts,CFGF_NONE),
	CFG_SEC("sip",sip_opts,CFGF_NONE),
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
	cfg_t *s;
	//database section
	s = cfg_getsec(c,"db");

	global_cfg_t::db_cfg &gdbc = cfg.db;

	gdbc.host = cfg_getstr(s,"host");
	gdbc.port = cfg_getint(s,"port");
	gdbc.user = cfg_getstr(s,"username");
	gdbc.pass = cfg_getstr(s,"password");
	gdbc.database = cfg_getstr(s,"database");
	gdbc.schema = cfg_getstr(s,"schema");
	gdbc.timeout = cfg_getint(s,"conn_timeout");
	gdbc.check_timeout = cfg_getint(s,"check_interval");

	//sip section
	s = cfg_getsec(c,"sip");
	cfg.sip.contact = cfg_getstr(s,"contact_user");
	cfg.sip.from_uri = cfg_getstr(s,"from_uri");
	cfg.sip.from_name = cfg_getstr(s,"from_name");

	//daemon section
	s = cfg_getsec(c,"daemon");
	cfg.bind_url = cfg_getstr(s,"listen_uri");

	return true;
}
