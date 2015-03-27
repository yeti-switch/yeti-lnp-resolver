#pragma once

#include <confuse.h>
#include <string>
using std::string;

class cfg_reader {
	cfg_t *c = NULL;
  public:
	cfg_reader();
	~cfg_reader();

	bool load(const char *path);
	bool apply();
};
