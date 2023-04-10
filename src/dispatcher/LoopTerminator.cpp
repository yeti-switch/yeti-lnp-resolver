#include "LoopTerminator.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

LoopTerminator::LoopTerminator()
    : EventHandler() {
    init_event();
}

LoopTerminator::~LoopTerminator() {
    if (event_fd >= 0)
        close(event_fd);
}

int LoopTerminator::init_event() {
    event_fd = eventfd(0, EFD_NONBLOCK);

    if (event_fd >= 0)
        link(event_fd, EPOLLIN);

    return event_fd;
}

int LoopTerminator::fire() {
    if (event_fd < 0)
        return -1;

    return eventfd_write(event_fd, 1);
}

/* EventHandler overrides */

int LoopTerminator::handle_event(int fd, uint32_t events, bool &stop) {
    eventfd_t value;
    int res = eventfd_read(event_fd, &value);
    stop = true;
    return res;
}
