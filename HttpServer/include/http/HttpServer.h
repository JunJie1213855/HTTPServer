#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../core/Awaitable.h"
#include "../middleware/MiddlewareChain.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace ssl
{
    class SslConfig;
    class SslContext;
} // namespace ssl

namespace http
{

    class HttpServer
    {
    public:
        using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

        HttpServer(int port, const std::string &name, bool useSSL = false);
        ~HttpServer();

        HttpServer(const HttpServer &) = delete;
        HttpServer &operator=(const HttpServer &) = delete;

        void setThreadNum(int numThreads) { workerNum_ = numThreads > 0 ? numThreads : 1; }

        void start();
        void stop();

        void setHttpCallback(const HttpCallback &cb) { httpCallback_ = cb; }

        void Get(const std::string &path, const HttpCallback &cb)
        {
            router_.registerCallback(HttpRequest::kGet, path, cb);
        }
        void Get(const std::string &path, router::Router::HandlerPtr handler)
        {
            router_.registerHandler(HttpRequest::kGet, path, handler);
        }
        void Post(const std::string &path, const HttpCallback &cb)
        {
            router_.registerCallback(HttpRequest::kPost, path, cb);
        }
        void Post(const std::string &path, router::Router::HandlerPtr handler)
        {
            router_.registerHandler(HttpRequest::kPost, path, handler);
        }
        void addRoute(HttpRequest::Method method, const std::string &path, router::Router::HandlerPtr handler)
        {
            router_.addRegexHandler(method, path, handler);
        }
        void addRoute(HttpRequest::Method method, const std::string &path, const router::Router::HandlerCallback &callback)
        {
            router_.addRegexCallback(method, path, callback);
        }

        void setSessionManager(std::unique_ptr<session::SessionManager> manager)
        {
            sessionManager_ = std::move(manager);
        }
        session::SessionManager *getSessionManager() const { return sessionManager_.get(); }

        void addMiddleware(std::shared_ptr<middleware::Middleware> middleware)
        {
            middlewareChain_.addMiddleware(middleware);
        }

        // -------- TLS / SSL --------
        void enableSSL(bool enable) { useSSL_ = enable; }
        bool sslEnabled() const { return useSSL_; }
        // Must be called before start() when useSSL is true. Aborts at start
        // if SSL is requested but SslConfig was never supplied or initialize
        // failed.
        void setSslConfig(const ssl::SslConfig &config);

        core::asio_ns::thread_pool &blockingPool() { return blockingPool_; }

        // Invoked from the connection loop helper. Public so the
        // free-function template can call it without a friend list.
        void runRequestPipeline(HttpRequest &req, HttpResponse &resp);

    private:
        core::awaitable<void> acceptLoop();
        core::awaitable<void> handleTcpConnection(core::tcp::socket socket);
        core::awaitable<void> handleSslConnection(core::tcp::socket socket);

        core::asio_ns::io_context &pickWorker();

        using WorkGuard = core::asio_ns::executor_work_guard<core::asio_ns::io_context::executor_type>;

        int port_;
        std::string name_;
        int workerNum_{4};
        bool useSSL_;

        core::asio_ns::io_context acceptorCtx_;
        core::tcp::acceptor acceptor_;

        std::vector<std::unique_ptr<core::asio_ns::io_context>> workerCtxs_;
        std::vector<WorkGuard> workerGuards_;
        std::vector<std::thread> workerThreads_;
        std::atomic<size_t> nextWorker_{0};

        core::asio_ns::thread_pool blockingPool_;

        std::atomic<bool> running_{false};

        HttpCallback httpCallback_;
        router::Router router_;
        std::unique_ptr<session::SessionManager> sessionManager_;
        middleware::MiddlewareChain middlewareChain_;
        std::unique_ptr<ssl::SslContext> sslCtx_;
    };

} // namespace http
#endif
