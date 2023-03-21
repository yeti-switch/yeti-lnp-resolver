#pragma once

#include "Driver.h"
#include "libs/cJSON.h"

#include <map>
using std::map;

/**
 * @brief Driver configuration class
 */
struct CHttpCoureAnqDriverCfg: public CDriverCfg
{
    const CfgProtocol_t * protocol = "http";
    string base_url;
    string username;
    string password;
    string country_code;
    CfgTimeout_t timeout;

    struct OperatorsMap_t
      : public map<string, unsigned int>
    {
        unsigned int default_value;
        void parse(cJSON &j);
        unsigned int resolve(const string &operator_name) const;
    } operators_map;

    explicit CHttpCoureAnqDriverCfg(const CDriverCfg::RawConfig_t & data);
};

/**
 * @brief Driver class
 */
class CHttpCoureAnqDriver: public CDriver
{
  private:
    CHttpCoureAnqDriverCfg cfg;
    string url_prefix;

  public:
    explicit CHttpCoureAnqDriver(const CDriverCfg::RawConfig_t & data);
    ~CHttpCoureAnqDriver() override = default;

    void showInfo() const override;
    void resolve(const string & inData, SResult_t & outResult) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return cfg.getUniqId(); }
};

