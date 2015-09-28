#include "resolver_driver_http_thinq.h"

#include <atomic>
#include <sstream>
#include <curl/curl.h>

#include "log.h"
#include "cJSON.h"

#define CURL_TIMEOUT 2

static std::atomic_flag curl_initialized = ATOMIC_FLAG_INIT;


resolver_driver_http_thinq::resolver_driver_http_thinq(const resolver_driver::driver_cfg &dcfg)
	: resolver_driver(dcfg)
{
	std::ostringstream urls;
	init_curl();

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

void resolver_driver_http_thinq::init_curl()
{
	if(curl_initialized.test_and_set())
		return;

	dbg("init curl");
	if(curl_global_init(CURL_GLOBAL_ALL)){
		throw string("initializing libcurl");
	}
}

size_t static write_func(void  *ptr,  size_t  size,  size_t nmemb,  void  *arg) {
	if(!arg) return size*nmemb;

	std::string &s = *reinterpret_cast<std::string *>(arg);
	s.append((char*)ptr, size*nmemb);

	return size*nmemb;
}

void resolver_driver_http_thinq::resolve(const string &in, resolver_driver::result &out)
{
	CURL *h = curl_easy_init();
	if(!h){
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't init http handler");
	}

	//set url
		/* https://api.thinq.com/lrn/extended/9194841422?format=json
		 * https://api.thinq.com/docs/ */
	string url = url_prefix + in + url_suffix;
	dbg("use url: '%s'",url.c_str());
	if(curl_easy_setopt(h, CURLOPT_URL, url.c_str())!= CURLE_OK){
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set url");
	}

	//disable SSL verification
	if(curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK
			|| (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L) != CURLE_OK)){
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't disable SSL verification");
	}

	//set auth header
	if(curl_easy_setopt(h, CURLOPT_HTTPAUTH, CURLAUTH_BASIC) != CURLE_OK
			|| (curl_easy_setopt(h,  CURLOPT_USERNAME , _cfg.thinq_username.c_str()) != CURLE_OK)
			|| (curl_easy_setopt(h,  CURLOPT_PASSWORD , _cfg.thinq_token.c_str()) != CURLE_OK)){
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set auth for request");
	}

	//set timeout
	if ((curl_easy_setopt(h, CURLOPT_TIMEOUT, _cfg.timeout) != CURLE_OK)
			|| (curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)) {
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set timeout for request");
	}


	string r;
	//set output handler
	if ((curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_func) != CURLE_OK)
			|| (curl_easy_setopt(h, CURLOPT_WRITEDATA, &r) != CURLE_OK)) {
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set output handler");
	}

	//set headers
	struct curl_slist *hdrs = NULL;
	hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
	if(!hdrs){
		curl_easy_cleanup(h);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't init headers list");
	}
	if(curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs) != CURLE_OK) {
		curl_easy_cleanup(h);
		curl_slist_free_all(hdrs);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set headers");
	}

	//set error buffer
	char curl_err[CURL_ERROR_SIZE];
	curl_err[0]='\0';
	if(curl_easy_setopt(h, CURLOPT_ERRORBUFFER, curl_err) != CURLE_OK) {
		curl_easy_cleanup(h);
		curl_slist_free_all(hdrs);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"can't set error buffer");
	}

	//perform request
	CURLcode code = curl_easy_perform(h);
	if(code){
		dbg("error on perform request: %s",curl_err);
		curl_easy_cleanup(h);
		curl_slist_free_all(hdrs);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,curl_err);
	}
	curl_easy_cleanup(h);
	curl_slist_free_all(hdrs);

	//process reply
	dbg("got reply: %s",r.c_str());
	cJSON *j = cJSON_Parse(r.c_str());
	if(!j){
		dbg("can't parse reply to JSON: '%s'",r.c_str());
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"result parsing error");
	}

	cJSON *jlrn = cJSON_GetObjectItem(j,"lrn");
	if(!jlrn){
		dbg("no 'lrn' field in returned JSON object: %s",r.c_str());
		cJSON_Delete(j);
		throw resolve_exception(RESOLVE_EXCEPTION_DRIVER,"unexpected response");
	}

	char *lrn = cJSON_Print(jlrn);
    out.lrn = lrn;
	free(lrn);
	//remove quotes
    if(out.lrn.compare(0,1,"\"")==0) out.lrn.erase(out.lrn.begin());
    if(out.lrn.compare(out.lrn.size()-1,1,"\"")==0) out.lrn.erase(out.lrn.end()-1);

    out.raw_data = r;
	cJSON_Delete(j);
}
