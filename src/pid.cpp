#include "pid.h"

#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <csignal>

#include "log.h"
#include "cfg.h"

void create_pid_file(){
	int file_pid;

	if(!cfg.pid) return;
	if(!cfg.pid_file) return;

	FILE* f = fopen(cfg.pid_file, "r");
	if(f) {
		if(fscanf(f,"%d",&file_pid)==1){
			if(file_pid!=cfg.pid){
				if(getpgid(file_pid) < 0) {
					cerr("there is staled staled pid file '%s' with pid %d. overwrite it",
						 cfg.pid_file,file_pid);
				} else {
					cerr("there is another instance with pid: %d",
						  file_pid);
					exit(EXIT_FAILURE);
				}
			}
		} else {
			cerr("garbage in pid file '%s'",cfg.pid_file);
			exit(EXIT_FAILURE);
		}
		fclose(f);
	}

	f = fopen(cfg.pid_file, "w");
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
    fclose(f);
		return;
	}

	if(cfg.pid!=file_pid){
		//err("pid in pid_file doesn't matched with our own. skip unlink",cfg.pid_file);
    fclose(f);
		return;
	}
  fclose(f);
	unlink(cfg.pid_file);
}
