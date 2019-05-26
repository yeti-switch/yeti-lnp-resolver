#ifndef SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_
#define SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_

#include <cstdint>
using std::uint16_t;
using std::uint32_t;

#include <stdexcept>
using std::runtime_error;

#include <string>
using std::string;

#include "thread.h"
#include "libs/fmterror.h"

/**
 * @brief Forward declarations
 */
class CSipClient;
class CSipClientDestructor;

/**
 * @brief SIP client singletone class
 */
class CSipClient: public thread
{
  public:
    // Client exception class
    class error: public runtime_error
    {
      public:
        template <typename ... Args>
        explicit error(const char * fmt, Args ... args) :
          runtime_error(fmterror(fmt, args ...).get()) { }
    };

    // Execution result code enumeration
    enum class ECCode : int8_t
    {
      FAIL = -1,
      OK   = 0
    };

    // Response data format
    struct SReplyData
    {
        condition<bool> processed;
        uint16_t statusCode;
        string rawContactData;

        SReplyData()
        {
          processed.set(false);
          statusCode     = 0;
          rawContactData = "";
        }
    };

    // Response data handler type definition
    using fnHandler_t = bool (*) (const struct sip_msg *, SReplyData *);

  private:
    friend class CSipClientDestructor;

    fnHandler_t  mHandler;
    const char * mContactField;
    const char * mFromName;
    const char * mFromUri;
    uint32_t     mTimeout;
    const char * mUserAgent;

    static CSipClient *         sInstance;
    static CSipClientDestructor sDestructor;

    CSipClient()                                  = default;
    ~CSipClient() override                        = default;

    CSipClient(const CSipClient & sc)             = delete;
    CSipClient & operator=(const CSipClient & sc) = delete;

    // Overrided thread methods
    void run() override;
    void on_stop() override;

  public:
    static CSipClient & getInstance(const char * userAgent = nullptr);

    static void setContactData(const char * data);
    static void setFromData(const char * name, const char * uri);
    static void setTimeout(const uint32_t timeout);

    static ECCode perform(const char * number, SReplyData & reply);
    static ECCode perform(const string & number, SReplyData & reply);
};

#endif /* SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_ */
