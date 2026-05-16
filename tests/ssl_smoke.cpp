// Tiny HTTPS smoke server used only for local verification of the
// asio::ssl integration. Not built by the main CMake.
#include <chrono>
#include <iostream>
#include <thread>

#include "core/Logging.h"
#include "http/HttpServer.h"
#include "ssl/SslConfig.h"

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: ssl_smoke <port> <cert.pem> <key.pem>\n";
        return 1;
    }
    int port = std::atoi(argv[1]);

    http::core::log::setLevel(http::core::log::Level::INFO);

    http::HttpServer server(port, "SslSmoke", /*useSSL=*/true);
    server.setThreadNum(2);

    ssl::SslConfig cfg;
    cfg.setCertificateFile(argv[2]);
    cfg.setPrivateKeyFile(argv[3]);
    cfg.setProtocolVersion(ssl::SSLVersion::TLS_1_2);
    server.setSslConfig(cfg);

    server.Get("/", [](const http::HttpRequest &, http::HttpResponse *resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody("hello from ssl\n");
    });
    server.Get("/echo", [](const http::HttpRequest &req, http::HttpResponse *resp) {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody(std::string("{\"path\":\"") + req.path() + "\"}");
    });

    server.start();
    return 0;
}
