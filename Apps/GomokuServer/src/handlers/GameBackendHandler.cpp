#include "../include/handlers/GameBackendHandler.h"
#include "../include/ResourceManager.h"

void GameBackendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string reqFile = ResourceManager::instance().getPath("Backend");
    FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        LOG_WARN << reqFile << " not exist.";
        fileOperater.resetDefaultFile();
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string htmlContent(buffer.data(), buffer.size());

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(htmlContent.size());
    resp->setBody(htmlContent);
}
