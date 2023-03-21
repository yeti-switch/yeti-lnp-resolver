#include <algorithm>
#include <cstring>
#include <cstdlib>

#include <cstdint>
using std::uint8_t;

#include "Driver.h"
#include "DriverConfig.h"

// Initialize static member and predefined variables
CDriverCfg::ECfgFormat_t CDriverCfg::sConfigType   = CDriverCfg::ECONFIG_DATA_INVALID;
const CDriverCfg::CfgTimeout_t defaultTimeout      = 4000;

/**
 * @brief Method to retrieve driver identify from dabatase raw
 *        with detection driver configuration format type
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return driver identifier
 *
 * @see EDriverId_t
 */
const ECDriverId CDriverCfg::getID(const RawConfig_t & data)
{
  ECDriverId driverId = ECDriverId::ERESOLVER_DRIVER_NULL;

  // Try to detect configuration format type
  if (ECONFIG_DATA_INVALID >= sConfigType)
  {
    do
    {
      try
      {
        if (data["parameters"].type())
        {
          sConfigType = ECONFIG_DATA_AS_JSON_STRING;
          break;
        }
      } catch (const pqxx::pqxx_exception & e) { }

      try
      {
        if (data["o_alkazar_key"].type())
        {
          sConfigType = ECONFIG_DATA_AS_SEPARATE_COLUMN_V2;
          break;
        }
      } catch (const pqxx::pqxx_exception & e) { }

      try
      {
        if (data["o_thinq_token"].type())
        {
          sConfigType = ECONFIG_DATA_AS_SEPARATE_COLUMN_V1;
          break;
        }
      } catch (const pqxx::pqxx_exception & e) { }

    } while(0);
  }

  // Handling driver identifier
  try
  {
    // Database format for yeti-web v1.8
    const char * strId = data["database_type"].c_str();
    if ((nullptr != strId) && (0 != std::strlen(strId)))
    {
      auto * drv = std::find_if(sDriverTypeArray.begin(),
                                sDriverTypeArray.end(),
                                [strId] (const SDriverTypeMap_t & id)
                                  { return 0 == std::strcmp(strId, id.stringId); }
      );

      driverId = drv->privateId;
    }
  }
  catch (const pqxx::pqxx_exception &e)
  {
    // Database format for yeti-web v1.7
    uint16_t numId = data["o_driver_id"].as<uint16_t>(0);
    if (0 < numId)
    {
      auto * drv = std::find_if(sDriverTypeArray.begin(),
                                sDriverTypeArray.end(),
                                [numId] (const SDriverTypeMap_t & id)
                                  { return numId == id.numberId; }
      );

      driverId = drv->privateId;
    }
  }

  if (ECDriverId::ERESOLVER_DRIVER_MAX_ID <= driverId)
  {
    driverId = ECDriverId::ERESOLVER_DRIVER_NULL;
  }
  else if ((ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR == driverId) &&
           (ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 >= sConfigType))
  {
    driverId = ECDriverId::ERESOLVER_DRIVER_NULL;
  }

  return driverId;
}

/**
 * @brief Getter for configuration format type
 *
 * @return format type
 *
 * @see CDriverCfg::ECfgFormat_t
 */
const CDriverCfg::ECfgFormat_t CDriverCfg::getFormatType()
{
  return sConfigType;
}

/**
 * @brief Getter for configuration format type string value
 *
 * @return string representation of the format type
 */
const char *CDriverCfg::getFormatStrType()
{
  const char *rv = "unknown configuration";

  switch(sConfigType)
  {
    case ECONFIG_DATA_AS_SEPARATE_COLUMN_V1:
      rv = "separate column configuration (ver. 1)";
      break;

    case ECONFIG_DATA_AS_SEPARATE_COLUMN_V2:
      rv = "separate column configuration (ver. 2)";
      break;

    case ECONFIG_DATA_AS_JSON_STRING:
      rv = "JSON format configuration";
      break;

    default:
      break;
  }

  return rv;
}

