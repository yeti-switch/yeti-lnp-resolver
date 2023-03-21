#pragma once

#include <string>
using std::string;

#include <list>

struct global_cfg_t {
	bool daemonize;
	int pid;
	char *pid_file;
	std::list<string> bind_urls;

	struct db_cfg {
		string host,user,pass,database,schema;
		unsigned int port, timeout, check_timeout;
		string get_conn_string();
	} db;

	struct sip_cfg {
		string contact, from_uri, from_name;
	} sip;

	struct prometheus_cfg {
		string host;
		unsigned int port;
	} prometheus;

	global_cfg_t();

	bool validate_opts();
};

extern global_cfg_t cfg;

