#ifndef RATELIMITMIDDLEWARE_H_
#define RATELIMITMIDDLEWARE_H_

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../Middleware.h"
#include "RateLimitConfig.h"

namespace http
{
    namespace middleware
    {

        class RateLimitMiddleware : public Middleware
        {
        public:
            explicit RateLimitMiddleware(const RateLimitConfig &config = RateLimitConfig::defaultConfig());

            void before(HttpRequest &request) override;
            void after(HttpResponse &response) override;

        private:
            using Clock = std::chrono::steady_clock;
            using TimePoint = Clock::time_point;

            void cleanup();
            int getRequestCount(const std::string &ip);
            void recordRequest(const std::string &ip);

            RateLimitConfig config_;
            std::mutex mutex_;
            std::unordered_map<std::string, std::deque<TimePoint>> requestRecords_;
            TimePoint lastCleanup_;
        };

    } // namespace middleware
} // namespace http
#endif
