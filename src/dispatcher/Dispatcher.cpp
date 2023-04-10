#include "Dispatcher.h"
#include "LoopTerminator.h"

#include <sys/epoll.h>
#include <string>
#include <cstring>

#define EPOLL_MAX_EVENTS 2048
#define MSG_SZ 1024 * 2

Dispatcher::Dispatcher() {
    epoll_fd = epoll_create(EPOLL_MAX_EVENTS);

    if(epoll_fd == -1)
        throw string("epoll_create call failed");
}

Dispatcher::~Dispatcher() {
    if (epoll_fd >= 0)
        close(epoll_fd);
}

int Dispatcher::register_handler(EventHandler *handler) {
    if (epoll_fd < 0 || handler == nullptr)
        return -1;

    handler->set_epoll(epoll_fd);
    handlers.push_back(handler);
    return 0;
}

int Dispatcher::unregister_handler(EventHandler *handler) {
    if (handler == nullptr)
        return -1;

    handler->set_epoll(-1);
    for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        if (*it == handler) {
            handlers.erase(it);
            break;
        }
    }

    return 0;
}

void Dispatcher::loop() {
    loop_terminator = make_unique<LoopTerminator>();
    pthread_setname_np(pthread_self(), "dispatcher");

    struct epoll_event events[EPOLL_MAX_EVENTS];
    bool stop = false;
    int ret;
    while (!stop) {
        ret = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS,-1);
        if(ret == -1 && errno != EINTR)
            err("dispatcher: epoll_wait: %s", strerror(errno));

        if(ret < 1)
            continue;

        for (int n = 0; n < ret; ++n) {
            // quit if needed
            if (stop) { break; }

            // handle event
            struct epoll_event &e = events[n];
            for (auto & h : handlers) {
                if (h->is_can_handle(e.data.fd, e.events)) {
                    h->handle_event(e.data.fd, e.events, stop);
                    break;
                }
            }
        }
    }
}

void Dispatcher::stop() {
    loop_terminator->fire();
}
