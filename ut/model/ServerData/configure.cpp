#include <GlobalVariable.hpp>

#include <AdminSchemas.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Global variables configuration:
const nlohmann::json GlobalVariableConfiguration__Success = R"(
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2"
}
)"_json;

const nlohmann::json GlobalVariableConfiguration__BadSchema = R"({ "variable_name_1": "variable_value_1", "variable_name_2": 2 })"_json;


class ServerData_test : public ::testing::Test
{
public:
    h2agent::model::GlobalVariable gvars_;

    ServerData_test() {
        ;
    }
};

TEST_F(ServerData_test, LoadGlobalVariableSuccess)
{
    EXPECT_TRUE(ServerData_test::gvars_.loadJson(GlobalVariableConfiguration__Success));
    EXPECT_EQ(ServerData_test::gvars_.asJson(), GlobalVariableConfiguration__Success);

    bool exists{};
    EXPECT_EQ(ServerData_test::gvars_.getValue("variable_name_2", exists), "variable_value_2");
    EXPECT_TRUE(exists);
    ServerData_test::gvars_.removeVariable("variable_name_2");
    EXPECT_EQ(ServerData_test::gvars_.getValue("variable_name_2", exists), std::string(""));
    EXPECT_FALSE(exists);
    ServerData_test::gvars_.loadVariable("another_variable", "another_value");
    EXPECT_EQ(ServerData_test::gvars_.getValue("another_variable", exists), "another_value");
    EXPECT_TRUE(exists);

    EXPECT_TRUE(ServerData_test::gvars_.clear());
    EXPECT_FALSE(ServerData_test::gvars_.clear());
}

TEST_F(ServerData_test, LoadGlobalVariableFail)
{
    EXPECT_FALSE(ServerData_test::gvars_.loadJson(GlobalVariableConfiguration__BadSchema));
    EXPECT_EQ(ServerData_test::gvars_.asJsonString(), "{}");
}

TEST_F(ServerData_test, GetGlobalVariableSchema)
{
    EXPECT_EQ(ServerData_test::gvars_.getSchema().getJson(), h2agent::adminSchemas::server_data_global);
}

