#include "cfg_reader.h"

#include "log.h"
#include "cfg.h"
#include <errno.h>
#include <cstring>

#include "yeti/yeticc.h"

#include <vector>
using std::vector;
#include <algorithm>

#define LNP_CFG_PART "lnp"

cfg_opt_t opts[] = {
	CFG_INT("node_id",0,CFGF_NODEFAULT),
	CFG_INT("cfg_timeout",1000,CFGF_NONE),
	CFG_STR_LIST("cfg_urls","{tcp://127.0.0.1:4445}",CFGF_NONE),
	CFG_END()
};

class cfg_value {
  public:
	enum {
		Undef = 0,
		Int,
		String
	};
  private:
	short type;
	union {
		long int	v_int;
		const char *v_str;
	};
  public:

	cfg_value(): type(Undef) {}

	cfg_value(const cfg_value& v) {
		type = Undef;
		*this = v;
	}

	cfg_value& operator=(const cfg_value& v){
	  if(this == &v)
	    return *this;
		type = v.type;
		switch(type){
		case Int: v_int = v.v_int; break;
		case String: v_str = strdup(v.v_str); break;
		case Undef: break;
		default: throw std::string("cfg_value: uknown rhs type");
		}
		return *this;
	}

	cfg_value(const int &v): type(Int), v_int(v) {}
	cfg_value(const long int &v): type(Int), v_int(v) {}

	cfg_value(const char *v): type(String) { v_str = strdup(v); }
	cfg_value(const std::string &v): type(String) { v_str = strdup(v.c_str()); }

	template <typename T>
	inline T get() const;

	std::string asString() const;

	~cfg_value(){
		switch(type){
		case String: free((void *)v_str); break;
		default: break;
		}
		type = Undef;
	}
};

template<>
inline std::string cfg_value::get() const {
	switch(type){
	case String: return std::string(v_str); break;
	case Int: {
		return std::to_string(v_int);
	} break;
	default: return std::string();
	}
}

template<>
inline int cfg_value::get() const {
	switch(type){
	case String:
		err("attempt to return string as integer");
		return 0;
		break;
	case Int:
		return v_int;
		break;
	default:
		return 0;
	}
}

class remote_cfg_reader : public yeti::cfg::reader {
  private:
	typedef std::map<std::string,cfg_value> cfg_keys_t;
	cfg_keys_t keys;
  public:
	void on_key_value_param(const string &name,const string &value){
		keys[name] = value;
	}
	void on_key_value_param(const string &name,int value){
		keys[name] = value;
	}
	const cfg_value &get_value(const char *s){
		cfg_keys_t::const_iterator i = keys.find(s);
		if(i==keys.end()){
			err("key '%s' missed in loaded configuration",s);
			throw std::string("configuration error",s);
		}
		return i->second;
	}
};

std::list<string> explode2list(const string& s, const string& delim,
				const bool keep_empty = false) {
	std::list<string> result;
	if (delim.empty()) {
		result.push_back(s);
		return result;
	}
	string::const_iterator substart = s.begin(), subend;
	while (true) {
		subend = std::search(substart, s.end(), delim.begin(), delim.end());
		string temp(substart, subend);
		if (keep_empty || !temp.empty()) {
			result.push_back(temp);
		}
		if (subend == s.end()) {
			break;
		}
		substart = subend + delim.size();
	}
	return result;
}

#define LOG_BUF_SIZE 2048
void cfg_reader_error(cfg_t *c, const char *fmt, va_list ap)
{
	char buf[LOG_BUF_SIZE];
	int ret = vsnprintf(buf,LOG_BUF_SIZE,fmt,ap);
	fprintf(stderr,"config_error: %.*s\n",ret,buf);
	err("%.*s",ret,buf);
}

bool apply(remote_cfg_reader &r){
#define str_val(dst,src) dst = r.get_value(#src).get<string>();
#define int_val(dst,src) dst = r.get_value(#src).get<int>();

	global_cfg_t::db_cfg &gdbc = cfg.db;

	str_val(gdbc.host,db.host);
	int_val(gdbc.port,db.port);
	str_val(gdbc.user,db.user);
	str_val(gdbc.pass,db.pass);
	str_val(gdbc.database,db.name);
	str_val(gdbc.schema ,db.schema);
	int_val(gdbc.timeout,db.conn_timeout);
	int_val(gdbc.check_timeout,db.check_interval);

	str_val(cfg.sip.contact,sip.contact_user);
	str_val(cfg.sip.from_uri,sip.from_uri);
	str_val(cfg.sip.from_name ,sip.from_name);

	cfg.bind_urls = explode2list(r.get_value("daemon.listen").get<string>(),",");

	int_val(log_level,daemon.log_level);
	if(log_level < L_ERR) log_level = L_ERR;
	if(log_level > L_DBG) log_level = L_DBG;

	return true;
#undef str_val
#undef int_val
}

bool load_cfg(const char *path)
{
	bool ret = false;
	remote_cfg_reader r;
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

	r.set_cfg_part(LNP_CFG_PART);
	r.set_node_id(cfg_getint(c,"node_id"));
	r.set_timeout(cfg_getint(c,"cfg_timeout"));
	for(unsigned int i = 0; i < cfg_size(c, "cfg_urls"); i++){
		r.add_url(cfg_getnstr(c, "cfg_urls", i));
	}

	try {
		r.load();
	} catch(yeti::cfg::server_exception &e){
		err("can't load yeti config: %d %s",e.code,e.what());
		goto out;
	} catch(std::exception &e){
		err("can't load yeti config: %s",e.what());
		goto out;
	}
	ret = apply(r);
out:
	cfg_free(c);
	return ret;
}
