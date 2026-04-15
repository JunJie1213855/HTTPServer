#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "session/Session.h"
#include "session/SessionStorage.h"
#include "session/SessionManager.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

using namespace http;
using namespace http::session;

// Helper function to add HTTP header
static void addHeader(http::HttpRequest& req, const std::string& key, const std::string& value) {
    std::string headerLine = key + ": " + value;
    req.addHeader(headerLine.c_str(),
                  headerLine.c_str() + key.size(),
                  headerLine.c_str() + headerLine.size());
}

// ==================== Session Tests ====================

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<SessionManager>(
            std::make_unique<MemorySessionStorage>());
    }

    std::unique_ptr<SessionManager> manager_;
};

TEST_F(SessionTest, ConstructorSetsId) {
    Session session("test-id", nullptr, 3600);
    EXPECT_EQ(session.getId(), "test-id");
}

TEST_F(SessionTest, NewSessionIsNotExpired) {
    Session session("test-id", nullptr, 3600);
    EXPECT_FALSE(session.isExpired());
}

TEST_F(SessionTest, ExpiredSessionIsExpired) {
    Session session("test-id", nullptr, 0);  // 0 seconds TTL
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(session.isExpired());
}

TEST_F(SessionTest, RefreshExtendsExpiry) {
    Session session("test-id", nullptr, 1);  // 1 second TTL
    EXPECT_FALSE(session.isExpired());  // Not expired immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));  // Wait > 1 second
    ASSERT_TRUE(session.isExpired());  // Now it should be expired
    session.refresh();  // Reset expiry
    EXPECT_FALSE(session.isExpired());  // Should not be expired after refresh
}

TEST_F(SessionTest, SetAndGetValue) {
    Session session("test-id", nullptr);
    session.setValue("user", "alice");
    EXPECT_EQ(session.getValue("user"), "alice");
}

TEST_F(SessionTest, GetNonExistentValueReturnsEmpty) {
    Session session("test-id", nullptr);
    EXPECT_EQ(session.getValue("nonexistent"), "");
}

TEST_F(SessionTest, RemoveValue) {
    Session session("test-id", nullptr);
    session.setValue("key", "value");
    session.remove("key");
    EXPECT_EQ(session.getValue("key"), "");
}

TEST_F(SessionTest, ClearRemovesAllValues) {
    Session session("test-id", nullptr);
    session.setValue("key1", "value1");
    session.setValue("key2", "value2");
    session.clear();
    EXPECT_EQ(session.getValue("key1"), "");
    EXPECT_EQ(session.getValue("key2"), "");
}

// ==================== MemorySessionStorage Tests ====================

class MemorySessionStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = std::make_unique<MemorySessionStorage>();
    }

    std::unique_ptr<MemorySessionStorage> storage_;
};

TEST_F(MemorySessionStorageTest, SaveAndLoad) {
    auto session = std::make_shared<Session>("session-1", nullptr);
    session->setValue("key", "value");

    storage_->save(session);
    auto loaded = storage_->load("session-1");

    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->getId(), "session-1");
    EXPECT_EQ(loaded->getValue("key"), "value");
}

TEST_F(MemorySessionStorageTest, LoadNonExistentReturnsNullptr) {
    auto result = storage_->load("nonexistent");
    EXPECT_EQ(result, nullptr);
}

TEST_F(MemorySessionStorageTest, RemoveSession) {
    auto session = std::make_shared<Session>("session-1", nullptr);
    storage_->save(session);

    storage_->remove("session-1");
    auto result = storage_->load("session-1");

    EXPECT_EQ(result, nullptr);
}

TEST_F(MemorySessionStorageTest, ExpiredSessionNotLoaded) {
    auto session = std::make_shared<Session>("session-1", nullptr, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    storage_->save(session);

    auto result = storage_->load("session-1");
    EXPECT_EQ(result, nullptr);
}

TEST_F(MemorySessionStorageTest, ConcurrentSaveAndLoad) {
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            auto session = std::make_shared<Session>(
                "session-" + std::to_string(i), nullptr);
            session->setValue("thread", std::to_string(i));
            storage_->save(session);

            auto loaded = storage_->load("session-" + std::to_string(i));
            if (loaded && loaded->getValue("thread") == std::to_string(i)) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, 10);
}

TEST_F(MemorySessionStorageTest, ConcurrentRemoveIsSafe) {
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Create sessions
    for (int i = 0; i < 10; ++i) {
        auto session = std::make_shared<Session>(
            "session-" + std::to_string(i), nullptr);
        storage_->save(session);
    }

    // Concurrently remove sessions
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &running]() {
            while (running) {
                storage_->remove("session-" + std::to_string(i));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash - basic sanity check
    SUCCEED();
}

// ==================== SessionManager Tests ====================

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<SessionManager>(
            std::make_unique<MemorySessionStorage>());
    }

    std::unique_ptr<SessionManager> manager_;
};

TEST_F(SessionManagerTest, GetSessionCreatesNewSession) {
    HttpRequest req;
    HttpResponse resp;
    req.setMethod("GET", "GET" + 3);

    auto session = manager_->getSession(req, &resp);

    ASSERT_NE(session, nullptr);
    EXPECT_FALSE(session->getId().empty());
}

TEST_F(SessionManagerTest, GetSessionSetsCookie) {
    HttpRequest req;
    HttpResponse resp;
    req.setMethod("GET", "GET" + 3);

    manager_->getSession(req, &resp);

    // Cookie should be set in response headers
    auto headers = resp.getHeaders();
    EXPECT_TRUE(headers.find("Set-Cookie") != headers.end());
}

