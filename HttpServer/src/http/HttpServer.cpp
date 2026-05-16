#include "../../include/http/HttpServer.h"

#include <chrono>
#include <cstring>
#include <exception>
#include <type_traits>

#include "../../include/core/IOBuffer.h"
#include "../../include/core/Logging.h"
#include "../../include/core/Ssl.h"
#include "../../include/http/HttpContext.h"
#include "../../include/ssl/SslConfig.h"
#include "../../include/ssl/SslContext.h"

namespace http
{

    namespace
    {

        constexpr size_t kReadChunk = 16 * 1024;

        void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
        {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }

        unsigned hardwareThreadDefault()
        {
            unsigned n = std::thread::hardware_concurrency();
            return n == 0 ? 4 : n;
        }

        // Compile-time fork on stream type. Plain sockets close at the TCP
        // layer; SSL streams attempt async_shutdown to send close_notify
        // first, then fall through to the TCP close.
        template <typename Stream>
        core::awaitable<void> shutdownStream(Stream &stream)
        {
            namespace asio_ns = core::asio_ns;
            if constexpr (std::is_same_v<Stream, core::ssl_stream>)
            {
                try
                {
                    co_await stream.async_shutdown(core::use_awaitable);
                }
                catch (const std::system_error &)
                {
                    // close_notify may have already been received; ignore.
                }
                asio_ns::error_code ec;
                stream.lowest_layer().shutdown(core::tcp::socket::shutdown_both, ec);
            }
            else
            {
                asio_ns::error_code ec;
                stream.shutdown(core::tcp::socket::shutdown_both, ec);
            }
            co_return;
        }

        // Resolve the underlying tcp::socket regardless of whether `stream`
        // is a plain socket or an SSL stream.
        template <typename Stream>
        core::tcp::socket &asTcpSocket(Stream &stream)
        {
            if constexpr (std::is_same_v<Stream, core::ssl_stream>)
            {
                return stream.next_layer();
            }
            else
            {
                return stream;
            }
        }

    } // namespace

    HttpServer::HttpServer(int port, const std::string &name, bool useSSL)
        : port_(port)
        , name_(name)
        , useSSL_(useSSL)
        , acceptor_(acceptorCtx_)
        , blockingPool_(hardwareThreadDefault())
    {
        httpCallback_ = defaultHttpCallback;
    }

    HttpServer::~HttpServer() = default;

