#pragma once

#include <map>
#include <stdint.h>

using namespace std;

class EventHandler {
public:
    EventHandler();
    virtual ~EventHandler();
    EventHandler(const EventHandler &) = delete;
    EventHandler& operator=(const EventHandler &) = delete;

    virtual void set_epoll(int epoll_fd);
    virtual bool is_can_handle(int fd, uint32_t events);
    virtual int handle_event(int fd, uint32_t events, bool &stop);
    virtual const char* name();

protected:
    int link(int fd, uint32_t events);
    int modify_link(int fd, uint32_t events);
    int unlink(int fd);
    int unlink_all_events();

private:
    void add_events(int fd, uint32_t events);
    uint32_t find_events(int fd);
    void remove_events(int fd);

    int epoll_fd = -1;
    map<int, uint32_t> events_map;
};
