#include <WaitManager.hpp>
#include <Vault.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>
#include <MockClientEvent.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <optional>
#include <vector>

class WaitManager_test : public ::testing::Test
{
public:
    h2agent::model::WaitManager wm_{};
    h2agent::model::Vault vault_{};

    WaitManager_test() {
        wm_.setVault(&vault_);
        vault_.setWaitManager(&wm_);
    }
};

TEST_F(WaitManager_test, VaultAnyChange)
{
    vault_.load("key1", "initial");

    nlohmann::json resultValue, previousValue;
    bool met = false;

    std::thread waiter([&] {
        met = wm_.waitForVault("key1", nullptr /* any change */, 5000, resultValue, previousValue);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    vault_.load("key1", "changed");

    waiter.join();
    EXPECT_TRUE(met);
    EXPECT_EQ(previousValue, "initial");
    EXPECT_EQ(resultValue, "changed");
}

TEST_F(WaitManager_test, VaultSpecificValue)
{
    vault_.load("key1", "v1");

    nlohmann::json resultValue, previousValue;
    bool met = false;

    std::thread waiter([&] {
        met = wm_.waitForVault("key1", "target", 5000, resultValue, previousValue);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    vault_.load("key1", "target");

    waiter.join();
    EXPECT_TRUE(met);
    EXPECT_EQ(resultValue, "target");
}

TEST_F(WaitManager_test, VaultAlreadySatisfied)
{
    vault_.load("key1", "target");

    nlohmann::json resultValue, previousValue;
    bool met = wm_.waitForVault("key1", "target", 100, resultValue, previousValue);
    EXPECT_TRUE(met);
    EXPECT_EQ(resultValue, "target");
}

TEST_F(WaitManager_test, VaultScalarsMatchByText)
{
    // Barrier must not depend on the stored type: 60 (number) matches "60" (string) and vice versa.
    nlohmann::json resultValue, previousValue;
    vault_.load("n", "60");
    EXPECT_TRUE(wm_.waitForVault("n", 60, 100, resultValue, previousValue));
    vault_.load("m", 60);
    EXPECT_TRUE(wm_.waitForVault("m", "60", 100, resultValue, previousValue));
    vault_.load("b", true);
    EXPECT_TRUE(wm_.waitForVault("b", "true", 100, resultValue, previousValue));
    vault_.load("s", "done");
    EXPECT_FALSE(wm_.waitForVault("s", "Done", 50, resultValue, previousValue));  // text is case-sensitive
}

TEST_F(WaitManager_test, VaultStructuredValuesMatchAsJson)
{
    nlohmann::json resultValue, previousValue;
    vault_.load("o", nlohmann::json{{"a", 1}});
    EXPECT_TRUE(wm_.waitForVault("o", nlohmann::json{{"a", 1}}, 100, resultValue, previousValue));
    EXPECT_FALSE(wm_.waitForVault("o", "{\"a\":1}", 50, resultValue, previousValue));  // a string is not the object
}

TEST_F(WaitManager_test, VaultTimeout)
{
    vault_.load("key1", "initial");

    nlohmann::json resultValue, previousValue;
    bool met = wm_.waitForVault("key1", nullptr /* any change */, 50, resultValue, previousValue);
    EXPECT_FALSE(met);
    EXPECT_EQ(previousValue, "initial");
}

TEST_F(WaitManager_test, MaxWaitersEnforced)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < h2agent::model::WaitManager::MAX_WAITERS; i++) {
        threads.emplace_back([this] {
            nlohmann::json rv, pv;
            wm_.waitForVault("key1", "never", 2000, rv, pv);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // This one should be rejected (429)
    nlohmann::json rv, pv;
    bool met = wm_.waitForVault("key1", "never", 100, rv, pv);
    EXPECT_FALSE(met);

    // Unblock all waiters by satisfying condition
    vault_.load("key1", "never");
    for (auto& t : threads) t.join();
}

///////////////////////////
// DATA WAITS (EVENTS)   //
///////////////////////////

class EventWait_test : public ::testing::Test
{
public:
    h2agent::model::WaitManager wm_{};
    h2agent::model::MockServerData server_{};
    h2agent::model::MockClientData client_{};
    nghttp2::asio_http2::header_map headers_{};
    std::uint64_t us_{1000};

    EventWait_test() {
        server_.setWaitManager(&wm_);
        client_.setWaitManager(&wm_);
    }

    void serverEvent(const std::string &uri, const std::string &state) {
        h2agent::model::DataPart body;
        const std::uint64_t us = us_++;  // reception timestamp and server sequence
        server_.loadEvent(h2agent::model::DataKey("POST", uri), "initial", state, std::chrono::microseconds(us), 200, headers_, headers_, body, "", us, 0, true);
    }

    void clientEvent(const std::string &provisionId, const std::string &state) {
        h2agent::model::DataPart body;
        client_.loadEvent(h2agent::model::DataKey("ep", "GET", "/c"), provisionId, "initial", state, std::chrono::microseconds(us_), std::chrono::microseconds(us_), 200, headers_, headers_, "", body, us_, 1, 0, 1000, true);
        us_++;
    }

    // Server spec on POST <uri> with an optional state condition
    h2agent::model::EventWaitSpec serverSpec(const std::string &uri, const std::string &state, bool prefix = false, std::uint64_t eventNumber = 0) {
        h2agent::model::EventWaitSpec spec;
        spec.eventNumber = eventNumber;
        spec.onlyNew = state.empty() && eventNumber == 0;
        spec.matches = [uri, state, prefix](const h2agent::model::DataKey &key, const h2agent::model::MockEvent &event) {
            if (key.getMethod() != "POST" || key.getUri() != uri) return false;
            if (state.empty()) return true;
            return prefix ? event.getState().rfind(state, 0) == 0 : event.getState() == state;
        };
        return spec;
    }

    h2agent::model::WaitOutcome waitServer(const h2agent::model::EventWaitSpec &spec, unsigned int timeoutMs, std::optional<h2agent::model::EventWaitMatch> &match) {
        return wm_.waitForEvent(spec, [&]() {
            return server_.collectEvents(spec.matches);
        }, timeoutMs, match);
    }
};

TEST_F(EventWait_test, AlreadyMetAnswersAtOnce)
{
    serverEvent("/a", "reauth");
    auto spec = serverSpec("/a", "reauth");
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 5000, match), h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->event->getState(), "reauth");
    EXPECT_EQ(match->key.getUri(), "/a");
    EXPECT_EQ(match->position, 1u);
}

TEST_F(EventWait_test, MetLater)
{
    serverEvent("/a", "initial");
    auto spec = serverSpec("/a", "reauth");
    std::optional<h2agent::model::EventWaitMatch> match;
    h2agent::model::WaitOutcome outcome{};
    std::thread waiter([&] { outcome = waitServer(spec, 5000, match); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    serverEvent("/b", "reauth");  // another key: ignored
    serverEvent("/a", "reauth");
    waiter.join();
    EXPECT_EQ(outcome, h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->key.getUri(), "/a");
    EXPECT_EQ(wm_.activeWaiters(), 0u);
}

TEST_F(EventWait_test, Timeout)
{
    serverEvent("/a", "initial");
    auto spec = serverSpec("/a", "reauth");
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 50, match), h2agent::model::WaitOutcome::Timeout);
    EXPECT_FALSE(match.has_value());
}

TEST_F(EventWait_test, OnlyNewIgnoresStoredEvents)
{
    serverEvent("/a", "first");
    auto spec = serverSpec("/a", "");
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 50, match), h2agent::model::WaitOutcome::Timeout);

    h2agent::model::WaitOutcome outcome{};
    std::thread waiter([&] { outcome = waitServer(spec, 5000, match); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    serverEvent("/a", "second");
    waiter.join();
    EXPECT_EQ(outcome, h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->event->getState(), "second");
    EXPECT_EQ(match->position, 0u);
}

TEST_F(EventWait_test, StatePrefix)
{
    serverEvent("/a", "done");
    serverEvent("/a", "__core:failed:requestSchemaId");
    auto spec = serverSpec("/a", "__core:failed:", true /* prefix */);
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 5000, match), h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->event->getState(), "__core:failed:requestSchemaId");
}

TEST_F(EventWait_test, EventNumberCountsMatchingEvents)
{
    serverEvent("/a", "notify");
    serverEvent("/a", "other");
    auto spec = serverSpec("/a", "notify", false, 2 /* second notify */);
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 50, match), h2agent::model::WaitOutcome::Timeout);

    h2agent::model::WaitOutcome outcome{};
    std::thread waiter([&] { outcome = waitServer(spec, 5000, match); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    serverEvent("/a", "notify");
    waiter.join();
    EXPECT_EQ(outcome, h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->position, 2u);

    // Issued after the event, it still answers at once:
    std::optional<h2agent::model::EventWaitMatch> again;
    EXPECT_EQ(waitServer(spec, 5000, again), h2agent::model::WaitOutcome::Met);
    EXPECT_EQ(again->event, match->event);
}

TEST_F(EventWait_test, ClientEventsByProvisionId)
{
    h2agent::model::EventWaitSpec spec;
    spec.matches = [](const h2agent::model::DataKey &, const h2agent::model::MockEvent &event) {
        return static_cast<const h2agent::model::MockClientEvent &>(event).getClientProvisionId() == "flow";
    };
    spec.onlyNew = true;
    std::optional<h2agent::model::EventWaitMatch> match;
    h2agent::model::WaitOutcome outcome{};
    std::thread waiter([&] {
        outcome = wm_.waitForEvent(spec, [&]() {
            return client_.collectEvents(spec.matches);
        }, 5000, match);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    clientEvent("other", "done");
    clientEvent("flow", "__core:failed:transport");
    waiter.join();
    EXPECT_EQ(outcome, h2agent::model::WaitOutcome::Met);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->key.getClientEndpointId(), "ep");
    EXPECT_EQ(match->event->getState(), "__core:failed:transport");
}

TEST_F(EventWait_test, SharesTheWaitersLimitWithVault)
{
    h2agent::model::Vault vault;
    wm_.setVault(&vault);
    vault.setWaitManager(&wm_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < h2agent::model::WaitManager::MAX_WAITERS; i++) {
        threads.emplace_back([&] {
            nlohmann::json rv, pv;
            wm_.waitForVault("k", "never", 2000, rv, pv);
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto spec = serverSpec("/a", "reauth");
    std::optional<h2agent::model::EventWaitMatch> match;
    EXPECT_EQ(waitServer(spec, 100, match), h2agent::model::WaitOutcome::Full);

    vault.load("k", "never");
    for (auto &t : threads) t.join();
}

TEST_F(EventWait_test, Shutdown)
{
    auto spec = serverSpec("/a", "reauth");
    std::optional<h2agent::model::EventWaitMatch> match;
    h2agent::model::WaitOutcome outcome{};
    std::thread waiter([&] { outcome = waitServer(spec, 5000, match); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    wm_.shutdown();
    waiter.join();
    EXPECT_EQ(outcome, h2agent::model::WaitOutcome::Shutdown);
}
