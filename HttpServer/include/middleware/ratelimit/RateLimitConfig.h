#pragma once

#include <string>

namespace http
{
    namespace middleware
    {

        struct RateLimitConfig
        {
            int maxRequests = 100;    // 时间窗口内最大请求数
            int windowSeconds = 60;   // 时间窗口（秒）
            int cleanupInterval = 60; // 清理过期记录的间隔（秒）

            static RateLimitConfig defaultConfig()
            {
                return RateLimitConfig();
            }
        };

    } // namespace middleware
} // namespace http
