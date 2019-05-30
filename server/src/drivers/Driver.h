#ifndef SERVER_SRC_DRIVERS_DRIVER_H_
#define SERVER_SRC_DRIVERS_DRIVER_H_

#include <string>
using std::string;

#include <memory>
using std::unique_ptr;

#include "libs/fmterror.h"
#include "DriverDefines.h"
#include "DriverConfig.h"

/**
 * @brief Resolver driver class
 */
class CDriver
{
  public:
    // Resolver exception class
    class error : public runtime_error
    {
      public:
        template <typename ... Args>
        explicit error(const char * fmt, Args ... args) :
          runtime_error(fmterror(fmt, args ...).get()) { }
    };

    /**
     * Resolving output data structure
     * @note uses routing number or/and tag to detect provider
     *       number, and resolver provides both in reply!
     */
    struct SResult_t
    {
        string localRoutingNumber;  // routing number like '+10901234567'
        string localRoutingTag;     // routing tag like 'AT&T mobile'
        string rawData;             // raw routing data
    };

  private:
     ECDriverId mId;      // Driver identifier
     const char * mName;  // Driver string name

  public:
    CDriver(const ECDriverId id, const char * name);
    virtual ~CDriver() = default;

    virtual const CDriverCfg::CfgUniqId_t getUniqueId() const = 0;
    virtual void showInfo() const = 0;
    virtual void resolve(const string & in, SResult_t & out) const = 0;

    const char * getName() const  { return mName; }

    static unique_ptr<CDriver> instantiate(const CDriverCfg::RawConfig_t & data);
};

#endif /* SERVER_SRC_DRIVERS_DRIVER_H_ */
