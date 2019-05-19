#include "log.h"
#include "MhashCsvDriver.h"

/**************************************************************
 * Configuration implementation
***************************************************************/
/**
 * @brief Retrieve CSV file path from configuration data (Basic format)
 *
 * @param[in] data     The database output with driver configuration
 *
 * @return CSV file path value
 */
const CDriverCfg::CfgFilePath_t CMhashCsvDriverCfg::getRawFilePath(
                                                      const RawConfig_t & data)
{
  CfgFilePath_t file;

  if ((ECONFIG_DATA_AS_SEPARATE_COLUMN_V1 == sConfigType) ||
      (ECONFIG_DATA_AS_SEPARATE_COLUMN_V2 == sConfigType))
  {
    file = data["o_csv_file"].as<CfgFilePath_t>();
  }

  return file;
}

/**
 * @brief Retrieve CSV file path from configuration data (JSON format)
 *
 * @param[in] data    The database output with driver configuration
 *
 * @return CSV file path value
 */
const CDriverCfg::CfgFilePath_t CMhashCsvDriverCfg::getRawFilePath(
                                                        JSONConfig_t & data)
{
  CfgFilePath_t file;

  if (ECONFIG_DATA_AS_JSON_STRING == sConfigType)
  {
    file = static_cast<CfgFilePath_t> (data["csv_file_path"]);
  }

  return file;
}

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CMhashCsvDriverCfg::CMhashCsvDriverCfg(const CDriverCfg::RawConfig_t & data)
  : CDriverCfg(data)
{
  if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING == CDriverCfg::getFormatType())
  {
    try
    {
      jsonxx jData(data["parameters"].c_str());

      mFilePath = getRawFilePath(jData);
      if (0 == mFilePath.length())
      {
        throw error(getLabel(), "invalid csv file path!");
      }

    }
    catch (std::exception & e)
    {
      throw error(getLabel(), e.what());
    }
  }
  else
  {
    mFilePath = getRawFilePath(data);
    if (0 == mFilePath.length())
    {
      throw error(getLabel(), "invalid csv file path!");
    }
  }
}

/**************************************************************
 * Driver implementation
***************************************************************/
/**
 * @brief Driver constructor
 *
 * @param[in] data  The raw configuration data
 */
CMhashCsvDriver::CMhashCsvDriver(const CDriverCfg::RawConfig_t & data)
  :CDriver(ECDriverId::ERESOLVER_DRIVER_MHASH_CSV, "MHASH/CSV")
{
  mCfg.reset(new CMhashCsvDriverCfg(data));

  mCsvHash.reset(new CCsvClient(mCfg->getFilePath(),
                                ECSV_FIELD_NUMBER,
                                ECSV_FIELD_MAX_VALUE));
}

/**
 * @brief Show drive information
 */
void CMhashCsvDriver::showInfo() const
{
  info("[%u/%s] '%s' driver => file '%s' "
       "[hash size - %zu items]",
       mCfg->getUniqId(),
       mCfg->getLabel(), getName(),
       mCfg->getFilePath(),
       mCsvHash->size());
}

/**
 * @brief Executing resolving procedure
 *
 * @param[in] inData      The source data for making resolving
 * @param[out] outResult  The structure for saving resolving result
 *
 * @note Supports the resolve tag and number in the output result
 */
void CMhashCsvDriver::resolve(const string & inData, SResult_t & outResult) const
{
  /*
   * CSV file has a format with following fields:
   * 0730112354,AT&T mobile,0901234455
   */

  dbg("resolving by in-memory search for number '%s'", inData.c_str());

  try
  {
    const CCsvClient::row_t * csvRow = mCsvHash->perform(inData);

    if (!csvRow)
    {
      //TODO: check if this logic required
      dbg("number '%s' not found in hash. Set out to input data with epty tag",
           inData.c_str());

      outResult.localRoutingNumber = inData;
    }
    else
    {
      outResult.localRoutingNumber = csvRow->field[ECSV_FIELD_ROUTING_NUMBER];
      outResult.localRoutingTag    = csvRow->field[ECSV_FIELD_ROUTING_TAG];
    }
  }
  catch (CCsvClient::error & e)
  {
    throw error(e.what());
  }
}
