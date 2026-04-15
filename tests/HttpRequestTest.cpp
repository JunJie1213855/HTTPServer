#include <gtest/gtest.h>
#include "http/HttpRequest.h"
#include <cstring>

using namespace http;

class HttpRequestTest : public ::testing::Test {
protected:
    HttpRequest req;
};

// ==================== Method Parsing Tests ====================

TEST_F(HttpRequestTest, SetMethod_ParsesGet) {
    EXPECT_TRUE(req.setMethod("GET", "GET" + 3));
    EXPECT_EQ(req.method(), HttpRequest::kGet);
}

TEST_F(HttpRequestTest, SetMethod_ParsesPost) {
    EXPECT_TRUE(req.setMethod("POST", "POST" + 4));
    EXPECT_EQ(req.method(), HttpRequest::kPost);
}

TEST_F(HttpRequestTest, SetMethod_ParsesPut) {
    EXPECT_TRUE(req.setMethod("PUT", "PUT" + 3));
    EXPECT_EQ(req.method(), HttpRequest::kPut);
}

TEST_F(HttpRequestTest, SetMethod_ParsesDelete) {
    EXPECT_TRUE(req.setMethod("DELETE", "DELETE" + 6));
    EXPECT_EQ(req.method(), HttpRequest::kDelete);
}

TEST_F(HttpRequestTest, SetMethod_ParsesOptions) {
    EXPECT_TRUE(req.setMethod("OPTIONS", "OPTIONS" + 7));
    EXPECT_EQ(req.method(), HttpRequest::kOptions);
}

TEST_F(HttpRequestTest, SetMethod_ParsesHead) {
    // Note: HEAD is not implemented in HttpRequest::setMethod
    EXPECT_FALSE(req.setMethod("HEAD", "HEAD" + 4));
    EXPECT_EQ(req.method(), HttpRequest::kInvalid);
}

TEST_F(HttpRequestTest, SetMethod_ParsesInvalid) {
    EXPECT_FALSE(req.setMethod("INVALID", "INVALID" + 7));
    EXPECT_EQ(req.method(), HttpRequest::kInvalid);
}

TEST_F(HttpRequestTest, SetMethod_EmptyMethod) {
    // Empty method string should fail
    EXPECT_FALSE(req.setMethod("", ""));  // Empty string
    EXPECT_EQ(req.method(), HttpRequest::kInvalid);
}

// ==================== Path Tests ====================

TEST_F(HttpRequestTest, SetPath_ExtractsSimplePath) {
    req.setPath("/api/users", "/api/users" + 10);
    EXPECT_EQ(req.path(), "/api/users");
}

TEST_F(HttpRequestTest, SetPath_ExtractsRootPath) {
    req.setPath("/", "/" + 1);
    EXPECT_EQ(req.path(), "/");
}

TEST_F(HttpRequestTest, SetPath_ExtractsDeepPath) {
    req.setPath("/api/v1/users/123", "/api/v1/users/123" + 17);
    EXPECT_EQ(req.path(), "/api/v1/users/123");
}

TEST_F(HttpRequestTest, Path_DefaultsToEmpty) {
    EXPECT_EQ(req.path(), "");
}

// ==================== Query Parameters Tests ====================

TEST_F(HttpRequestTest, SetQueryParameters_ParsesSimple) {
    req.setQueryParameters("a=1&b=2", "a=1&b=2" + 7);
    EXPECT_EQ(req.getQueryParameters("a"), "1");
    EXPECT_EQ(req.getQueryParameters("b"), "2");
}

TEST_F(HttpRequestTest, SetQueryParameters_ParsesSingleParam) {
    req.setQueryParameters("page=5", "page=5" + 6);
    EXPECT_EQ(req.getQueryParameters("page"), "5");
}

TEST_F(HttpRequestTest, SetQueryParameters_ParsesEmptyValue) {
    req.setQueryParameters("key=", "key=" + 4);
    EXPECT_EQ(req.getQueryParameters("key"), "");
}

TEST_F(HttpRequestTest, GetQueryParameters_ReturnsEmptyForMissing) {
    req.setQueryParameters("a=1", "a=1" + 3);
    EXPECT_EQ(req.getQueryParameters("nonexistent"), "");
}

TEST_F(HttpRequestTest, SetQueryParameters_ParsesMultipleParams) {
    req.setQueryParameters("a=1&b=2&c=3", "a=1&b=2&c=3" + 11);
    EXPECT_EQ(req.getQueryParameters("a"), "1");
    EXPECT_EQ(req.getQueryParameters("b"), "2");
    EXPECT_EQ(req.getQueryParameters("c"), "3");
}

// ==================== Headers Tests ====================

TEST_F(HttpRequestTest, AddHeader_ParsesSimpleHeader) {
    std::string header = "Host: localhost";
    req.addHeader(header.c_str(),
                   header.c_str() + 4,  // points to "Host"
                   header.c_str() + header.size());  // points to end
    EXPECT_EQ(req.getHeader("Host"), "localhost");
}

TEST_F(HttpRequestTest, AddHeader_TrimsWhitespace) {
    std::string header = "Content-Type:   application/json  ";
    req.addHeader(header.c_str(),
                   header.c_str() + 12,
                   header.c_str() + header.size());
    EXPECT_EQ(req.getHeader("Content-Type"), "application/json");
}