    void HttpServer::setSslConfig(const ssl::SslConfig &config)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if (!sslCtx_->initialize())
        {
            LOG_FATAL << "SSL context initialization failed";
        }
    }

    void HttpServer::start()
    {
        namespace asio_ns = core::asio_ns;

        if (useSSL_ && !sslCtx_)
        {
            LOG_FATAL << "useSSL=true but setSslConfig() was never called";
            return;
        }

        core::tcp::endpoint endpoint(core::tcp::v4(), static_cast<unsigned short>(port_));
        asio_ns::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { LOG_FATAL << "acceptor.open failed: " << ec.message(); return; }
        acceptor_.set_option(asio_ns::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        if (ec) { LOG_FATAL << "acceptor.bind(port=" << port_ << ") failed: " << ec.message(); return; }
        acceptor_.listen(asio_ns::socket_base::max_listen_connections, ec);
        if (ec) { LOG_FATAL << "acceptor.listen failed: " << ec.message(); return; }

        LOG_WARN << "HttpServer[" << name_ << "] listening on port " << port_
                 << " (workers=" << workerNum_
                 << ", ssl=" << (useSSL_ ? "on" : "off")
                 << ", model=one-loop-per-thread)";

        workerCtxs_.reserve(workerNum_);
        workerGuards_.reserve(workerNum_);
        workerThreads_.reserve(workerNum_);

        for (int i = 0; i < workerNum_; ++i)
        {
            auto ctx = std::make_unique<asio_ns::io_context>(/*concurrency_hint=*/1);
            workerGuards_.emplace_back(asio_ns::make_work_guard(ctx->get_executor()));
            workerCtxs_.emplace_back(std::move(ctx));
        }
        for (int i = 0; i < workerNum_; ++i)
        {
            auto *raw = workerCtxs_[i].get();
            workerThreads_.emplace_back([raw, i] {
                try { raw->run(); }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "worker io_context[" << i << "] threw: " << e.what();
                }
            });
        }

        running_.store(true);
        core::spawnLogged(acceptorCtx_, acceptLoop(), "acceptLoop");

        try { acceptorCtx_.run(); }
        catch (const std::exception &e)
        {
            LOG_ERROR << "acceptor io_context threw: " << e.what();
        }

        for (auto &g : workerGuards_) g.reset();
        for (auto &t : workerThreads_) if (t.joinable()) t.join();
        workerThreads_.clear();
        workerGuards_.clear();
        workerCtxs_.clear();
    }

    void HttpServer::stop()
    {
        if (!running_.exchange(false)) return;
        core::asio_ns::error_code ec;
        acceptor_.close(ec);
        acceptorCtx_.stop();
        for (auto &g : workerGuards_) g.reset();
        for (auto &ctx : workerCtxs_) ctx->stop();
    }

    core::asio_ns::io_context &HttpServer::pickWorker()
    {
        const size_t n = workerCtxs_.size();
        const size_t idx = nextWorker_.fetch_add(1, std::memory_order_relaxed) % n;
        return *workerCtxs_[idx];
    }

    core::awaitable<void> HttpServer::acceptLoop()
    {
        using core::use_awaitable;

        while (running_.load())
        {
            try
            {
                auto &workerCtx = pickWorker();
                core::tcp::socket sock(workerCtx);
                co_await acceptor_.async_accept(sock, use_awaitable);
                sock.set_option(core::tcp::no_delay(true));

                if (useSSL_)
                {
                    core::spawnLogged(workerCtx.get_executor(),
                                      handleSslConnection(std::move(sock)),
                                      "handleSslConnection");
                }
                else
                {
                    core::spawnLogged(workerCtx.get_executor(),
                                      handleTcpConnection(std::move(sock)),
                                      "handleTcpConnection");
                }
            }
            catch (const std::exception &e)
            {
                if (running_.load())
                {
                    LOG_ERROR << "accept loop error: " << e.what();
                }
                else
                {
                    co_return;
                }
            }
        }
    }

    // Stream-agnostic per-connection read/parse/respond loop. Reused for
    // both plain `tcp::socket` and `asio::ssl::stream<tcp::socket>`.
    template <typename Stream>
    static core::awaitable<void> runConnectionLoop(HttpServer &server,
                                                   Stream &stream,
                                                   std::string peerIp,
                                                   core::asio_ns::any_io_executor workerExec)
    {
        using core::use_awaitable;
        namespace asio_ns = core::asio_ns;

        core::IOBuffer buf;
        HttpContext context;

        while (true)
        {
            buf.ensureWritableBytes(kReadChunk);
            std::size_t n = 0;
            try
            {
                n = co_await stream.async_read_some(
                    asio_ns::buffer(buf.beginWrite(), buf.writableBytes()),
                    use_awaitable);
            }
            catch (const std::system_error &e)
            {
                const auto code = e.code();
                if (code != asio_ns::error::eof &&
                    code != asio_ns::error::connection_reset &&
                    code != asio_ns::error::operation_aborted)
                {
                    LOG_WARN << "read error from " << peerIp << ": " << code.message();
                }
                break;
            }
            if (n == 0) break;
            buf.hasWritten(n);

            while (true)
            {
                if (!context.parseRequest(&buf, std::chrono::system_clock::now()))
                {
                    const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    try
                    {
                        co_await asio_ns::async_write(stream, asio_ns::buffer(bad, std::strlen(bad)), use_awaitable);
                    }
                    catch (...) {}
                    co_await shutdownStream(stream);
                    co_return;
                }
                if (!context.gotAll()) break;

                HttpRequest req = context.request();
                context.reset();

                std::string forwardedFor = req.getHeader("X-Forwarded-For");
                if (!forwardedFor.empty())
                {
                    auto pos = forwardedFor.find(',');
                    req.setClientIp(pos != std::string::npos ? forwardedFor.substr(0, pos) : forwardedFor);
                }
                else
                {
                    std::string realIp = req.getHeader("X-Real-IP");
                    req.setClientIp(!realIp.empty() ? realIp : peerIp);
                }

                const std::string &conn = req.getHeader("Connection");
                const bool close = (conn == "close") ||
                                   (req.getVersion() == "HTTP/1.0" && conn != "Keep-Alive");
                HttpResponse response(close);

#ifdef HTTPSERVER_INLINE_PIPELINE
                // Bench variant: run handlers inline on the worker IO thread.
                // Safe only when no handler may block - good as an upper-bound
                // ceiling for "framework overhead per request".
                (void)workerExec;
                server.runRequestPipeline(req, response);
#else
                co_await asio_ns::post(server.blockingPool(), use_awaitable);
                server.runRequestPipeline(req, response);
                co_await asio_ns::post(workerExec, use_awaitable);
#endif

                core::IOBuffer outBuf;
                response.appendToBuffer(outBuf);

                bool writeFailed = false;
                try
                {
                    co_await asio_ns::async_write(
                        stream,
                        asio_ns::buffer(outBuf.peek(), outBuf.readableBytes()),
                        use_awaitable);
                }
                catch (const std::system_error &e)
                {
                    LOG_WARN << "write error: " << e.code().message();
                    writeFailed = true;
                }

                if (writeFailed || response.closeConnection())
                {
                    co_await shutdownStream(stream);
                    co_return;
                }
            }
        }
    }

    void HttpServer::runRequestPipeline(HttpRequest &req, HttpResponse &resp)
    {
        try
        {
            middlewareChain_.processBefore(req);
            if (!router_.route(req, &resp)) httpCallback_(req, &resp);
            middlewareChain_.processAfter(resp);
        }
        catch (const HttpResponse &thrown) { resp = thrown; }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Pipeline exception: " << e.what();
            resp.setStatusCode(HttpResponse::k500InternalServerError);
            resp.setStatusMessage("Internal Server Error");
            resp.setBody(e.what());
        }
    }

    core::awaitable<void> HttpServer::handleTcpConnection(core::tcp::socket socket)
    {
        namespace asio_ns = core::asio_ns;
        auto workerExec = co_await asio_ns::this_coro::executor;

        std::string peerIp;
        try { peerIp = socket.remote_endpoint().address().to_string(); }
        catch (...) {}

        co_await runConnectionLoop(*this, socket, std::move(peerIp), workerExec);
    }

    core::awaitable<void> HttpServer::handleSslConnection(core::tcp::socket socket)
    {
        namespace asio_ns = core::asio_ns;
        auto workerExec = co_await asio_ns::this_coro::executor;

        std::string peerIp;
        try { peerIp = socket.remote_endpoint().address().to_string(); }
        catch (...) {}

        core::ssl_stream stream(std::move(socket), sslCtx_->native());

        try
        {
            co_await stream.async_handshake(core::asio_ssl::stream_base::server,
                                            core::use_awaitable);
        }
        catch (const std::system_error &e)
        {
            LOG_WARN << "SSL handshake failed from " << peerIp << ": " << e.code().message();
            co_return;
        }

        co_await runConnectionLoop(*this, stream, std::move(peerIp), workerExec);
    }

} // namespace http