/**
 * @brief Method to retrieve raw data for driver unique identifier
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return configuration identifier
 */
const CDriverCfg::CfgUniqId_t CDriverCfg::getRawUniqId(const RawConfig_t & data)
{
  CfgUniqId_t uniqID = -1;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
           (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    uniqID = data["o_id"].as<CfgUniqId_t>(-1);
  }
  else if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    uniqID = data["id"].as<CfgUniqId_t>(-1);
  }

  return uniqID;
}

/**
 * @brief Method to retrieve driver user defined label
 *
 * @param[in] data  The database output with driver data
 *
 * @return driver label
 */
const CDriverCfg::CfgLabel_t CDriverCfg::getRawLabel(const RawConfig_t & data)
{
  CfgLabel_t label;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
           (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    label = data["o_name"].as<CfgLabel_t>();
  }
  else if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    label = data["name"].as<CfgLabel_t>();
  }

  return label;
}

/**
 * @brief Method for retrieving timeout value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return timeout value
 */
const CDriverCfg::CfgTimeout_t CDriverCfg::getRawTimeout(const RawConfig_t & data)
{
  CfgTimeout_t timeout = defaultTimeout;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    timeout = data["o_timeout"].as<CfgTimeout_t>(defaultTimeout);
  }

  return timeout;
}

/**
 * @brief Method for retrieving timeout value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return timeout value
 */
const CDriverCfg::CfgTimeout_t CDriverCfg::getRawTimeout(JSONConfig_t & data)
{
  CfgTimeout_t timeout = defaultTimeout;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    timeout = static_cast<CfgTimeout_t> (data["timeout"]);
  }

  return timeout;
}

/**
 * @brief Method for retrieving port value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return port value
 */
const CDriverCfg::CfgPort_t CDriverCfg::getRawPort(const RawConfig_t & data)
{
  CfgPort_t port = 0;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    port = data["o_port"].as<CfgPort_t>(0);
  }

  return port;
}

/**
 * @brief Method for retrieving port value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return port value
 */
const CDriverCfg::CfgPort_t CDriverCfg::getRawPort(JSONConfig_t & data)
{
  CfgPort_t port = 0;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    port = static_cast<CfgPort_t> (data["port"]);
  }

  return port;
}

/**
 * @brief Method for retrieving host value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return host value
 */
const CDriverCfg::CfgHost_t CDriverCfg::getRawHost(const RawConfig_t & data)
{
  CfgHost_t host;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    host = data["o_host"].as<CfgHost_t>();
  }

  return host;
}

/**
 * @brief Method for retrieving host value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return host value
 */
const CDriverCfg::CfgHost_t CDriverCfg::getRawHost(JSONConfig_t & data)
{
  CfgHost_t host;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    host = static_cast<CfgHost_t> (data["host"]);
  }

  return host;
}

/**
 * @brief Method for retrieving user name value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return user name value
 */
const CDriverCfg::CfgUserName_t CDriverCfg::getRawUserName(const RawConfig_t & data)
{
  CfgUserName_t userName;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    userName = data["o_thinq_username"].as<CfgUserName_t>();
  }

  return userName;
}

/**
 * @brief Method for retrieving user name value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 *
 * @return user name value
 */
const CDriverCfg::CfgUserName_t CDriverCfg::getRawUserName(JSONConfig_t & data)
{
  CfgUserName_t userName;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    userName = static_cast<CfgUserName_t> (data["username"]);
  }

  return userName;
}

/**
 * @brief Constructor method for basic class
 */
CDriverCfg::CDriverCfg(const RawConfig_t & data)
{
  mUniqId = getRawUniqId(data);
  if (-1 == mUniqId)
  {
    throw error("not found driver unique identifier!");
  }

  mLabel = getRawLabel(data);
  if (0 == mLabel.length())
  {
    throw error("not found user defined name for driver!");
  }
}
