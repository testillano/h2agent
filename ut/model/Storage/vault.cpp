#include <Vault.hpp>

#include <AdminSchemas.hpp>

#include <limits>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Vault configuration:
const nlohmann::json VaultConfiguration__Success = R"(
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2"
}
)"_json;

const nlohmann::json VaultConfiguration__BadSchema = R"({ "variable.with.dots": "value" })"_json;


class Vault_test : public ::testing::Test
{
public:
    h2agent::model::Vault vault_{};

    Vault_test() {
        ;
    }
};

TEST_F(Vault_test, LoadVaultSuccess)
{
    EXPECT_TRUE(Vault_test::vault_.loadJson(VaultConfiguration__Success));
    EXPECT_EQ(Vault_test::vault_.getJson(), VaultConfiguration__Success);

    bool exists = false;
    EXPECT_EQ(Vault_test::vault_.get("variable_name_2", exists), nlohmann::json("variable_value_2"));
    EXPECT_TRUE(exists);
    Vault_test::vault_.remove("variable_name_2", exists);
    EXPECT_TRUE(exists);
    EXPECT_EQ(Vault_test::vault_.get("variable_name_2", exists), nlohmann::json());
    EXPECT_FALSE(exists);
    Vault_test::vault_.load("another_variable", nlohmann::json("another_value"));
    EXPECT_EQ(Vault_test::vault_.get("another_variable", exists), nlohmann::json("another_value"));
    EXPECT_TRUE(exists);

    EXPECT_TRUE(Vault_test::vault_.clear());
    EXPECT_FALSE(Vault_test::vault_.clear());
}

TEST_F(Vault_test, LoadVaultFail)
{
    EXPECT_FALSE(Vault_test::vault_.loadJson(VaultConfiguration__BadSchema));
    EXPECT_EQ(Vault_test::vault_.asJsonString(), "{}");
}

TEST_F(Vault_test, GetVaultSchema)
{
    EXPECT_EQ(Vault_test::vault_.getSchema().getJson(), h2agent::adminSchemas::vault);
}

// --- JSON native storage ---

TEST_F(Vault_test, LoadJsonTypes)
{
    vault_.load("str", nlohmann::json("hello"));
    vault_.load("num", nlohmann::json(42));
    vault_.load("flt", nlohmann::json(3.14));
    vault_.load("boo", nlohmann::json(true));
    vault_.load("nul", nlohmann::json(nullptr));
    vault_.load("arr", nlohmann::json::array({1,2,3}));
    vault_.load("obj", nlohmann::json({{"a",1},{"b","two"}}));

    bool exists = false;
    EXPECT_EQ(vault_.get("str", exists), nlohmann::json("hello"));
    EXPECT_EQ(vault_.get("num", exists), nlohmann::json(42));
    EXPECT_EQ(vault_.get("flt", exists), nlohmann::json(3.14));
    EXPECT_EQ(vault_.get("boo", exists), nlohmann::json(true));
    EXPECT_EQ(vault_.get("nul", exists), nlohmann::json(nullptr));
    EXPECT_EQ(vault_.get("arr", exists), nlohmann::json::array({1,2,3}));
    EXPECT_EQ(vault_.get("obj", exists), nlohmann::json({{"a",1},{"b","two"}}));
}

TEST_F(Vault_test, JsonToString)
{
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json("hello")), "hello");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json(42)), "42");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json(3.14)), "3.14");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json(true)), "true");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json(nullptr)), "");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json::array({1,2})), "[1,2]");
    EXPECT_EQ(h2agent::model::jsonToString(nlohmann::json({{"k","v"}})), "{\"k\":\"v\"}");
}

// --- loadAtPath ---

TEST_F(Vault_test, LoadAtPathCreatesObject)
{
    vault_.loadAtPath("key1", "/name", nlohmann::json("alice"));
    bool exists = false;
    EXPECT_EQ(vault_.get("key1", exists), nlohmann::json({{"name","alice"}}));
    EXPECT_TRUE(exists);
}

