#include <iostream>
#include <string>
#include <unistd.h>

#include "../../../HttpServer/include/core/Logging.h"
#include "MarkdownServer.h"

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();

    std::string serverName = "MarkdownServer";
    int port = 8080;

    int opt;
    const char* str = "p:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                break;
        }
    }

    http::core::log::setLevel(http::core::log::Level::WARN);
    MarkdownServer server(port, serverName);
    server.setThreadNum(4);
    server.start();
}
