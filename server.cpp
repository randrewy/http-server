#include "server.h"
#include <pthread.h>
#include <unistd.h>
#include <list>
using namespace std;

static void conn_eventcb(bufferevent *bev, short events, void*)
{
    if (events & BEV_EVENT_ERROR) {
        cerr << "Error from bufferevent";
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

static void conn_writecb(bufferevent *bev, void*)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0 && BEV_EVENT_EOF ) {
        bufferevent_free(bev);
    }
}

static void conn_readcb(bufferevent *bev, void*)
{
    createResponse(bev);

    bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_WRITE);

}

static void accept_conn_cb( evconnlistener*, evutil_socket_t fd, sockaddr*, int, void *data)
{
    event_base *base = static_cast<event_base*>(data);
    bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        cerr << "Error initializing bufferevent!\n";
        event_base_loopbreak(base);
        return;
    }
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
    if (bufferevent_enable(bev, EV_READ) == -1) {
        cerr << "Error enabling bufferevent!\n";
        event_base_loopbreak(base);
        return;
    }
}

static void accept_error_cb(evconnlistener *listener, void*)
{
    event_base *base = evconnlistener_get_base(listener);
    int error = EVUTIL_SOCKET_ERROR();
    cerr << "Connection monitor error " << error << " (" << evutil_socket_error_to_string(error) << ")\n";

    event_base_loopexit(base, NULL);
}

int server::start(unsigned int port, int workers)
{
    cout << "Hello from " << SERVER <<"! Current time: ";
    const time_t timer = time(NULL);
    std::cout << ctime(&timer);

    event_base *base;
    evconnlistener *listener;
    sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        cerr << "Could not initialize libevent!\n";
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;

    listener = evconnlistener_new_bind(base, accept_conn_cb, base, LISTENER_OPTS,
                        BACKLOG, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));

    if (!listener) {
        cerr << "Could not create a listener!\n";
        return 1;
    }

    evconnlistener_set_error_cb(listener, accept_error_cb);
    cout << "Listen started on port " << port <<".\n";

    if (workers > 0) {
        for(int i = 0; i < workers; ++i) {
            pid_t pid;
            switch((pid = fork())) {
            case -1:
                cerr << "error on fork()!\n";
                abort();
            case 0:
                cout << "Subprocess " << i << " (" << pid << ") created.\n";
                event_reinit(base);
                cout << "Dispatching from worker " << i << "\n";
                event_base_dispatch(base);

                break;
            default:
                if (i == workers - 1) {
                    event_reinit(base);
                    cout << "Dispatching from master\n";
                    event_base_dispatch(base);
                }
                break;
           }
        }
    } else {
        cerr << "No worker subproceses!\n";
    }


    evconnlistener_free(listener);
    event_base_free(base);
    cout << "exiting...\n";
    return 0;
}
