#include <sstream>
#include <vector>
using std::vector;

#include "log.h"
#include "cfg.h"
#include "resolver/Resolver.h"
#include "SipDriver.h"

/**************************************************************
 * Implementation helpers
***************************************************************/
/**
 * @brief Split string by delimiter and save results to vector
 */
vector<string> split(const string & inStr, const char delimiter)
{
  vector<string> rv;
  std::istringstream inStream(inStr);
  string token;

  while (std::getline(inStream, token, delimiter))
  {
    rv.push_back(token);
  }

  return rv;
}

/**************************************************************
 * Configuration implementation
***************************************************************/
/**
 * Initialize static members
 */
CSipDriverCfg::SipCfgName_t CSipDriverCfg::sFromName;
CSipDriverCfg::SipCfgUri_t CSipDriverCfg::sFromUri;
CSipDriverCfg::SipCfgContact_t CSipDriverCfg::sContact;

/**
 * @brief Setter with data from static configuration
 */
void CSipDriverCfg::setStaticData()
{
  static bool isConfigured = false;

  if (false == isConfigured)
  {
    sFromName    = cfg.sip.from_name;
    sFromUri     = cfg.sip.from_uri;
    sContact     = cfg.sip.contact;

    isConfigured = true;

    dbg("'SIP/301-302' driver => [from field - '%s' <%s>, contact - <%s>]",
                                    getFromName(), getFromUri(), getContact());
  }
}

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CSipDriverCfg::CSipDriverCfg(const CDriverCfg::RawConfig_t & data)
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

    mPort    = getRawPort(data);
    mTimeout = getRawTimeout(data);
  }

  // Initialize static data by global configuration values
  setStaticData();
}

/**************************************************************
 * Driver implementation
***************************************************************/
/**
 * @brief Driver constructor
 *
 * @param[in] data  The raw configuration data
 */
CSipDriver::CSipDriver(const CDriverCfg::RawConfig_t & data)
  : CDriver(ECDriverId::ERESOLVER_DRIVER_SIP, "SIP/301-302")
{
  mCfg.reset(new CSipDriverCfg(data));

  CSipClient & sip = CSipClient::getInstance(mCfg->getUserAgent());
  sip.setContactData(mCfg->getContact());
  sip.setFromData(mCfg->getFromName(), mCfg->getFromUri());

  std::ostringstream prefix;
  prefix << mCfg->getProtocol() << ":";
  mURIPrefix = prefix.str();

  std::ostringstream suffix;
  suffix << "@" << mCfg->getHost();
  CDriverCfg::CfgPort_t port = mCfg->getPort();
  if (port)
  {
    suffix << ":" << std::dec << port;
  }
  mURISuffix = suffix.str();
}

/**
 * @brief Show drive information
 */
void CSipDriver::showInfo() const
{
  info("[%u/%s] '%s' driver => address <%s://%s:%u> "
       "[timeout - %u seconds]",
       mCfg->getUniqId(),
       mCfg->getLabel(), getName(),
       mCfg->getProtocol(),
       mCfg->getHost(), mCfg->getPort(),
       mCfg->getTimeout());
}

/**
 * @brief Executing resolving procedure
 * @note used SIP 301/302 redirect message with Contact field tag 'rn'
 */
void CSipDriver::resolve(ResolverRequest &request,
                         Resolver *resolver,
                         ResolverHandler *handler) const {

   /*
    * Request SIP Invite message
    *
    * INVITE sip:12345563234@yeti.example.org:5060 SIP/2.0
    * Via: SIP/2.0/UDP yeti.example.org:60715;branch=z9hG4bK713e3b0aa798bde5;rport
    * Contact: <sip:yeti-lnp-resolver@yeti.example.org:60715>
    * Max-Forwards: 70
    * To: <sip:12345563234@yeti.example.org:5060>
    * From: "LNP resolver" <sip:yeti-lnp-resolver@yeti.example.org>;tag=3e078f3166fec4d6
    * Call-ID: 0f0a3b2ff62587bd
    * CSeq: 19067 INVITE
    * User-Agent: Yeti LNP resolver
    * Content-Length: 0
    */

    string dstUri = mURIPrefix + request.data + mURISuffix;
    dbg("resolving by INVITE request <%s>", dstUri.c_str());

    CSipClient::SReplyData replyBuf;
    try
    {
        CSipClient & sip = CSipClient::getInstance();
        sip.setTimeout(mCfg->getTimeout());
        if (CSipClient::ECCode::OK != sip.perform(dstUri, replyBuf))
        {
            dbg("error on request performing: could not receive response");
            throw CDriver::error("no SIP response");
        }
    }
    catch (CSipClient::error & e)
    {
        throw error(e.what());
    }

    dbg("SIP reply: [%u] '%s'", replyBuf.statusCode,
                            replyBuf.rawContactData.c_str());

    parse(replyBuf.rawContactData, request);
    request.is_done = true;
}

/**
 * @brief Parses contact URI elements
 * @note used SIP 301/302 redirect message with Contact field tag 'rn'
 */
void CSipDriver::parse(const string &data, ResolverRequest &request) const {

   /*
    * Processing reply. Example:
    *
    * SIP/2.0 302 Moved Temporarily
    * Via: SIP/2.0/UDP yeti.example.org:60715;branch=z9hG4bK713e3b0aa798bde5;rport
    * From: "yeti-lnp-resolver" <sip:yeti-lnp-resolver@localhost>;tag=3e078f3166fec4d6
    * To: <sip:12345563234@yeti.example.org:5060>;tag=1
    * Call-ID: 0f0a3b2ff62587bd
    * CSeq: 19067 INVITE
    * Contact: <sip:yeti-sip;rn=4681665911@yeti.example.org:5060;transport=UDP>
    * Content-Length: 0
    */

    if(string::npos == data.find_first_of(';')) {
        //failover to use whole username as LRN
        //TODO: separate driver type in DB
        dbg("no user-params in reply Contact. use whole user-part as LRN");
        request.result.localRoutingNumber = request.result.rawData = data;
        return;
    }

    vector<string> uriElements = split(data, ';');
    // Parse elements to name and value
    bool isValueFound = false;
    for (const auto & i : uriElements)
    {
        // elm[0] = name
        // elm[1] = value
        vector<string> elm = split(i, '=');
        if ((2 == elm.size()) && ("rn" == elm[0]))
        {
            // rn=<local_routing_number>
            request.result.localRoutingNumber = elm[1];
            isValueFound           = true;

            break;
        }
    }

    if (!isValueFound)
    {
        throw error("Сontact user without 'rn' parameter");
    }

    request.result.rawData = data;
}
