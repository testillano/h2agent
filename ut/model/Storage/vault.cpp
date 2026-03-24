#include <Vault.hpp>

#include <AdminSchemas.hpp>

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

