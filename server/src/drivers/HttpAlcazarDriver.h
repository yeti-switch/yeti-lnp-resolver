#ifndef SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_
#define SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_

#include "Driver.h"

/*
 * Driver configuration class
 */
class CHttpAlcazarDriverCfg: public CDriverCfg
{
  private:
    const CfgHost_t * mHost;
    CfgPort_t         mPort;
    const CfgKey_t *  mKey;
    CfgTimeout_t      mTimeout;

  public:
    CHttpAlcazarDriverCfg(const CDriverCfg::RawConfig_t & data,
                          const ECDriverId drvId);

    const CfgHost_t *  getHost() const    { return mHost; }
    const CfgPort_t    getPort() const    { return mPort; }
    const CfgKey_t *   geKey() const      { return mKey; }
    const CfgTimeout_t getTimeout() const { return mTimeout; }
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
    ~CHttpAlcazarDriver() = default;

    void showInfo() const override;
    void resolve(const string & inData, SResult_t & outResult) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const
                              { return mCfg->getUniqId(); }
};

#endif /* SERVER_SRC_DRIVERS_HTTPALCAZARDRIVER_H_ */
