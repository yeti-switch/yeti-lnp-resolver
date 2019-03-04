#include <pqxx/pqxx>
#include <sys/time.h>

#include "cfg.h"
#include "cache.h"
#include "drivers/Driver.h"
#include "drivers/DriverConfig.h"
#include "Resolver.h"

static const char * sLoadLNPConfigSTMT = "SELECT * FROM load_lnp_databases()";

#if 0
void _resolver::stop()
{
	for(auto &i: databases)
		i.second->driver->on_stop();
}

void _resolver::launch()
{
	for(const auto &it: databases) {
		const database_entry &e = *it.second;
		e.driver->launch();
	}
}
#endif

/*
 * Load resolver drivers configuration from database table
 *
 * @param[in,out] dbMap   The map of the successfully loaded drivers
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
      const pqxx::result::tuple & dbResultRaw = dbResult[i];
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
    // show driver configuration error
    err("Driver config [%s]: %s", e.getIdent(), e.what())
  }
  catch (...)
  {
    err("Unexpected exception");
  }

  return rv;
}

//FIXME: description
//TODO: make refactoring after driver implementation
bool CResolver::configure()
{
	Database_t dbMap;
	bool rv = loadResolveDrivers(dbMap);
	if (rv)
	{
	  //Mutex required to proper processing SIGHUP signal
	  guard(mDatabaseMutex);
	  mDatabaseMap.swap(dbMap);
	  //
//	  try {
//	    launch();
//	  } catch(...) { };

	}

	//FIXME: check on_stop method for necessity of use

	return rv;
}

/*
 * Method to make incoming string resolving by selected driver id
 *
 * @param[in]  dbId       The unique identity of the driver for resolving
 * @param[in]  inData     The input string with number for lookup
 * @param[out] outResult  The structure used to save a resolving result
 * @return none
 */
void CResolver::resolve(const CDriverCfg::CfgUniqId_t dbId,
                        const string & inData,
                        CDriver::SResult_t & outResult)
{
  //Mutex required to proper processing SIGHUP signal
	guard(mDatabaseMutex);

  auto mapItem = mDatabaseMap.find(dbId);
  if (mapItem == mDatabaseMap.end())
  {
    throw CDriver::error("unknown database id");
  }

  // Declare variables for resolving procedure time execution
  timeval req_start;
  timeval req_end;
  timeval req_diff;

  CDriver * drv = mapItem->second.get();

	try
	{
	    gettimeofday(&req_start, NULL);

	    // Make resolving action
	    drv->resolve(inData, outResult);

	    gettimeofday(&req_end, NULL);
	    timersub(&req_end, &req_start, &req_diff);

	    dbg("Resolved (by '%s/%d'): %s -> %s (tag: '%s') [in %ld.%06ld ms]",
	        drv->getName(), drv->getUniqueId(),
	        inData.c_str(),
	        outResult.localNumberPortability.c_str(),
	        outResult.tag.c_str(),
	        req_diff.tv_sec, req_diff.tv_usec);

	}
	catch(CDriver::error & e)
	{
		throw e;
	}
	catch(...)
	{
		dbg("unknown resolve exception");
		throw CDriver::error("internal error");
	}

	lnp_cache::instance()->sync(
	          new cache_entry(drv->getUniqueId(), inData, outResult)
	);
}
