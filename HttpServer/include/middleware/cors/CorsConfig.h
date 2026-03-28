#pragma once

#include <string>
#include <vector>

namespace http
{
    namespace middleware
    {

        struct CorsConfig
        {
            std::vector<std::string> allowedOrigins;
            std::vector<std::string> allowedMethods;
            std::vector<std::string> allowedHeaders;
            bool allowCredentials = false;            // 不允许凭证
            int maxAge = 3600;                        // 一个小时
            // 
            static CorsConfig defaultConfig()
            {
                CorsConfig config;
                config.allowedOrigins = {"*"};        // 允许所有浏览器获取
                config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"}; // 允许的方法
                config.allowedHeaders = {"Content-Type", "Authorization"};           // 允许的请求头
                return config;
            }
        };

    } // namespace middleware
} // namespace http