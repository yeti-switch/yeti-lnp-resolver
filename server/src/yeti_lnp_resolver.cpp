#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "cfg.h"
#include "pid.h"
#include "sig.h"
#include "opts.h"
#include "usage.h"
#include "dispatcher.h"
#include "resolver.h"
#include "cache.h"

#include "cfg_reader.h"

int main(int argc,char *argv[])
{
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
		cfg_reader c;
		if(!c.load(CFG_DIR"/lnp_resolver.cfg")){
			throw std::string("can't load config");
		}
		info("start");
		if(!resolver::instance()->configure()){
			throw std::string("can't init resolvers");
		}
		lnp_cache::instance()->start();
		dispatcher::instance()->loop();
	} catch(std::string &s){
		err("%s",s.c_str());
	} catch(std::exception &e) {
		err("%s\n",e.what());
	}

	delete_pid_file();

	info("terminated");
	close_log();

	return 0;
}

