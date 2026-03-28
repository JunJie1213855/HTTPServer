#include "../../../include/middleware/compress/CompressMiddleware.h"

#include <zlib.h>
#include <muduo/base/Logging.h>
#include <algorithm>

namespace http
{
    namespace middleware
    {

        CompressMiddleware::CompressMiddleware(const CompressConfig &config)
            : config_(config)
        {
        }

        void CompressMiddleware::before(HttpRequest &request)
        {
            // 缓存 Accept-Encoding 头，供 after() 使用
            acceptEncoding_ = request.getHeader("Accept-Encoding");
        }

        void CompressMiddleware::after(HttpResponse &response)
        {
            // 已经压缩过的不再处理
            auto it = response.getHeaders().find("Content-Encoding");
            if (it != response.getHeaders().end())
            {
                return;
            }

            // 检查客户端是否支持 gzip
            if (!clientAcceptsGzip())
            {
                return;
            }

            // 检查 body 大小
            const std::string &body = response.getBody();
            if (static_cast<int>(body.size()) < config_.minBodySize)
            {
                return;
            }

            // 检查 Content-Type
            auto contentTypeIt = response.getHeaders().find("Content-Type");
            if (contentTypeIt == response.getHeaders().end())
            {
                return;
            }
            if (!shouldCompress(contentTypeIt->second))
            {
                return;
            }

            // 执行压缩
            std::string compressed;
            if (gzipCompress(body, compressed))
            {
                response.setBody(compressed);
                response.addHeader("Content-Encoding", "gzip");
                response.setContentLength(compressed.size());
                LOG_DEBUG << "Compressed response: " << body.size()
                          << " -> " << compressed.size() << " bytes";
            }
        }

        bool CompressMiddleware::clientAcceptsGzip() const
        {
            // 检查 Accept-Encoding 中是否包含 gzip
            return acceptEncoding_.find("gzip") != std::string::npos;
        }

        bool CompressMiddleware::shouldCompress(const std::string &contentType) const
        {
            for (const auto &mime : config_.mimeTypes)
            {
                if (contentType.compare(0, mime.size(), mime) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        bool CompressMiddleware::gzipCompress(const std::string &input, std::string &output)
        {
            // 使用 zlib 的 gzip 模式压缩
            z_stream stream;
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;

            // windowBits = 15 + 16 启用 gzip 格式
            int ret = deflateInit2(&stream, config_.compressionLevel, Z_DEFLATED,
                                   15 + 16, 8, Z_DEFAULT_STRATEGY);
            if (ret != Z_OK)
            {
                LOG_ERROR << "deflateInit2 failed: " << ret;
                return false;
            }

            // 预分配输出缓冲区
            output.resize(deflateBound(&stream, static_cast<uLong>(input.size())));

            stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
            stream.avail_in = static_cast<uInt>(input.size());
            stream.next_out = reinterpret_cast<Bytef *>(&output[0]);
            stream.avail_out = static_cast<uInt>(output.size());

            ret = deflate(&stream, Z_FINISH);
            if (ret != Z_STREAM_END)
            {
                LOG_ERROR << "deflate failed: " << ret;
                deflateEnd(&stream);
                return false;
            }

            output.resize(stream.total_out);
            deflateEnd(&stream);
            return true;
        }

    } // namespace middleware
} // namespace http
