#include "../../../include/middleware/logging/LoggingMiddleware.h"
#include <muduo/base/Logging.h>
#include <sstream>
#include <iomanip>

namespace http
{
    namespace middleware
    {

        // thread_local 成员定义
        thread_local std::string LoggingMiddleware::tlMethod_;
        thread_local std::string LoggingMiddleware::tlPath_;
        thread_local std::string LoggingMiddleware::tlIp_;
        thread_local muduo::Timestamp LoggingMiddleware::tlStartTime_;

        LoggingMiddleware::LoggingMiddleware(const std::string &logFilePath)
        {
            logFile_.open(logFilePath, std::ios::app);
            if (!logFile_.is_open())
            {
                LOG_ERROR << "LoggingMiddleware: cannot open log file: " << logFilePath;
            }
            else
            {
                LOG_INFO << "LoggingMiddleware: log file opened: " << logFilePath;
            }
        }

        void LoggingMiddleware::before(HttpRequest &request)
        {
            tlMethod_ = methodToString(request.method());
            tlPath_ = request.path();
            tlIp_ = request.getClientIp();
            tlStartTime_ = muduo::Timestamp::now();
        }

        void LoggingMiddleware::after(HttpResponse &response)
        {
            muduo::Timestamp endTime = muduo::Timestamp::now();
            double elapsedMs = muduo::timeDifference(endTime, tlStartTime_) * 1000.0;

            // 格式化时间戳
            time_t seconds = static_cast<time_t>(endTime.secondsSinceEpoch());
            struct tm tm_time;
            gmtime_r(&seconds, &tm_time);
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                     tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                     tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                     static_cast<int>(endTime.microSecondsSinceEpoch() % 1000000 / 1000));

            // 格式化日志行: [时间] 方法 路径 状态码 耗时 IP
            std::ostringstream line;
            line << "[" << timeBuf << "] "
                 << std::left << std::setw(6) << tlMethod_
                 << std::setw(20) << tlPath_
                 << std::right << std::setw(3) << response.getStatusCode() << "  "
                 << std::fixed << std::setprecision(1) << elapsedMs << "ms  "
                 << tlIp_;

            writeLog(line.str());
        }

        std::string LoggingMiddleware::methodToString(HttpRequest::Method m)
        {
            switch (m)
            {
            case HttpRequest::kGet:
                return "GET";
            case HttpRequest::kPost:
                return "POST";
            case HttpRequest::kPut:
                return "PUT";
            case HttpRequest::kDelete:
                return "DELETE";
            case HttpRequest::kHead:
                return "HEAD";
            case HttpRequest::kOptions:
                return "OPTIONS";
            default:
                return "UNKNOWN";
            }
        }

        void LoggingMiddleware::writeLog(const std::string &line)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (logFile_.is_open())
            {
                logFile_ << line << "\n";
                logFile_.flush();
            }
        }

    } // namespace middleware
} // namespace http
