#ifndef SERVER_SRC_DRIVERS_SIPDRIVER_H_
#define SERVER_SRC_DRIVERS_SIPDRIVER_H_

#include "Driver.h"
#include "drivers/modules/SipClient.h"

/**
 * @brief Driver configuration class
 */
class CSipDriverCfg: public CDriverCfg
{
  private:
    using SipCfgName_t      = string; // From/To name value
    using SipCfgUri_t       = string; // From/To URI value
    using SipCfgContact_t   = string;
    using SipCfgUserAgent_t = char;   // SIP User-Agent

    // Predefined driver configuration
    const SipCfgUserAgent_t * mUserAgent = "Yeti LNP resolver";
    const CfgProtocol_t *     mProtocol  = "sip";

    // Static driver configuration (system text configuration)
    static SipCfgName_t      sFromName;
    static SipCfgUri_t       sFromUri;
    static SipCfgContact_t   sContact;

    // Dynamic driver configuration (user settings from database)
    CfgHost_t             mHost;
    CfgPort_t             mPort;
    CfgTimeout_t          mTimeout;

  public:
    explicit CSipDriverCfg(const CDriverCfg::RawConfig_t & data);

    const char *        getUserAgent() const { return mUserAgent; }
    const char *        getProtocol() const  { return mProtocol; }
    const char *        getHost() const      { return mHost.c_str(); }
    const CfgPort_t     getPort() const      { return mPort; }
    const CfgTimeout_t  getTimeout() const   { return mTimeout; }

    static void         setStaticData();
    static const char * getFromName() { return sFromName.c_str(); }
    static const char * getFromUri()  { return sFromUri.c_str(); }
    static const char * getContact()  { return sContact.c_str(); }
};

/**
 * @brief Driver class
 */
class CSipDriver: public CDriver
{
  private:
    unique_ptr<CSipDriverCfg> mCfg;
    string mURIPrefix;
    string mURISuffix;

  public:
    explicit CSipDriver(const CDriverCfg::RawConfig_t & data);
    ~CSipDriver() override = default;

    void showInfo() const override;
    void resolve(ResolverRequest &request,
                 Resolver *resolver,
                 ResolverHandler *handler) const override;
    void parse(const string &data, ResolverRequest &request) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return mCfg->getUniqId(); }
};

#endif /* SERVER_SRC_DRIVERS_SIPDRIVER_H_ */
