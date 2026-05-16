#ifndef LOGGINGMIDDLEWARE_H_
#define LOGGINGMIDDLEWARE_H_

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>

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
            std::string methodToString(HttpRequest::Method m);
            void writeLog(const std::string &line);

            std::ofstream logFile_;
            std::mutex mutex_;

            static thread_local std::string tlMethod_;
            static thread_local std::string tlPath_;
            static thread_local std::string tlIp_;
            static thread_local std::chrono::steady_clock::time_point tlStartTime_;
        };

    } // namespace middleware
} // namespace http
#endif
