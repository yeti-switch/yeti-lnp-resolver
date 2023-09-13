#include "AsyncHttpClient.h"

#include <stdio.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <memory>

#include "log.h"

/* Information associated with a specific easy handle */

typedef struct ConnInfo {
    uint32_t id;
    AsyncHttpClient *http_client;
    CURL *easy;
    struct curl_slist *header_list;
    char error[CURL_ERROR_SIZE];
    string response;
} ConnInfo;

/* Information associated with a specific socket */

typedef struct SockInfo {
    curl_socket_t sock_fd;
    CURL *easy;
    int action;
    long timeout;
} SockInfo;

/* static callbacks */

/* - CURLMOPT_SOCKETFUNCTION */

static int socket_cb_static(CURL *e, curl_socket_t s, int what, AsyncHttpClient *http_client, SockInfo *sock_info) {
    return http_client->socket_cb(e, s, what, sock_info);
}

/* - CURLMOPT_TIMERFUNCTION */

static int timer_cb_static(CURLM *multi, long timeout_ms, AsyncHttpClient *http_client) {
    return http_client->timer_cb(multi, timeout_ms);
}

/* - CURLOPT_WRITEFUNCTION */

static size_t write_cb_static(void *ptr, size_t size, size_t nmemb, ConnInfo *conn_info) {
    int curr_size = conn_info->response.size();
    int append_size = size * nmemb;

    conn_info->response.resize(curr_size + append_size);
    memcpy((char*)conn_info->response.data() + curr_size, (char*)ptr, append_size);

    return append_size;
}

/* HttpClient */

AsyncHttpClient::AsyncHttpClient(AsyncHttpClientHandler *http_handler)
  : handler(http_handler)
{
    init_timer();
    init_respose_notifier();
    multi = curl_multi_init();
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, socket_cb_static);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, timer_cb_static);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, this);
}

AsyncHttpClient::~AsyncHttpClient() {
    curl_multi_cleanup(multi);

    if (respose_notifier_fd >= 0) {
        unlink(respose_notifier_fd);
        close(respose_notifier_fd);
    }

    if (timer_fd >= 0) {
        unlink(timer_fd);
        close(timer_fd);
    }
}

/* request */

int AsyncHttpClient::make_request(const HttpRequest &request) {
    //dbg("AsyncHttpClient make request");
    CURL *easy = curl_easy_init();

    if (!easy) {
        err("easy creation error");
        return -1;
    }

    // connection info
    ConnInfo *conn = (ConnInfo *)calloc(1, sizeof(ConnInfo));

    auto free_conn_info = [conn](){
        if (conn->header_list) {
            curl_slist_free_all(conn->header_list);
            conn->header_list = nullptr;
        }

        free(conn);
    };

    conn->http_client = this;
    conn->id = request.id;
    conn->easy = easy;
    conn->error[0]='\0';

    // url
    const char * url = request.url;
    if (url) {
        if (CURLE_OK != curl_easy_setopt(easy, CURLOPT_URL, url)) {
            free_conn_info();
            throw error("URL processing error");
        }
    }

    // ssl
    const bool verify_ssl = request.verify_ssl;
    const long verify = verify_ssl ? 2L : 0L;
    if ((CURLE_OK != curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, verify)) ||
        (CURLE_OK != curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, verify))) {
        free_conn_info();
        throw error("SSL verification option error");
    }

    // auth type
    if (CURLE_OK != curl_easy_setopt(easy, CURLOPT_HTTPAUTH, request.auth_type)) {
        free_conn_info();
        throw error("authorization type processing error");
    }

    // auth credentials
    const char *login = request.login;
    const char *pass = request.pass;
    if (login != nullptr && pass != nullptr) {
        if ((CURLE_OK != curl_easy_setopt(easy,  CURLOPT_USERNAME, login)) ||
            (CURLE_OK != curl_easy_setopt(easy,  CURLOPT_PASSWORD, pass))) {
            free_conn_info();
            throw error("authorization credentials processing error");
        }
    }

    // timeout
    const long timeout_ms = request.timeout_ms;

    if ((CURLE_OK != curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, timeout_ms)) ||
        (CURLE_OK != curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L))) {
        free_conn_info();
        throw error("set up timeout value error");
    }

    // headers
    struct curl_slist *header_list = nullptr;
    for (const auto & header : request.headers) {
        header_list = curl_slist_append(header_list, header);

        if (!header_list) {
            free_conn_info();
            throw error("appending header to request error");
        }
    }

    if (header_list) {
        if (CURLE_OK != curl_easy_setopt(easy, CURLOPT_HTTPHEADER, header_list)) {
            free_conn_info();
            throw error("headers processing error");
        }

        // store header_list, it will be free() after request has been done
        conn->header_list = header_list;
    }

    // write func callback
    if ((CURLE_OK != curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb_static)) ||
        (CURLE_OK != curl_easy_setopt(easy, CURLOPT_WRITEDATA, conn))) {
        free_conn_info();
        throw error("initializing write callback data error");
    }

    // private pointer
    if(CURLE_OK != curl_easy_setopt(easy, CURLOPT_PRIVATE, conn)) {
        free_conn_info();
        throw error("private pointer initialization error");
    }

    // error buffer
    if(CURLE_OK != curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, conn->error)) {
        free_conn_info();
        throw error("error buffer initializing error");
    }

    //dbg("adding easy %p to multi %p (%s)", easy, multi, url);

    // add handle
    CURLMcode rc = curl_multi_add_handle(multi, easy);

    if (rc != CURLM_OK) {
        err("adding easy failed");
        free_conn_info();
        return -1;
    }

    /* note that the add_handle() will set a time-out to trigger very soon so
       that the necessary socket_action() call will be called by this app */

    return 0;
}

