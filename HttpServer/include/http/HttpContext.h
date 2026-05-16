#ifndef HTTPCONTEXT_H_
#define HTTPCONTEXT_H_

#include "../core/IOBuffer.h"
#include "HttpRequest.h"

namespace http
{

class HttpContext
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext()
        : state_(kExpectRequestLine)
    {
    }

    bool parseRequest(core::IOBuffer *buf, HttpRequest::TimePoint receiveTime);

    bool gotAll() const { return state_ == kGotAll; }

    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        request_.swap(dummyData);
    }

    const HttpRequest &request() const { return request_; }
    HttpRequest &request() { return request_; }

private:
    bool processRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_;
    HttpRequest request_;
};

} // namespace http
#endif
