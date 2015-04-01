#include "resolver_driver_sip.h"
#include "resolver.h"

#include <string.h>
#include "cfg.h"

#define RESPONSE_WAIT_TIMEOUT 4000

#include <vector>
using std::vector;

#include <algorithm>
#include <sstream>

static const string uri_param_delim(";");
static const string param_value_delim("=");
static const string lrn_param_name("rn");

static struct sip *sip_stack = NULL;
static  struct sipsess_sock *sip_socket = NULL;
static  struct dnsc *dns_client = NULL;
static  struct sa dns_servers[16];

static int libre_refs = 0;
static mutex libre_mutex;

// Explode string by a separator to a vector
// see http://stackoverflow.com/questions/236129/c-how-to-split-a-string
vector<string> explode(const string& s, const string& delim,
				const bool keep_empty = false) {
	vector<string> result;
	if (delim.empty()) {
		result.push_back(s);
		return result;
	}
	string::const_iterator substart = s.begin(), subend;
	while (true) {
		subend = search(substart, s.end(), delim.begin(), delim.end());
		string temp(substart, subend);
		if (keep_empty || !temp.empty()) {
			result.push_back(temp);
		}
		if (subend == s.end()) {
			break;
		}
		substart = subend + delim.size();
	}
	return result;
}

static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
	(void)err;

	struct sip_resolver_ctx *ctx = static_cast<struct sip_resolver_ctx *>(arg);
	if(ctx && ctx->self)
		ctx->self->on_response_ready(*ctx,msg);
}

resolver_driver_sip::resolver_driver_sip(string host, unsigned short port):
	resolver_driver(host,port,RESOLVER_DRIVER_SIP)
{
	int ret;
	struct sa laddr;

	libre_mutex.lock();
	if(libre_refs>0){
		libre_refs++;
		dbg("libre have %d refs",libre_refs);
		libre_mutex.unlock();
		return;
	}

	//library
	if(ret=libre_init()){
		dbg("libre_init() = %d",ret);
		throw string("can't init re library");
	}

	//DNS client
	uint32_t nsc = ARRAY_SIZE(dns_servers);
	if(ret=dns_srv_get(NULL, 0, dns_servers, &nsc)){
		dbg("dns_srv_get() = %d",ret);
		throw string("can't get dns servers");
	}
	if(ret=dnsc_alloc(&dns_client, NULL, dns_servers, nsc)){
		dbg("dnsc_alloc() = %d",ret);
		throw string("can't create dns client");
	}

	//SIP stack
	if(ret=sip_alloc(&sip_stack, dns_client, 32, 32, 32,"yeti-lnp-resolver",NULL,NULL)){
		dbg("sip_alloc() = %d",ret);
		throw string("can't create sip stack instance");
	}

	if(ret=net_default_source_addr_get(AF_INET, &laddr)){
		dbg("net_default_source_addr_get() = %d",ret);
		throw string("can't get default local address");
	}
	sa_set_port(&laddr,0);
	if(ret=sip_transp_add(sip_stack, SIP_TRANSP_UDP, &laddr)){
		dbg("sip_transp_add() = %d",ret);
		throw string("can't add transport");
	}
	if(ret=sipsess_listen(&sip_socket, sip_stack, 32, NULL, NULL)){
		dbg("sipsess_listen() = %d",ret);
		throw string("can't create transport socket");
	}

	start(); //start library main loop

	libre_refs++;
	dbg("libre have %d refs",libre_refs);
	libre_mutex.unlock();
}

void resolver_driver_sip::on_stop(){
	libre_mutex.lock();
	if(libre_refs--<=0){
		re_cancel();
		sip_close(sip_stack, false);
	}
	dbg("libre have %d refs",libre_refs);
	libre_mutex.unlock();
}

resolver_driver_sip::~resolver_driver_sip(){
	mem_deref(sip_socket);
	mem_deref(sip_stack);
	mem_deref(dns_client);
	libre_close();

	tmr_debug();
	mem_debug();
}

