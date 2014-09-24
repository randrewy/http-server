#include "http.h"
using namespace std;


const char* SERVER = "TP-HL-SERVER";
const char* CONNECTION= "close";
RequestInfo RequestInfo::BAD_REQUEST = RequestInfo(UNSUPPORTED, "");

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

RequestMethod stringToRequestMethod(const string& s) {
    if(s == "GET") {
        return GET;
    } else if (s == "HEAD") {
        return HEAD;
    } else {
        return UNSUPPORTED;
    }
}


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


void createResponse(bufferevent *bev)
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

