#include <sstream>
#include <cstring>
#include <memory>

#include "log.h"
#include "resolver/Resolver.h"
#include "HttpBulkvsDriver.h"

using namespace std;

/**
 * @brief Method for retrieving token value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return key value
 */
const CDriverCfg::CfgKey_t
CHttpBulkvsDriverCfg::getRawToken(JSONConfig_t & data)
{
  CfgKey_t token;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    token = static_cast<CfgKey_t> (data["token"]);
  }

  return token;
}

/**
 * @brief Method for retrieving url value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return key value
 */
const CDriverCfg::CfgUrl_t
CHttpBulkvsDriverCfg::getRawUrl(JSONConfig_t & data)
{
  CfgKey_t url;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    url = static_cast<CfgKey_t> (data["url"]);
  }

  return url;
}

/**
 * @brief Method for retrieving validate_https_certifcate flag
 * value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return key value
 */
const CDriverCfg::CfgFlag_t
CHttpBulkvsDriverCfg::getRawValidateHttpsCer(JSONConfig_t & data)
{
  CfgFlag_t flag = false;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    flag = static_cast<CfgFlag_t> (data["validate_https_certificate"]);
  }

  return flag;
}

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CHttpBulkvsDriverCfg::CHttpBulkvsDriverCfg(const CDriverCfg::RawConfig_t & data)
  : CDriverCfg(data)
{
  if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING == CDriverCfg::getFormatType())
  {
    try
    {
      jsonxx jData(data["parameters"].c_str());

      mUrl = getRawUrl(jData);
      if (0 == mUrl.length())
      {
        throw error(getLabel(), "url value is invalid!");
      }

      mToken = getRawToken(jData);
      if (0 == mToken.length())
      {
        throw error(getLabel(), "token value is invalid!");
      }

      mTimeout = getRawTimeout(jData);
      mValidateHttpsCer = getRawValidateHttpsCer(jData);
    }
    catch (std::exception & e)
    {
      throw error(getLabel(), e.what());
    }
  }
}

/**************************************************************
 * Driver implementation
***************************************************************/
/**
 * @brief Driver constructor
 *
 * @param[in] data  The raw configuration data
 */
CHttpBulkvsDriver::CHttpBulkvsDriver(const CDriverCfg::RawConfig_t & data)
    :CDriver(ECDriverId::ERESOLVER_DIRVER_HTTP_BULKVS, "Buklvs API") {
    mCfg.reset(new CHttpBulkvsDriverCfg(data));
}

/**
 * @brief Show drive information
 */
void CHttpBulkvsDriver::showInfo() const
{
    info("[%u/%s] '%s' driver => address <%s> [timeout - %u seconds]",
    mCfg->getUniqId(),
    mCfg->getLabel(), getName(),
    mCfg->getUrl().c_str(),
    mCfg->getTimeout());
}

/**
 * @brief Executing resolving procedure
 */
void CHttpBulkvsDriver::resolve(ResolverRequest &request,
                                Resolver *resolver,
                                ResolverDelegate *delegate) const {
  /*
   * Request URL:
   * {url}/?id={token}&did={inData}&format=json
   */

    std::ostringstream url;
    url << mCfg->getUrl();
    url << "/?id=" << mCfg->getToken();
    url << "&did=" << request.data;
    url << "&format=json";

    string dstURL = url.str();
    dbg("resolving by URL: '%s'", dstURL.c_str());

    HttpRequest http_request;
    http_request.id = request.id;
    http_request.method = GET;
    http_request.url = dstURL.c_str();
    http_request.verify_ssl = mCfg->getValidateHttpsCer();
    http_request.auth_type = ECAuth::BASIC;
    http_request.timeout_ms = mCfg->getTimeout();

    if (delegate != nullptr)
        delegate->make_http_request(resolver, request, http_request);
}

 void CHttpBulkvsDriver::parse(const string &data, ResolverRequest &request) const {

   /*
    * Processing reply. Example:
    * {
    *    name: "BULK SOLUTIONS",
    *    number: "3109060901",
    *    time: 1680002903
    * }
    */

    try {
        request.result.localRoutingNumber =
            static_cast<decltype(request.result.localRoutingNumber)>(jsonxx(data)["name"]);
    }
    catch (std::exception & e) {
        warn("couldn't parse reply as JSON format: '%s'", data.c_str());
        throw error(e.what());
    }

    request.result.rawData = data;
}
