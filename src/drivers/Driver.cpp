#include "SipDriver.h"
#include "HttpThinqDriver.h"
#include "MhashCsvDriver.h"
#include "HttpAlcazarDriver.h"
#include "HttpCoureAnqDriver.h"
#include "CnamHttpDriver.h"
#include "HttpBulkvsDriver.h"
#include "Driver.h"

#include "statistics/prometheus/prometheus_exporter.h"

/**
 * @brief Class constructor
 */
CDriver::CDriver(const ECDriverId id, const char * name)
    : mId(ECDriverId::ERESOLVER_DRIVER_NULL), mName("NULL")
{
  if (name != nullptr)
  {
    mId    = id;
    mName  = name;
  }
}

/**
 * @brief Static method to create driver required object.
 *
 * @param[in] data  The input data to make detection
 *
 * @return The pointer to driver object otherwise null pointer.
 */
unique_ptr<CDriver> CDriver::instantiate(const CDriverCfg::RawConfig_t & data)
{
  unique_ptr<CDriver> rv(nullptr);
  ECDriverId driverId = CDriverCfg::getID(data);

  switch (driverId)
  {
    case ECDriverId::ERESOLVER_DRIVER_SIP:
      rv.reset((new CSipDriver(data)));
      break;

    case ECDriverId::ERESOLVER_DRIVER_HTTP_THINQ:
      rv.reset(new CHttpThinqDriver(data));
      break;

    case ECDriverId::ERESOLVER_DRIVER_MHASH_CSV:
      rv.reset(new CMhashCsvDriver(data));
      break;

    case ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR:
      rv.reset(new CHttpAlcazarDriver(data));
      break;

    case ECDriverId::ERESOLVER_DIRVER_HTTP_COUREANQ:
      rv.reset(new CHttpCoureAnqDriver(data));
      break;

    case ECDriverId::ERESOLVER_DIRVER_CNAM_HTTP:
      rv.reset(new CCnamHttpDriver(data));
      break;

    case ECDriverId::ERESOLVER_DIRVER_HTTP_BULKVS:
      rv.reset(new CHttpBulkvsDriver(data));
      break;

    default:
      break;
  }

  if(rv) rv->init_metrics();

  return rv;
};

void CDriver::init_metrics()
{
    prometheus_exporter::instance()->
        driver_init_metrics(getName(), getUniqueId());
}

void CDriver::requests_count_increment()
{
    prometheus_exporter::instance()->
        driver_requests_count_increment(getName(), getUniqueId());
}

void CDriver::requests_failed_increment()
{
    prometheus_exporter::instance()->
        driver_requests_failed_increment(getName(), getUniqueId());
}

void CDriver::requests_finished_increment(const double time_consumed)
{
    prometheus_exporter::instance()->
        driver_requests_finished_increment(getName(), getUniqueId(), time_consumed);
}
