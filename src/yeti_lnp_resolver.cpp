#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "cfg.h"
#include "pid.h"
#include "sig.h"
#include "opts.h"
#include "usage.h"
#include "dispatcher.h"
#include "cache.h"
#include "statistics/prometheus/prometheus_exporter.h"

#include "cfg_reader.h"
#include "Resolver.h"

int main(int argc,char *argv[])
{
	int ret = 1;

	parse_opts(argc,argv);
	if(!cfg.validate_opts()) {
		usage();
		return EXIT_FAILURE;
	}

	if(cfg.daemonize){
		if(!cfg.pid_file) cfg.pid_file = strdup(DEFAULT_PID_FILE);

		int pid;
		if ((pid=fork())<0){
			cerr("can't fork: %d, errno = %d",pid,errno);
		} else if(pid!=0) {
			return 0;
		}

		cfg.pid = getpid();
		create_pid_file();

		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
	}
	freopen("/dev/null", "r", stdin);

	open_log();

	set_sighandlers();
	try {
		if(!load_cfg(CFG_DIR "/lnp_resolver.cfg")){
			throw std::string("can't load config");
		}
		info("start");
		prometheus_exporter::instance()->start();
		if(!resolver::instance()->configure()){
			throw std::string("can't init resolvers");
		}
		lnp_cache::instance()->start();
		dispatcher::instance()->loop();
		ret = 0;
	} catch(std::string &s){
		err("%s",s.c_str());
	} catch(std::exception &e) {
		err("%s\n",e.what());
	}

	delete_pid_file();

	info("terminated");
	close_log();

	return ret;
}

