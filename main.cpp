#include <iostream>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/in.h>
#include <cstring>
#include <errno.h>
#include <time.h>
#include <algorithm>
#include <string>

using namespace std;

enum Status {
    OK = 200,
    NOT_FOUND = 404,
    BAD_REQUEST = 405,
};
const char* SERVER = "TP-HL-SERVER";
const char* CONNECTION= "close";

const char* statusMessgae(const Status& s)
{
    switch(s) {
    case OK:
        return "200 OK";
    case NOT_FOUND:
        return "404 Not Found";
    case BAD_REQUEST:
        return "405 Method Not Allowed";
    }
    return "500 internal error";
}


inline void writeHeader(bufferevent *bev, Status s, const char* type, int len)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  headers[]=    "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "Server: %s\r\n"
                        "Connection: %s\r\n"
                        "Date: %s\r\n"
                        "\r\n";
    const time_t timer = time(NULL);
    evbuffer_add_printf(output, headers, statusMessgae(s), type,
                        len, SERVER, CONNECTION, ctime(&timer));
}



void sampleResponse(bufferevent *bev)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  body[]    =   "<HTML><TITLE>Test</TITLE>\r\n"
                        "<BODY><P>Sample answer\r\n"
                        "</BODY></HTML>\r\n";

    writeHeader(bev, OK, "text/html", strlen(body));
    evbuffer_add(output, body, strlen(body));
}

void writeBadRequest(bufferevent *bev)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  response[]=   "HTTP/1.0 405 Method Not Allowed\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n"
                        "%s";
    char  body[]    =   "405 Not Allowed\r\n";

    evbuffer_add_printf(output, response, 17, body);
}




enum RequestMethod{
    GET,
    HEAD,
    UNSUPPORTED,
};

RequestMethod stringToRequestMethod(const string& s) {
    if(s == "GET") {
        return GET;
    } else if (s == "HEAD") {
        return HEAD;
    } else {
        return UNSUPPORTED;
    }
}

struct RequestInfo{
    RequestMethod method;
    string path;

    RequestInfo(const RequestMethod& m, const string& p) : method(m), path(p) {}
    static RequestInfo BAD_REQUEST;
};
RequestInfo RequestInfo::BAD_REQUEST = RequestInfo(UNSUPPORTED, "");


RequestInfo getRequestInfo(const string& line)
{
    size_t mp = line.find(" ");
    string method = string(line, 0, mp);

    size_t pp = line.find(" ", mp+1);
    string path = string(line, mp+1, pp-mp);

    if (line.find(" ", pp+1) != string::npos) { // extra spaces in request
        return RequestInfo::BAD_REQUEST;
    } else {
        return RequestInfo(stringToRequestMethod(method), path);
    }
}


inline void createResponse(bufferevent *bev)
{
    evbuffer *in = bufferevent_get_input(bev);
    size_t sz = 0;
    RequestInfo rinf = getRequestInfo(string(evbuffer_readln(in, &sz, EVBUFFER_EOL_CRLF)));

    if(sz !=0) {
        switch (rinf.method) {
        case HEAD:
            sampleResponse(bev);
            break;

        case GET:
            sampleResponse(bev);
            break;

        case UNSUPPORTED :
            writeBadRequest(bev);
        }
    }
}







enum {
    PORT = 8080,
    BACKLOG = -1,
    LISTENER_OPTS = LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,
};

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

void accept_error_cb(evconnlistener *listener, void*)
{
    event_base *base = evconnlistener_get_base(listener);
    int error = EVUTIL_SOCKET_ERROR();
    cerr << "Connection monitor error " << error << " (" << evutil_socket_error_to_string(error) << ")\n";

    event_base_loopexit(base, NULL);
}

int main()
{
    cout << "Hello from " << SERVER <<"!\n";

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

