#include "../../include/http/HttpResponse.h"

#include <cstdio>

namespace http
{

void HttpResponse::appendToBuffer(core::IOBuffer &outputBuf) const
{
    char buf[32];
    snprintf(buf, sizeof buf, "%s %d ", httpVersion_.c_str(), statusCode_);

    outputBuf.append(buf);
    outputBuf.append(statusMessage_);
    outputBuf.append("\r\n");

    if (closeConnection_)
    {
        outputBuf.append("Connection: close\r\n");
    }
    else
    {
        outputBuf.append("Connection: Keep-Alive\r\n");
    }

    const bool hasContentLength = headers_.find("Content-Length") != headers_.end();
    for (const auto &header : headers_)
    {
        outputBuf.append(header.first);
        outputBuf.append(": ");
        outputBuf.append(header.second);
        outputBuf.append("\r\n");
    }
    if (!hasContentLength)
    {
        char clBuf[48];
        snprintf(clBuf, sizeof clBuf, "Content-Length: %zu\r\n", body_.size());
        outputBuf.append(clBuf);
    }
    outputBuf.append("\r\n");

    outputBuf.append(body_);
}

void HttpResponse::setStatusLine(const std::string &version,
                                 HttpStatusCode statusCode,
                                 const std::string &statusMessage)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

} // namespace http
