#pragma once
#include <stdexcept>
#include <string>

namespace http
{
    namespace db
    {
        // 异常
        class DbException : public std::runtime_error
        {
        public:
            explicit DbException(const std::string &message)
                : std::runtime_error(message) {}

            explicit DbException(const char *message)
                : std::runtime_error(message) {}
        };

    } // namespace db
} // namespace http