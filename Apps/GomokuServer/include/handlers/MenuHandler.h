#ifndef MENUHANDLER_H_
#define MENUHANDLER_H_

#include "../../../HttpServer/include/router/RouterHandler.h"
#include "../GomokuServer.h"


class MenuHandler : public http::router::RouterHandler
{
public:
    explicit MenuHandler(GomokuServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    GomokuServer* server_;
};
#endif
