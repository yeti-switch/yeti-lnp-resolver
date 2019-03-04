#ifndef SERVER_SRC_DRIVERS_DRIVER_H_
#define SERVER_SRC_DRIVERS_DRIVER_H_

#include <string>
using std::string;

#include <memory>
using std::unique_ptr;

#include <pqxx/pqxx>

#include "DriverDefines.h"
#include "DriverConfig.h"


/*
 * Resolver driver class
 */
class CDriver
{
  public:
    // Resolver exception class
    class error : public logic_error
    {
       public:
           explicit error(const std::string & what)
               : logic_error(what) { };
    };

    // Resolving output data
    struct SResult_t
    {
        string localNumberPortability;
        string tag;                //TODO: is this usable ?
        string rawData;
    };

  private:
     ECDriverId mId;      // Driver identifier
     const char * mName;  // Driver string name

  protected:
    const ECDriverId  getId() const   { return mId; }

  public:
    CDriver(const ECDriverId id, const char * name);
    virtual ~CDriver() = default;

    const char * getName() const  { return mName; }
    virtual const CDriverCfg::CfgUniqId_t getUniqueId() const = 0;
    virtual void showInfo() const = 0;
    virtual void resolve(const string & in, SResult_t & out) const = 0;

//    virtual void on_stop() {};  //FIXME: make analysis to remove this method
//    virtual void launch() {};   //FIXME: make analysis to remove this method

    static unique_ptr<CDriver> instantiate(const CDriverCfg::RawConfig_t & data);
};

#endif /* SERVER_SRC_DRIVERS_DRIVER_H_ */
