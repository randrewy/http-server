#ifndef SERVER_H
#define SERVER_H
#include <iostream>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/in.h>
#include <errno.h>
#include <cstring>

#include "http.h"


namespace server{
enum {
    PORT = 8080,
    BACKLOG = -1,
    LISTENER_OPTS = LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,
};

    int start();

}

#endif // SERVER_H
