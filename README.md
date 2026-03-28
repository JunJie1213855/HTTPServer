# HTTPServer

基于 [Muduo](https://github.com/chenshuo/muduo) 网络库构建的 C++ HTTP 服务器框架，提供路由、中间件、会话管理、SSL、数据库连接池等能力，并附带两个完整应用示例。

---

## 项目结构

```
HTTPServer/
├── HttpServer/                        # 核心框架
│   ├── include/
│   │   ├── http/                      # HTTP 协议（Request/Response/Context/Server）
│   │   ├── router/                    # 路由系统
│   │   ├── session/                   # 会话管理
│   │   ├── middleware/                # 中间件
│   │   │   ├── cors/                  # CORS 中间件
│   │   │   ├── compress/              # Gzip 压缩中间件
│   │   │   ├── ratelimit/             # 限流中间件
│   │   │   └── logging/               # 访问日志中间件
│   │   ├── ssl/                       # SSL/TLS 支持
│   │   └── utils/                     # 工具类（FileUtil, JsonUtil, MysqlUtil, DB连接池）
│   └── src/
├── Apps/
│   ├── MarkdownServer/                # Markdown 笔记应用
│   └── GomokuServer/                  # 五子棋 AI 对战应用
├── CMakeLists.txt
└── build/                             # 编译输出目录
```

---

## 核心框架

### HttpServer (`http::HttpServer`)

事件驱动的 HTTP 服务器，封装 Muduo TcpServer：

```cpp
http::HttpServer server(8080, "MyServer");
server.setThreadNum(4);

// 注册路由
server.Get("/api/info", [](const http::HttpRequest& req, http::HttpResponse* resp) {
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(R"({"version": "1.0"})");
});

// 添加中间件
server.addMiddleware(std::make_shared<http::middleware::CorsMiddleware>());
server.addMiddleware(std::make_shared<http::middleware::RateLimitMiddleware>());

server.start();
```

### 路由 (`http::router::Router`)

支持**静态路由**和**动态路由**（正则参数）：

```cpp
// 静态路由
server.Get("/api/notes", handler);

// 动态路由（:name 为路径参数）
server.addRoute(http::HttpRequest::kGet, "/api/notes/:name", handler);

// 在 handler 中获取参数
std::string name = req.getPathParameters("name");
```

### 请求与响应

**HttpRequest**:
```cpp
req.method()                    // kGet, kPost, kPut, kDelete ...
req.path()                      // URL 路径
req.getHeader("User-Agent")     // 请求头
req.getBody()                   // 请求体
req.getClientIp()               // 客户端 IP（支持 X-Forwarded-For / X-Real-IP）
req.getPathParameters("name")   // 动态路由参数
req.getQueryParameters("page")   // 查询参数 ?page=1
```

**HttpResponse**:
```cpp
resp->setStatusCode(http::HttpResponse::k200Ok);
resp->setContentType("application/json");
resp->setBody(R"({"key": "value"})");
resp->addHeader("X-Custom-Header", "value");
resp->setCloseConnection(false);  // Keep-Alive
```

### 会话管理 (`http::session::SessionManager`)

基于 Cookie 的会话，支持内存存储：

```cpp
auto sessionManager = std::make_unique<http::session::SessionManager>(
    std::make_unique<http::session::MemorySessionStorage>());
server.setSessionManager(std::move(sessionManager));

// 在 handler 中使用
auto session = server.getSessionManager()->getSession(req, resp);
session->setValue("userId", "123");
session->setValue("isLoggedIn", "true");
std::string userId = session->getValue("userId");
```

---

## 中间件系统

### 可用中间件

| 中间件 | 功能 | 配置类 |
|--------|------|--------|
| `CorsMiddleware` | CORS 预检请求处理、响应头注入 | `CorsConfig` |
| `CompressMiddleware` | Gzip 响应压缩（zlib） | `CompressConfig` |
| `RateLimitMiddleware` | 基于 IP 的滑动窗口限流 | `RateLimitConfig` |
| `LoggingMiddleware` | 结构化访问日志（写入 .log 文件） | 日志文件路径 |

### 中间件接口

```cpp
class Middleware {
public:
    virtual ~Middleware() = default;
    virtual void before(HttpRequest& request) = 0;   // 请求预处理
    virtual void after(HttpResponse& response) = 0;  // 响应后处理
};
```

### 执行顺序

```
请求 → Logging(before) → RateLimit(before) → Cors(before) → Compress(before)
                                                      → Handler
       Compress(after) ← Cors(after) ← RateLimit(after) ← Logging(after) → 响应
```

- `before()` 按注册顺序执行
- `after()` 按**逆序**执行，保证嵌套正确

### 添加自定义中间件

1. 继承 `Middleware` 基类
2. 在 `before()` 实现请求拦截逻辑
3. 在 `after()` 实现响应拦截逻辑
4. 通过 `server.addMiddleware(std::make_shared<MyMiddleware>())` 注册

---

## 应用示例

### MarkdownServer — Markdown 笔记应用

笔记的增删改查，附带 Web 管理界面：

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/notes` | 列出所有笔记 |
| `GET` | `/api/notes/:name` | 获取指定笔记内容 |
| `POST` | `/api/notes` | 创建新笔记（JSON body） |
| `PUT` | `/api/notes/:name` | 更新笔记 |
| `DELETE` | `/api/notes/:name` | 删除笔记 |
| `GET` | `/` | Web 首页 |
| `GET` | `/editor.html` | 编辑器页面 |

```
中间件链: Logging → Cors → RateLimit → Compress
访问日志: Apps/MarkdownServer/access.log
笔记目录: Apps/MarkdownServer/notes/
```

### GomokuServer — 五子棋 AI 对战

用户注册/登录后与 AI 对战五子棋，使用 MySQL 存储用户数据：

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/entry` | 登录/注册页面 |
| `POST` | `/login` | 用户登录 |
| `POST` | `/register` | 用户注册 |
| `POST` | `/user/logout` | 登出 |
| `GET` | `/menu` | 游戏菜单（需登录） |
| `GET` | `/aiBot/start` | 开始 AI 对局 |
| `POST` | `/aiBot/move` | 落子 |
| `GET` | `/aiBot/restart` | 重新开始 |
| `GET` | `/backend` | 后台统计页面 |
| `GET` | `/backend_data` | 在线人数等统计数据（JSON） |

```
中间件链: Logging → RateLimit → Cors → Compress
访问日志: Apps/GomokuServer/access.log
资源配置: Apps/GomokuServer/resource_path.json
```

**AI 算法** (`AiGame`):
- 棋盘 15×15
- 玩家执黑（先手），AI 执白
- 胜利条件：任意方向连续 5 子
- AI 策略：检测必杀/必防点 → 评估威胁 → 优先靠近已有棋子位置

**数据库要求**（MySQL）:
```sql
CREATE DATABASE Gomoku;
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255) UNIQUE,
    password VARCHAR(255)
);
```

---

## 编译构建

### 依赖

- Muduo（`muduo_net`, `muduo_base`）
- OpenSSL（`ssl`, `crypto`）
- ZLIB
- MySQL Connector/C++（`mysqlcppconn`, `mysqlclient`）
- nlohmann/json（头文件）
- C++17 编译器

### 构建

```bash
cd /root/project/HTTPServer
mkdir -p build && cd build
cmake ..
make          # 同时编译两个应用

# 或单独编译
make markdown_server   # MarkdownServer
make gomoku_server     # GomokuServer（需要 MySQL）
```

### 构建产物

```
build/markdown_server   # MarkdownServer 可执行文件
build/gomoku_server     # GomokuServer 可执行文件
```

---

## 运行

```bash
# MarkdownServer（默认端口 8080）
./markdown_server -p 9000

# GomokuServer（默认端口 80，需 MySQL）
./gomoku_server -p 8080
```

---

## 配置说明

### resource_path.json（GomokuServer）

```json
{
    "Backend":      "Apps/GomokuServer/resource/Backend.html",
    "ChessGameVsAi": "Apps/GomokuServer/resource/ChessGameVsAi.html",
    "entry":       "Apps/GomokuServer/resource/entry.html",
    "menu":        "Apps/GomokuServer/resource/menu.html",
    "NotFound":    "Apps/GomokuServer/resource/NotFound.html"
}
```

由 `ResourceManager` 单例在服务启动时加载，用于运行时查找 HTML 静态资源路径。

### 中间件配置

**限流**（默认 100 请求/60 秒）:
```cpp
http::middleware::RateLimitConfig config;
config.maxRequests = 200;
config.windowSeconds = 60;
auto rateLimit = std::make_shared<http::middleware::RateLimitMiddleware>(config);
```

**压缩**（默认阈值 256 字节，级别 6）:
```cpp
http::middleware::CompressConfig config;
config.minBodySize = 512;
config.compressionLevel = 9;
auto compress = std::make_shared<http::middleware::CompressMiddleware>(config);
```

**日志**:
```cpp
auto logging = std::make_shared<http::middleware::LoggingMiddleware>("logs/access.log");
```

**CORS**:
```cpp
http::middleware::CorsConfig config;
config.allowedOrigins = {"*"};
config.allowCredentials = false;
config.maxAge = 3600;
auto cors = std::make_shared<http::middleware::CorsMiddleware>(config);
```

---

## 扩展指南

### 添加新中间件

```cpp
// 1. 头文件 HttpServer/include/middleware/myext/MyExtMiddleware.h
class MyExtMiddleware : public Middleware {
public:
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;
};

// 2. 实现 HttpServer/src/middleware/myext/MyExtMiddleware.cpp

// 3. 在应用中注册
server.addMiddleware(std::make_shared<MyExtMiddleware>());
```

### 添加新路由

```cpp
// Lambda 方式（简单路由）
server.Get("/api/status", [](const http::HttpRequest& req, http::HttpResponse* resp) {
    resp->setBody(R"({"status": "ok"})");
    resp->setContentType("application/json");
});

// 类方式（复杂逻辑）
class StatusHandler : public http::router::RouterHandler {
public:
    explicit StatusHandler(Server* srv) : server_(srv) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
};
server.Get("/api/status", std::make_shared<StatusHandler>(this));
```

---

## 技术栈

- **网络**: Muduo（Reactor 模式，多线程事件循环）
- **语言**: C++17
- **压缩**: Zlib（gzip）
- **加密**: OpenSSL（TLS 1.0 - 1.3）
- **数据库**: MySQL + MySQL Connector/C++
- **JSON**: nlohmann/json
