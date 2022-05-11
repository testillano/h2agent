#include <GlobalVariablesData.hpp>

#include <AdminSchemas.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Global variables configuration:
const nlohmann::json GlobalVariablesConfiguration__Success = R"(
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2"
}
)"_json;

const nlohmann::json GlobalVariablesConfiguration__BadSchema = R"({ "variable_name_1": "variable_value_1", "variable_name_2": 2 })"_json;


class ServerData_test : public ::testing::Test
{
public:
    h2agent::model::GlobalVariablesData gvardata_;

    ServerData_test() {
        ;
    }
};

TEST_F(ServerData_test, LoadGlobalVariablesSuccess)
{
    EXPECT_TRUE(ServerData_test::gvardata_.loadJson(GlobalVariablesConfiguration__Success));
    EXPECT_EQ(ServerData_test::gvardata_.asJson(), GlobalVariablesConfiguration__Success);

    bool exists{};
    EXPECT_EQ(ServerData_test::gvardata_.getValue("variable_name_2", exists), "variable_value_2");
    EXPECT_TRUE(exists);
    ServerData_test::gvardata_.removeVariable("variable_name_2");
    EXPECT_EQ(ServerData_test::gvardata_.getValue("variable_name_2", exists), std::string(""));
    EXPECT_FALSE(exists);
    ServerData_test::gvardata_.loadVariable("another_variable", "another_value");
    EXPECT_EQ(ServerData_test::gvardata_.getValue("another_variable", exists), "another_value");
    EXPECT_TRUE(exists);

    EXPECT_TRUE(ServerData_test::gvardata_.clear());
    EXPECT_FALSE(ServerData_test::gvardata_.clear());
}

TEST_F(ServerData_test, LoadGlobalVariablesFail)
{
    EXPECT_FALSE(ServerData_test::gvardata_.loadJson(GlobalVariablesConfiguration__BadSchema));
    EXPECT_EQ(ServerData_test::gvardata_.asJsonString(), "{}");
}

TEST_F(ServerData_test, GetGlobalVariablesSchema)
{
    EXPECT_EQ(ServerData_test::gvardata_.getSchema().getJson(), h2agent::adminSchemas::server_data_global);
}

