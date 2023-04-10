#ifndef SERVER_SRC_DRIVERS_HTTPBULKVSDRIVER_H_
#define SERVER_SRC_DRIVERS_HTTPBULKVSDRIVER_H_

#include "Driver.h"
#include "resolver/Resolver.h"

/**
 * @brief Driver configuration class
 */
class CHttpBulkvsDriverCfg: public CDriverCfg
{
  private:
    CfgUrl_t              mUrl;
    CfgKey_t              mToken;
    CfgTimeout_t          mTimeout;
    CfgFlag_t             mValidateHttpsCer;

    static const CfgUrl_t getRawUrl(JSONConfig_t & data);
    static const CfgKey_t getRawToken(JSONConfig_t & data);
    static const CfgFlag_t getRawValidateHttpsCer(JSONConfig_t & data);

  public:
    explicit CHttpBulkvsDriverCfg(const CDriverCfg::RawConfig_t & data);

    const CfgKey_t       getToken() const               { return mToken; }
    const CfgUrl_t       getUrl() const                 { return mUrl; }
    const CfgTimeout_t   getTimeout() const             { return mTimeout; }
    const CfgFlag_t      getValidateHttpsCer() const    { return mValidateHttpsCer; }
};

/**
 * @brief Driver class
 */
class CHttpBulkvsDriver: public CDriver
{
  private:
    unique_ptr<CHttpBulkvsDriverCfg> mCfg;

  public:
    explicit CHttpBulkvsDriver(const CDriverCfg::RawConfig_t & data);
    ~CHttpBulkvsDriver() override = default;

    void showInfo() const override;
    void resolve(ResolverRequest &request,
                 Resolver *resolver,
                 ResolverDelegate *delegate) const override;

    void parse(const string &data, ResolverRequest &request) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return mCfg->getUniqId(); }
};

#endif /* SERVER_SRC_DRIVERS_HTTPBULKVSDRIVER_H_ */
