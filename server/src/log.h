#pragma once

#include <syslog.h>
#include <cstdio>

enum log_levels {
	L_ERR = 1,
	L_INFO,
	L_DBG,
};
extern int log_level;

#define cerr(fmt,args...) fprintf(stderr,"error: "fmt"\n",##args);

#ifdef VERBOSE_LOGGING
#define _LOG(level,fmt,args...) syslog(level,"%s:%d:%s "fmt,__FILENAME__,__LINE__,__PRETTY_FUNCTION__,##args);
#else
#define _LOG(level,fmt,args...) syslog(level,fmt,##args);
#endif

#define err(fmt,args...) _LOG(LOG_ERR,"error: "fmt,##args);
#define info(fmt,args...) if(log_level > L_ERR) _LOG(LOG_INFO,"info: "fmt,##args);
#define dbg(fmt,args...) if(log_level > L_INFO) _LOG(LOG_DEBUG,"dbg: "fmt,##args);

#define dbg_func(fmt,args...) dbg("%s "fmt,__PRETTY_FUNCTION__,##args);

void open_log();
void close_log();

#include <execinfo.h>
#include "log.h"

void log_stacktrace();
