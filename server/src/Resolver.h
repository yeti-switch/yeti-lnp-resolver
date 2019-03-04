#ifndef SERVER_SRC_RESOLVER_H_
#define SERVER_SRC_RESOLVER_H_

#include <string>
using std::string;

#include <memory>
using std::unique_ptr;

#include <stdexcept>
using std::logic_error;

#include <map>

#include "singleton.h"
#include "drivers/Driver.h"

/*
 * Forward declaration for
 * singleton driver type define
 */
class CResolver;
using resolver = singleton<CResolver>;

/*
 * Resolver class
 */
class CResolver
{
  public:
    // Redefine resolver error
    using error = CDriver::error;

  private:
    // Databases type defines
    using Database_t = std::map<CDriverCfg::CfgUniqId_t, unique_ptr<CDriver> >;

    Database_t mDatabaseMap;
    mutex      mDatabaseMutex;

    bool loadResolveDrivers(Database_t & db);

  public:
    CResolver() = default;
    ~CResolver() = default;

#if 0
    void launch();
    void stop();
#endif

    bool configure();
    void resolve(const CDriverCfg::CfgUniqId_t dbId,
                 const string & inData,
                 CDriver::SResult_t & outResult);
};

#endif

