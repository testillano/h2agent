#include <WaitManager.hpp>
#include <Vault.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <chrono>

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
