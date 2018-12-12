#pragma once

#include "resolver_driver.h"

class resolver_driver_http_thinq: public resolver_driver {
	string url_prefix,url_suffix;
  public:
	resolver_driver_http_thinq(const resolver_driver::driver_cfg &dcfg);
	~resolver_driver_http_thinq();
	void on_stop();
    void resolve(const string &in, resolver_driver::result &out);
};
