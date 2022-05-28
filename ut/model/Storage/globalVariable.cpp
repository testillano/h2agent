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


class GlobalVariable_test : public ::testing::Test
{
public:
    h2agent::model::GlobalVariable gvars_{};

    GlobalVariable_test() {
        ;
    }
};

TEST_F(GlobalVariable_test, LoadGlobalVariableSuccess)
{
    EXPECT_TRUE(GlobalVariable_test::gvars_.loadJson(GlobalVariableConfiguration__Success));
    EXPECT_EQ(GlobalVariable_test::gvars_.asJson(), GlobalVariableConfiguration__Success);

    bool exists = false;
    EXPECT_EQ(GlobalVariable_test::gvars_.getValue("variable_name_2", exists), "variable_value_2");
    EXPECT_TRUE(exists);
    GlobalVariable_test::gvars_.removeVariable("variable_name_2");
    EXPECT_EQ(GlobalVariable_test::gvars_.getValue("variable_name_2", exists), std::string(""));
    EXPECT_FALSE(exists);
    GlobalVariable_test::gvars_.loadVariable("another_variable", "another_value");
    EXPECT_EQ(GlobalVariable_test::gvars_.getValue("another_variable", exists), "another_value");
    EXPECT_TRUE(exists);

    EXPECT_TRUE(GlobalVariable_test::gvars_.clear());
    EXPECT_FALSE(GlobalVariable_test::gvars_.clear());
}

TEST_F(GlobalVariable_test, LoadGlobalVariableFail)
{
    EXPECT_FALSE(GlobalVariable_test::gvars_.loadJson(GlobalVariableConfiguration__BadSchema));
    EXPECT_EQ(GlobalVariable_test::gvars_.asJsonString(), "{}");
}

TEST_F(GlobalVariable_test, GetGlobalVariableSchema)
{
    EXPECT_EQ(GlobalVariable_test::gvars_.getSchema().getJson(), h2agent::adminSchemas::server_data_global);
}

