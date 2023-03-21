#pragma once

#include <syslog.h>
#include <cstdio>

#include <unistd.h>
#include <sys/syscall.h>

enum log_levels {
	L_ERR = 0,
	L_WARN,
	L_INFO,
	L_DBG
};
extern volatile int log_level;

#define cerr(fmt,args...) fprintf(stderr,"error: " fmt "\n",##args);

#ifdef VERBOSE_LOGGING
//#define _LOG(level,level_str,fmt,args...) syslog(level,"[%u] " level_str "%s:%d:%s " fmt ,syscall(__NR_gettid), __FILENAME__,__LINE__,__PRETTY_FUNCTION__,##args);
#define _LOG(level,level_str,fmt,args...) syslog(level,"[%ld] " level_str "%s:%d: " fmt ,syscall(__NR_gettid), __FILE__,__LINE__,##args);
#else
#define _LOG(level,level_str,fmt,args...) syslog(level,fmt,##args);
#endif

#define err(fmt,args...) _LOG(LOG_ERR,"error: ",fmt,##args);
#define warn(fmt,args...) if(log_level > L_ERR) _LOG(LOG_ERR,"warning: ",fmt,##args);
#define info(fmt,args...) if(log_level > L_WARN) _LOG(LOG_INFO,"info: ",fmt,##args);
#define dbg(fmt,args...) if(log_level > L_INFO) _LOG(LOG_DEBUG,"dbg: ",fmt,##args);

void open_log();
void close_log();

#include <execinfo.h>
void log_stacktrace();
