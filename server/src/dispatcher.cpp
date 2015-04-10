#include "dispatcher.h"
#include "resolver.h"
#include "cfg.h"

#define PDU_HDR_SIZE 2

_dispatcher::_dispatcher():
	_stop(false)
{
}

void _dispatcher::loop()
{
	s = nn_socket(AF_SP,NN_REP);
	if(s < 0){
		throw std::string("nn_socket() = %d",s);
	}

	bool binded = false;
	if(cfg.bind_urls.empty()){
		throw std::string("no listen endpoints specified. check your config");
	}

	for(const auto &i : cfg.bind_urls) {
		const char *url = i.c_str();
		int ret = nn_bind(s, url);
		if(ret < 0){
			err("can't bind to url '%s': %d (%s)",url,
				 errno,nn_strerror(errno));
			continue;
		}
		binded = true;
		info("listen on %s",url);
	}

	if(!binded){
		err("there are no listened interfaces after bind. check log and your config");
		throw std::string("can't bind to any url");
	}

	while(!_stop){
		char *msg = NULL;
		int l = nn_recv(s, &msg, NN_MSG, 0);
		if(l < 0){
			if(errno==EINTR) continue;
			dbg("nn_recv() = %d, errno = %d(%s)",l,errno,nn_strerror(errno));
			//!TODO: handle timeout, etc
			continue;
		}
		process_peer(msg,l);
		nn_freemsg(msg);
	}
}

int _dispatcher::process_peer(char *msg, int len)
{
	char *reply = NULL;
	int reply_len = 0;

	try {
		create_reply(reply,reply_len,msg,len);
	} catch(std::string &e){
		err("got string exception: %s",e.c_str());
		create_error_reply(reply,reply_len,255,std::string("Internal Error"));
	} catch(resolve_exception &e){
		err("got resolve exception: %d <%s>",e.code,e.reason.c_str());
		create_error_reply(reply,reply_len,e.code,e.reason);
	}

	int l = nn_send(s, reply, reply_len, 0);
	delete[] reply;
	if(l!=reply_len){
		err("nn_send(): %d, while msg to send size was %d",l,reply_len);
	}
	return 0;
}

void _dispatcher::str2reply(char *&msg,int &len,int code,const std::string &s)
{
	int l = s.size();
	len = l+PDU_HDR_SIZE;
	msg = new char[len];
	msg[0]  = code;
	msg[1] = l;
	memcpy(msg+PDU_HDR_SIZE,s.c_str(),l);
}

void _dispatcher::create_error_reply(char *&msg, int &len,
									 int code, std::string description)
{
	str2reply(msg,len,code,description);
}

void _dispatcher::create_reply(char *&msg, int &len, const char *req, int req_len)
{
	std::string lnr;

	if(req_len < PDU_HDR_SIZE){
		throw resolve_exception(1,"request too small");
	}

	int data_len = req[1];

	if(data_len > req_len - PDU_HDR_SIZE){
		throw resolve_exception(1,"invalid data length");
	}
	int database_id = req[0];

	std::string lnp(req+2,data_len);

	dbg("process request: database: %d, lnp: %s",
		database_id,lnp.c_str());

	resolver::instance()->resolve(database_id,lnp,lnr);

	str2reply(msg,len,0,lnr);
}