/* callbacks */

int AsyncHttpClient::socket_cb(CURL *easy, curl_socket_t sock_fd, int what, SockInfo *sock_info) {
    //const char *whatstr[] = { "none", "IN", "OUT", "INOUT", "REMOVE" };

    if(what == CURL_POLL_NONE) return 0;

    if (what == CURL_POLL_REMOVE) {
        //dbg("remove socket");
        rem_sock(sock_info);
    }
    else {
        if(!sock_info) {
            //dbg("adding data: %s", whatstr[what]);
            add_sock(sock_fd, easy, what);
        }
        else {
            //dbg("changing action from %s to %s", whatstr[sock_info->action], whatstr[what]);
            set_sock(sock_info, sock_fd, easy, what);
        }
    }

    return 0;
}

int AsyncHttpClient::timer_cb(CURLM *, long timeout_ms) {
    set_timer_value(timeout_ms);
    return 0;
}

/* Check for completed transfers, and remove their easy handles */

void AsyncHttpClient::check_multi_info() {
    char *eff_url;
    CURLMsg *msg;
    int msgs_left;
    ConnInfo *conn;
    CURL *easy;
    CURLcode res;

    while ((msg = curl_multi_info_read(multi, &msgs_left))) {
        if (msg->msg != CURLMSG_DONE)
            continue;

        easy = msg->easy_handle;
        res = msg->data.result;
        curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
        curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);

        auto response = make_unique<HttpResponse>();
        response->id = conn->id;

        if (res == CURLE_OK) {
            response->is_success = true;
            response->data = conn->response;
        } else {
            response->is_success = false;
            response->data = conn->error;
        }

        response_queue.push(move(response));
        //dbg("DONE: %s => (%d) %s", eff_url, res, conn->error);

        curl_multi_remove_handle(multi, easy);
        curl_easy_cleanup(easy);

        // free header_list
        if (conn->header_list) {
            curl_slist_free_all(conn->header_list);
            conn->header_list = nullptr;
        }

        free(conn); conn = nullptr;

        // notify
        notify_respose_available();
    }
}

/* sockets */

void AsyncHttpClient::add_sock(curl_socket_t sock_fd, CURL *easy, int action) {
    SockInfo *sock_info = (SockInfo*)calloc(1, sizeof(SockInfo));
    set_sock(sock_info, sock_fd, easy, action);
    curl_multi_assign(multi, sock_fd, sock_info);
}

void AsyncHttpClient::set_sock(SockInfo *sock_info, curl_socket_t sock_fd, CURL *easy, int action) {
    int events = EPOLLERR;
    switch(action) {
    case CURL_POLL_IN:
        events |= EPOLLIN;
        break;
    case CURL_POLL_OUT:
        events |= EPOLLOUT;
        break;
    case CURL_POLL_INOUT:
        events |= EPOLLOUT | EPOLLIN;
        break;
    }

    if (sock_info->sock_fd) {
        modify_link(sock_info->sock_fd, events);
    } else {
        link(sock_fd, events);
    }

    sock_info->sock_fd = sock_fd;
    sock_info->action = action;
    sock_info->easy = easy;
}

