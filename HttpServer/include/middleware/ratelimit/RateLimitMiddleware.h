#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include <mutex>

#include <muduo/base/Timestamp.h>

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
            // 清理过期记录
            void cleanup();

            // 获取指定 IP 在窗口内的请求数
            int getRequestCount(const std::string &ip);

            // 记录一次请求
            void recordRequest(const std::string &ip);

        private:
            RateLimitConfig config_;
            std::mutex mutex_;
            // IP -> 请求时间戳队列, 用 ip 记录对应的请求次数
            std::unordered_map<std::string, std::deque<muduo::Timestamp>> requestRecords_;
            // 上次清理时间
            muduo::Timestamp lastCleanup_;
        };

    } // namespace middleware
} // namespace http
