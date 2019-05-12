#ifndef SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_
#define SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_

#include <cstdint>
using std::uint16_t;
using std::uint32_t;

#include <stdexcept>
using std::logic_error;

#include <string>
using std::string;

#include "thread.h"

/*
 * Forward declarations
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
    class error: public logic_error
    {
      public:
        explicit error(const string & what) :
            logic_error("{SipClient} " + what) { }
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

    void setContactData(const char * data);
    void setFromData(const char * name, const char * uri);
    void setTimeout(const uint32_t timeout);

    ECCode perform(const char * number, SReplyData & reply);
    ECCode perform(const string & number, SReplyData & reply);
};

#endif /* SERVER_SRC_DRIVERS_MODULES_SIPCLIENT_H_ */
