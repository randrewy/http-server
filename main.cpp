#include <iostream>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/in.h>
#include <cstring>
#include <errno.h>


using namespace std;


enum {
    PORT = 8083,
    BACKLOG = -1,
    LISTENER_OPTS = LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,
};

void conn_eventcb(struct bufferevent *bev, short events, void*)
{
    cout << "event callback\n";
    if (events & BEV_EVENT_ERROR) {
        cerr << "Error from bufferevent";
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

void conn_readcb(struct bufferevent *bev, void*)
{
    evbuffer *input = bufferevent_get_input(bev);
    evbuffer *output = bufferevent_get_output(bev);

    evbuffer_add_buffer(output, input);
}

void accept_conn_cb(struct evconnlistener*, evutil_socket_t fd, struct sockaddr*, int, void *data)
{
    event_base *base = static_cast<event_base*>(data);
    bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        cerr << "Error initializing bufferevent!\n";
        event_base_loopbreak(base);
        return;
    }
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
    if (bufferevent_enable(bev, EV_READ | EV_WRITE) == -1) {
        cerr << "Error enabling bufferevent!\n";
        event_base_loopbreak(base);
        return;
    }
}

void accept_error_cb(struct evconnlistener *listener, void*)
{
    event_base *base = evconnlistener_get_base(listener);
    int error = EVUTIL_SOCKET_ERROR();
    cerr << "Connection monitor error " << error << " (" << evutil_socket_error_to_string(error) << ")\n";

    event_base_loopexit(base, NULL);
}

int main()
{
    cout << "Hello Server!\n";

    event_base *base;
    evconnlistener *listener;
    sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        cerr << "Could not initialize libevent!\n";
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;

    listener = evconnlistener_new_bind(base, accept_conn_cb, base, LISTENER_OPTS,
                        BACKLOG, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));

    if (!listener) {
        cerr << "Could not create a listener!\n";
        return 1;
    }

    evconnlistener_set_error_cb(listener, accept_error_cb);
    cout << "Listen started on port " << PORT <<".\n";
    event_base_dispatch(base);


    evconnlistener_free(listener);
    event_base_free(base);
    cout << "exiting...\n";
    return 0;
}

