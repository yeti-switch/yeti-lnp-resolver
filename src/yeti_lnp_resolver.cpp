#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "cfg.h"
#include "sig.h"
#include "cache.h"
#include "dispatcher/Dispatcher.h"
#include "statistics/prometheus/prometheus_exporter.h"

#include "cfg_reader.h"
#include "resolver/Resolver.h"
#include "transport/Transport.h"

int main(int argc,char *argv[])
{
	int ret = EXIT_FAILURE;

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

		transport::instance()->set_handler(resolver::instance());
		lnp_cache::instance()->start();
		dispatcher::instance()->loop();
		ret = 0;
	} catch(std::string &s){
		err("%s",s.c_str());
	} catch(std::exception &e) {
		err("%s\n",e.what());
	}

	info("terminated");
	close_log();

	return ret;
}

