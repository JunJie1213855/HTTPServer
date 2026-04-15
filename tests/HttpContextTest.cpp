#include <gtest/gtest.h>
#include "http/HttpContext.h"
#include <muduo/net/Buffer.h>
#include <muduo/base/Timestamp.h>

using namespace http;

class HttpContextTest : public ::testing::Test {
protected:
    muduo::net::Buffer buffer;
    HttpContext context;
    muduo::Timestamp timestamp;

    // Helper to append string to buffer
    void appendToBuffer(const std::string& data) {
        buffer.append(data.data(), data.size());
    }
};

// ==================== Simple GET Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_SimpleGetRequestLine) {
    appendToBuffer("GET / HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kGet);
    EXPECT_EQ(context.request().path(), "/");
    EXPECT_EQ(context.request().getVersion(), "HTTP/1.1");
}

TEST_F(HttpContextTest, ParseRequest_GetWithPath) {
    appendToBuffer("GET /api/users HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kGet);
    EXPECT_EQ(context.request().path(), "/api/users");
}

TEST_F(HttpContextTest, ParseRequest_GetWithQuery) {
    appendToBuffer("GET /api?page=1&size=10 HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kGet);
    EXPECT_EQ(context.request().path(), "/api");
    EXPECT_EQ(context.request().getQueryParameters("page"), "1");
    EXPECT_EQ(context.request().getQueryParameters("size"), "10");
}

// ==================== GET with Headers Tests ====================

TEST_F(HttpContextTest, ParseRequest_GetWithHostHeader) {
    appendToBuffer("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getHeader("Host"), "localhost");
}

TEST_F(HttpContextTest, ParseRequest_GetWithMultipleHeaders) {
    appendToBuffer("GET /api HTTP/1.1\r\n"
                   "Host: localhost\r\n"
                   "Content-Type: application/json\r\n"
                   "Accept: */*\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getHeader("Host"), "localhost");
    EXPECT_EQ(context.request().getHeader("Content-Type"), "application/json");
    EXPECT_EQ(context.request().getHeader("Accept"), "*/*");
}

TEST_F(HttpContextTest, ParseRequest_GetWithEmptyHeaderValue) {
    appendToBuffer("GET / HTTP/1.1\r\nHost:\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getHeader("Host"), "");
}

// ==================== POST Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_PostWithContentLength) {
    appendToBuffer("POST /api/data HTTP/1.1\r\n"
                   "Content-Length: 5\r\n\r\n"
                   "hello");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kPost);
    EXPECT_EQ(context.request().path(), "/api/data");
    EXPECT_EQ(context.request().contentLength(), 5);
    EXPECT_EQ(context.request().getBody(), "hello");
}

TEST_F(HttpContextTest, ParseRequest_PostWithJsonBody) {
    std::string body = R"({"name":"test","value":123})";
    appendToBuffer("POST /api/json HTTP/1.1\r\n"
                   "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
                   body);

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getBody(), body);
}

TEST_F(HttpContextTest, ParseRequest_PostWithoutContentLength_Fails) {
    appendToBuffer("POST /api/data HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_FALSE(result);
}

// ==================== PUT Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_PutWithBody) {
    appendToBuffer("PUT /api/data/1 HTTP/1.1\r\n"
                   "Content-Length: 7\r\n\r\n"
                   "updated");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kPut);
    EXPECT_EQ(context.request().path(), "/api/data/1");
    EXPECT_EQ(context.request().getBody(), "updated");
}

// ==================== DELETE Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_DeleteRequest) {
    appendToBuffer("DELETE /api/users/123 HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kDelete);
    EXPECT_EQ(context.request().path(), "/api/users/123");
}

// ==================== Incomplete Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_IncompleteRequestLine_ReturnsTrue) {
    appendToBuffer("GET /api");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);  // Returns true, meaning "need more data"
    EXPECT_FALSE(context.gotAll());
}

TEST_F(HttpContextTest, ParseRequest_IncompleteHeaders_ReturnsTrue) {
    appendToBuffer("GET / HTTP/1.1\r\n"
                   "Host: localhost");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_FALSE(context.gotAll());
}

TEST_F(HttpContextTest, ParseRequest_IncompleteBody_ReturnsTrue) {
    appendToBuffer("POST /api HTTP/1.1\r\n"
                   "Content-Length: 10\r\n\r\n"
                   "short");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);  // Needs more body data
    EXPECT_FALSE(context.gotAll());
}

TEST_F(HttpContextTest, ParseRequest_ContinuesWhenMoreDataArrives) {
    // First partial request
    appendToBuffer("GET /api");
    bool result1 = context.parseRequest(&buffer, timestamp);
    EXPECT_TRUE(result1);
    EXPECT_FALSE(context.gotAll());

    // Append rest of request
    appendToBuffer(" HTTP/1.1\r\n\r\n");
    bool result2 = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result2);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().path(), "/api");
}

// ==================== Malformed Request Tests ====================

