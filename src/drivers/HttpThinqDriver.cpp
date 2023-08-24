#include <sstream>
#include <cstring>

#include "log.h"
#include "resolver/Resolver.h"
#include "HttpThinqDriver.h"


/**************************************************************
 * Configuration implementation
***************************************************************/
/**
 * @brief Method for retrieving token value from configuration data (Basic format)
 *
 * @param[in] data    The database output with driver configuration
 *
 * @return token value
 */
const CDriverCfg::CfgToken_t CHttpThinqDriverCfg::getRawToken(const RawConfig_t & data)
{
  CfgToken_t token;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    token = data["o_thinq_token"].as<CfgToken_t>();
  }

  return token;
}

/**
 * @brief Method for retrieving token value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return token value
 */
const CDriverCfg::CfgToken_t CHttpThinqDriverCfg::getRawToken(JSONConfig_t & data)
{
  CfgToken_t token;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    token = static_cast<CfgKey_t> (data["token"]);
  }

  return token;
}

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CHttpThinqDriverCfg::CHttpThinqDriverCfg(const CDriverCfg::RawConfig_t & data)
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

      mUserName = getRawUserName(jData);
      if (0 == mUserName.length())
      {
        throw error(getLabel(), "user name value is invalid!");
      }

      mToken = getRawToken(jData);
      if (0 == mToken.length())
      {
        throw error(getLabel(), "token value is invalid!");
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

    mUserName = getRawUserName(data);
    if (0 == mUserName.length())
    {
      throw error(getLabel(), "user name value is invalid!");
    }

    mToken = getRawToken(data);
    if (0 == mToken.length())
    {
      throw error(getLabel(), "token value is invalid!");
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
CHttpThinqDriver::CHttpThinqDriver(const CDriverCfg::RawConfig_t & data)
: CDriver(ECDriverId::ERESOLVER_DRIVER_HTTP_THINQ, "REST/ThinQ")
{
  mCfg.reset(new CHttpThinqDriverCfg(data));

  std::ostringstream url;
  CDriverCfg::CfgPort_t port = mCfg->getPort();

  url << mCfg->getProtocol() << "://" << mCfg->getHost();

  if (port)
  {
    url << ":" << std::dec << port;
  }

  url << "/lrn/extended/";
  mURLPrefix = url.str();

  mURLSuffix = "?format=json";
}

/**
 * @brief Show drive information
 */
void CHttpThinqDriver::showInfo() const
{
  info("[%u/%s] '%s' driver => address <%s://%s:%u> "
       "[username - %s, timeout - %u milliseconds]",
       mCfg->getUniqId(),
       mCfg->getLabel(), getName(),
       mCfg->getProtocol(),
       mCfg->getHost(), mCfg->getPort(),
       mCfg->getUserName(),
       mCfg->getTimeout());
}

/**
 * @brief Executing resolving procedure
 * @docs - https://api.thinq.com/docs/
 */
void CHttpThinqDriver::resolve(ResolverRequest &request,
                               Resolver *resolver,
                               ResolverHandler *handler) const {
    /*
   * Request URL:
   * https://api.thinq.com/lrn/extended/9194841422?format=json
   */

    string dstURL = mURLPrefix + request.data + mURLSuffix;
    dbg("resolving by URL: '%s'", dstURL.c_str());

    HttpRequest http_request;
    http_request.id = request.id;
    http_request.method = GET;
    http_request.verify_ssl = false;
    http_request.auth_type = ECAuth::BASIC;
    http_request.login = mCfg->getUserName();
    http_request.pass = mCfg->geToken();
    http_request.timeout_ms = mCfg->getTimeout();
    http_request.url = dstURL.c_str();
    http_request.headers = { "Content-Type: application/json" };

    if (handler != nullptr)
        handler->make_http_request(resolver, request, http_request);
}

void CHttpThinqDriver::parse(const string &data, ResolverRequest &request) const {

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

    try {
        request.result.localRoutingNumber =
            static_cast<decltype(request.result.localRoutingNumber)>(jsonxx(data)["lrn"]);
    }
    catch (std::exception & e) {
        warn("couldn't parse reply as JSON format: '%s'", data.c_str());
        throw error(e.what());
    }

    request.result.rawData = data;
}
