#ifndef HTTPSERVER_CORE_LOGGING_H_
#define HTTPSERVER_CORE_LOGGING_H_

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>

// Drop-in replacement for muduo's stream-style log macros:
//   LOG_DEBUG << "msg" << value;
//   LOG_INFO  << "msg";
//   LOG_WARN  << "msg";
//   LOG_ERROR << "msg";
//   LOG_FATAL << "msg";  // aborts after writing
//
// All output is serialized through a single mutex and flushed line-by-line.

namespace http::core::log
{

enum class Level
{
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4,
};

inline std::atomic<int>& levelRef()
{
    static std::atomic<int> level{static_cast<int>(Level::INFO)};
    return level;
}

inline void setLevel(Level lvl) { levelRef().store(static_cast<int>(lvl)); }
inline Level getLevel() { return static_cast<Level>(levelRef().load()); }

inline std::mutex& sinkMutex()
{
    static std::mutex m;
    return m;
}

inline const char* levelName(Level lvl)
{
    switch (lvl)
    {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO ";
        case Level::WARN:  return "WARN ";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
    }
    return "?????";
}

class LogStream
{
public:
    LogStream(Level lvl, const char* file, int line)
        : level_(lvl)
        , enabled_(static_cast<int>(lvl) >= levelRef().load())
        , file_(file)
        , line_(line)
    {
    }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream()
    {
        if (!enabled_)
        {
            if (level_ == Level::FATAL)
            {
                std::abort();
            }
            return;
        }

        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto t = system_clock::to_time_t(now);
        const auto us = duration_cast<microseconds>(now.time_since_epoch()).count() % 1'000'000;
        std::tm tm{};
        ::gmtime_r(&t, &tm);

        std::ostringstream prefix;
        prefix << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
               << '.' << std::setw(6) << std::setfill('0') << us
               << ' ' << levelName(level_)
               << ' ' << std::this_thread::get_id()
               << ' ';
        // file:line tail (file only, not full path)
        std::string_view sv(file_);
        auto slash = sv.find_last_of('/');
        if (slash != std::string_view::npos) sv.remove_prefix(slash + 1);

        std::lock_guard<std::mutex> lock(sinkMutex());
        std::ostream& out = (level_ >= Level::ERROR) ? std::cerr : std::cout;
        out << prefix.str() << buf_.str() << "  - " << sv << ':' << line_ << '\n';
        out.flush();

        if (level_ == Level::FATAL)
        {
            std::abort();
        }
    }

    template <typename T>
    LogStream& operator<<(T&& value)
    {
        if (enabled_)
        {
            buf_ << std::forward<T>(value);
        }
        return *this;
    }

private:
    Level level_;
    bool enabled_;
    const char* file_;
    int line_;
    std::ostringstream buf_;
};

} // namespace http::core::log

#define LOG_DEBUG ::http::core::log::LogStream(::http::core::log::Level::DEBUG, __FILE__, __LINE__)
#define LOG_INFO  ::http::core::log::LogStream(::http::core::log::Level::INFO,  __FILE__, __LINE__)
#define LOG_WARN  ::http::core::log::LogStream(::http::core::log::Level::WARN,  __FILE__, __LINE__)
#define LOG_ERROR ::http::core::log::LogStream(::http::core::log::Level::ERROR, __FILE__, __LINE__)
#define LOG_FATAL ::http::core::log::LogStream(::http::core::log::Level::FATAL, __FILE__, __LINE__)

#endif
