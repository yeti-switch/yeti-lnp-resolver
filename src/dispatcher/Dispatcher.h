#pragma once

#include "singleton.h"
#include "EventHandler.h"

#include <memory>
#include <vector>

using namespace std;

class LoopTerminator;
class Dispatcher;

using dispatcher = singleton<Dispatcher>;

class Dispatcher {
public:
    Dispatcher();
    virtual ~Dispatcher();

    int register_handler(EventHandler *event_handler);
    int unregister_handler(EventHandler *event_handler);

    void loop();
    void stop();

private:
    int epoll_fd = -1;
    vector<EventHandler*> handlers;
    unique_ptr<LoopTerminator> loop_terminator;
};
