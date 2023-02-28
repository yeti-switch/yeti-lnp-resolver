#include "uri_parser.h"

#include <string.h>
#include <stdlib.h>

UriComponents parseAddr(const char *addr) {
	UriComponents res;
	char* delim;
	size_t comp_sz;

	memset(&res, 0, sizeof(UriComponents));

	// check whether address is valid
	if (addr == NULL || strlen(addr) == 0 || strlen(addr) >= SOCKADDR_SIZE)
		return res;

	// parse protocol
	delim = strstr(addr, "://");

	if (delim != NULL)
	{
		comp_sz = delim - addr;

		if (comp_sz > 0 && comp_sz <= PROTOCOL_SIZE)
			strncpy(res.proto, addr, comp_sz);

		addr += comp_sz + strlen("://");
	}

	// parse host
	delim = strstr(addr, ":");

	if (delim != NULL)
	{
		comp_sz = delim - addr;

		if (comp_sz > 0 && comp_sz <= HOST_SIZE)
			strncpy(res.host, addr, comp_sz);

		addr += comp_sz + strlen(":");

		// parse port
		res.port = atoi(addr);
	}
	else
	{
		comp_sz = strlen(addr);

		if (comp_sz > 0 && comp_sz <= HOST_SIZE)
			strncpy(res.host, addr, comp_sz);

		strncpy(res.host, addr, strlen(addr));
	}

	return res;
}
