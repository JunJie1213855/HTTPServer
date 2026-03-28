#pragma once

#include <string>
#include <vector>

namespace http
{
    namespace middleware
    {

        struct CompressConfig
        {
            int minBodySize = 256;    // 最小压缩阈值（字节）
            int compressionLevel = 6; // zlib 压缩级别 (1-9, 默认6)
            // 需要压缩的 MIME 类型前缀
            std::vector<std::string> mimeTypes = {
                "text/",
                "application/json",
                "application/javascript",
                "application/xml",
                "application/html",
                "image/svg+xml"};

            static CompressConfig defaultConfig()
            {
                return CompressConfig();
            }
        };

    } // namespace middleware
} // namespace http
