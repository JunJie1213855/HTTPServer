#ifndef RESOURCEMANAGER_H_
#define RESOURCEMANAGER_H_

#include <string>
#include <unordered_map>
#include <mutex>

#include <muduo/base/Logging.h>
#include "../../../HttpServer/include/utils/JsonUtil.h"

class ResourceManager
{
public:
    // 获取单例
    static ResourceManager& instance()
    {
        static ResourceManager instance;
        return instance;
    }

    // 从 JSON 文件加载资源配置
    bool load(const std::string& configPath)
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            LOG_ERROR << "ResourceManager: cannot open config file: " << configPath;
            return false;
        }

        try
        {
            json config = json::parse(file);
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto it = config.begin(); it != config.end(); ++it)
            {
                resources_[it.key()] = it.value().get<std::string>();
            }
            LOG_INFO << "ResourceManager: loaded " << resources_.size()
                     << " resource paths from " << configPath;
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "ResourceManager: parse error: " << e.what();
            return false;
        }
    }

    // 根据资源名获取路径，找不到返回空字符串
    std::string getPath(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(name);
        if (it != resources_.end())
        {
            return it->second;
        }
        LOG_WARN << "ResourceManager: resource not found: " << name;
        return "";
    }

    // 删除拷贝和赋值
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

private:
    ResourceManager() = default;

    std::mutex mutex_;
    std::unordered_map<std::string, std::string> resources_;
};
#endif
