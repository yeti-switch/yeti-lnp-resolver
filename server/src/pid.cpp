#include "pid.h"

#include <cstdio>
#include <cstdlib>

#include <unistd.h>

#include "log.h"
#include "cfg.h"

void create_pid_file(){
	if(!cfg.pid) return;
	if(!cfg.pid_file) return;

	FILE* f = fopen(cfg.pid_file, "w");
	if(!f){
		cerr("can't create pid_file: '%s'",cfg.pid_file);
		exit(EXIT_FAILURE);
	}
	fprintf(f,"%d",cfg.pid);
	fclose(f);
}

void delete_pid_file(){
	if(!cfg.pid) return;
	if(!cfg.pid_file) return;
	if(!cfg.daemonize) return;

	FILE* f = fopen(cfg.pid_file, "r");
	if(!f){
		err("can't open own pid_file '%s",cfg.pid_file);
		return;
	}

	int file_pid;
	if(fscanf(f,"%d",&file_pid)!=1){
		err("can't get pid from pid_file '%s",cfg.pid_file);
		return;
	}

	if(cfg.pid!=file_pid){
		err("pid in pid_file doesn't matched with our own. skip unlink",cfg.pid_file);
		return;
	}
	unlink(cfg.pid_file);
}
