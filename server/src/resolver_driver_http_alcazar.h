#pragma once

#include "resolver_driver.h"

class resolver_driver_http_alcazar : public resolver_driver
{
    string m_url_prefix;
  public:
    resolver_driver_http_alcazar(const resolver_driver::driver_cfg & dcfg);
    ~resolver_driver_http_alcazar() = default;

    void resolve(const string & in, resolver_driver::result & out) override;
};