TEST_F(HttpContextTest, ParseRequest_MissingVersion_Fails) {
    appendToBuffer("GET /api\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_FALSE(result);
}

TEST_F(HttpContextTest, ParseRequest_InvalidMethod_Fails) {
    appendToBuffer("INVALID /api HTTP/1.1\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_FALSE(result);
}

TEST_F(HttpContextTest, ParseRequest_InvalidHTTPVersion_Fails) {
    appendToBuffer("GET /api HTTP/2.0\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_FALSE(result);
}

TEST_F(HttpContextTest, ParseRequest_EmptyRequest_ReturnsTrue) {
    // Empty buffer returns true (needs more data), not failure
    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);  // Returns true meaning "need more data"
    EXPECT_FALSE(context.gotAll());
}

// ==================== State Machine Tests ====================

TEST_F(HttpContextTest, ParseRequest_HeadersStateAfterRequestLine) {
    appendToBuffer("GET /api HTTP/1.1\r\n");

    context.parseRequest(&buffer, timestamp);

    // After request line, should be in headers state
    // This is implicit - we verify by adding more headers
    appendToBuffer("Host: localhost\r\n\r\n");
    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getHeader("Host"), "localhost");
}

TEST_F(HttpContextTest, ParseRequest_BodyStateForPost) {
    appendToBuffer("POST /api HTTP/1.1\r\n"
                   "Content-Length: 5\r\n\r\n");

    context.parseRequest(&buffer, timestamp);

    // Should be waiting for body
    // Append body and verify it gets parsed
    appendToBuffer("hello");
    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().getBody(), "hello");
}

// ==================== Reset Tests ====================

TEST_F(HttpContextTest, Reset_ClearsState) {
    appendToBuffer("GET /api HTTP/1.1\r\n"
                   "Host: localhost\r\n\r\n");

    context.parseRequest(&buffer, timestamp);
    EXPECT_TRUE(context.gotAll());

    context.reset();
    EXPECT_FALSE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kInvalid);
}

TEST_F(HttpContextTest, Reset_AllowsNewRequest) {
    // First request
    appendToBuffer("GET /first HTTP/1.1\r\n\r\n");
    context.parseRequest(&buffer, timestamp);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().path(), "/first");

    // Reset and process second request
    context.reset();
    appendToBuffer("POST /second HTTP/1.1\r\n"
                   "Content-Length: 4\r\n\r\n"
                   "test");
    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().method(), HttpRequest::kPost);
    EXPECT_EQ(context.request().path(), "/second");
    EXPECT_EQ(context.request().getBody(), "test");
}

// ==================== GotAll Tests ====================

TEST_F(HttpContextTest, GotAll_FalseInitially) {
    EXPECT_FALSE(context.gotAll());
}

TEST_F(HttpContextTest, GotAll_TrueAfterCompleteGet) {
    appendToBuffer("GET / HTTP/1.1\r\n\r\n");
    context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(context.gotAll());
}

TEST_F(HttpContextTest, GotAll_TrueAfterCompletePost) {
    appendToBuffer("POST /api HTTP/1.1\r\n"
                   "Content-Length: 5\r\n\r\n"
                   "hello");
    context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(context.gotAll());
}

// ==================== Request Object Access Tests ====================

TEST_F(HttpContextTest, Request_ReturnsParsedRequest) {
    appendToBuffer("GET /api?page=1 HTTP/1.1\r\n"
                   "Host: example.com\r\n\r\n");

    context.parseRequest(&buffer, timestamp);

    const auto& req = context.request();
    EXPECT_EQ(req.method(), HttpRequest::kGet);
    EXPECT_EQ(req.path(), "/api");
    EXPECT_EQ(req.getQueryParameters("page"), "1");
    EXPECT_EQ(req.getHeader("Host"), "example.com");
}

// ==================== Edge Cases ====================

TEST_F(HttpContextTest, ParseRequest_HeaderWithTrailingSpaces) {
    appendToBuffer("GET / HTTP/1.1\r\n"
                   "Host:   localhost   \r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    // Whitespace trimming is done by addHeader
    EXPECT_EQ(context.request().getHeader("Host"), "localhost");
}

TEST_F(HttpContextTest, ParseRequest_ZeroContentLength) {
    appendToBuffer("POST /api HTTP/1.1\r\n"
                   "Content-Length: 0\r\n\r\n");

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.gotAll());
    EXPECT_EQ(context.request().contentLength(), 0);
    EXPECT_EQ(context.request().getBody(), "");
}

TEST_F(HttpContextTest, ParseRequest_WhitespaceOnlyHeaderLine_Fails) {
    // Note: Whitespace-only header line is treated as malformed and fails
    appendToBuffer("GET / HTTP/1.1\r\n"
                   "   \r\n\r\n");  // Whitespace-only header line

    bool result = context.parseRequest(&buffer, timestamp);

    EXPECT_FALSE(result);  // Whitespace-only line is treated as malformed
}
