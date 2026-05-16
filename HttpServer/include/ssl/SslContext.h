#ifndef SSL_CONTEXT_H_
#define SSL_CONTEXT_H_

#include "../core/Ssl.h"
#include "SslConfig.h"

namespace ssl
{

    // Thin wrapper around asio::ssl::context. Owns the underlying SSL_CTX*,
    // applies the SslConfig knobs (cert/key/protocol/cipher/verify) on init,
    // and hands out a reference for asio::ssl::stream construction.
    class SslContext
    {
    public:
        explicit SslContext(const SslConfig &config);

        SslContext(const SslContext &) = delete;
        SslContext &operator=(const SslContext &) = delete;

        // Apply the SslConfig knobs. Returns false on any OpenSSL error
        // (cert/key load, mismatched key, bad cipher list, etc.).
        bool initialize();

        // Reference passed to asio::ssl::stream<tcp::socket>(socket, ctx).
        http::core::asio_ssl::context &native() { return ctx_; }

    private:
        static http::core::asio_ssl::context::method versionToMethod(SSLVersion v);

        SslConfig config_;
        http::core::asio_ssl::context ctx_;
    };

} // namespace ssl

#endif
