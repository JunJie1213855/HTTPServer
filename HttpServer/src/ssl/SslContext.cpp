#include "../../include/ssl/SslContext.h"

#include "../../include/core/Logging.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace ssl
{

    namespace acore = http::core;

    SslContext::SslContext(const SslConfig &config)
        : config_(config)
        , ctx_(versionToMethod(config.getProtocolVersion()))
    {
    }

    acore::asio_ssl::context::method SslContext::versionToMethod(SSLVersion v)
    {
        using m = acore::asio_ssl::context::method;
        switch (v)
        {
        case SSLVersion::TLS_1_0: return m::tlsv1_server;
        case SSLVersion::TLS_1_1: return m::tlsv11_server;
        case SSLVersion::TLS_1_2: return m::tlsv12_server;
        case SSLVersion::TLS_1_3: return m::tlsv13_server;
        }
        return m::tlsv12_server;
    }

    bool SslContext::initialize()
    {
        using namespace acore;
        asio_ns::error_code ec;

        // Sensible server-side defaults: no SSLv2/v3, no renegotiation,
        // server picks cipher order.
        ctx_.set_options(
                asio_ssl::context::default_workarounds |
                asio_ssl::context::no_sslv2 |
                asio_ssl::context::no_sslv3 |
                asio_ssl::context::no_compression |
                asio_ssl::context::single_dh_use,
            ec);
        if (ec)
        {
            LOG_ERROR << "SslContext::set_options: " << ec.message();
            return false;
        }

        // Cipher server preference.
        SSL_CTX_set_options(ctx_.native_handle(), SSL_OP_CIPHER_SERVER_PREFERENCE);

        if (!config_.getCertificateFile().empty())
        {
            ctx_.use_certificate_file(config_.getCertificateFile(),
                                      asio_ssl::context::pem, ec);
            if (ec)
            {
                LOG_ERROR << "use_certificate_file(" << config_.getCertificateFile()
                          << "): " << ec.message();
                return false;
            }
        }
        else
        {
            LOG_ERROR << "SslContext: certificate file not configured";
            return false;
        }

        if (!config_.getPrivateKeyFile().empty())
        {
            ctx_.use_private_key_file(config_.getPrivateKeyFile(),
                                      asio_ssl::context::pem, ec);
            if (ec)
            {
                LOG_ERROR << "use_private_key_file(" << config_.getPrivateKeyFile()
                          << "): " << ec.message();
                return false;
            }
        }
        else
        {
            LOG_ERROR << "SslContext: private key file not configured";
            return false;
        }

        if (!config_.getCertificateChainFile().empty())
        {
            ctx_.use_certificate_chain_file(config_.getCertificateChainFile(), ec);
            if (ec)
            {
                LOG_ERROR << "use_certificate_chain_file: " << ec.message();
                return false;
            }
        }

        if (!config_.getCipherList().empty())
        {
            if (SSL_CTX_set_cipher_list(ctx_.native_handle(),
                                        config_.getCipherList().c_str()) <= 0)
            {
                char buf[256];
                ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
                LOG_ERROR << "set_cipher_list failed: " << buf;
                return false;
            }
        }

        if (config_.getVerifyClient())
        {
            ctx_.set_verify_mode(
                asio_ssl::verify_peer | asio_ssl::verify_fail_if_no_peer_cert,
                ec);
            if (ec) { LOG_ERROR << "set_verify_mode: " << ec.message(); return false; }
            SSL_CTX_set_verify_depth(ctx_.native_handle(), config_.getVerifyDepth());
        }
        else
        {
            ctx_.set_verify_mode(asio_ssl::verify_none, ec);
        }

        SSL_CTX_set_session_cache_mode(ctx_.native_handle(), SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(ctx_.native_handle(), config_.getSessionCacheSize());
        SSL_CTX_set_timeout(ctx_.native_handle(), config_.getSessionTimeout());

        LOG_INFO << "SslContext initialized (cert=" << config_.getCertificateFile() << ")";
        return true;
    }

} // namespace ssl
