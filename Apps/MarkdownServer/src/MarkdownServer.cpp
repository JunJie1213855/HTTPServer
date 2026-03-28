#include "MarkdownServer.h"
#include "../../../HttpServer/include/middleware/compress/CompressMiddleware.h"
#include "../../../HttpServer/include/middleware/ratelimit/RateLimitMiddleware.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>

MarkdownServer::MarkdownServer(int port, const std::string& name, muduo::net::TcpServer::Option option)
    : httpServer_(port, name, false, option)
{
    // 设置笔记存储目录
    notesDir_ = "Apps/MarkdownServer/notes";

    // 创建笔记目录
    mkdir(notesDir_.c_str(), 0755);
}

void MarkdownServer::start()
{
    initialize();
    httpServer_.start();
}

void MarkdownServer::initialize()
{
    initializeRouter();      // 初始化路由
    initializeMiddleware();  // 初始化中间件，这里仅仅是 CORS
}

void MarkdownServer::initializeRouter()
{
    // 获取所有笔记列表
    httpServer_.Get("/api/notes", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        getAllNotes(req, resp);
    });

    // 获取单个笔记内容 - 使用动态路由
    httpServer_.addRoute(http::HttpRequest::kGet, "/api/notes/:name", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        getNote(req, resp);
    });

    // 创建新笔记
    httpServer_.Post("/api/notes", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        createNote(req, resp);
    });

    // 更新笔记
    httpServer_.addRoute(http::HttpRequest::kPut, "/api/notes/:name", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        updateNote(req, resp);
    });

    // 删除笔记
    httpServer_.addRoute(http::HttpRequest::kDelete, "/api/notes/:name", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        deleteNote(req, resp);
    });

    // 静态文件服务
    httpServer_.Get("/", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        serveStatic(req, resp);
    });

    // 其他静态资源
    httpServer_.Get("/index.html", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        serveStatic(req, resp);
    });

    httpServer_.Get("/editor.html", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        serveStatic(req, resp);
    });

    httpServer_.Get("/css/:file", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        serveStatic(req, resp);
    });

    httpServer_.Get("/js/:file", [this](const http::HttpRequest& req, http::HttpResponse* resp) {
        serveStatic(req, resp);
    });
}

void MarkdownServer::initializeMiddleware()
{
    // 初始化中间件
    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();
    auto rateLimitMiddleware = std::make_shared<http::middleware::RateLimitMiddleware>();
    auto compressMiddleware = std::make_shared<http::middleware::CompressMiddleware>();
    // 添加中间件
    httpServer_.addMiddleware(corsMiddleware);
    httpServer_.addMiddleware(rateLimitMiddleware);
    httpServer_.addMiddleware(compressMiddleware);
}

std::string MarkdownServer::getNotesDirectory() const
{
    return notesDir_;
}

std::string MarkdownServer::urlDecode(const std::string& str)
{
    std::string result;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.size())
        {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> value)
            {
                result += static_cast<char>(value);
                i += 2;
            }
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

bool MarkdownServer::fileExists(const std::string& path) const
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void MarkdownServer::getAllNotes(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // std::cout << "get all notes call" << std::endl;
    using json = nlohmann::json;

    json notes = json::array();

    DIR* dir = opendir(notesDir_.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name(entry->d_name);
            if (name.length() > 3 && name.substr(name.length() - 3) == ".md")
            {
                std::string title = name.substr(0, name.length() - 3);

                // 读取文件内容作为摘要
                std::string filePath = notesDir_ + "/" + name;
                std::ifstream file(filePath);
                std::string firstLine;
                if (std::getline(file, firstLine))
                {
                    // 去掉 # 号
                    if (firstLine.length() > 0 && firstLine[0] == '#')
                    {
                        firstLine = firstLine.substr(1);
                        while (firstLine.length() > 0 && (firstLine[0] == ' ' || firstLine[0] == '\t'))
                        {
                            firstLine = firstLine.substr(1);
                        }
                    }
                }

                notes.push_back(json{
                    {"name", title},
                    {"title", firstLine.empty() ? title : firstLine}
                });
            }
        }
        closedir(dir);
    }

    resp->setVersion("HTTP/1.1");
    resp->setVersion("HTTP/1.1");
    resp->setVersion("HTTP/1.1");
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("application/json");
    resp->setBody(notes.dump());
}

void MarkdownServer::getNote(const http::HttpRequest& req, http::HttpResponse* resp)
{
    using json = nlohmann::json;

    std::string name = req.getPathParameters("param1");
    name = urlDecode(name);

    // 防止路径遍历攻击
    if (name.find("..") != std::string::npos || name.find("/") != std::string::npos)
    {
        resp->setVersion("HTTP/1.1");
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Invalid note name"}}.dump());
        return;
    }

    std::string filePath = notesDir_ + "/" + name + ".md";

    if (!fileExists(filePath))
    {
        resp->setVersion("HTTP/1.1");
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Note not found"}}.dump());
        return;
    }

    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    resp->setVersion("HTTP/1.1");
    resp->setVersion("HTTP/1.1");
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("application/json");
    resp->setBody(json{
        {"name", name},
        {"content", content}
    }.dump());
}

