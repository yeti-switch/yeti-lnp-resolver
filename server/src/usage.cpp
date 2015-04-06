#include "usage.h"

#include <cstdio>

#include "pid.h"

void usage(){
#define opt(name,desc) "\n   -"#name" "desc
#define opt_ext(name,desc,ext) opt(name,desc)"\n         "ext
#define opt_arg(name,arg,desc) "\n   -"#name" <"arg"> "desc
#define opt_arg_ext(name,arg,desc,ext) opt_arg(name,arg,desc)"\n         "ext
	printf(
	"usage:"
		opt(h,"this help")
		opt(f,"run foreground (don't daemonize)")
		opt_arg_ext(p,"pid_file","use custom pid file","default: "DEFAULT_PID_FILE)
	"\n\n"
	);
#undef opt_arg_ext
#undef opt_arg
#undef opt_ext
#undef opt
}
