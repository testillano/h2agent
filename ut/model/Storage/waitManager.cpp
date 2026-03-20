#include <WaitManager.hpp>
#include <GlobalVariable.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <chrono>

class WaitManager_test : public ::testing::Test
{
public:
    h2agent::model::WaitManager wm_{};
    h2agent::model::GlobalVariable global_var_{};

    WaitManager_test() {
        wm_.setGlobalVariable(&global_var_);
        global_var_.setWaitManager(&wm_);
    }
};

TEST_F(WaitManager_test, GlobalVariableAnyChange)
{
    global_var_.load("key1", "initial");

    std::string resultValue, previousValue;
    bool met = false;

    std::thread waiter([&] {
        met = wm_.waitForGlobalVariable("key1", "" /* any change */, 5000, resultValue, previousValue);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    global_var_.load("key1", "changed");

    waiter.join();
    EXPECT_TRUE(met);
    EXPECT_EQ(previousValue, "initial");
    EXPECT_EQ(resultValue, "changed");
}

TEST_F(WaitManager_test, GlobalVariableSpecificValue)
{
    global_var_.load("key1", "v1");

    std::string resultValue, previousValue;
    bool met = false;

    std::thread waiter([&] {
        met = wm_.waitForGlobalVariable("key1", "target", 5000, resultValue, previousValue);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    global_var_.load("key1", "target");

    waiter.join();
    EXPECT_TRUE(met);
    EXPECT_EQ(resultValue, "target");
}

TEST_F(WaitManager_test, GlobalVariableAlreadySatisfied)
{
    global_var_.load("key1", "target");

    std::string resultValue, previousValue;
    bool met = wm_.waitForGlobalVariable("key1", "target", 100, resultValue, previousValue);
    EXPECT_TRUE(met);
    EXPECT_EQ(resultValue, "target");
}

TEST_F(WaitManager_test, GlobalVariableTimeout)
{
    global_var_.load("key1", "initial");

    std::string resultValue, previousValue;
    bool met = wm_.waitForGlobalVariable("key1", "" /* any change */, 50, resultValue, previousValue);
    EXPECT_FALSE(met);
    EXPECT_EQ(previousValue, "initial");
}

TEST_F(WaitManager_test, MaxWaitersEnforced)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < h2agent::model::WaitManager::MAX_WAITERS; i++) {
        threads.emplace_back([this] {
            std::string rv, pv;
            wm_.waitForGlobalVariable("key1", "never", 2000, rv, pv);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // This one should be rejected (429)
    std::string rv, pv;
    bool met = wm_.waitForGlobalVariable("key1", "never", 100, rv, pv);
    EXPECT_FALSE(met);

    // Unblock all waiters by satisfying condition
    global_var_.load("key1", "never");
    for (auto& t : threads) t.join();
}
