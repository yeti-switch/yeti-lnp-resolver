#ifndef SERVER_SRC_DRIVERS_MODULES_HTTPCLIENT_H_
#define SERVER_SRC_DRIVERS_MODULES_HTTPCLIENT_H_

#include <cstdint>
using std::int8_t;
using std::uint8_t;
using std::uint32_t;

#include <stdexcept>
using std::runtime_error;

#include <string>
using std::string;

#include <curl/curl.h>

#include "libs/fmterror.h"

/**
 * @brief HTTP client class
 */
class CHttpClient
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

    // Authorization enumerations
    enum class ECAuth : uint8_t
    {
      NONE  = CURLAUTH_NONE,
      BASIC = CURLAUTH_BASIC
    };

    // Request code enumeration
    enum class ECReqCode : int8_t
    {
      FAIL = -1,
      OK   = CURLE_OK
    };

  private:
    // Client data types
    using Ctx_t = CURL;
    using Hdr_t = struct curl_slist;
    using WriteFunc_t = size_t (* )
                      (void * ptr, size_t size, size_t nmemb, void * userdata);

    char        mErrBuffer[CURL_ERROR_SIZE];
    Ctx_t *     mCtx;
    Hdr_t *     mHdrList;

    static bool sInitialize;

    // Copying procedure denied
    CHttpClient(const CHttpClient & hc) = delete;
    CHttpClient & operator=(const CHttpClient & hc) = delete;

  public:
    CHttpClient();
    ~CHttpClient();

    // Configuration setters
    void setSSLVerification(bool onoff);

    void setAuthType(ECAuth type);

    void setAuthData(const char * login, const char * pass);
    void setAuthData(const string & login, const string & pass);

    void setTimeout(const uint32_t timeout);

    void setHeader(const char * header);
    void setHeader(const string & header);

    void setWriteCallback(WriteFunc_t fn, void * ptr);

    // Other methods
    const char * getErrorStr() const;
    ECReqCode    perform(const char * url);
    ECReqCode    perform(const string & url);
};

#endif /* SERVER_SRC_DRIVERS_MODULES_HTTPCLIENT_H_ */
