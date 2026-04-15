#ifndef MIDDLEWARECHAIN_H_
#define MIDDLEWARECHAIN_H_

#include <vector>
#include <memory>
#include "Middleware.h"

namespace http
{
    namespace middleware
    {
        // 中间件链条
        class MiddlewareChain
        {
        public:
            // @brief 添加中间件
            // @example A -> B -> C -> D -> E
            void addMiddleware(std::shared_ptr<Middleware> middleware);
            
            // @brief 正序让中间件预处理
            // @example  -> A -> B -> C -> D -> E
            void processBefore(HttpRequest &request);

            // @brief 反序让中间件后处理
            // @example -> E -> D -> C -> B -> A
            void processAfter(HttpResponse &response);

        private:
            // @example A、B、C、B、E 
            std::vector<std::shared_ptr<Middleware>> middlewares_;
        };

    } // namespace middleware
} // namespace http
#endif
