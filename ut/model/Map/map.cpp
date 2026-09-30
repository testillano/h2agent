// Unit tests for the sharded Map<Key,Value,Shards> template (src/model/Map.hpp).
//
// Sharding is opt-in via the Shards template parameter (default 1 == the
// historical single-mutex behaviour). Per-key operations lock a single shard;
// whole-map operations (getJson/forEach/size/empty/clear/copy) lock all shards
// in a fixed order. These tests cover both the single-shard (default) and the
// multi-shard cases, plus a concurrency smoke test.

#include <Map.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>
#include <vector>

using h2agent::model::Map;

// A concrete multi-shard map for testing.
template <std::size_t N>
using StrIntMap = Map<std::string, int, N>;

// ---------------------------------------------------------------------------
// Per-key operations (multi-shard)
// ---------------------------------------------------------------------------
TEST(Map, AddGetExistsRemove_MultiShard) {
    StrIntMap<8> m;
    bool exists = false;

    m.add("alpha", 1);
    m.add("beta", 2);

    EXPECT_TRUE(m.exists("alpha"));
    EXPECT_EQ(m.get("alpha", exists), 1);
    EXPECT_TRUE(exists);
    EXPECT_EQ(m.get("beta", exists), 2);
    EXPECT_TRUE(exists);

    m.get("missing", exists);
    EXPECT_FALSE(exists);

    m.remove("alpha", exists);
    EXPECT_TRUE(exists);
    EXPECT_FALSE(m.exists("alpha"));
    m.remove("alpha", exists);
    EXPECT_FALSE(exists);  // already gone
}

TEST(Map, TryGet_MultiShard) {
    StrIntMap<4> m;
    m.add("k", 42);
    int out = 0;
    EXPECT_TRUE(m.tryGet("k", out));
    EXPECT_EQ(out, 42);
    EXPECT_FALSE(m.tryGet("nope", out));
}

TEST(Map, ModifyOrInsert_MultiShard) {
    StrIntMap<8> m;
    // insert path (default-constructed then modified)
    m.modifyOrInsert("c", [](int &v) { v += 1; });
    bool exists = false;
    EXPECT_EQ(m.get("c", exists), 1);
    // modify path
    m.modifyOrInsert("c", [](int &v) { v += 10; });
    EXPECT_EQ(m.get("c", exists), 11);
}

// ---------------------------------------------------------------------------
// Whole-map operations (must lock all shards)
// ---------------------------------------------------------------------------
TEST(Map, SizeEmptyClear_MultiShard) {
    StrIntMap<8> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);

    for (int i = 0; i < 50; ++i) m.add("k" + std::to_string(i), i);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m.size(), 50u);

    EXPECT_TRUE(m.clear());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_FALSE(m.clear());  // already empty
}

TEST(Map, ForEach_VisitsAllAcrossShards) {
    StrIntMap<8> m;
    std::set<std::string> expected;
    for (int i = 0; i < 40; ++i) {
        std::string k = "k" + std::to_string(i);
        m.add(k, i);
        expected.insert(k);
    }
    std::set<std::string> seen;
    int sum = 0;
    m.forEach([&](const std::string &k, const int &v) {
        seen.insert(k);
        sum += v;
    });
    EXPECT_EQ(seen, expected);
    EXPECT_EQ(sum, 40 * 39 / 2);
}

TEST(Map, GetJson_SnapshotAcrossShards) {
    StrIntMap<8> m;
    for (int i = 0; i < 20; ++i) m.add("k" + std::to_string(i), i);
    nlohmann::json j = m.getJson();
    ASSERT_TRUE(j.is_object());
    EXPECT_EQ(j.size(), 20u);
    EXPECT_EQ(j["k7"], 7);
    EXPECT_EQ(j["k19"], 19);
}

TEST(Map, CopyConstructor_MultiShard) {
    StrIntMap<8> m;
    for (int i = 0; i < 10; ++i) m.add("k" + std::to_string(i), i);
    StrIntMap<8> copy(m);
    EXPECT_EQ(copy.size(), 10u);
    bool exists = false;
    EXPECT_EQ(copy.get("k5", exists), 5);
    EXPECT_TRUE(exists);
}

// ---------------------------------------------------------------------------
// Shards=1 (default) must behave exactly like the historical single map.
// ---------------------------------------------------------------------------
TEST(Map, DefaultIsSingleShard_BehaviourUnchanged) {
    Map<std::string, int> m;  // default Shards
    bool exists = false;
    m.add("x", 5);
    EXPECT_EQ(m.get("x", exists), 5);
    EXPECT_TRUE(exists);
    m.modifyOrInsert("x", [](int &v) { v *= 2; });
    EXPECT_EQ(m.get("x", exists), 10);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.getJson()["x"], 10);
}

// ---------------------------------------------------------------------------
// Concurrency: distinct-key writers from many threads + a concurrent snapshot.
// Asserts no crash/deadlock and that all writes land (per-key atomicity).
// ---------------------------------------------------------------------------
TEST(Map, ConcurrentDistinctKeyWritesAndSnapshot) {
    StrIntMap<16> m;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 2000;

    std::atomic<bool> stop{false};
    // reader thread: hammers getJson()/size() while writers run (must not deadlock/crash)
    std::thread reader([&]() {
        while (!stop.load()) {
            volatile auto s = m.size();
            (void)s;
            nlohmann::json j = m.getJson();
            (void)j;
        }
    });

    std::vector<std::thread> writers;
    for (int t = 0; t < kThreads; ++t) {
        writers.emplace_back([&, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                std::string k = "t" + std::to_string(t) + "_" + std::to_string(i);
                m.modifyOrInsert(k, [](int &v) { v += 1; });
            }
        });
    }
    for (auto &w : writers) w.join();
    stop.store(true);
    reader.join();

    // Every distinct key written exactly once -> value 1, total count == threads*perThread.
    EXPECT_EQ(m.size(), static_cast<std::size_t>(kThreads * kPerThread));
    bool exists = false;
    EXPECT_EQ(m.get("t3_1234", exists), 1);
    EXPECT_TRUE(exists);
}

// Concurrency: many threads incrementing the SAME key must serialize correctly
// (per-key modifyOrInsert is atomic under a single shard lock -> no lost updates).
TEST(Map, ConcurrentSameKeyIncrementsAreAtomic) {
    StrIntMap<16> m;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 5000;

    std::vector<std::thread> writers;
    for (int t = 0; t < kThreads; ++t) {
        writers.emplace_back([&]() {
            for (int i = 0; i < kPerThread; ++i) {
                m.modifyOrInsert("shared", [](int &v) { v += 1; });
            }
        });
    }
    for (auto &w : writers) w.join();

    bool exists = false;
    EXPECT_EQ(m.get("shared", exists), kThreads * kPerThread);  // no lost updates
    EXPECT_TRUE(exists);
}