TEST_F(Vault_test, LoadAtPathUpdatesExisting)
{
    vault_.load("key1", nlohmann::json({{"a",1},{"b",2}}));
    vault_.loadAtPath("key1", "/b", nlohmann::json(99));
    bool exists = false;
    EXPECT_EQ(vault_.get("key1", exists), nlohmann::json({{"a",1},{"b",99}}));
}

TEST_F(Vault_test, LoadAtPathNestedPath)
{
    vault_.loadAtPath("key1", "/x/y/z", nlohmann::json("deep"));
    bool exists = false;
    auto val = vault_.get("key1", exists);
    EXPECT_EQ(val[nlohmann::json::json_pointer("/x/y/z")], nlohmann::json("deep"));
}

TEST_F(Vault_test, LoadAtPathOverwritesNonObject)
{
    vault_.load("key1", nlohmann::json("just a string"));
    vault_.loadAtPath("key1", "/field", nlohmann::json(42));
    bool exists = false;
    EXPECT_EQ(vault_.get("key1", exists), nlohmann::json({{"field",42}}));
}

// --- loadJson with JSON types ---

TEST_F(Vault_test, LoadJsonWithMixedTypes)
{
    nlohmann::json mixed = R"({
        "str_key": "value",
        "num_key": 123,
        "obj_key": {"nested": true}
    })"_json;
    EXPECT_TRUE(vault_.loadJson(mixed));
    bool exists = false;
    EXPECT_EQ(vault_.get("str_key", exists), nlohmann::json("value"));
    EXPECT_EQ(vault_.get("num_key", exists), nlohmann::json(123));
    EXPECT_EQ(vault_.get("obj_key", exists), nlohmann::json({{"nested",true}}));
}

// Fetch-and-add: missing key from 0, integers and integer texts, negative deltas;
// other values are kept and rejected; no lost update under concurrency.
TEST_F(Vault_test, IncrementCounters)
{
    long long r = 0;
    EXPECT_TRUE(vault_.increment("c", 1, r));
    EXPECT_EQ(r, 1);
    EXPECT_TRUE(vault_.increment("c", 5, r));
    EXPECT_EQ(r, 6);
    EXPECT_TRUE(vault_.increment("c", -10, r));
    EXPECT_EQ(r, -4);
    nlohmann::json v;
    ASSERT_TRUE(vault_.tryGet("c", v));
    EXPECT_EQ(v, nlohmann::json(-4));  // stored as an integer

    vault_.load("t", nlohmann::json("41"));  // integer text (e.g. a literal)
    EXPECT_TRUE(vault_.increment("t", 1, r));
    EXPECT_EQ(r, 42);
    ASSERT_TRUE(vault_.tryGet("t", v));
    EXPECT_EQ(v, nlohmann::json(42));

    for (const nlohmann::json &bad : {
                nlohmann::json("4x"), nlohmann::json(" 4"), nlohmann::json(1.5), nlohmann::json(true), nlohmann::json::object(), nlohmann::json::array()
            }) {
        SCOPED_TRACE(bad.dump());
        vault_.load("b", bad);
        EXPECT_FALSE(vault_.increment("b", 1, r));
        ASSERT_TRUE(vault_.tryGet("b", v));
        EXPECT_EQ(v, bad);  // untouched
    }

    vault_.load("max", nlohmann::json(std::numeric_limits<long long>::max()));
    EXPECT_FALSE(vault_.increment("max", 1, r));  // overflow
}

TEST_F(Vault_test, IncrementIsAtomic)
{
    constexpr int threads = 8, increments = 2000;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++) {
        pool.emplace_back([this]() {
            long long r = 0;
            for (int i = 0; i < increments; i++) vault_.increment("shared", 1, r);
        });
    }
    for (auto &t : pool) t.join();
    nlohmann::json v;
    ASSERT_TRUE(vault_.tryGet("shared", v));
    EXPECT_EQ(v, nlohmann::json(threads * increments));
}
