#ifndef COMPRESSMIDDLEWARE_H_
#define COMPRESSMIDDLEWARE_H_

#include <string>

#include "../Middleware.h"
#include "CompressConfig.h"

namespace http
{
    namespace middleware
    {

        class CompressMiddleware : public Middleware
        {
        public:
            explicit CompressMiddleware(const CompressConfig &config = CompressConfig::defaultConfig());

            void before(HttpRequest &request) override;
            void after(HttpResponse &response) override;

        private:
            // 检查客户端是否支持 gzip
            bool clientAcceptsGzip() const;

            // 检查 Content-Type 是否应该压缩
            bool shouldCompress(const std::string &contentType) const;

            // 执行 gzip 压缩
            bool gzipCompress(const std::string &input, std::string &output);

        private:
            CompressConfig config_;
            // 缓存当前请求的 Accept-Encoding
            std::string acceptEncoding_;
        };

    } // namespace middleware
} // namespace http
#endif
