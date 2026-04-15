#ifndef MIDDLEWARE_H_
#define MIDDLEWARE_H_

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
    namespace middleware
    {
        // 中间件纯虚类接口
        class Middleware
        {
        public:
            virtual ~Middleware() = default;

            // 请求前处理
            virtual void before(HttpRequest &request) = 0;

            // 响应后处理
            virtual void after(HttpResponse &response) = 0;

            // 设置下一个中间件
            void setNext(std::shared_ptr<Middleware> next)
            {
                nextMiddleware_ = next;
            }
        // 成员定义为保护属性，基础的子类可以访问
        protected:
            std::shared_ptr<Middleware> nextMiddleware_;
        };

    } // namespace middleware
} // namespace http
#endif
