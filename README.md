# HTTPServer

基于 **standalone Asio + io_uring + C++20 协程** 的 HTTP/HTTPS 服务器框架，提供路由、中间件、会话管理、SSL、MySQL 连接池等能力，并附带两个完整应用示例（Markdown 笔记、五子棋 AI 对战）。

> 历史：本项目最初基于 [Muduo](https://github.com/chenshuo/muduo)，已重写为 standalone Asio + io_uring 后端，使用 C++20 协程（`co_await` / `awaitable`）替换原有 Reactor 回调。框架对外保留了与 muduo 时期一致的接口风格（如 `IOBuffer`、`LOG_*` 流式宏），原有应用代码可平滑迁移。

---

## 项目结构

```
HTTPServer/
├── HttpServer/                         # 核心框架（静态库 http_server）
│   ├── include/
│   │   ├── core/                       # 网络/日志基础设施
│   │   │   ├── Awaitable.h             # asio 协程别名（awaitable/use_awaitable/spawnLogged）
│   │   │   ├── IOBuffer.h              # 兼容 muduo::Buffer 的可读写缓冲
│   │   │   ├── Logging.h               # 流式日志宏 LOG_DEBUG/INFO/WARN/ERROR/FATAL
│   │   │   └── Ssl.h                   # asio::ssl 别名
│   │   ├── http/                       # HttpServer / HttpContext / HttpRequest / HttpResponse
│   │   ├── router/                     # 路由（静态 + 正则动态）
│   │   ├── session/                    # 会话（Cookie + 内存存储）
│   │   ├── middleware/                 # 中间件
│   │   │   ├── compress/               # Gzip 响应压缩
│   │   │   ├── cors/                   # CORS
│   │   │   ├── logging/                # 访问日志
│   │   │   └── ratelimit/              # 滑动窗口限流
│   │   ├── ssl/                        # SslConfig / SslContext / SslTypes
│   │   └── utils/                      # FileUtil / JsonUtil / MysqlUtil / db 连接池
│   └── src/
├── Apps/
│   ├── MarkdownServer/                 # Markdown 笔记应用（无 DB 依赖）
│   └── GomokuServer/                   # 五子棋 AI 对战应用（需 MySQL）
├── tests/                              # GoogleTest 单元测试 + 压测服务
│   ├── SessionTest.cpp
│   ├── HttpRequestTest.cpp
│   ├── HttpContextTest.cpp
│   ├── bench_server.cpp                # 最小化基准服务（无中间件）
│   └── ssl_smoke.cpp                   # SSL 冒烟测试
├── third_party/nlohmann/               # vendored nlohmann/json 单头文件
├── CMakeLists.txt
└── build/
```

---

## 架构概览

### 并发模型：one-loop-per-thread

- **1 个 acceptor `io_context`**：仅运行 `acceptLoop` 协程
- **N 个 worker `io_context`**（默认 = `setThreadNum(N)`）：每个独占一个线程，循环负载均衡分发新连接
- **1 个 `asio::thread_pool blockingPool_`**：把可能阻塞的业务处理（DB、文件 I/O）从 worker 线程切走，避免拖慢事件循环

每条连接是一个 C++20 协程（`awaitable<void>`），由 worker 的 io_context 调度。流程：

```
async_read_some → parseRequest → (post to blockingPool)
                                    └─ middleware.before → router.route → middleware.after
                                 ←──────── post back to worker ────────
async_write → keep-alive ? loop : shutdown
```

> 通过定义 `HTTPSERVER_INLINE_PIPELINE` 可让 handler 直接在 worker 线程内联执行，作为"框架开销上限"的基准测试入口（仅当所有 handler 都不会阻塞时安全）。

### io_uring 后端

CMakeLists 中通过编译开关启用 Asio 的 io_uring 后端：

```cmake
ASIO_HAS_IO_URING=1
ASIO_DISABLE_EPOLL=1
ASIO_NO_DEPRECATED=1
ASIO_STANDALONE=1
```

依赖 `liburing`（通过 pkg-config 检测）。

### muduo 兼容层

为了让上层代码不需要重写，`core/` 提供与 muduo 同名/同语义的设施：

| muduo 原物 | 本项目替代 | 位置 |
|------------|-----------|------|
| `muduo::net::Buffer` | `http::core::IOBuffer`（同样布局：prepend / readable / writable） | `core/IOBuffer.h` |
| `LOG_INFO << ...` 等 | 同名宏 | `core/Logging.h` |
| `EventLoop` + `TcpServer` | `asio::io_context` + 协程 acceptor | `http/HttpServer.cpp` |

---

## 核心 API

### HttpServer

```cpp
#include "http/HttpServer.h"

http::HttpServer server(8080, "MyServer");   // 第三参数 useSSL，默认 false
server.setThreadNum(4);

// 路由
server.Get("/api/info", [](const http::HttpRequest& req, http::HttpResponse* resp) {
    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(R"({"version":"2.0"})");
});

// 动态路由（路径参数）
server.addRoute(http::HttpRequest::kGet, "/api/notes/:name", handler);
// handler 内通过 req.getPathParameters("name") 获取

// 中间件
server.addMiddleware(std::make_shared<http::middleware::CorsMiddleware>());
server.addMiddleware(std::make_shared<http::middleware::RateLimitMiddleware>());

server.start();    // 阻塞，直到 stop()
```

### HttpRequest

```cpp
req.method()                      // kGet / kPost / kPut / kDelete ...
req.path()
req.getHeader("User-Agent")
req.getBody()
req.getClientIp()                 // 自动解析 X-Forwarded-For / X-Real-IP
req.getPathParameters("name")     // 动态路由参数
req.getQueryParameters("page")    // ?page=1
```

### HttpResponse

```cpp
resp->setStatusCode(http::HttpResponse::k200Ok);
resp->setContentType("application/json");
resp->setBody(R"({"key":"value"})");
resp->addHeader("X-Custom", "v");
resp->setCloseConnection(false);  // Keep-Alive
```

### 会话

```cpp
auto sm = std::make_unique<http::session::SessionManager>(
            std::make_unique<http::session::MemorySessionStorage>());
server.setSessionManager(std::move(sm));

// handler 内
auto s = server.getSessionManager()->getSession(req, resp);
s->setValue("userId", "123");
s->setValue("isLoggedIn", "true");
```

### SSL / TLS

```cpp
ssl::SslConfig cfg;
cfg.setCertificateFile("server.crt");
cfg.setPrivateKeyFile("server.key");
cfg.setProtocolVersion(ssl::SSLVersion::TLS_1_3);

http::HttpServer server(443, "HttpsServer", /*useSSL=*/true);
server.setSslConfig(cfg);            // 必须在 start() 之前
server.start();
```

每条 SSL 连接走 `asio::ssl::stream<tcp::socket>`，先 `async_handshake` 再进入与明文相同的请求循环。

---

## 中间件系统

| 中间件 | 功能 | 配置类 |
|--------|------|--------|
| `CorsMiddleware` | CORS 预检处理、响应头注入 | `CorsConfig` |
| `CompressMiddleware` | Gzip 响应压缩（zlib） | `CompressConfig` |
| `RateLimitMiddleware` | 基于 IP 的滑动窗口限流 | `RateLimitConfig` |
| `LoggingMiddleware` | 结构化访问日志（写文件） | 日志文件路径 |

### 接口

```cpp
class Middleware {
public:
    virtual ~Middleware() = default;
    virtual void before(HttpRequest&)  = 0;   // 请求前
    virtual void after(HttpResponse&)  = 0;   // 响应后
};
```

### 执行顺序

`before` 按注册顺序执行，`after` 按**逆序**执行（洋葱模型）：

```
请求 → Logging.before → RateLimit.before → Cors.before → Compress.before → Handler
响应 ←── Logging.after ── RateLimit.after ── Cors.after ── Compress.after ──┘
```

### 自定义

```cpp
class MyMw : public http::middleware::Middleware {
public:
    void before(http::HttpRequest& req) override { /* ... */ }
    void after(http::HttpResponse& resp) override { /* ... */ }
};
server.addMiddleware(std::make_shared<MyMw>());
```

---

## 应用示例

### MarkdownServer — Markdown 笔记

无数据库依赖，文件系统存储。

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/notes` | 列出所有笔记 |
| `GET` | `/api/notes/:name` | 获取指定笔记 |
| `POST` | `/api/notes` | 创建笔记（JSON body） |
| `PUT` | `/api/notes/:name` | 更新笔记 |
| `DELETE` | `/api/notes/:name` | 删除笔记 |
| `GET` | `/`、`/editor.html` | Web 页面 |

- 中间件链：`Logging → Cors → RateLimit → Compress`
- 日志：`Apps/MarkdownServer/access.log`
- 笔记目录：`Apps/MarkdownServer/notes/`

### GomokuServer — 五子棋 AI 对战

带用户系统（MySQL）与 AI 算法。

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/`、`/entry` | 登录/注册入口 |
| `POST` | `/login` | 登录 |
| `POST` | `/register` | 注册 |
| `POST` | `/user/logout` | 登出 |
| `GET` | `/menu` | 游戏菜单（需登录） |
| `GET` | `/aiBot/start` | 开始 AI 对局 |
| `POST` | `/aiBot/move` | 落子 |
| `GET` | `/aiBot/restart` | 重开 |
| `GET` | `/backend` | 后台统计页面 |
| `GET` | `/backend_data` | 在线/注册数等（JSON） |

- 中间件链：`Logging → RateLimit → Cors → Compress`
- AI 棋盘 15×15，玩家执黑先手，AI 执白；策略：检测必杀/必防点 → 评估威胁 → 优先靠近已有棋子位置
- 资源路径配置：`Apps/GomokuServer/resource_path.json`
- 默认 MySQL 连接：`tcp://127.0.0.1:3306`，用户 `root`/`root`，库名 `Gomoku`，池大小 10（见 `GomokuServer.cpp`）

#### MySQL 初始化

```bash
mysql -uroot -p < Apps/GomokuServer/database.sql
# 或新版 MySQL（8.x）：
mysql -uroot -p < Apps/GomokuServer/database-8.x.sql
```

如需让 MySQL 监听非本地：

```bash
bash Apps/GomokuServer/config.sh
```

---

## 编译构建

### 依赖

| 必需 | 说明 |
|------|------|
| C++20 编译器 | GCC 11+ / Clang 14+ |
| CMake | ≥ 3.16 |
| **standalone Asio** | 头文件，安装到 `/usr/include` 或 `/usr/local/include` |
| **liburing** | pkg-config 可发现（`liburing-dev`） |
| OpenSSL | `ssl` + `crypto` |
| zlib | `ZLIB::ZLIB` |
| GoogleTest | 单元测试 |

| 可选 | 用途 |
|------|------|
| nlohmann/json | GomokuServer 必需。未在系统中找到时自动 fallback 到 `third_party/nlohmann/json.hpp` |
| MySQL Connector/C++ | 启用 DB 连接池与 GomokuServer。未找到时框架仍可编译，DB 层 / GomokuServer 自动跳过 |

CMake 会输出 `HAVE_MYSQL` 状态行，未找到 MySQL 时不会失败，仅跳过 `gomoku_server` 目标。

### 构建

```bash
cd /home/ros/lib/HTTPServer
mkdir -p build && cd build
cmake ..
cmake --build . -j

# 或单独编译
cmake --build . --target markdown_server
cmake --build . --target gomoku_server         # 需要 MySQL
cmake --build . --target bench_server          # 基准
cmake --build . --target session_test          # GoogleTest
```

### 产物

```
build/markdown_server
build/gomoku_server          # 若 HAVE_MYSQL=ON
build/bench_server
build/session_test
build/http_request_test
build/http_context_test
build/ssl_smoke              # （由 tests/CMakeLists 决定，此处随项目演进）
```

---

## 运行

```bash
# MarkdownServer（默认端口 8080）
./build/markdown_server -p 9000

# GomokuServer（默认端口 80，需要 MySQL 已初始化）
sudo ./build/gomoku_server -p 80
# 或非特权端口
./build/gomoku_server -p 8000

# 基准服务（无中间件，仅 /hello /json /echo）
./build/bench_server 9000 4
```

### 测试

```bash
cd build
ctest --output-on-failure
# 或单独跑
./session_test
./http_request_test
./http_context_test
```

---

## 配置示例

### resource_path.json（GomokuServer）

```json
{
    "Backend":       "Apps/GomokuServer/resource/Backend.html",
    "ChessGameVsAi": "Apps/GomokuServer/resource/ChessGameVsAi.html",
    "entry":         "Apps/GomokuServer/resource/entry.html",
    "menu":          "Apps/GomokuServer/resource/menu.html",
    "NotFound":      "Apps/GomokuServer/resource/NotFound.html"
}
```

由 `ResourceManager` 单例在服务启动时加载，运行时供 handler 查找静态资源路径。

### 中间件配置

```cpp
// 限流（默认 100 请求 / 60 秒）
http::middleware::RateLimitConfig rl;
rl.maxRequests   = 200;
rl.windowSeconds = 60;
server.addMiddleware(std::make_shared<http::middleware::RateLimitMiddleware>(rl));

// 压缩（默认阈值 256 字节，级别 6）
http::middleware::CompressConfig cz;
cz.minBodySize       = 512;
cz.compressionLevel  = 9;
server.addMiddleware(std::make_shared<http::middleware::CompressMiddleware>(cz));

// 访问日志
server.addMiddleware(
    std::make_shared<http::middleware::LoggingMiddleware>("logs/access.log"));

// CORS
http::middleware::CorsConfig cors;
cors.allowedOrigins   = {"*"};
cors.allowCredentials = false;
cors.maxAge           = 3600;
server.addMiddleware(std::make_shared<http::middleware::CorsMiddleware>(cors));
```

---

## 扩展指南

### 新增路由

```cpp
// Lambda 形式
server.Get("/api/status", [](const http::HttpRequest&, http::HttpResponse* resp) {
    resp->setContentType("application/json");
    resp->setBody(R"({"status":"ok"})");
});

// 类形式
class StatusHandler : public http::router::RouterHandler {
public:
    void handle(const http::HttpRequest&, http::HttpResponse* resp) override { /* ... */ }
};
server.Get("/api/status", std::make_shared<StatusHandler>());
```

### 新增中间件

1. 继承 `http::middleware::Middleware`，实现 `before/after`
2. `server.addMiddleware(std::make_shared<MyMw>())`

### 调整日志级别

```cpp
http::core::log::setLevel(http::core::log::Level::DEBUG);
```

应用模板默认设为 `WARN`。

---

## 技术栈

- **网络**：standalone Asio + **io_uring**（`ASIO_HAS_IO_URING=1`），one-loop-per-thread
- **并发**：C++20 协程（`co_await`/`awaitable`），独立 blocking thread_pool 兜底阻塞 handler
- **语言**：C++20（CMake `CXX_STANDARD 20`）
- **TLS**：`asio::ssl` + OpenSSL（TLS 1.0–1.3）
- **压缩**：zlib（gzip）
- **数据库**：MySQL Connector/C++（带连接池），可选依赖
- **JSON**：nlohmann/json（系统安装或 vendored）
- **测试**：GoogleTest + CTest
