#ifndef SERVER_SRC_DRIVERS_HTTPTHINQDRIVER_H_
#define SERVER_SRC_DRIVERS_HTTPTHINQDRIVER_H_

#include "Driver.h"

/*
 * Driver configuration class
 */
class CHttpThinqDriverCfg: public CDriverCfg
{
  private:
    const CfgProtocol_t * mProtocol = "http";   //FIXME: replase to "https://"
    CfgHost_t             mHost;
    CfgPort_t             mPort;
    CfgUserName_t         mUserName;
    CfgToken_t            mToken;
    CfgTimeout_t          mTimeout;

    // Driver specific getters for raw configuration processing
    static const CfgToken_t getRawToken(const RawConfig_t & data);
    static const CfgToken_t getRawToken(JSONConfig_t * data);

  public:
    CHttpThinqDriverCfg(const CDriverCfg::RawConfig_t & data);

    const char *        getProtocol() const { return mProtocol; }
    const char *        getHost() const     { return mHost.c_str(); }
    const CfgPort_t     getPort() const     { return mPort; }
    const char *        getUserName() const { return mUserName.c_str(); }
    const char *        geToken() const     { return mToken.c_str(); }
    const CfgTimeout_t  getTimeout() const  { return mTimeout; }
};

/*
 * Driver class
 */
class CHttpThinqDriver: public CDriver
{
  private:
    unique_ptr<CHttpThinqDriverCfg> mCfg;
    string mURLPrefix;
    string mURLSuffix;

  public:
    CHttpThinqDriver(const CDriverCfg::RawConfig_t & data);
    ~CHttpThinqDriver() override = default;

    void showInfo() const override;
    void resolve(const string & inData, SResult_t & outResult) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return mCfg->getUniqId(); }
};



#endif /* SERVER_SRC_DRIVERS_HTTPTHINQDRIVER_H_ */
