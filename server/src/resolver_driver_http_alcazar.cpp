#include "resolver_driver_http_alcazar.h"
#include <sstream>
#include "http_request.h"
#include "log.h"
#include "cJSON.h"

size_t static write_func(void * ptr,  size_t size,  size_t nmemb,  void * userdata)
{
    if (!userdata)
        return size*nmemb;


    std::string & s = *(reinterpret_cast<std::string *>(userdata));
    s.append((char *)ptr, size*nmemb);

    return size*nmemb;
}

resolver_driver_http_alcazar::resolver_driver_http_alcazar(const resolver_driver::driver_cfg & dcfg)
    : resolver_driver(dcfg)
{
    /*
     * http://api.east.alcazarnetworks.com/api/2.2/lrn?extended=true&output=json& \
     *      key=5ddc2fba-0cc4-4c93-9a28-bd28ddf5e6d4&tn=14846642959
     */

    std::ostringstream url;
    url << "http://" << _cfg.host;
    if (_cfg.port)
        url << ":" << std::dec << _cfg.port;
    url << "/api/2.2/lrn?extended=true&output=json";
    url << "&key=" << _cfg.alkazar_key;
    url << "&tn=";

    m_url_prefix = url.str();
}

void resolver_driver_http_alcazar::resolve(const string & in, resolver_driver::result & out)
{
    /*
     * Set request URL:
     * http://api.east.alcazarnetworks.com/api/2.2/lrn?extended=true&output=json& \
     *      key=5ddc2fba-0cc4-4c93-9a28-bd28ddf5e6d4&tn=14846642959
     */

    string url = m_url_prefix + in;
    dbg("resolving by URL: '%s'", url.c_str());

    string reply;
    try
    {
        http_request hreq;
        hreq.setSSLVerification(false);
        hreq.setAuthType(http_request::AUTH_TYPE::NONE);
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
     *    "LRN":"14847880088",
     *    "SPID":"7513",
     *    "OCN":"7513",
     *    "LATA":"228",
     *    "CITY":"ALLENTOWN",
     *    "STATE":"PA",
     *    "JURISDICTION":"INDETERMINATE",
     *    "LEC":"CTSI, INC. - PA",
     *    "LINETYPE":"LANDLINE"
     * }
     */

    dbg("http request reply: %s", reply.c_str());
    cJSON * j = cJSON_Parse(reply.c_str());
    if (!j)
    {
        dbg("can't parse reply as JSON format: '%s'", reply.c_str());
        throw resolve_exception(RESOLVE_EXCEPTION_DRIVER, "result parsing error");
    }

    cJSON * jlrn = cJSON_GetObjectItem(j,"LRN");
    if (!jlrn)
    {
        dbg("no 'lrn' field in returned JSON object: %s", reply.c_str());
        cJSON_Delete(j);
        throw resolve_exception(RESOLVE_EXCEPTION_DRIVER, "unexpected response");
    }

    char * lrn = cJSON_Print(jlrn);
    out.lrn    = lrn;
    free(lrn);

    // remove quotes
    if (out.lrn.compare(0, 1, "\"") == 0)
        out.lrn.erase(out.lrn.begin());
    if (out.lrn.compare(out.lrn.size()-1, 1, "\"") == 0)
        out.lrn.erase(out.lrn.end()-1);

    out.raw_data = reply;
    cJSON_Delete(j);
}
