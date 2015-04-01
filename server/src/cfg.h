#pragma once

#include <string>
using std::string;

struct global_cfg_t {
	bool daemonize;
	int pid;
	char *pid_file;
	char *bind_url;

	struct db_cfg {
		string host,user,pass,database,schema;
		unsigned int port, timeout, check_timeout;
		string get_conn_string();
	} db;

	struct sip_cfg {
		string contact, from_uri, from_name;
	} sip;

	global_cfg_t();

	bool validate_opts();
};

extern global_cfg_t cfg;

