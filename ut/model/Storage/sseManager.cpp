#include <SseManager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class SseManager_test : public ::testing::Test
{
public:
    h2agent::model::SseManager mgr_{};
};

TEST_F(SseManager_test, NotifyMatchingKey)
{
    std::string received;
    auto id = mgr_.addConnection({"key1"}, [&](const std::string &data) {
        received = data;
    });

    mgr_.notify("key1", nlohmann::json("value1"));
    EXPECT_NE(received.find("\"key\":\"key1\""), std::string::npos);
    EXPECT_NE(received.find("\"value\":\"value1\""), std::string::npos);
    EXPECT_NE(received.find("event: vault-set\n"), std::string::npos);

    mgr_.removeConnection(id);
}

TEST_F(SseManager_test, NotifyNonMatchingKeyIsFiltered)
{
    std::string received;
    auto id = mgr_.addConnection({"key1"}, [&](const std::string &data) {
        received = data;
    });

    mgr_.notify("other_key", nlohmann::json("val"));
    EXPECT_TRUE(received.empty());

    mgr_.removeConnection(id);
}

TEST_F(SseManager_test, EmptyFilterReceivesAll)
{
    int count = 0;
    auto id = mgr_.addConnection({}, [&](const std::string &) {
        count++;
    });

    mgr_.notify("a", nlohmann::json(1));
    mgr_.notify("b", nlohmann::json(2));
    EXPECT_EQ(count, 2);

    mgr_.removeConnection(id);
}

TEST_F(SseManager_test, RemoveConnectionStopsNotifications)
{
    int count = 0;
    auto id = mgr_.addConnection({}, [&](const std::string &) {
        count++;
    });

    mgr_.notify("x", nlohmann::json("before"));
    mgr_.removeConnection(id);
    mgr_.notify("x", nlohmann::json("after"));

    EXPECT_EQ(count, 1);
}

TEST_F(SseManager_test, ActiveConnections)
{
    EXPECT_EQ(mgr_.activeConnections(), 0u);
    auto id1 = mgr_.addConnection({}, [](const std::string &) {});
    auto id2 = mgr_.addConnection({"k"}, [](const std::string &) {});
    EXPECT_EQ(mgr_.activeConnections(), 2u);
    mgr_.removeConnection(id1);
    EXPECT_EQ(mgr_.activeConnections(), 1u);
    mgr_.removeConnection(id2);
    EXPECT_EQ(mgr_.activeConnections(), 0u);
}
