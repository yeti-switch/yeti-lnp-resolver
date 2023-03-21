#include "log.h"
#include <execinfo.h>
#include <cstdlib>

#define DAEMON_NAME "yeti-lnp-resolver"

volatile int log_level = L_ERR;

void open_log(){
	openlog(DAEMON_NAME,LOG_PID,LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_DEBUG));
}

void close_log(){
	closelog();
}

void log_stacktrace()
{
	void* callstack[128];
	int i, frames = backtrace(callstack, 128);
	char** strs = backtrace_symbols(callstack, frames);
	for (i = 0; i < frames; ++i)
		err("stack-trace(%i): %s", i, strs[i]);
	free(strs);
}
