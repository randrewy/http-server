#ifndef HTTP_H
#define HTTP_H
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <cstring>
#include <string>

enum Status {
    OK = 200,
    NOT_FOUND = 404,
    BAD_REQUEST = 405,
};

enum RequestMethod{
    GET,
    HEAD,
    UNSUPPORTED,
};

extern const char* SERVER;
extern const char* CONNECTION;

struct RequestInfo{
    RequestMethod method;
    std::string path;

    RequestInfo(const RequestMethod& m, const std::string& p) : method(m), path(p) {}
    const static RequestInfo BAD_REQUEST;
};

void createResponse(bufferevent*);

#endif // HTTP_H
