#pragma once

#include "dispatcher/EventHandler.h"
#include "libs/fmterror.h"

#include <string>
#include <memory>
#include <curl/curl.h>
#include <stdexcept>
#include <vector>
#include <queue>

using namespace std;

class AsyncHttpClient;
struct SockInfo;

enum HttpMethod {
    GET
};

// Authorization enumerations
enum class ECAuth : uint8_t {
    NONE  = CURLAUTH_NONE,
    BASIC = CURLAUTH_BASIC
};

struct HttpRequest {
    uint32_t id = -1;
    HttpMethod method = GET;
    const char *url = nullptr;
    ECAuth auth_type = ECAuth::NONE;
    const char *login = nullptr;
    const char *pass = nullptr;
    bool verify_ssl = false;
    long timeout_ms = 0;
    vector<const char *> headers;
};

struct HttpResponse {
    uint32_t id = -1;
    bool is_success = false;
    string data;
};

/**
 * @brief AsyncHttpClientHandler
 */

class AsyncHttpClientHandler {
public:
    virtual void on_http_response_received(
        AsyncHttpClient *http_client,
        const HttpResponse &response) = 0;
};

/**
 * @brief HTTP client class
 */
class AsyncHttpClient: public EventHandler
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

    AsyncHttpClient(AsyncHttpClientHandler *http_handler);
    virtual ~AsyncHttpClient();

    int make_request(const HttpRequest &request);

    int socket_cb(CURL *easy, curl_socket_t sock_fd, int what, SockInfo *sock_info);
    int timer_cb(CURLM *multi, long timeout_ms);

    /* EventHandler overrides */
    int handle_event(int fd, uint32_t events, bool &stop) override;

protected:
    void check_multi_info();

    /* sockets */
    void add_sock(curl_socket_t sock_fd, CURL *easy, int action);
    void set_sock(SockInfo *sock_info, curl_socket_t sock_fd, CURL *easy, int action);
    void rem_sock(SockInfo *sock_info);
    void socket_event_handler(curl_socket_t sock_fd, int events);

    /* timer functions */
    int init_timer();
    int set_timer_value(long timeout_ms);
    int read_timer();
    void timer_event_handler();

    /* respose */
    int init_respose_notifier();
    void notify_respose_available();
    void respose_notifier_event_handler();

private:
    queue<unique_ptr<HttpResponse>> response_queue;
    int respose_notifier_fd = -1;
    int timer_fd = -1;
    int still_running;
    CURLM *multi;
    AsyncHttpClientHandler *handler;
};
