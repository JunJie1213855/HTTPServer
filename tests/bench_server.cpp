// Minimal HTTP bench server. No middleware, no logging, no rate limit.
//   GET  /hello   -> "Hello, World!" (text/plain)
//   GET  /json    -> {"hello":"world"} (application/json)
//   POST /echo    -> echoes request body
// Usage:  bench_server <port> [workers]
#include <cstdlib>
#include <string>

#include "core/Logging.h"
#include "http/HttpServer.h"

int main(int argc, char **argv)
{
    int port = (argc > 1) ? std::atoi(argv[1]) : 9000;
    int workers = (argc > 2) ? std::atoi(argv[2]) : 4;

    // Silence the framework's own logging during the bench run.
    http::core::log::setLevel(http::core::log::Level::ERROR);

    http::HttpServer server(port, "bench");
    server.setThreadNum(workers);

    server.Get("/hello", [](const http::HttpRequest &, http::HttpResponse *resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody("Hello, World!");
    });

    server.Get("/json", [](const http::HttpRequest &, http::HttpResponse *resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody("{\"hello\":\"world\"}");
    });

    server.Post("/echo", [](const http::HttpRequest &req, http::HttpResponse *resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/octet-stream");
        resp->setBody(req.getBody());
    });

    server.start();
    return 0;
}
