#ifndef SERVER_SRC_DRIVERS_DRIVERCONFIG_H_
#define SERVER_SRC_DRIVERS_DRIVERCONFIG_H_

#include<cstdint>
using std::uint8_t;

#include <stdexcept>
using std::runtime_error;

#include <string>
using std::string;

#include <pqxx/pqxx>

#include "libs/jsonxx.h"
#include "libs/fmterror.h"

/**
 * @brief Enum typedef forward declaration
 */
enum class ECDriverId : uint8_t;

/**
 * @brief Driver configuration interface
 */
class CDriverCfg
{
  public:
    // Driver configuration exception class
    class error : public runtime_error
    {
      private:
        string mIdent;

       public:
        template <typename ... Args>
        explicit error(const char * fmt, Args ... args) :
          runtime_error(fmterror(fmt, args ...).get()), mIdent("Unknown") { }

        template <typename ... Args>
        explicit error(const char * ident, const char * fmt, Args ... args) :
          runtime_error(fmterror(fmt, args ...).get()), mIdent(ident) { }

        const char * getIdent() const { return mIdent.c_str(); }
    };

    // Configuration data types
    using CfgUniqId_t   = int32_t;
    using CfgLabel_t    = string;
    using CfgTimeout_t  = uint32_t;
    using CfgPort_t     = uint16_t;
    using CfgProtocol_t = char;
    using CfgHost_t     = string;
    using CfgUserName_t = string;
    using CfgToken_t    = string;
    using CfgKey_t      = string;
    using CfgFilePath_t = string;

    // Raw driver configuration
    using RawConfig_t = pqxx::row;
    // JSON driver configuration
    using JSONConfig_t = jsonxx;

  private:
    CfgUniqId_t mUniqId;       // Driver unique identifier depend on
                               // database primary key. Common for all drivers

    CfgLabel_t mLabel;         // User defined label for driver

  protected:
    // Supported driver configuration formats
    // NOTE: the list should keep its order as it is!
    enum ECfgFormat_t
    {
      ECONFIG_DATA_INVALID = 0,
      ECONFIG_DATA_AS_SEPARATE_COLUMN_V1,   // initial configuration format
      ECONFIG_DATA_AS_SEPARATE_COLUMN_V2,   // with Alcazar network data
      ECONFIG_DATA_AS_JSON_STRING,          // JSON string configuration
      ECONFIG_DATA_NOT_SUPPORTED
    };

    // Protected members
    static ECfgFormat_t sConfigType;

    // General getters for raw configuration processing
    static const CfgUniqId_t getRawUniqId(const RawConfig_t & data);
    static const CfgLabel_t getRawLabel(const RawConfig_t & data);

    static const CfgTimeout_t getRawTimeout(const RawConfig_t & data);
    static const CfgTimeout_t getRawTimeout(JSONConfig_t & data);

    static const CfgPort_t getRawPort(const RawConfig_t & data);
    static const CfgPort_t getRawPort(JSONConfig_t & data);

    static const CfgHost_t getRawHost(const RawConfig_t & data);
    static const CfgHost_t getRawHost(JSONConfig_t & data);

    static const CfgUserName_t getRawUserName(const RawConfig_t & data);
    static const CfgUserName_t getRawUserName(JSONConfig_t & data);

  public:
    explicit CDriverCfg(const RawConfig_t & data);
    virtual ~CDriverCfg() = default;

    static const ECDriverId   getID(const RawConfig_t & data);
    static const ECfgFormat_t getFormatType();
    static const char *       getFormatStrType();

    const CfgUniqId_t  getUniqId() const { return mUniqId; }
    const char * getLabel() const  { return mLabel.c_str(); }
};

#endif /* SERVER_SRC_DRIVERS_DRIVERCONFIG_H_ */
