#include "../../../include/middleware/ratelimit/RateLimitMiddleware.h"

#include <algorithm>

#include "../../../include/core/Logging.h"

namespace http
{
    namespace middleware
    {

        namespace
        {
            double secondsBetween(std::chrono::steady_clock::time_point later,
                                  std::chrono::steady_clock::time_point earlier)
            {
                using namespace std::chrono;
                return duration_cast<duration<double>>(later - earlier).count();
            }
        } // namespace

        RateLimitMiddleware::RateLimitMiddleware(const RateLimitConfig &config)
            : config_(config), lastCleanup_(Clock::now())
        {
        }

        void RateLimitMiddleware::before(HttpRequest &request)
        {
            std::string ip = request.getClientIp();
            if (ip.empty()) ip = "unknown";

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
            response.addHeader("X-RateLimit-Limit", std::to_string(config_.maxRequests));
            response.addHeader("X-RateLimit-Window", std::to_string(config_.windowSeconds) + "s");
        }

        void RateLimitMiddleware::cleanup()
        {
            const auto now = Clock::now();
            const double elapsed = secondsBetween(now, lastCleanup_);

            if (elapsed < config_.cleanupInterval) return;

            const double windowSec = static_cast<double>(config_.windowSeconds);
            for (auto it = requestRecords_.begin(); it != requestRecords_.end();)
            {
                auto &timestamps = it->second;
                while (!timestamps.empty() && secondsBetween(now, timestamps.front()) > windowSec)
                {
                    timestamps.pop_front();
                }

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
            if (it == requestRecords_.end()) return 0;
            return static_cast<int>(it->second.size());
        }

        void RateLimitMiddleware::recordRequest(const std::string &ip)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            requestRecords_[ip].push_back(Clock::now());
        }

    } // namespace middleware
} // namespace http
