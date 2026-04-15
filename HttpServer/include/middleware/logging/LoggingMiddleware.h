#ifndef LOGGINGMIDDLEWARE_H_
#define LOGGINGMIDDLEWARE_H_

#include <string>
#include <fstream>
#include <mutex>

#include <muduo/base/Timestamp.h>

#include "../Middleware.h"

namespace http
{
    namespace middleware
    {

        class LoggingMiddleware : public Middleware
        {
        public:
            explicit LoggingMiddleware(const std::string &logFilePath = "access.log");

            void before(HttpRequest &request) override;
            void after(HttpResponse &response) override;

        private:
            // 将 Method 枚举转为字符串
            std::string methodToString(HttpRequest::Method m);

            // 格式化并写入日志行
            void writeLog(const std::string &line);

        private:
            std::ofstream logFile_;
            std::mutex mutex_;

            // thread_local 暂存请求信息，保证 before/after 在同一线程传递
            static thread_local std::string tlMethod_;
            static thread_local std::string tlPath_;
            static thread_local std::string tlIp_;
            static thread_local muduo::Timestamp tlStartTime_;
        };

    } // namespace middleware
} // namespace http
#endif
