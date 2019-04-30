#ifndef SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_
#define SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_

#include "Driver.h"

/*
 * Driver configuration class
 */
class CHttpAlcazarDriverCfg: public CDriverCfg
{
  private:
    const CfgProtocol_t * mProtocol = "http";
    CfgHost_t             mHost;
    CfgPort_t             mPort;
    CfgKey_t              mKey;
    CfgTimeout_t          mTimeout;

    // Driver specific getters for raw configuration processing
    static const CfgKey_t getRawKey(const RawConfig_t & data);
    static const CfgKey_t getRawKey(JSONConfig_t & data);

  public:
    CHttpAlcazarDriverCfg(const CDriverCfg::RawConfig_t & data);

    const char *       getProtocol() const { return mProtocol; }
    const char *       getHost() const     { return mHost.c_str(); }
    const CfgPort_t    getPort() const     { return mPort; }
    const char *       geKey() const       { return mHost.c_str(); }
    const CfgTimeout_t getTimeout() const  { return mTimeout; }
};

/*
 * Driver class
 */
class CHttpAlcazarDriver: public CDriver
{
  private:
    unique_ptr<CHttpAlcazarDriverCfg> mCfg;
    string mURLPrefix;

  public:
    CHttpAlcazarDriver(const CDriverCfg::RawConfig_t & data);
    ~CHttpAlcazarDriver() override = default;

    void showInfo() const override;
    void resolve(const string & inData, SResult_t & outResult) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return mCfg->getUniqId(); }
};

#endif /* SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_ */
