#include <sstream>

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
      s.append((char *)ptr, (size * nmemb));
    }

    return (size * nmemb);
}

/**************************************************************
 * Configuration implementation
***************************************************************/
/*
 * Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 * @param[in] drvId   The driver identifier
 * @param[in] drvName The string driver name
 */
CHttpAlcazarDriverCfg::CHttpAlcazarDriverCfg(const CDriverCfg::RawConfig_t & data,
                                             const ECDriverId drvId)
  : CDriverCfg(data)
{
  if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING == CDriverCfg::getFormatType())
  {
    const char * errMsg = nullptr;
    cJSON *      jData  = cJSON_Parse(data["parameters"].c_str());

    do
    {
      if (nullptr == jData)
      {
        errMsg = "driver JSON config parsing error";
        break;
      }

      mHost = getRawHost(jData);
      if (nullptr == mHost)
      {
        errMsg = "host value is invalid!";
        break;
      }

      mKey = getRawKey(jData, drvId);
      if (nullptr == mKey)
      {
        errMsg = "key value is invalid!";
        break;
      }

      mPort    = getRawPort(jData);
      mTimeout = getRawTimeout(jData);

    } while(0);

    if (nullptr != jData)
    {
      cJSON_Delete(jData);
    }
    if (nullptr != errMsg)
    {
      throw error(getLabel(), errMsg);
    }
  }
  else
  {
    mHost = getRawHost(data);
    if (nullptr == mHost)
    {
      throw error(getLabel(), "host value is invalid!");
    }

    mKey = getRawKey(data, drvId);
    if (nullptr == mKey)
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
  mCfg.reset(new CHttpAlcazarDriverCfg(data, getId()));

  std::ostringstream url;
  CDriverCfg::CfgPort_t port = mCfg->getPort();

  url << "http://" << mCfg->getHost();

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
  info("[%u/%s] '%s' driver => address <%s:%u> [timeout - %u seconds]",
      mCfg->getUniqId(),
      mCfg->getLabel(), getName(),
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

  cJSON * j = cJSON_Parse(replyBuf.c_str());
  if (!j)
  {
    warn("couldn't parse reply as JSON format: '%s'", replyBuf.c_str());
    throw CDriver::error("HTTP reply parsing error");
  }

  cJSON * jlrn = cJSON_GetObjectItem(j, "LRN");
  if (!jlrn)
  {
    dbg("no 'LRN' field in returned JSON object: %s", replyBuf.c_str());
    cJSON_Delete(j);
    throw CDriver::error("unexpected response");
  }

  char * lrn = cJSON_Print(jlrn);
  outResult.localNumberPortability = lrn;
  free(lrn);

  outResult.rawData = replyBuf;

  // remove quotes from result LNP
  string & lnp = outResult.localNumberPortability;
  if (0 == lnp.compare(0, 1, "\""))
  {
    lnp.erase(lnp.begin());
  }
  if (0 == lnp.compare(lnp.size()-1, 1, "\""))
  {
    lnp.erase(lnp.end() - 1);
  }

  cJSON_Delete(j);
}



