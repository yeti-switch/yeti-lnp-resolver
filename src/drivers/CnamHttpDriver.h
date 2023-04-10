#pragma once

#include "Driver.h"
#include "libs/cJSON.h"

#include <map>
using std::map;

#include <vector>
using std::vector;

/**
 * @brief Driver configuration class
 */
struct CCnamHttpDriverCfg: public CDriverCfg
{
    string url;
    CfgTimeout_t timeout;

    class TemplateProcessor {
      private:
        struct UriPart {
            enum uri_part_type {
                URI_PART_STRING = 0,
                URI_PART_PLACEHOLDER
            } type;
            //string or placeholder key
            string data;
            UriPart(uri_part_type type, const string &data)
              : type(type),
                data(data)
            {}
        };
        vector<UriPart> uri_parts;
      public:
        void parse_url_template(string &uri);
        string get_url(cJSON *j) const;
    } template_processor;

    explicit CCnamHttpDriverCfg(const CDriverCfg::RawConfig_t & data);
};

/**
 * @brief Driver class
 */
class CCnamHttpDriver: public CDriver
{
  private:
    CCnamHttpDriverCfg cfg;
    //
  public:
    explicit CCnamHttpDriver(const CDriverCfg::RawConfig_t & data);
    ~CCnamHttpDriver() override = default;

    void showInfo() const override;
    int getDriverType() const override
    {
        return DriverTypeCnam;
    };
    void resolve(ResolverRequest &request,
                 Resolver *resolver,
                 ResolverDelegate *delegate) const override;
    void parse(const string &data, ResolverRequest &request) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
    {
        return cfg.getUniqId();
    }
};

