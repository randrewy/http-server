#include "http.h"
using namespace std;


const char* SERVER = "TP-HL-SERVER";
const char* CONNECTION= "close";
const RequestInfo RequestInfo::BAD_REQUEST = RequestInfo(UNSUPPORTED, "");

bool check_path(const char* path);

void urlDecode(char *dst, const char *src)
{
        char a, b;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
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

inline const char* contentTypeString(const ContentType& t)
{
    return ContentString[t];
}


inline void writeHeader(bufferevent *bev, Status s, const ContentType& t, int len)
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
    evbuffer_add_printf(output, headers, statusMessgae(s), contentTypeString(t),
                        len, SERVER, CONNECTION, ctime(&timer));
}

void sampleResponse(bufferevent *bev)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  body[]    =   "<HTML><TITLE>Test</TITLE>\r\n"
                        "<BODY><P>Sample answer\r\n"
                        "</BODY></HTML>\r\n";

    writeHeader(bev, OK, HTML, strlen(body));
    evbuffer_add(output, body, strlen(body));
}

void wrieResponse(bufferevent *bev, const char* path)
{

    evbuffer *output = bufferevent_get_output(bev);

    char  body[]    =   "<HTML><TITLE>Test</TITLE>\r\n"
                        "<BODY><P>Sample answer\r\n"
                        "</BODY></HTML>\r\n";

    writeHeader(bev, OK, HTML, strlen(body));
    evbuffer_add(output, body, strlen(body));
}

void writeBadRequest(bufferevent *bev)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  body[]    =   "405 Not Allowed\r\n";
    writeHeader(bev, BAD_REQUEST, HTML, 17);
    evbuffer_add(output, body, strlen(body));
}

void createResponse(bufferevent *bev)
{
    evbuffer *in = bufferevent_get_input(bev);
    size_t sz = 0;
    RequestInfo rinf = getRequestInfo(string(evbuffer_readln(in, &sz, EVBUFFER_EOL_CRLF)));

    size_t param_delim = rinf.path.find('?');
    string raw_path = rinf.path.substr(0, param_delim);
    char* path = new char[raw_path.length()]; // at most the same size
    urlDecode(path, raw_path.c_str());

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
    delete[] path;
}



bool check_path_security(string path)
{

}
