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

/*
 * Method to retrieve driver identify from dabatase raw
 * with detection driver configuration format type
 *
 * @param[in] data  The database output with driver configuration
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
      } catch (const pqxx::pqxx_exception &e) { }

      try
      {
        if (data["o_alkazar_key"].type())
        {
          sConfigType = ECONFIG_DATA_AS_SEPARATE_COLUMN_V2;
          break;
        }
      } catch (const pqxx::pqxx_exception &e) { }

      try
      {
        if (data["o_thinq_token"].type())
        {
          sConfigType = ECONFIG_DATA_AS_SEPARATE_COLUMN_V1;
          break;
        }
      } catch (const pqxx::pqxx_exception &e) { }

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

/*
 * Getter for configuration format type
 *
 * @return format type
 *
 * @see CDriverCfg::ECfgFormat_t
 */
const CDriverCfg::ECfgFormat_t CDriverCfg::getFormatType()
{
  return sConfigType;
}

/*
 * Getter for configuration format type string value
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

/*
 * Constructor method for basic class
 */
CDriverCfg::CDriverCfg(const RawConfig_t & data)
{
  mUniqId = getRawUniqId(data);
  if (-1 == mUniqId)
  {
    throw error(nullptr, "not found driver unique identifier!");
  }

  mLabel = getRawLabel(data);
  if ((nullptr == mLabel) ||
      (0 == std::strlen(static_cast<const char *> (mLabel))))
  {
    throw error(nullptr, "not found user defined name for driver!");
  }
}

/*
 * Method to retrieve raw data for driver unique identifier
 *
 * @param[in] data  The database output with driver configuration
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

/*
 * Method to retrieve driver user defined label
 *
 * @param[in] data  The database output with driver data
 * @return driver label
 */
const CDriverCfg::CfgLabel_t * CDriverCfg::getRawLabel(const RawConfig_t & data)
{
  const CfgLabel_t * label = nullptr;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
           (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    label = data["o_name"].c_str();
  }
  else if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    label = data["name"].c_str();
  }

  return label;
}

/*
 * Method for retrieving timeout value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
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

/*
 * Method for retrieving timeout value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 * @return timeout value
 */
const CDriverCfg::CfgTimeout_t CDriverCfg::getRawTimeout(JSONConfig_t * data)
{
  CfgTimeout_t timeout = defaultTimeout;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      cJSON * jPort = cJSON_GetObjectItem(data, "timeout");
      if (jPort)
      {
        char * strTimeout = cJSON_Print(jPort);
        if (nullptr != strTimeout)
        {
          timeout = static_cast<CfgPort_t> (
                            std::atoi(static_cast<const char *> (strTimeout)));
          free(strTimeout);
        }
      }
    }
  }

  return timeout;
}

/*
 * Method for retrieving port value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
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

/*
 * Method for retrieving port value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 * @return port value
 */
const CDriverCfg::CfgPort_t CDriverCfg::getRawPort(JSONConfig_t * data)
{
  CfgPort_t port = 0;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      cJSON * jPort = cJSON_GetObjectItem(data, "port");
      if (jPort)
      {
        char * strPort = cJSON_Print(jPort);
        if (nullptr != strPort)
        {
          port = static_cast<CfgPort_t> (
                        std::atoi(static_cast<const char *> (strPort)));
          free(strPort);
        }
      }
    }
  }

  return port;
}

/*
 * Method for retrieving host value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 * @return host value
 */
const CDriverCfg::CfgHost_t * CDriverCfg::getRawHost(const RawConfig_t & data)
{
  const CfgHost_t * host = nullptr;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    host = static_cast<const CfgHost_t *> (data["o_host"].c_str());
  }

  return host;
}

/*
 * Method for retrieving host value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 * @return host value
 */
const CDriverCfg::CfgHost_t * CDriverCfg::getRawHost(JSONConfig_t * data)
{
  CfgHost_t * host = nullptr;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      cJSON * jHost = cJSON_GetObjectItem(data, "host");
      if (jHost)
      {
        host = cJSON_Print(jHost);
        //TODO: improve this code chunk to remove quotes
        host += 1;
        host[std::strlen(host) - 1] = '\0';
      }
    }
  }

  return host;
}

