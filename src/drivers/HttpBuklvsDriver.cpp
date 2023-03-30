#include <sstream>
#include <cstring>

#include "log.h"
#include "drivers/modules/HttpClient.h"
#include "HttpBulkvsDriver.h"

/**************************************************************
 * Implementation helpers
***************************************************************/
/**
 * @brief Callback function for processing HTTP response
 */
size_t static write_func(void * ptr, size_t size, size_t nmemb, void * userdata)
{
    if (userdata)
    {
      std::string & s = *(reinterpret_cast<std::string *>(userdata));
      s.append(static_cast<char *> (ptr), (size * nmemb));
    }

    return (size * nmemb);
}

/**
 * @brief Method for retrieving token value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return key value
 */
const CDriverCfg::CfgKey_t
CHttpBuklvsDriverCfg::getRawToken(JSONConfig_t & data)
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
CHttpBuklvsDriverCfg::getRawUrl(JSONConfig_t & data)
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
CHttpBuklvsDriverCfg::getRawValidateHttpsCer(JSONConfig_t & data)
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
CHttpBuklvsDriverCfg::CHttpBuklvsDriverCfg(const CDriverCfg::RawConfig_t & data)
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
CHttpBuklvsDriver::CHttpBuklvsDriver(const CDriverCfg::RawConfig_t & data)
  : CDriver(ECDriverId::ERESOLVER_DIRVER_HTTP_BULKVS, "Buklvs API")
{
  mCfg.reset(new CHttpBuklvsDriverCfg(data));
}

/**
 * @brief Show drive information
 */
void CHttpBuklvsDriver::showInfo() const
{
  info("[%u/%s] '%s' driver => address <%s> [timeout - %u seconds]",
      mCfg->getUniqId(),
      mCfg->getLabel(), getName(),
      mCfg->getUrl().c_str(),
      mCfg->getTimeout());
}

/**
 * @brief Executing resolving procedure
 *
 * @param[in] inData      The source data for making resolving
 * @param[out] outResult  The structure for saving resolving result
 */
void CHttpBuklvsDriver::resolve(const string & inData, SResult_t & outResult) const
{
  /*
   * Request URL:
   * {url}/?id={token}&did={inData}&format=json
   */

  std::ostringstream url;
  url << mCfg->getUrl();
  url << "/?id=" << mCfg->getToken();
  url << "&did=" << inData;
  url << "&format=json";

  string dstURL = url.str();
  dbg("resolving by URL: '%s'", dstURL.c_str());

  string replyBuf;
  try
  {
    CHttpClient http;
    http.setSSLVerification(mCfg->getValidateHttpsCer());
    http.setTimeout(mCfg->getTimeout() * 1000); // miliseconds
    http.setWriteCallback(write_func, &replyBuf);

    if (CHttpClient::ECReqCode::OK != http.perform(dstURL))
    {
      dbg("error on perform request: %s", http.getErrorStr());
      throw CDriver::error(http.getErrorStr());
    }
  }
  catch (CHttpClient::error & e)
  {
    throw CDriver::error(e.what());
  }

  /*
   * Processing reply. Example:
   * {
   *    name: "BULK SOLUTIONS",
   *    number: "3109060901",
   *    time: 1680002903
   * }
   */

  dbg("HTTP reply: %s", replyBuf.c_str());

  try {
    outResult.localRoutingNumber =
        static_cast<decltype(outResult.localRoutingNumber)> (jsonxx(replyBuf)["name"]);
  }
  catch (std::exception & e)
  {
    warn("couldn't parse reply as JSON format: '%s'", replyBuf.c_str());
    throw error(e.what());
  }

  outResult.rawData = replyBuf;
}