void AsyncHttpClient::rem_sock(SockInfo *sock_info) {
    if (!sock_info)
        return;

    if (sock_info->sock_fd)
        unlink(sock_info->sock_fd);

    free(sock_info);
    sock_info = nullptr;
}

void AsyncHttpClient::socket_event_handler(curl_socket_t sock_fd, int events) {
    int action = ((events & EPOLLIN) ? CURL_CSELECT_IN : 0) |
                 ((events & EPOLLOUT) ? CURL_CSELECT_OUT : 0) |
                 ((events & EPOLLERR) ? CURL_CSELECT_ERR : 0);

    CURLMcode rc = curl_multi_socket_action(multi, sock_fd, action, &still_running);
    if (rc != CURLM_OK) {
        err("curl_multi_socket_action error");
        return;
    }

    check_multi_info();

    if (still_running <= 0) {
        dbg("last transfer done. clear timeout");
        set_timer_value(-1);
    }
}

/* timer functions */

int AsyncHttpClient::init_timer() {
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if (timer_fd >= 0)
        link(timer_fd, EPOLLIN);

    return timer_fd;
}

int AsyncHttpClient::set_timer_value(long timeout_ms) {
    if (timer_fd < 0)
        return -1;

    struct itimerspec its;
    memset(&its, 0, sizeof(struct itimerspec));

    if (timeout_ms > 0) {
        its.it_value.tv_sec = timeout_ms / 1000;
        its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    } else if (timeout_ms == 0) {
        /* libcurl wants us to timeout now, however setting both fields of
         * new_value.it_value to zero disarms the timer. The closest we can
         * do is to schedule the timer to fire in 1 ns. */
        its.it_value.tv_nsec = 1;
    }

    timerfd_settime(timer_fd, 0, &its, NULL);
    return 0;
}

int AsyncHttpClient::read_timer() {
    if (timer_fd < 0)
        return -1;

    uint64_t count = 0;
    ssize_t err = 0;

    err = read(timer_fd, &count, sizeof(uint64_t));

    if (err == -1) {
        /* Note that we may call the timer callback even if the timerfd is not
         * readable. It's possible that there are multiple events stored in the
         * epoll buffer (i.e. the timer may have fired multiple times). The
         * event count is cleared after the first call so future events in the
         * epoll buffer will fail to read from the timer. */
        if (errno == EAGAIN) {
            err("read_timer error %s", strerror(errno));
            return -1;
        }
    }

    if(err != sizeof(uint64_t)) {
        err("error read_timer");
        return 0;
    }

    return 0;
}

void AsyncHttpClient::timer_event_handler() {
    if (read_timer() < 0)
        return;

    CURLMcode rc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);

    if (rc != CURLM_OK) {
        err("set CURL_SOCKET_TIMEOUT action failed");
        return;
    }

    check_multi_info();
}

/* respose */

int AsyncHttpClient::init_respose_notifier() {
    respose_notifier_fd = eventfd(0, EFD_NONBLOCK);

    if (respose_notifier_fd >= 0)
        link(respose_notifier_fd, EPOLLIN);

    return respose_notifier_fd;
}

void AsyncHttpClient::notify_respose_available() {
    if (respose_notifier_fd < 0)
        return;

    eventfd_write(respose_notifier_fd, 1);
}

void AsyncHttpClient::respose_notifier_event_handler() {
    if (respose_notifier_fd < 0)
        return;

    eventfd_t value;
    eventfd_read(respose_notifier_fd, &value);

    while (response_queue.size()) {
        auto response = move(response_queue.front());
        response_queue.pop();

        if (handler != nullptr)
            handler->on_http_response_received(this, *(move(response)));
    }
}

/* EventHandler overrides */

int AsyncHttpClient::handle_event(int fd, uint32_t events, bool &) {
    if (fd == timer_fd) {
        timer_event_handler();
    } else if (fd == respose_notifier_fd) {
        respose_notifier_event_handler();
    } else {
        socket_event_handler(fd, events);
    }

    return 0;
}