void resolver_driver_sip::resolve(const string &in, string &out, string &data)
{
	int ret;
	struct sipsess *sess;
	(void)data;
	sip_resolver_ctx ctx(this);

	out.clear();
	data.clear();

	std::ostringstream to_uri;
	unsigned int port = get_port();

	to_uri << "sip:" << in << "@" << get_host();
	if(port) to_uri << ":" << std::dec << port;

	ret = sipsess_connect(&sess, sip_socket,		//sessp, sock
			to_uri.str().c_str(),					//to_uri
			cfg.sip.from_name.c_str(),				//from_name
			cfg.sip.from_uri.c_str(),				//from_uri
			cfg.sip.contact.c_str(),				//cuser or uri
			NULL, 0,								//routev, routec
			"", NULL,								//ctype,desc
			NULL, NULL, false,						//authh, aarg, aref
			NULL, NULL,								//offerh, answerh
			NULL, NULL,								//progrh, estabh
			NULL, NULL,								//infoh, referh
			close_handler, &ctx, NULL);				//closeh, arg, fmt

	if(ret){
		dbg("sipsess_connect() = %d (%s)",ret,strerror(ret));
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't build request");
	}

	//wait for response
	ret = ctx.request_processed.wait_for_to(RESPONSE_WAIT_TIMEOUT);

	//terminate session
	mem_deref(sess);

	if(ret){
		process_reply(ctx,out,data);
		if(out.empty()){
			dbg("lnr is empty. number not ported. set out to in");
			out = in;
		}
	} else {
		dbg("reply processing timeout");
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"reply timeout");
	}
}

void resolver_driver_sip::on_response_ready(struct sip_resolver_ctx &ctx, const struct sip_msg *msg)
{
	const struct sip_hdr *contact;

	if(NULL==msg){
		err("no message");
		goto out;
	}

	if(!(msg->scode==302||msg->scode==301)){
		err("unexpected reply %d %.*s",msg->scode,msg->reason.l,msg->reason.p);
		goto out;
	}

	contact = sip_msg_hdr(msg, SIP_HDR_CONTACT);
	if(!contact){
		err("no contact header in redirect reply");
		goto out;	}

	pl_dup(&ctx.contact_value,&contact->val);

	if (sip_addr_decode(&ctx.contact_addr, &ctx.contact_value)){
		err("can't parse contact header: %.*s",
			ctx.contact_value.l,ctx.contact_value.p);
		goto out;
	}
	ctx.have_response = true;
out:
	ctx.request_processed.set(true);
}

void resolver_driver_sip::process_reply(sip_resolver_ctx &ctx,string &out, string &data)
{
	if(!ctx.have_response){
		dbg("unexpected reply");
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,RESOLVE_EXCEPTON_REASON_UNEXPECTED);
	}

	struct pl &user = ctx.contact_addr.uri.user;
	if(!pl_isset(&user)){
		dbg("contact uri have no userpart");
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,RESOLVE_EXCEPTON_REASON_UNEXPECTED);
	}
	string contact_uri(user.p,user.l);
	vector<string> params = explode(contact_uri,uri_param_delim);
	if(params.size() < 2){
		dbg("contact uri have no user parameters. likely wrong number");
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,RESOLVE_EXCEPTON_REASON_UNEXPECTED);
	}

	//walk over user parameters
	vector<string>::const_iterator i = params.begin();
	for(++i;i!=params.end();++i){
		//dbg("%s",i->c_str());
		//split parameter name and value
		vector<string> v = explode(*i,param_value_delim);
		if(v.size()>2){
			dbg("unexpected parameter format: %s",i->c_str());
			throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,RESOLVE_EXCEPTON_REASON_UNEXPECTED);
		}
		if(v[0]==lrn_param_name){
			if(v.size()<2){
				dbg("parameter %s without value",
					lrn_param_name.c_str());
					throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,RESOLVE_EXCEPTON_REASON_UNEXPECTED);
			}
			out = v[1];
		}
	}
	data = contact_uri;
}

void resolver_driver_sip::run()
{
	set_name("resolv_driver");
	dbg("start libre main loop at %ld",thread_pid);
	re_main(NULL);
}
