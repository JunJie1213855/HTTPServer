#include "../../../include/middleware/ratelimit/RateLimitMiddleware.h"
#include <muduo/base/Logging.h>
#include <algorithm>

namespace http
{
    namespace middleware
    {

        RateLimitMiddleware::RateLimitMiddleware(const RateLimitConfig &config)
            : config_(config), lastCleanup_(muduo::Timestamp::now())
        {
        }

        void RateLimitMiddleware::before(HttpRequest &request)
        {
            std::string ip = request.getClientIp();
            if (ip.empty())
            {
                ip = "unknown";
            }

            int count = getRequestCount(ip);
            if (count >= config_.maxRequests)
            {
                LOG_WARN << "Rate limit exceeded for IP: " << ip
                         << ", count: " << count
                         << ", limit: " << config_.maxRequests;

                HttpResponse response;
                response.setStatusCode(HttpResponse::k429TooManyRequests);
                response.setStatusMessage("Too Many Requests");
                response.setContentType("application/json");
                response.setBody("{\"error\": \"Too Many Requests\", \"retry_after\": " +
                                 std::to_string(config_.windowSeconds) + "}");
                response.addHeader("Retry-After", std::to_string(config_.windowSeconds));
                throw response;
            }

            recordRequest(ip);
        }

        void RateLimitMiddleware::after(HttpResponse &response)
        {
            // 添加限流信息头
            response.addHeader("X-RateLimit-Limit", std::to_string(config_.maxRequests));
            response.addHeader("X-RateLimit-Window", std::to_string(config_.windowSeconds) + "s");
        }

        void RateLimitMiddleware::cleanup()
        {
            muduo::Timestamp now = muduo::Timestamp::now();
            double elapsed = timeDifference(now, lastCleanup_);

            if (elapsed < config_.cleanupInterval)
            {
                return;
            }

            double windowSec = static_cast<double>(config_.windowSeconds);
            for (auto it = requestRecords_.begin(); it != requestRecords_.end();)
            {
                // 移除过期的时间戳
                auto &timestamps = it->second;
                while (!timestamps.empty() && timeDifference(now, timestamps.front()) > windowSec)
                {
                    timestamps.pop_front();
                }

                // 如果该 IP 没有记录了，删除整个条目
                if (timestamps.empty())
                {
                    it = requestRecords_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            lastCleanup_ = now;
        }

        int RateLimitMiddleware::getRequestCount(const std::string &ip)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            cleanup();

            auto it = requestRecords_.find(ip);
            if (it == requestRecords_.end())
            {
                return 0;
            }

            return static_cast<int>(it->second.size());
        }

        void RateLimitMiddleware::recordRequest(const std::string &ip)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            requestRecords_[ip].push_back(muduo::Timestamp::now());
        }

    } // namespace middleware
} // namespace http
