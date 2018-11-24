#pragma once

#include "resolver_driver.h"

#include <stdint.h>
#include <re.h>
#include "thread.h"

#define PL_FMT "%.*s"
#define PL_ARG(a) a.l,a.p

class resolver_driver_sip;

struct sip_resolver_ctx {
	resolver_driver_sip *self;
	struct pl contact_value;
	struct sip_addr contact_addr;
	condition<bool> request_processed;
	bool have_response;

	sip_resolver_ctx(resolver_driver_sip *owner):
		have_response(false),
		self(owner)
	{
		contact_value.p = NULL;
		contact_value.l = 0;
		request_processed.set(false);
	}

	~sip_resolver_ctx(){
		if(pl_isset(&contact_value))
			mem_deref((void *)contact_value.p);
	}
};

class resolver_driver_sip: public resolver_driver, public thread {
	void process_reply(sip_resolver_ctx &ctx,string &out, string &data);
	bool lib_owner;
  public:
	resolver_driver_sip(const resolver_driver::driver_cfg &dcfg);
	~resolver_driver_sip();
	void on_stop();
	void on_response_ready(struct sip_resolver_ctx &ctx, const struct sip_msg *msg);
	void launch();
	void run();
    void resolve(const string &in, resolver_driver::result &out);
};
