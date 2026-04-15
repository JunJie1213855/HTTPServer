#ifndef MYSQLUTIL_H_
#define MYSQLUTIL_H_
#include "db/DbConnectionPool.h"

#include <string>

namespace http
{
    // mysql 工具
    class MysqlUtil
    {
    public:
        // 初始化 mysql 连接
        static void init(const std::string &host, const std::string &user,
                         const std::string &password, const std::string &database,
                         size_t poolSize = 10)
        {
            http::db::DbConnectionPool::getInstance().init(
                host, user, password, database, poolSize);
        }
        // 执行语句
        template <typename... Args>
        sql::ResultSet *executeQuery(const std::string &sql, Args &&...args)
        {
            auto conn = http::db::DbConnectionPool::getInstance().getConnection();
            return conn->executeQuery(sql, std::forward<Args>(args)...);
        }
        // 执行更新
        template <typename... Args>
        int executeUpdate(const std::string &sql, Args &&...args)
        {
            auto conn = http::db::DbConnectionPool::getInstance().getConnection();
            return conn->executeUpdate(sql, std::forward<Args>(args)...);
        }
    };

} // namespace http
#endif
