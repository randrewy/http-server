#include "server.h"
#include <pthread.h>
#include <mutex>
#include <unistd.h>
#include <list>
using namespace std;

static void * thread_func(void *vptr_args);

list<evutil_socket_t>sock_list;
std::mutex sock_list_mutex;

void conn_eventcb(bufferevent *bev, short events, void*)
{
    if (events & BEV_EVENT_ERROR) {
        cerr << "Error from bufferevent";
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

void conn_writecb(bufferevent *bev, void*)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0 && BEV_EVENT_EOF ) {
        bufferevent_free(bev);
    }
}

void conn_readcb(bufferevent *bev, void*)
{
    createResponse(bev);

    bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_WRITE);

}

void accept_conn_cb( evconnlistener*, evutil_socket_t fd, sockaddr*, int, void *data)
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

void accept_conn_cb_main (evconnlistener*, evutil_socket_t fd, sockaddr*, int, void*)
{
    sock_list_mutex.lock();
    sock_list.push_back(fd);
    sock_list_mutex.unlock();
}

void accept_error_cb(evconnlistener *listener, void*)
{
    event_base *base = evconnlistener_get_base(listener);
    int error = EVUTIL_SOCKET_ERROR();
    cerr << "Connection monitor error " << error << " (" << evutil_socket_error_to_string(error) << ")\n";

    event_base_loopexit(base, NULL);
}

int server::start(unsigned int port)
{
    cout << "Hello from " << SERVER <<"! Current time: ";
    const time_t timer = time(NULL);
    std::cout << ctime(&timer);

    std::cout << (0) << '\n';

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

    listener = evconnlistener_new_bind(base, accept_conn_cb_main, base, LISTENER_OPTS,
                        BACKLOG, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));

    if (!listener) {
        cerr << "Could not create a listener!\n";
        return 1;
    }

    evconnlistener_set_error_cb(listener, accept_error_cb);
    cout << "Listen started on port " << port <<".\n";

    if (NUM_THREADS > 0) {
        pthread_t thread[NUM_THREADS];
        for(int i = 0; i < NUM_THREADS; ++i) {
            pthread_create(&thread[i], NULL, thread_func, NULL);
        }
        cout << "Dispatching\n";
        event_base_dispatch(base);
    } else {
        cerr << "No worker threads!\n";
    }

    evconnlistener_free(listener);
    event_base_free(base);
    cout << "exiting...\n";
    return 0;
}


static void *thread_func(void*)
{
    event_config* cfg = event_config_new();
    event_config_set_flag(cfg, EVENT_BASE_FLAG_NOLOCK );
    event_config_set_flag(cfg, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST );
    event_base *base = event_base_new_with_config(cfg);

    timespec nts = {0, 24};
    timespec some = {0, 24};

    if (!base) {
        cerr << "Could not initialize event_base in thread!\n";
        return NULL;
    }
    while(true) {
        event_base_loop(base, EVLOOP_ONCE);

        sock_list_mutex.lock();
        if (!sock_list.empty()) {
            evutil_socket_t fd = sock_list.back();
            sock_list.pop_back();
            accept_conn_cb(NULL, fd, NULL, 0 , (void*)base);
        }
        sock_list_mutex.unlock();
        nanosleep(&nts, &some);
    }
    cerr << "Thread: Unexpected loop exit!\n";
    return NULL;
}