/*
 * Method for retrieving user name value from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 * @return user name value
 */
const CDriverCfg::CfgUserName_t * CDriverCfg::getRawUserName(const RawConfig_t & data)
{
  const CfgUserName_t * userName = nullptr;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    userName = static_cast<const CfgUserName_t *> (data["o_thinq_username"].c_str());
  }

  return userName;
}

/*
 * Method for retrieving user name value from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 * @return user name value
 */
const CDriverCfg::CfgUserName_t * CDriverCfg::getRawUserName(JSONConfig_t * data)
{
  CfgUserName_t * userName = nullptr;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      cJSON * jUserName = cJSON_GetObjectItem(data, "username");
      if (jUserName)
      {
        userName = cJSON_Print(jUserName);
        //TODO: improve this code chunk to remove quotes
        userName += 1;
        userName[std::strlen(userName) - 1] = '\0';
      }
    }
  }

  return userName;
}

/*
 * Method for retrieving key value from configuration data (Basic format).
 * This can be hash/token/password, etc.
 *
 * @param[in] data    The database output with driver configuration
 * @param[in] drvId   The driver identifier
 * @return key value
 */
const CDriverCfg::CfgKey_t * CDriverCfg::getRawKey(const RawConfig_t & data,
                                                   const ECDriverId drvId)
{
  const CfgKey_t * key = nullptr;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    switch(drvId)
    {
      case ECDriverId::ERESOLVER_DRIVER_HTTP_THINQ:
        key = static_cast<const CfgKey_t *> (data["o_thinq_token"].c_str());
        break;
      case ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR:
        key = static_cast<const CfgKey_t *> (data["o_alkazar_key"].c_str());
        break;
      default:
        key = nullptr;
        break;
    }
  }

  return key;
}

/*
 * Method for retrieving key value from configuration data (JSON format).
 * This can be hash/token/password, etc.
 *
 * @param[in] data  The database output with driver configuration
 * @param[in] drvId   The driver identifier
 * @return key value
 */
const CDriverCfg::CfgKey_t * CDriverCfg::getRawKey(JSONConfig_t * data,
                                                   const ECDriverId drvId)
{
  CfgKey_t * key = nullptr;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      if (ECDriverId::ERESOLVER_DRIVER_HTTP_THINQ == drvId)
      {
        cJSON * jKey = cJSON_GetObjectItem(data, "token");
        if (jKey)
        {
          key = cJSON_Print(jKey);
        }
      }
      else if (ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR == drvId)
      {
        cJSON * jKey = cJSON_GetObjectItem(data, "key");
        if (jKey)
        {
          key = cJSON_Print(jKey);
        }
      }
      if (nullptr != key)
      {
        //TODO: improve this code chunk to remove quotes
        key += 1;
        key[std::strlen(key) - 1] = '\0';
      }
    }
  }

  return key;
}

/*
 * Method for retrieving file path from configuration data (Basic format)
 *
 * @param[in] data  The database output with driver configuration
 * @return file path value
 */
const CDriverCfg::CfgFilePath_t * CDriverCfg::getRawFilePath(const RawConfig_t & data)
{
  const CfgFilePath_t * filePath = nullptr;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    filePath = static_cast<const CfgFilePath_t *> (data["o_csv_file"].c_str());
  }

  return filePath;
}

/*
 * Method for retrieving file path from configuration data (JSON format)
 *
 * @param[in] data  The database output with driver configuration
 * @return file path value
 */
const CDriverCfg::CfgFilePath_t * CDriverCfg::getRawFilePath(JSONConfig_t * data)
{
  CfgFilePath_t * filePath = nullptr;

  if (nullptr != data)
  {
    if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
    {
      cJSON * jFilePath = cJSON_GetObjectItem(data, "csv_file_path");
      if (jFilePath)
      {
        filePath = cJSON_Print(jFilePath);
        //TODO: improve this code chunk to remove quotes
        filePath += 1;
        filePath[std::strlen(filePath) - 1] = '\0';
      }
    }
  }

  return filePath;
}
