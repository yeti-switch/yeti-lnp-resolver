#include <sstream>
#include <cstring>

#include "log.h"
#include "drivers/modules/HttpClient.h"
#include "HttpAlcazarDriver.h"

/**************************************************************
 * Implementation helpers
***************************************************************/
/*
 * Callback function for processing HTTP response
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

/**************************************************************
 * Configuration implementation
***************************************************************/
/*
 * Method for retrieving key value from configuration data (Basic format).
 *
 * @param[in] data    The database output with driver configuration
 * @return key value
 */
const CDriverCfg::CfgKey_t CHttpAlcazarDriverCfg::getRawKey(const RawConfig_t & data)
{
  CfgKey_t key;

  if (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType)
  {
    key = data["o_alkazar_key"].as<CfgKey_t>();
  }

  return key;
}

/*
 * Method for retrieving key value from configuration data (JSON format).
 *
 * @param[in] data  The database output with driver configuration
 * @return key value
 */
const CDriverCfg::CfgKey_t CHttpAlcazarDriverCfg::getRawKey(JSONConfig_t & data)
{
  CfgKey_t key;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    key = static_cast<CfgKey_t> (data["key"]);
  }

  return key;
}

/*
 * Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 * @param[in] drvName The string driver name
 */
CHttpAlcazarDriverCfg::CHttpAlcazarDriverCfg(const CDriverCfg::RawConfig_t & data)
  : CDriverCfg(data)
{
  if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING == CDriverCfg::getFormatType())
  {
    try
    {
      jsonxx jData(data["parameters"].c_str());

      mHost = getRawHost(jData);
      if (0 == mHost.length())
      {
        throw error(getLabel(), "host value is invalid!");
      }

      mKey = getRawKey(jData);
      if (0 == mKey.length())
      {
        throw error(getLabel(), "key value is invalid!");
      }

      mPort    = getRawPort(jData);
      mTimeout = getRawTimeout(jData);
    }
    catch (std::exception & e)
    {
      throw error(getLabel(), e.what());
    }
  }
  else
  {
    mHost = getRawHost(data);
    if (0 == mHost.length())
    {
      throw error(getLabel(), "host value is invalid!");
    }

    mKey = getRawKey(data);
    if (0 == mKey.length())
    {
      throw error(getLabel(), "key value is invalid!");
    }

    mPort    = getRawPort(data);
    mTimeout = getRawTimeout(data);
  }
}

/**************************************************************
 * Driver implementation
***************************************************************/
/*
 * Driver constructor
 *
 * @param[in] data  The raw configuration data
 */
CHttpAlcazarDriver::CHttpAlcazarDriver(const CDriverCfg::RawConfig_t & data)
  : CDriver(ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR, "Alcazar Networks")
{
  mCfg.reset(new CHttpAlcazarDriverCfg(data));

  std::ostringstream url;
  CDriverCfg::CfgPort_t port = mCfg->getPort();

  url << mCfg->getProtocol() << "://" << mCfg->getHost();

  if (port)
  {
    url << ":" << std::dec << port;
  }

  url << "/api/2.2/lrn?extended=true&output=json";
  url << "&key=" << mCfg->geKey();
  url << "&tn=";

  mURLPrefix = url.str();
}

/*
 * Show drive information
 *
 * @return none
 */
void CHttpAlcazarDriver::showInfo() const
{
  info("[%u/%s] '%s' driver => address <%s://%s:%u> [timeout - %u seconds]",
      mCfg->getUniqId(),
      mCfg->getLabel(), getName(),
      mCfg->getProtocol(),
      mCfg->getHost(), mCfg->getPort(),
      mCfg->getTimeout());
}

/*
 * Executing resolving procedure
 *
 * @param[in] inData      The source data for making resolving
 * @param[out] outResult  The structure for saving resolving result
 * @return none
 */
void CHttpAlcazarDriver::resolve(const string & inData, SResult_t & outResult) const
{
  /*
   * Request URL:
   * http://api.east.alcazarnetworks.com/api/2.2/lrn?extended=true&output=json& \
   *      key=5ddc2fba-0cc4-4c93-9a28-bd28ddf5e6d4&tn=14846642959
   */

  string dstURL = mURLPrefix + inData;
  dbg("resolving by URL: '%s'", dstURL.c_str());

  string replyBuf;
  try
  {
    CHttpClient http;

    http.setSSLVerification(false);
    http.setAuthType(CHttpClient::ECAuth::NONE);
    http.setTimeout(mCfg->getTimeout());
    http.setWriteCallback(write_func, &replyBuf);
    http.setHeader("Content-Type: application/json");

    if (http.perform(dstURL) != CHttpClient::ECReqCode::OK)
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

  dbg("HTTP reply: %s", replyBuf.c_str());

  try {
    outResult.localRoutingNumber =
        static_cast<decltype(outResult.localRoutingNumber)> (jsonxx(replyBuf)["LRN"]);
  }
  catch (std::exception & e)
  {
    warn("couldn't parse reply as JSON format: '%s'", replyBuf.c_str());
    throw error(e.what());
  }

  outResult.rawData = replyBuf;
}
