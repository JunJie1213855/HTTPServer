#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>

#include "../../HttpServer/include/http/HttpServer.h"
#include "../../HttpServer/include/http/HttpRequest.h"
#include "../../HttpServer/include/http/HttpResponse.h"
#include "../../HttpServer/include/utils/JsonUtil.h"
#include "../../HttpServer/include/middleware/cors/CorsMiddleware.h"
#include "../../HttpServer/include/middleware/cors/CorsConfig.h"

class MarkdownServer
{
public:
    MarkdownServer(int port,
                   const std::string& name,
                   muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

    void setThreadNum(int numThreads)
    {
        httpServer_.setThreadNum(numThreads);
    }

    void start();

private:
    void initialize();
    void initializeRouter();
    void initializeMiddleware();

    // API handlers
    void getAllNotes(const http::HttpRequest& req, http::HttpResponse* resp);
    void getNote(const http::HttpRequest& req, http::HttpResponse* resp);
    void createNote(const http::HttpRequest& req, http::HttpResponse* resp);
    void updateNote(const http::HttpRequest& req, http::HttpResponse* resp);
    void deleteNote(const http::HttpRequest& req, http::HttpResponse* resp);

    // Static file handlers
    void serveStatic(const http::HttpRequest& req, http::HttpResponse* resp);

    // Utility
    std::string getNotesDirectory() const;
    std::string urlDecode(const std::string& str);
    bool fileExists(const std::string& path) const;

private:
    http::HttpServer httpServer_;
    std::string notesDir_;
};