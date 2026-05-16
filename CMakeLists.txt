cmake_minimum_required(VERSION 3.16)
project(HTTPServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# --- Threads --------------------------------------------------------------
find_package(Threads REQUIRED)

# --- Standalone Asio ------------------------------------------------------
find_path(ASIO_INCLUDE_DIR asio.hpp
    PATHS /usr/include /usr/local/include
    REQUIRED
)
message(STATUS "Asio include: ${ASIO_INCLUDE_DIR}")

# --- liburing (io_uring) --------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED IMPORTED_TARGET liburing)

# --- ZLIB ----------------------------------------------------------------
find_package(ZLIB REQUIRED)

# --- OpenSSL (for asio::ssl) ---------------------------------------------
find_package(OpenSSL REQUIRED)

# --- GoogleTest ----------------------------------------------------------
find_package(GTest REQUIRED)
enable_testing()

# --- nlohmann/json -------------------------------------------------------
find_path(NLOHMANN_JSON_INCLUDE nlohmann/json.hpp
    PATHS /usr/include /usr/local/include /home/ros/miniconda3/include
)
if(NOT NLOHMANN_JSON_INCLUDE)
    # Fall back to the vendored single-header bundle in third_party/.
    if(EXISTS "${PROJECT_SOURCE_DIR}/third_party/nlohmann/json.hpp")
        set(NLOHMANN_JSON_INCLUDE "${PROJECT_SOURCE_DIR}/third_party"
            CACHE PATH "Vendored nlohmann/json single-header location" FORCE)
        message(STATUS "Using vendored nlohmann/json at ${NLOHMANN_JSON_INCLUDE}")
    else()
        message(WARNING "nlohmann/json.hpp not found - GomokuServer cannot be built")
    endif()
endif()

# --- MySQL Connector/C++ (optional) --------------------------------------
find_path(MYSQL_CONCPP_INCLUDE_DIR cppconn/connection.h
    PATHS
        /usr/local/mysql/connector-c++-9.5.0/include/jdbc
        /usr/include/mysql-cppconn-8
        /usr/include
        /usr/local/include
)
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn
    PATHS
        /usr/local/mysql/connector-c++-9.5.0/lib64
        /usr/local/mysql/connector-c++-9.5.0/lib64/debug
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
)
find_library(MYSQLCLIENT_LIBRARY
    NAMES mysqlclient
    PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib
)

set(HAVE_MYSQL OFF)
if(MYSQL_CONCPP_INCLUDE_DIR AND MYSQLCPPCONN_LIBRARY)
    set(HAVE_MYSQL ON)
    message(STATUS "MySQL Connector/C++ found at ${MYSQL_CONCPP_INCLUDE_DIR}")
    message(STATUS "  libmysqlcppconn: ${MYSQLCPPCONN_LIBRARY}")
    message(STATUS "  libmysqlclient:  ${MYSQLCLIENT_LIBRARY}")
else()
    message(STATUS "MySQL Connector/C++ NOT found - DB layer and GomokuServer disabled")
endif()

# --- Project-wide compile flags ------------------------------------------
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
add_compile_definitions(
    ASIO_HAS_IO_URING=1
    ASIO_DISABLE_EPOLL=1
    ASIO_NO_DEPRECATED=1
    ASIO_STANDALONE=1
)

# --- HttpServer framework lib --------------------------------------------
set(HTTP_SERVER_SRC
    HttpServer/src/http/HttpContext.cpp
    HttpServer/src/http/HttpRequest.cpp
    HttpServer/src/http/HttpResponse.cpp
    HttpServer/src/http/HttpServer.cpp
    HttpServer/src/middleware/MiddlewareChain.cpp
    HttpServer/src/middleware/compress/CompressMiddleware.cpp
    HttpServer/src/middleware/cors/CorsMiddleware.cpp
    HttpServer/src/middleware/logging/LoggingMiddleware.cpp
    HttpServer/src/middleware/ratelimit/RateLimitMiddleware.cpp
    HttpServer/src/router/Router.cpp
    HttpServer/src/session/Session.cpp
    HttpServer/src/session/SessionManager.cpp
    HttpServer/src/session/SessionStorage.cpp
    HttpServer/src/ssl/SslContext.cpp
)
if(HAVE_MYSQL)
    list(APPEND HTTP_SERVER_SRC
        HttpServer/src/utils/db/DbConnection.cpp
        HttpServer/src/utils/db/DbConnectionPool.cpp
    )
endif()

add_library(http_server STATIC ${HTTP_SERVER_SRC})
target_include_directories(http_server PUBLIC
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${ASIO_INCLUDE_DIR}
)
if(NLOHMANN_JSON_INCLUDE)
    target_include_directories(http_server PUBLIC ${NLOHMANN_JSON_INCLUDE})
endif()
target_link_libraries(http_server PUBLIC
    Threads::Threads
    PkgConfig::URING
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(HAVE_MYSQL)
    target_include_directories(http_server PUBLIC ${MYSQL_CONCPP_INCLUDE_DIR})
    target_link_libraries(http_server PUBLIC ${MYSQLCPPCONN_LIBRARY})
    if(MYSQLCLIENT_LIBRARY)
        target_link_libraries(http_server PUBLIC ${MYSQLCLIENT_LIBRARY})
    endif()
    target_compile_definitions(http_server PUBLIC HTTPSERVER_HAVE_MYSQL=1)
endif()

# ==================== MarkdownServer ====================
file(GLOB MARKDOWN_SERVER_SRC "${PROJECT_SOURCE_DIR}/Apps/MarkdownServer/src/*.cpp")
add_executable(markdown_server ${MARKDOWN_SERVER_SRC})
target_include_directories(markdown_server PRIVATE
    ${PROJECT_SOURCE_DIR}/Apps/MarkdownServer/include
)
target_link_libraries(markdown_server PRIVATE http_server)

# ==================== GomokuServer (requires MySQL + nlohmann json) ====================
if(HAVE_MYSQL AND NLOHMANN_JSON_INCLUDE)
    file(GLOB_RECURSE GOMOKU_SERVER_SRC "${PROJECT_SOURCE_DIR}/Apps/GomokuServer/src/*.cpp")
    add_executable(gomoku_server ${GOMOKU_SERVER_SRC})
    target_include_directories(gomoku_server PRIVATE
        ${PROJECT_SOURCE_DIR}/Apps/GomokuServer/include
    )
    target_link_libraries(gomoku_server PRIVATE http_server)
endif()

# ==================== Tests ====================
add_executable(session_test tests/SessionTest.cpp)
target_link_libraries(session_test PRIVATE http_server GTest::GTest GTest::Main)
add_test(NAME session_test COMMAND session_test)

add_executable(http_request_test tests/HttpRequestTest.cpp)
target_link_libraries(http_request_test PRIVATE http_server GTest::GTest GTest::Main)
add_test(NAME http_request_test COMMAND http_request_test)

add_executable(http_context_test tests/HttpContextTest.cpp)
target_link_libraries(http_context_test PRIVATE http_server GTest::GTest GTest::Main)
add_test(NAME http_context_test COMMAND http_context_test)
