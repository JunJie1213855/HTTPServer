#ifndef HTTPREQUEST_H_
#define HTTPREQUEST_H_

#include <chrono>
#include <map>
#include <string>
#include <unordered_map>

namespace http
{

class HttpRequest
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    enum Method
    {
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete,
        kOptions
    };

    HttpRequest()
        : method_(kInvalid), version_("Unknown")
    {
    }

    void setReceiveTime(TimePoint t);
    TimePoint receiveTime() const { return receiveTime_; }

    bool setMethod(const char *start, const char *end);
    Method method() const { return method_; }

    void setPath(const char *start, const char *end);
    std::string path() const { return path_; }

    void setPathParameters(const std::string &key, const std::string &value);
    std::string getPathParameters(const std::string &key) const;

    void setQueryParameters(const char *start, const char *end);
    std::string getQueryParameters(const std::string &key) const;

    void setVersion(std::string v) { version_ = std::move(v); }
    std::string getVersion() const { return version_; }

    void addHeader(const char *start, const char *colon, const char *end);
    std::string getHeader(const std::string &field) const;

    const std::map<std::string, std::string> &headers() const { return headers_; }

    void setBody(const std::string &body) { content_ = body; }
    void setBody(const char *start, const char *end)
    {
        if (end >= start)
        {
            content_.assign(start, end - start);
        }
    }

    std::string getBody() const { return content_; }

    void setContentLength(uint64_t length) { contentLength_ = length; }
    uint64_t contentLength() const { return contentLength_; }

    void setClientIp(const std::string &ip) { clientIp_ = ip; }
    std::string getClientIp() const { return clientIp_; }

    void swap(HttpRequest &that);

private:
    Method method_;
    std::string clientIp_;
    std::string version_;
    std::string path_;
    std::unordered_map<std::string, std::string> pathParameters_;
    std::unordered_map<std::string, std::string> queryParameters_;
    TimePoint receiveTime_;
    std::map<std::string, std::string> headers_;
    std::string content_;
    uint64_t contentLength_{0};
};

} // namespace http
#endif
