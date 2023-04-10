#pragma once

#include "dispatcher/EventHandler.h"

class LoopTerminator: public EventHandler
{
    public:
        LoopTerminator();
        virtual ~LoopTerminator();
        int fire();
        /* EventHandler overrides */
        int handle_event(int fd, uint32_t events, bool &stop) override;

    private:
        int init_event();
        int event_fd = -1;
};
