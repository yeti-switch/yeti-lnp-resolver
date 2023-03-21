#include <pqxx/pqxx>
#include <sys/time.h>

#include "cfg.h"
#include "cache.h"
#include "drivers/Driver.h"
#include "drivers/DriverConfig.h"
#include "Resolver.h"
#include "statistics/prometheus/prometheus_exporter.h"

static const char * sLoadLNPConfigSTMT = "SELECT * FROM load_lnp_databases()";

/**
 * @brief Load resolver drivers configuration from database table
 *
 * @param[in,out] dbMap   The map of the successfully loaded drivers
 *
 * @return boolean value as a loading procedure status
 */
bool CResolver::loadResolveDrivers(Database_t & dbMap)
{
  // Status about successfully loaded
  // database and initialized resolvers map
  bool rv = false;

  // Try to load drivers from database configuration
  try
  {
    pqxx::result dbResult;
    pqxx::connection dbConn(cfg.db.get_conn_string());
    dbConn.set_variable("search_path", cfg.db.schema + ", public");

    pqxx::work t(dbConn);
    dbResult = t.exec(sLoadLNPConfigSTMT);

    for (pqxx::result::size_type i = 0; i < dbResult.size(); ++i)
    {
      const pqxx::row & dbResultRaw = dbResult[i];
      unique_ptr<CDriver> drv = CDriver::instantiate(dbResultRaw);
      if (drv)
      {
        dbMap.emplace(drv->getUniqueId(), std::move(drv));
      }
      else
      {
        warn("Not supported driver provided in the raw %lu", i);
      }
    }

    // Show information about loaded drivers
    info("Loaded %lu drivers with %s", dbMap.size(), CDriverCfg::getFormatStrType());
    for (const auto & i : dbMap)
    {
      i.second->showInfo();
    }
    rv = true;
  }
  catch (const pqxx::pqxx_exception & e)
  {
    // show pqxx exception messages
    err("pqxx_exception: %s ", e.base().what());
  }
  catch (const CDriverCfg::error & e)
  {
    // show driver configuration error (abort main process)
    err("Driver config [%s]: %s", e.getIdent(), e.what());
  }
  catch (...)
  {
    // general exception handler (abort main process)
    err("Unexpected driver loading exception");
  }

  return rv;
}

/**
 * @brief Root method to load driver configuration
 *
 * @return boolean status about loading result
 */
bool CResolver::configure()
{
  Database_t dbMap;
  bool rv = loadResolveDrivers(dbMap);
  if (rv)
  {
    //Mutex required to proper processing SIGHUP signal
    guard(mDriversMutex);
    mDriversMap.swap(dbMap);
  }

  return rv;
}

/**
 * @brief Method to make incoming string resolving by selected driver id
 *
 * @param[in]  dbId       The unique identity of the driver for resolving
 * @param[in]  inData     The input string with number for lookup
 * @param[out] outResult  The structure used to save a resolving result
 */
void CResolver::resolve(int request_type,
                        const CDriverCfg::CfgUniqId_t dbId,
                        const string & inData,
                        CDriver::SResult_t & outResult)
{
  //Mutex required to proper processing SIGHUP signal
  guard(mDriversMutex);

  auto mapItem = mDriversMap.find(dbId);
  if (mapItem == mDriversMap.end())
  {
    throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, "unknown database id");
  }

  // Declare variables for resolving procedure time execution
  timeval req_start;
  timeval req_end;
  timeval req_diff;
  uint64_t req_diff_millis;

  CDriver * drv = mapItem->second.get();

  if(drv->getDriverType() != request_type) {
    throw CResolverError(
      ECErrorId::GENERAL_RESOLVING_ERROR,
      "request type is unsupported");
  }

  try
  {
    drv->requests_count_increment();
    gettimeofday(&req_start, nullptr);

    // Make resolving action
    drv->resolve(inData, outResult);

    gettimeofday(&req_end, nullptr);
    timersub(&req_end, &req_start, &req_diff);
    req_diff_millis = req_diff.tv_sec * 1000 + req_diff.tv_usec / 1000;

    if(drv->getDriverType() == CDriver::DriverTypeTagged) {
        dbg("Resolved (by '%s/%d'): %s -> %s (tag: '%s') [in %ld.%06ld seconds]",
            drv->getName(), drv->getUniqueId(),
            inData.c_str(),
            outResult.localRoutingNumber.c_str(),
            outResult.localRoutingTag.c_str(),
            req_diff.tv_sec, req_diff.tv_usec);
    } else {
        dbg("Resolved (by '%s/%d'): %s -> %s [in %ld.%06ld seconds]",
            drv->getName(), drv->getUniqueId(),
            inData.data(),
            outResult.rawData.data(),
            req_diff.tv_sec, req_diff.tv_usec);
    }

    drv->requests_finished_increment(req_diff_millis);
  }
  catch(const CDriver::error & e)
  {
    drv->requests_failed_increment();
    throw CResolverError(ECErrorId::DRIVER_RESOLVING_ERROR, e.what());
  }
  catch(...)
  {
    drv->requests_failed_increment();
    throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR,
                         "unknown resovling exception");
  }

  if(drv->getDriverType() == CDriver::DriverTypeTagged)
    lnp_cache::instance()->sync(
      new cache_entry(drv->getUniqueId(), inData, outResult));
}