void MarkdownServer::createNote(const http::HttpRequest& req, http::HttpResponse* resp)
{
    using json = nlohmann::json;

    std::string body = req.getBody();
    if (body.empty())
    {
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Empty request body"}}.dump());
        return;
    }

    try
    {
        json data = json::parse(body);
        std::string title = data.value("title", "untitled");
        std::string content = data.value("content", "");

        // 替换非法字符
        std::replace(title.begin(), title.end(), '/', '_');
        std::replace(title.begin(), title.end(), '\\', '_');

        std::string filePath = notesDir_ + "/" + title + ".md";

        if (fileExists(filePath))
        {
            resp->setVersion("HTTP/1.1");
            resp->setStatusCode(http::HttpResponse::k409Conflict);
            resp->setStatusMessage("Conflict");
            resp->setContentType("application/json");
            resp->setBody(json{{"error", "Note already exists"}}.dump());
            return;
        }

        std::ofstream file(filePath);
        if (!file.is_open())
        {
            resp->setVersion("HTTP/1.1");
            resp->setStatusCode(http::HttpResponse::k500InternalServerError);
            resp->setStatusMessage("Internal Server Error");
            resp->setContentType("application/json");
            resp->setBody(json{{"error", "Failed to create note"}}.dump());
            return;
        }

        file << "# " << title << "\n\n" << content;
        file.close();

        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k201Created);
        resp->setStatusMessage("Created");
        resp->setContentType("application/json");
        resp->setBody(json{
            {"name", title},
            {"message", "Note created successfully"}
        }.dump());
    }
    catch (const std::exception& e)
    {
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", e.what()}}.dump());
    }
}

void MarkdownServer::updateNote(const http::HttpRequest& req, http::HttpResponse* resp)
{
    using json = nlohmann::json;

    std::string name = req.getPathParameters("param1");
    name = urlDecode(name);

    // 防止路径遍历攻击
    if (name.find("..") != std::string::npos || name.find("/") != std::string::npos)
    {
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Invalid note name"}}.dump());
        return;
    }

    std::string filePath = notesDir_ + "/" + name + ".md";

    if (!fileExists(filePath))
    {
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Note not found"}}.dump());
        return;
    }

    std::string body = req.getBody();
    try
    {
        json data = json::parse(body);
        std::string content = data.value("content", "");

        std::ofstream file(filePath);
        if (!file.is_open())
        {
            resp->setVersion("HTTP/1.1");
            resp->setStatusCode(http::HttpResponse::k500InternalServerError);
            resp->setStatusMessage("Internal Server Error");
            resp->setContentType("application/json");
            resp->setBody(json{{"error", "Failed to update note"}}.dump());
            return;
        }

        file << "# " << name << "\n\n" << content;
        file.close();

        resp->setVersion("HTTP/1.1");
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody(json{
            {"name", name},
            {"message", "Note updated successfully"}
        }.dump());
    }
    catch (const std::exception& e)
    {
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", e.what()}}.dump());
    }
}

void MarkdownServer::deleteNote(const http::HttpRequest& req, http::HttpResponse* resp)
{
    using json = nlohmann::json;

    std::string name = req.getPathParameters("param1");
    name = urlDecode(name);

    // 防止路径遍历攻击
    if (name.find("..") != std::string::npos || name.find("/") != std::string::npos)
    {
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Invalid note name"}}.dump());
        return;
    }

    std::string filePath = notesDir_ + "/" + name + ".md";

    if (!fileExists(filePath))
    {
        resp->setVersion("HTTP/1.1");
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Note not found"}}.dump());
        return;
    }

    if (remove(filePath.c_str()) != 0)
    {
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody(json{{"error", "Failed to delete note"}}.dump());
        return;
    }

    resp->setVersion("HTTP/1.1");
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("application/json");
    resp->setBody(json{
        {"name", name},
        {"message", "Note deleted successfully"}
    }.dump());
}

void MarkdownServer::serveStatic(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string path = req.path();

    // 默认返回 index.html
    if (path == "/" || path == "/index.html")
    {
        path = "/index.html";
    }

    // 获取文件路径
    std::string filePath = "Apps/MarkdownServer/resource" + path;

    // 读取文件
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/html");
        resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // 根据文件扩展名设置 Content-Type
    std::string ext = path.substr(path.rfind('.') + 1);
    if (ext == "html")
    {
        resp->setContentType("text/html");
    }
    else if (ext == "css")
    {
        resp->setContentType("text/css");
    }
    else if (ext == "js")
    {
        resp->setContentType("application/javascript");
    }
    else if (ext == "json")
    {
        resp->setContentType("application/json");
    }
    else
    {
        resp->setContentType("text/plain");
    }

    resp->setVersion("HTTP/1.1");
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setBody(content);
}