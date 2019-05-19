#ifndef SERVER_SRC_DRIVERS_MHASHCSVDRIVER_H_
#define SERVER_SRC_DRIVERS_MHASHCSVDRIVER_H_

#include "Driver.h"
#include "drivers/modules/CsvClient.h"

/**
 * @brief Driver configuration class
 */
class CMhashCsvDriverCfg: public CDriverCfg
{
  private:
    CfgFilePath_t mFilePath;

    // Driver specific getters for war configuration processing
    static const CfgFilePath_t getRawFilePath(const RawConfig_t & data);
    static const CfgFilePath_t getRawFilePath(JSONConfig_t & data);

  public:
    CMhashCsvDriverCfg(const CDriverCfg::RawConfig_t & data);
    ~CMhashCsvDriverCfg() override = default;

    const char * getFilePath() const  { return mFilePath.c_str(); }
};

/**
 * @brief Driver class
 *
 * @note Supported tag result field! But raw
 *       field will not used for this driver!
 */
class CMhashCsvDriver: public CDriver
{
  public:
    // CSV file fields description
    enum ECCsvFields_t : uint8_t
    {
       ECSV_FIELD_NUMBER          // Primary field (used for searching)
      ,ECSV_FIELD_ROUTING_TAG     // Field value used for result routing tag
      ,ECSV_FIELD_ROUTING_NUMBER  // Field value used for result routing number
      ,ECSV_FIELD_MAX_VALUE
    };

  private:
    unique_ptr<CMhashCsvDriverCfg> mCfg;
    unique_ptr<CCsvClient> mCsvHash;

  public:
    CMhashCsvDriver(const CDriverCfg::RawConfig_t & data);
    ~CMhashCsvDriver() override = default;

    void showInfo() const override;
    void resolve(const string & inData, SResult_t & outResult) const override;

    const CDriverCfg::CfgUniqId_t getUniqueId() const override
                                                { return mCfg->getUniqId(); }
};

#endif /* SERVER_SRC_DRIVERS_MHASHCSVDRIVER_H_ */
