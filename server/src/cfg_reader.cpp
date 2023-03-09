#include "cfg_reader.h"

#include "log.h"
#include "cfg.h"
#include <errno.h>
#include <cstring>

#include <vector>
using std::vector;
#include <algorithm>

#define LNP_CFG_PART "lnp"

cfg_opt_t daemon_section_opts[] = {
	CFG_STR_LIST((char*)"listen",(char *)"{tcp://127.0.0.1:4444}",CFGF_NODEFAULT),
	CFG_INT((char *)"log_level",L_INFO, CFGF_NODEFAULT),
	CFG_END()
};

cfg_opt_t lnp_section_db_opts[] = {
	CFG_INT("port",0,CFGF_NODEFAULT),
	CFG_STR("host",nullptr,CFGF_NODEFAULT),
	CFG_STR("name",nullptr,CFGF_NODEFAULT),
	CFG_STR("user",nullptr,CFGF_NODEFAULT),
	CFG_STR("pass",nullptr,CFGF_NODEFAULT),
	CFG_INT("conn_timeout",0,CFGF_NODEFAULT),
	CFG_INT("check_interval",0,CFGF_NODEFAULT),
	CFG_STR("schema",nullptr,CFGF_NODEFAULT),
	CFG_END()
};

cfg_opt_t lnp_section_sip_opts[] = {
	CFG_STR("contact_user","yeti-lnp-resolver",CFGF_NONE),
	CFG_STR("from_uri","sip:yeti-lnp-resolver@localhost",CFGF_NONE),
	CFG_STR("from_name","yeti-lnp-resolver",CFGF_NONE),
	CFG_END()
};

cfg_opt_t prometheus_section_opts[] = {
	CFG_INT("port",9091,CFGF_NONE),
	CFG_STR("host","127.0.0.1",CFGF_NONE),
	CFG_END()
};

cfg_opt_t opts[] = {
	CFG_INT("node_id",0,CFGF_NODEFAULT),
	CFG_SEC("daemon",daemon_section_opts,CFGF_NONE),
	CFG_SEC("db",lnp_section_db_opts,CFGF_NONE),
	CFG_SEC("sip",lnp_section_sip_opts,CFGF_NONE),
	CFG_SEC("prometheus",prometheus_section_opts,CFGF_NONE),
	CFG_END()
};

#define LOG_BUF_SIZE 2048
void cfg_reader_error(cfg_t *c, const char *fmt, va_list ap)
{
	char buf[LOG_BUF_SIZE];
	int ret = vsnprintf(buf,LOG_BUF_SIZE,fmt,ap);
	fprintf(stderr,"config_error: %.*s\n",ret,buf);
	err("%.*s",ret,buf);
}

bool load_cfg(const char *path)
{
#define with_section(SECTION_NAME) if(cfg_t *s = cfg_getsec(c, SECTION_NAME))
	bool ret = false;
	//remote_cfg_reader r;
	cfg_t *c = cfg_init(opts, CFGF_NONE);
	cfg_set_error_function(c,cfg_reader_error);

	switch(cfg_parse(c, path)) {
	case CFG_SUCCESS:
		break;
	case CFG_FILE_ERROR:
		err("configuration file: '%s' could not be read: %s", path, strerror(errno));
		goto out;
	case CFG_PARSE_ERROR:
		err("configuration file '%s' parse error: %s", path, strerror(errno));
		goto out;
	default:
		err("unexpected error on configuration file '%s' processing", path);
	}

	with_section("daemon") {
		for(unsigned int i = 0; i < cfg_size(s, "listen"); i++) {
			cfg.bind_urls.emplace_back(cfg_getnstr(s, "listen", i));
		}

		log_level = cfg_getint(s,"log_level");
		if(log_level < L_ERR) log_level = L_ERR;
		if(log_level > L_DBG) log_level = L_DBG;
	}

	with_section("db") {
		cfg.db.host = cfg_getstr(s, "host");
		cfg.db.port = cfg_getint(s, "port");

		cfg.db.user = cfg_getstr(s, "user");
		cfg.db.pass = cfg_getstr(s, "pass");

		cfg.db.database = cfg_getstr(s, "name");
		cfg.db.schema = cfg_getstr(s, "schema");

		cfg.db.timeout = cfg_getint(s, "conn_timeout");
		cfg.db.check_timeout = cfg_getint(s, "check_interval");
	}

	with_section("sip") {
		cfg.sip.contact = cfg_getstr(s, "contact_user");
		cfg.sip.from_uri = cfg_getstr(s, "from_uri");
		cfg.sip.from_name = cfg_getstr(s, "from_name");
	}
	
	with_section("prometheus") {
		cfg.prometheus.host = cfg_getstr(s, "host");
		cfg.prometheus.port = cfg_getint(s, "port");
	}

	ret = true;
out:
	cfg_free(c);
	return ret;
#undef with_section
}
