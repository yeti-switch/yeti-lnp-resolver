#pragma once

#include <curl/curl.h>
#include <stdexcept>
using std::logic_error;
#include <string>
using std::string;

class http_request
{
    public:
        class error : public logic_error
        {
            public:
                explicit error(const std::string & what)
                    : logic_error("http_request: " + what) {};
        };

        typedef enum struct AUTH_TYPE
        {
             NONE  = CURLAUTH_NONE
            ,BASIC = CURLAUTH_BASIC
        } auth_t;

        typedef enum struct REQ_CODE
        {
             FAIL   = -1
            ,OK     = CURLE_OK
        } req_code_t;

    private:
        typedef CURL ctx_t;
        typedef struct curl_slist hdr_t;
        typedef size_t (* wcb_fn) (void * ptr, size_t size, size_t nmemb, void * userdata);

        char m_errBuffer[CURL_ERROR_SIZE];
        static bool s_initialize;
        ctx_t * m_ctx;
        hdr_t * m_hdrs_list;

        http_request(const http_request & hc) = delete;
        http_request & operator=(const http_request & hc) = delete;
    public:
        http_request();
        ~http_request();
        void setSSLVerification(bool onoff);                            // enabled by default
        void setAuthType(auth_t type);                                  // default AUTH_TYPE::BASIC
        void setAuthData(const char * login, const char * pass);
        void setAuthData(const string & login, const string & pass);
        void setTimeout(const long timeout);
        void setHeader(const char * header);
        void setHeader(const string & header);
        void setWriteCallback(wcb_fn fn, void * userdata);
        const char * getErrorStr() const;
        req_code_t perform(const char * url);
        req_code_t perform(const string & url);
};