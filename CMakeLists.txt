cmake_minimum_required(VERSION 3.10)
project(HTTPServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)

# 启用测试
enable_testing()

# 添加头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${PROJECT_SOURCE_DIR}/Apps/MarkdownServer/include
    ${PROJECT_SOURCE_DIR}/Apps/GomokuServer/include
    /usr/include/mysql-cppconn-8
    /usr/include/mysql
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# 查找 MySQL 库
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# HttpServer 核心源文件
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp"
)

# 应用源文件
file(GLOB_RECURSE MARKDOWN_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/Apps/MarkdownServer/src/*.cpp"
)

file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/Apps/GomokuServer/src/*.cpp"
)

# 公共链接库
set(COMMON_LIBS
    pthread
    muduo_net
    muduo_base
    ssl
    crypto
    ${ZLIB_LIBRARIES}
)

# ==================== MarkdownServer ====================
add_executable(markdown_server
    ${HTTP_SERVER_SRC}
    ${MARKDOWN_SERVER_SRC}
)
target_link_libraries(markdown_server
    ${COMMON_LIBS}
    mysqlcppconn
    mysqlclient
)

# ==================== GomokuServer ====================
add_executable(gomoku_server
    ${HTTP_SERVER_SRC}
    ${GOMOKU_SERVER_SRC}
)
target_link_libraries(gomoku_server
    ${COMMON_LIBS}
    mysqlcppconn
    mysqlclient
)

# 打印调试信息
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Markdown Server sources: ${MARKDOWN_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")

# ==================== Session Tests ====================
add_executable(session_test
    tests/SessionTest.cpp
    HttpServer/src/session/Session.cpp
    HttpServer/src/session/SessionManager.cpp
    HttpServer/src/session/SessionStorage.cpp
    HttpServer/src/http/HttpRequest.cpp
    HttpServer/src/http/HttpResponse.cpp
)
target_link_libraries(session_test
    GTest::GTest
    GTest::Main
    pthread
    muduo_net
    muduo_base
)
target_include_directories(session_test PRIVATE
    ${PROJECT_SOURCE_DIR}/HttpServer/include
)
add_test(NAME session_test COMMAND session_test)
