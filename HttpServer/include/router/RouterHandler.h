#pragma once
#include <string>
#include <memory>
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
    namespace router
    {
        // 路由句柄虚基类接口
        class RouterHandler
        {
        public:
            virtual ~RouterHandler() = default;
            virtual void handle(const HttpRequest &req, HttpResponse *resp) = 0;
        };
        // 没有子类, 纯虚基类无法使用

    } // namespace router
} // namespace http