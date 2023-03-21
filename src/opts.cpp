#include "opts.h"

#include "log.h"
#include "cfg.h"
#include "usage.h"

#include <cstdlib>
#include <ctype.h>
#include <unistd.h>

void parse_opts(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "hfp:"))!=-1){ switch(c){
		case 'h': usage(); exit(EXIT_SUCCESS); break;
		case 'f': cfg.daemonize = false; break;
		case 'p': cfg.pid_file = optarg; break;
		case '?':
			switch(optopt){
			case 0:
				continue;
				break;
			case 'p':
				cerr("Option -%c requires an argument. use -h for details", optopt);
				break;
			default:
				if(isprint(optopt)) cerr("Unknown option `-%c'.",optopt)
				else cerr("Unknown option character `\\x%x'.",optopt);
			}
			exit(EXIT_FAILURE);
		default:
			abort();
	}}
}
