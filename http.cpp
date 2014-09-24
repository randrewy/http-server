#include "http.h"
#include <iostream>
using namespace std;

const char* FORBIDDEN_MSG = "405 Forbidden";
const int   FORBIDDEN_LEN = 13;
const char* NOT_FOUND_MSG = "404 Not found";
const int   NOT_FOUND_LEN = 13;
const char* CRLF = "\r\n";

const RequestInfo RequestInfo::BAD_REQUEST = RequestInfo(UNSUPPORTED, "");

std::map<std::string, const char*> CONTENTS =   {{"html","text/html"},   {"css", "text/css"},   {"js", "application/javascript"},
                                                 {"jpeg", "image/jpeg"}, {"jpg", "image/jpeg"}, {"png", "image/png"},
                                                 {"gif", "image/gif"},   {"swf",  "application/x-shockwave-flash"}
                                                };

constexpr const char* ContentString[] = {"text/html", "text/css",  "application/javascript", "image/jpeg",
                                         "image/jpeg","image/png", "image/gif",              "application/x-shockwave-flash"
                                         "unknown"};

bool check_path_security(const string& path);

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
    string path = string(line, mp+1, pp-mp-1);

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
    case FORBIDDEN:
        return "403 Forbidden";
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

ContentType getContentType(const string& path)
{
    int point_pos = path.find_last_of('.');
    string ext = path.substr(point_pos+1);
    return ContentType(0);
}

const char* getMappedContentType(const string& path)
{
    int point_pos = path.find_last_of('.');
    string ext = path.substr(point_pos+1);
    return CONTENTS[ext]; //to lower case!
}

inline void writeHeader(bufferevent *bev, Status s, const ContentType& t, int len)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  headers[]=    "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "Server: %s\r\n"
                        "Connection: %s\r\n"
                        "Date: %s\r\n";
    const time_t timer = time(NULL);
    evbuffer_add_printf(output, headers, statusMessgae(s), contentTypeString(t),
                        len, SERVER, CONNECTION, ctime(&timer));
}

inline void writeHeader(bufferevent *bev, Status s, const char* t, int len)
{
    evbuffer *output = bufferevent_get_output(bev);

    char  headers[]=    "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "Server: %s\r\n"
                        "Connection: %s\r\n"
                        "Date: %s\r\n";
    const time_t timer = time(NULL);
    evbuffer_add_printf(output, headers, statusMessgae(s), t,
                        len, SERVER, CONNECTION, ctime(&timer));
}



void writeResponse(bufferevent *bev, const char* path, RequestMethod method)
{
    evbuffer *output = bufferevent_get_output(bev);

    string full_path = string(DOCUMENT_ROOT);
    full_path.append(string(path));

    if(!check_path_security(path)) {
        writeHeader(bev, FORBIDDEN, HTML, FORBIDDEN_LEN);
        evbuffer_add(output, FORBIDDEN_MSG, FORBIDDEN_LEN);
        return;
    }

    bool index = false;
    if (full_path[full_path.length()-1] == '/') { // directory
        full_path.append("index.html");
        index = true;
    }

    int fd = open(full_path.c_str(), O_NONBLOCK|O_RDONLY);
    if (fd == -1) {
        if (index) {
            writeHeader(bev, FORBIDDEN, HTML, FORBIDDEN_LEN);
            evbuffer_add(output, FORBIDDEN_MSG, FORBIDDEN_LEN);
        } else {
            writeHeader(bev, NOT_FOUND, HTML, NOT_FOUND_LEN);
            evbuffer_add(output, NOT_FOUND_MSG, NOT_FOUND_LEN);
        }
        return;
    }

    struct stat st;
    fstat(fd, &st);
    writeHeader(bev, OK, getMappedContentType(full_path), st.st_size);
    if(method == GET) {
        evbuffer_add_file(output, fd, 0, st.st_size);
    } else {
        // ctime gives only /n so we'll add proper CRLF
        evbuffer_add(output, CRLF, 2);
    }
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
    char* path = new char[raw_path.length()+1]; // at most the same size + term\0
    urlDecode(path, raw_path.c_str());

    if(sz !=0) {
        switch (rinf.method) {
        case HEAD:  case GET:
            writeResponse(bev, path, rinf.method);
            break;

        case UNSUPPORTED :
            writeBadRequest(bev);
        }
    }
    delete[] path;
}



bool check_path_security(const string& path)
{
    int len = path.length();
    int i = 0;
    int level = 0;
    while(i < len) {
        if(path[i] == '/'){
            if(i < len - 2 && path[i+1] == '.' && path[i+2] == '.') {
                --level;
                i += 3;
            } else {
                ++level;
                ++i;
            }
        } else {
            ++i;
        }
        if (level < 0) return false;
    }
    return true;
}
