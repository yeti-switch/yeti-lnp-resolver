#include "sig.h"

#include <signal.h>

#include "dispatcher.h"
#include "cache.h"
#include "resolver.h"

void sig_handler(int sig){
	dbg("got sig = %d",sig);
	if(sig==SIGHUP){
		resolver::instance()->configure();
		return;
	}
	dispatcher::instance()->stop();
	resolver::instance()->stop();
	lnp_cache::instance()->stop();
}

void set_sighandlers() {
	static int sigs[] = {SIGTERM, SIGINT, SIGHUP, 0 };
	for (int* sig = sigs; *sig; sig++) {
		if(signal(*sig, sig_handler) == SIG_IGN) signal(*sig,SIG_IGN);
	}
}
