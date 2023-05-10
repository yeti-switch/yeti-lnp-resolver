#include "EventHandler.h"
#include "log.h"
#include "dispatcher/Dispatcher.h"

#include <errno.h>
#include <cstring>
#include <sys/epoll.h>
#include <typeinfo>

EventHandler::EventHandler() {
    dispatcher::instance()->register_handler(this);
}

EventHandler::~EventHandler() {
    unlink_all_events();
    dispatcher::instance()->unregister_handler(this);
}

void EventHandler::set_epoll(int epoll_fd) {
    this->epoll_fd = epoll_fd;
}

bool EventHandler::is_can_handle(int fd, uint32_t events) {
    return find_events(fd) & events;
}

int EventHandler::handle_event(int fd, uint32_t events, bool &stop) {
    return 0;
}

const char* EventHandler::name() {
    return typeid(*this).name();
}

int EventHandler::unlink_all_events() {
    if (epoll_fd < 0)
        return -1;

    int res;
    int fd;
    auto it = events_map.begin();

    while (it != events_map.end()) {
        fd = (*it).first;
        res = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

        if (res < 0) {
            err("EPOLL_CTL_DEL failed for fd: %d : %s", fd, strerror(errno));
        }

        events_map.erase(it);
        it = events_map.begin();
    }

    return 0;
}

void EventHandler::iterate_events_map(
    std::function<void (int fd, uint32_t events)> callback)
{
    for(const auto &ev: events_map)
        callback(ev.first, ev.second);
}

int EventHandler::link(int fd, uint32_t events) {
    if (epoll_fd < 0)
        return -1;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));
    ev.events = events;
    ev.data.fd = fd;
    int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    if (res == 0) {
        add_events(fd, events);
    } else {
        err("EPOLL_CTL_ADD failed for fd: %d : %s", fd, strerror(errno));
    }

    return res;
}

int EventHandler::modify_link(int fd, uint32_t events) {
    auto ev_it = events_map.find(fd);
    if(ev_it == events_map.end()) {
        err("modify_link for NX fd: %d, failover to link()", fd);
        return link(fd, events);
    }

    if (epoll_fd < 0)
        return -1;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));
    ev.events = events;
    ev.data.fd = fd;

    int res = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    if (res == 0) {
        ev_it->second = events;
    } else {
        err("EPOLL_CTL_MOD failed for fd: %d : %s", fd, strerror(errno));
    }

    return res;
}
int EventHandler::unlink(int fd) {
    if (epoll_fd < 0)
        return -1;

    int res = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (res < 0) {
        err("EPOLL_CTL_DEL failed for fd: %d : %s", fd, strerror(errno));
    }

    remove_events(fd);
    return res;
}

uint32_t EventHandler::find_events(int fd) {
    auto it = events_map.find(fd);

    if (it != events_map.end()) {
        return it->second;
    }

    return 0;
}

void EventHandler::add_events(int fd, uint32_t events) {
    events_map[fd] = events;
}

void EventHandler::remove_events(int fd) {
    auto it = events_map.find(fd);

    if (it != events_map.end()) {
        events_map.erase(it);
    }
}
