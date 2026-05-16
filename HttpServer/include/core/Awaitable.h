#ifndef HTTPSERVER_CORE_AWAITABLE_H_
#define HTTPSERVER_CORE_AWAITABLE_H_

// Standalone Asio with io_uring backend + C++20 coroutines.
// All Asio types used through the framework are routed through this header
// so backend swaps remain a single-file change.

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>

#include <exception>
#include <utility>

#include "Logging.h"

namespace http::core
{

namespace asio_ns = ::asio;
using tcp = asio_ns::ip::tcp;

template <typename T>
using awaitable = asio_ns::awaitable<T>;

using asio_ns::use_awaitable;
using asio_ns::detached;

// Spawn a coroutine and log unhandled exceptions instead of swallowing them.
template <typename Executor, typename Coro>
void spawnLogged(Executor&& ex, Coro&& coro, const char* tag)
{
    asio_ns::co_spawn(std::forward<Executor>(ex), std::forward<Coro>(coro),
        [tag](std::exception_ptr eptr) {
            if (!eptr) return;
            try { std::rethrow_exception(eptr); }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Coroutine [" << tag << "] threw: " << e.what();
            }
            catch (...)
            {
                LOG_ERROR << "Coroutine [" << tag << "] threw unknown exception";
            }
        });
}

} // namespace http::core

#endif
