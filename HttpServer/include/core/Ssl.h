#ifndef HTTPSERVER_CORE_SSL_H_
#define HTTPSERVER_CORE_SSL_H_

// Pulls in the SSL portion of standalone Asio. Kept separate from
// Awaitable.h so translation units that don't touch TLS don't pay the
// OpenSSL include cost.

#include <asio/ssl.hpp>

#include "Awaitable.h"

namespace http::core
{

    namespace asio_ssl = ::asio::ssl;
    using ssl_stream = asio_ssl::stream<tcp::socket>;

} // namespace http::core

#endif