TEST_F(HttpRequestTest, GetHeader_CaseSensitive) {
    // Note: getHeader is case-sensitive (uses std::map find)
    std::string header = "Content-Type: application/json";
    req.addHeader(header.c_str(),
                   header.c_str() + 12,
                   header.c_str() + header.size());
    EXPECT_EQ(req.getHeader("Content-Type"), "application/json");
    EXPECT_EQ(req.getHeader("content-type"), "");  // Case sensitive - not found
    EXPECT_EQ(req.getHeader("CONTENT-TYPE"), "");   // Case sensitive - not found
}

TEST_F(HttpRequestTest, GetHeader_ReturnsEmptyForMissing) {
    EXPECT_EQ(req.getHeader("X-Custom-Header"), "");
}

TEST_F(HttpRequestTest, AddHeader_MultipleHeaders) {
    std::string header1 = "Host: localhost";
    std::string header2 = "Content-Type: application/json";

    req.addHeader(header1.c_str(),
                   header1.c_str() + 4,
                   header1.c_str() + header1.size());
    req.addHeader(header2.c_str(),
                   header2.c_str() + 12,
                   header2.c_str() + header2.size());

    EXPECT_EQ(req.getHeader("Host"), "localhost");
    EXPECT_EQ(req.getHeader("Content-Type"), "application/json");
}

// ==================== Body Tests ====================

TEST_F(HttpRequestTest, SetBody_SetsContent) {
    req.setBody("Hello World");
    EXPECT_EQ(req.getBody(), "Hello World");
}

TEST_F(HttpRequestTest, SetBody_WithPointer) {
    const char* body = "Test Body";
    req.setBody(body, body + 9);
    EXPECT_EQ(req.getBody(), "Test Body");
}

TEST_F(HttpRequestTest, SetBody_EmptyBody) {
    req.setBody("");
    EXPECT_EQ(req.getBody(), "");
}

TEST_F(HttpRequestTest, GetBody_DefaultsToEmpty) {
    EXPECT_EQ(req.getBody(), "");
}

// ==================== ContentLength Tests ====================

TEST_F(HttpRequestTest, SetContentLength_SetsLength) {
    req.setContentLength(1024);
    EXPECT_EQ(req.contentLength(), 1024);
}

TEST_F(HttpRequestTest, SetContentLength_Zero) {
    req.setContentLength(0);
    EXPECT_EQ(req.contentLength(), 0);
}

TEST_F(HttpRequestTest, ContentLength_DefaultsToZero) {
    EXPECT_EQ(req.contentLength(), 0);
}

// ==================== Path Parameters Tests ====================

TEST_F(HttpRequestTest, SetPathParameters_StoresValue) {
    req.setPathParameters("id", "123");
    EXPECT_EQ(req.getPathParameters("id"), "123");
}

TEST_F(HttpRequestTest, GetPathParameters_ReturnsEmptyForMissing) {
    EXPECT_EQ(req.getPathParameters("nonexistent"), "");
}

TEST_F(HttpRequestTest, SetPathParameters_MultipleParams) {
    req.setPathParameters("collection", "users");
    req.setPathParameters("id", "456");
    EXPECT_EQ(req.getPathParameters("collection"), "users");
    EXPECT_EQ(req.getPathParameters("id"), "456");
}

// ==================== Version Tests ====================

TEST_F(HttpRequestTest, SetVersion_SetsVersion) {
    req.setVersion("HTTP/1.1");
    EXPECT_EQ(req.getVersion(), "HTTP/1.1");
}

TEST_F(HttpRequestTest, SetVersion_HTTP10) {
    req.setVersion("HTTP/1.0");
    EXPECT_EQ(req.getVersion(), "HTTP/1.0");
}

TEST_F(HttpRequestTest, GetVersion_DefaultsToUnknown) {
    EXPECT_EQ(req.getVersion(), "Unknown");
}

// ==================== ClientIp Tests ====================

TEST_F(HttpRequestTest, SetClientIp_SetsIp) {
    req.setClientIp("192.168.1.1");
    EXPECT_EQ(req.getClientIp(), "192.168.1.1");
}

TEST_F(HttpRequestTest, GetClientIp_DefaultsToEmpty) {
    EXPECT_EQ(req.getClientIp(), "");
}

// ==================== Swap Tests ====================

TEST_F(HttpRequestTest, Swap_ExchangesData) {
    req.setMethod("GET", "GET" + 3);
    req.setPath("/api", "/api" + 4);
    req.setVersion("HTTP/1.1");

    HttpRequest other;
    other.setMethod("POST", "POST" + 4);
    other.setPath("/other", "/other" + 6);
    other.setVersion("HTTP/1.0");

    req.swap(other);

    EXPECT_EQ(req.method(), HttpRequest::kPost);
    EXPECT_EQ(req.path(), "/other");
    EXPECT_EQ(req.getVersion(), "HTTP/1.0");

    EXPECT_EQ(other.method(), HttpRequest::kGet);
    EXPECT_EQ(other.path(), "/api");
    EXPECT_EQ(other.getVersion(), "HTTP/1.1");
}

// ==================== Headers Map Tests ====================

TEST_F(HttpRequestTest, Headers_ReturnsAllHeaders) {
    std::string header1 = "Host: localhost";
    std::string header2 = "Accept: */*";

    req.addHeader(header1.c_str(),
                   header1.c_str() + 4,
                   header1.c_str() + header1.size());
    req.addHeader(header2.c_str(),
                   header2.c_str() + 6,
                   header2.c_str() + header2.size());

    const auto& headers = req.headers();
    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers.at("Host"), "localhost");
    EXPECT_EQ(headers.at("Accept"), "*/*");
}