TEST_F(SessionManagerTest, GetSessionReturnsExistingSession) {
    HttpRequest req1;
    HttpRequest req2;
    HttpResponse resp1;
    HttpResponse resp2;
    req1.setMethod("GET", "GET" + 3);
    req2.setMethod("GET", "GET" + 3);

    auto session1 = manager_->getSession(req1, &resp1);
    auto sessionId = session1->getId();

    // Simulate cookie in second request
    addHeader(req2, "Cookie", "sessionId=" + sessionId);

    auto session2 = manager_->getSession(req2, &resp2);

    EXPECT_EQ(session1->getId(), session2->getId());
}

TEST_F(SessionManagerTest, GetSessionIdFromCookie) {
    // First create a session
    HttpRequest req1;
    HttpResponse resp1;
    req1.setMethod("GET", "GET" + 3);
    auto session1 = manager_->getSession(req1, &resp1);
    auto sessionId = session1->getId();

    // Now use that session ID in a new request
    HttpRequest req2;
    HttpResponse resp2;
    req2.setMethod("GET", "GET" + 3);
    addHeader(req2, "Cookie", "sessionId=" + sessionId);

    auto session2 = manager_->getSession(req2, &resp2);

    // Should return the SAME session, not create a new one
    EXPECT_EQ(session2->getId(), sessionId);
}

TEST_F(SessionManagerTest, DestroySessionRemovesIt) {
    HttpRequest req;
    HttpResponse resp;
    req.setMethod("GET", "GET" + 3);

    auto session = manager_->getSession(req, &resp);
    auto sessionId = session->getId();

    manager_->destroySession(sessionId);

    // Next getSession should create a new session
    addHeader(req, "Cookie", "sessionId=" + sessionId);
    auto newSession = manager_->getSession(req, &resp);

    EXPECT_NE(newSession->getId(), sessionId);
}

TEST_F(SessionManagerTest, GenerateSessionIdIsUnique) {
    std::set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
        HttpRequest req;
        HttpResponse resp;
        req.setMethod("GET", "GET" + 3);
        auto session = manager_->getSession(req, &resp);
        ids.insert(session->getId());
    }

    // All 100 session IDs should be unique
    EXPECT_EQ(ids.size(), 100);
}

TEST_F(SessionManagerTest, GenerateSessionIdIsHex) {
    HttpRequest req;
    HttpResponse resp;
    req.setMethod("GET", "GET" + 3);

    auto session = manager_->getSession(req, &resp);
    const auto& id = session->getId();

    EXPECT_EQ(id.length(), 32);  // 32 hex characters
    for (char c : id) {
        EXPECT_TRUE(std::isxdigit(c)) << "Invalid character: " << c;
    }
}

TEST_F(SessionManagerTest, SessionDataPersistsAcrossRequests) {
    HttpRequest req1;
    HttpRequest req2;
    HttpResponse resp1;
    HttpResponse resp2;
    req1.setMethod("GET", "GET" + 3);
    req2.setMethod("GET", "GET" + 3);

    auto session1 = manager_->getSession(req1, &resp1);
    session1->setValue("userId", "123");

    auto sessionId = session1->getId();
    addHeader(req2, "Cookie", "sessionId=" + sessionId);

    auto session2 = manager_->getSession(req2, &resp2);

    EXPECT_EQ(session2->getValue("userId"), "123");
}

TEST_F(SessionManagerTest, UpdateSessionSavesChanges) {
    // First request - create session and set value
    HttpRequest req1;
    HttpResponse resp1;
    req1.setMethod("GET", "GET" + 3);
    auto session = manager_->getSession(req1, &resp1);
    auto sessionId = session->getId();
    session->setValue("key", "value");

    // Explicitly update session to persist
    manager_->updateSession(session);

    // Second request with same cookie - should get the same session
    HttpRequest req2;
    HttpResponse resp2;
    req2.setMethod("GET", "GET" + 3);
    addHeader(req2, "Cookie", "sessionId=" + sessionId);

    auto loaded = manager_->getSession(req2, &resp2);
    EXPECT_EQ(loaded->getValue("key"), "value");
}

// ==================== Session Manager Concurrency Tests ====================

class SessionManagerConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<SessionManager>(
            std::make_unique<MemorySessionStorage>());
    }

    std::unique_ptr<SessionManager> manager_;
};

TEST_F(SessionManagerConcurrencyTest, ConcurrentGetSessionIsSafe) {
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                HttpRequest req;
                HttpResponse resp;
                req.setMethod("GET", "GET" + 3);
                addHeader(req, "Cookie", "sessionId=session-" + std::to_string(i));

                auto session = manager_->getSession(req, &resp);
                session->setValue("thread", std::to_string(i));

                if (!session->getId().empty()) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, 1000);
}

TEST_F(SessionManagerConcurrencyTest, ConcurrentDestroyIsSafe) {
    std::vector<std::string> sessionIds;
    for (int i = 0; i < 10; ++i) {
        HttpRequest req;
        HttpResponse resp;
        req.setMethod("GET", "GET" + 3);
        auto session = manager_->getSession(req, &resp);
        sessionIds.push_back(session->getId());
    }

    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    threads.emplace_back([this, &sessionIds, &running]() {
        for (int i = 0; i < 100; ++i) {
            for (const auto& id : sessionIds) {
                manager_->destroySession(id);
            }
        }
    });

    threads.emplace_back([this, &sessionIds, &running]() {
        while (running) {
            for (int i = 0; i < 10; ++i) {
                HttpRequest req;
                HttpResponse resp;
                req.setMethod("GET", "GET" + 3);
                manager_->getSession(req, &resp);
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();  // Should not crash
}
