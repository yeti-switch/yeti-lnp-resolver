#include <sstream>
#include <cstring>

#include "log.h"
#include "resolver/Resolver.h"
#include "HttpAlcazarDriver.h"

/**************************************************************
 * Configuration implementation
***************************************************************/
/**
 * @brief Method for retrieving key value from configuration data (Basic format)
 *
 * @param[in] data    The database output with driver configuration
 *
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

/**
 * @brief Method for retrieving key value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
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

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
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
/**
 * @brief Driver constructor
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

/**
 * @brief Show drive information
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

/**
 * @brief Executing resolving procedure
 */
void CHttpAlcazarDriver::resolve(ResolverRequest &request,
                                 Resolver *resolver,
                                 ResolverHandler *handler) const {

  /*
   * Request URL:
   * http://api.east.alcazarnetworks.com/api/2.2/lrn?extended=true&output=json& \
   *      key=5ddc2fba-0cc4-4c93-9a28-bd28ddf5e6d4&tn=14846642959
   */

    string dstURL = mURLPrefix + request.data;
    dbg("resolving by URL: '%s'", dstURL.c_str());

    HttpRequest http_request;
    http_request.id = request.id;
    http_request.method = GET;
    http_request.url = dstURL.c_str();

    http_request.verify_ssl = false;
    http_request.auth_type = ECAuth::NONE;
    http_request.timeout_ms = mCfg->getTimeout();
    http_request.headers = { "Content-Type: application/json" };

    if (handler != nullptr)
        handler->make_http_request(resolver, request, http_request);
}

void CHttpAlcazarDriver::parse(const string &data, ResolverRequest &request) const {
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

    try {
      request.result.localRoutingNumber =
          static_cast<decltype(request.result.localRoutingNumber)> (jsonxx(data)["LRN"]);
    }
    catch (std::exception & e)
    {
      warn("couldn't parse reply as JSON format: '%s'", data.c_str());
      throw error(e.what());
    }

    request.result.rawData = data;
}
