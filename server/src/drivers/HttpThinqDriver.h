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
    const CfgHost_t *     mHost;
    CfgPort_t             mPort;
    const CfgUserName_t * mUserName;
    const CfgToken_t *    mToken;
    CfgTimeout_t          mTimeout;

    // Driver specific getters for raw configuration processing
    static const CfgToken_t * getRawToken(const RawConfig_t & data);
    static const CfgToken_t * getRawToken(JSONConfig_t * data);

  public:
    CHttpThinqDriverCfg(const CDriverCfg::RawConfig_t & data);

    const CfgProtocol_t * getProtocol() const { return mProtocol; }
    const CfgHost_t *     getHost() const     { return mHost; }
    const CfgPort_t       getPort() const     { return mPort; }
    const CfgUserName_t * getUserName() const { return mUserName; }
    const CfgToken_t *    geToken() const     { return mToken; }
    const CfgTimeout_t    getTimeout() const  { return mTimeout; }
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
