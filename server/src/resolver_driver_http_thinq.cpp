#include "resolver_driver_http_thinq.h"
#include "http_request.h"

#include <sstream>

#include "log.h"
#include "cJSON.h"

resolver_driver_http_thinq::resolver_driver_http_thinq(const resolver_driver::driver_cfg &dcfg)
	: resolver_driver(dcfg)
{
	std::ostringstream urls;

	//https://api.thinq.com/lrn/extended/9194841422?format=json

	urls << "https://" << _cfg.host;
	if(_cfg.port) urls << ":" << std::dec << _cfg.port;
	urls << "/lrn/extended/";
	url_prefix = urls.str();
	url_suffix = "?format=json";
}

resolver_driver_http_thinq::~resolver_driver_http_thinq()
{}

void resolver_driver_http_thinq::on_stop()
{}

size_t static write_func(void  *ptr,  size_t  size,  size_t nmemb,  void  *arg) {
	if(!arg) return size*nmemb;

	std::string &s = *reinterpret_cast<std::string *>(arg);
	s.append((char*)ptr, size*nmemb);

	return size*nmemb;
}

void resolver_driver_http_thinq::resolve(const string &in, resolver_driver::result &out)
{
    /*
     * Set request URL:
	 * https://api.thinq.com/lrn/extended/9194841422?format=json
     * @docs - https://api.thinq.com/docs/
     */

	string url = url_prefix + in + url_suffix;
	dbg("resolving by URL: '%s'",url.c_str());

    string reply;
    try
    {
        http_request hreq;
        hreq.setSSLVerification(false);
        hreq.setAuthType(http_request::AUTH_TYPE::BASIC);
        hreq.setAuthData(_cfg.thinq_username,_cfg.thinq_token);
        hreq.setTimeout(_cfg.timeout);
        hreq.setWriteCallback(write_func, &reply);
        hreq.setHeader("Content-Type: application/json");
        if (hreq.perform(url) != http_request::REQ_CODE::OK)
        {
            dbg("error on perform request: %s",hreq.getErrorStr());
            throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,hreq.getErrorStr());
        }
    }
    catch(http_request::error & e)
    {
        throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,e.what());
    }

    /*
     * Processing reply. Example:
     * {
     *    "lrn": "9198900000",
     *    "lerg": {
     *      "npa": "919",
     *      "nxx": "287",
     *      "y": "A",
     *      "lata": "426",
     *      "ocn": "7555",
     *      "company": "TW TELECOM OF NC",
     *      "rc": "DURHAM",
     *      "state": "NC"
     *    }
     * }
     */

	dbg("http request reply: %s",reply.c_str());
	cJSON *j = cJSON_Parse(reply.c_str());
	if(!j){
		dbg("can't parse reply as JSON format: '%s'",reply.c_str());
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"result parsing error");
	}

	cJSON *jlrn = cJSON_GetObjectItem(j,"lrn");
	if(!jlrn){
		dbg("no 'lrn' field in returned JSON object: %s",reply.c_str());
		cJSON_Delete(j);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"unexpected response");
	}

	char *lrn = cJSON_Print(jlrn);
    out.lrn = lrn;
	free(lrn);
	//remove quotes
    if(out.lrn.compare(0,1,"\"")==0) out.lrn.erase(out.lrn.begin());
    if(out.lrn.compare(out.lrn.size()-1,1,"\"")==0) out.lrn.erase(out.lrn.end()-1);

    out.raw_data = reply;
	cJSON_Delete(j);
}
