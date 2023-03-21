#pragma once

#define DEFAULT_PID_FILE	"/var/run/yeti_lnp_resolver.pid"

void create_pid_file();
void delete_pid_file();
