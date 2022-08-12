#include <Configuration.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// General configuration:
const nlohmann::json DefaultConfiguration = R"(
{
  "longTermFilesCloseDelayUsecs": 1000000,
  "shortTermFilesCloseDelayUsecs": 0
}
)"_json;

class Configuration_test : public ::testing::Test
{
public:
    h2agent::model::Configuration configuration_{};

    Configuration_test() {
        ;
    }
};

TEST_F(Configuration_test, Defaults)
{
    EXPECT_EQ(Configuration_test::configuration_.getLongTermFilesCloseDelayUsecs(), DefaultConfiguration["longTermFilesCloseDelayUsecs"]);
    EXPECT_EQ(Configuration_test::configuration_.getShortTermFilesCloseDelayUsecs(), DefaultConfiguration["shortTermFilesCloseDelayUsecs"]);
}

TEST_F(Configuration_test, Getters)
{
    Configuration_test::configuration_.setLongTermFilesCloseDelayUsecs(2000000);
    EXPECT_EQ(Configuration_test::configuration_.getLongTermFilesCloseDelayUsecs(), 2000000);

    Configuration_test::configuration_.setShortTermFilesCloseDelayUsecs(5000);
    EXPECT_EQ(Configuration_test::configuration_.getShortTermFilesCloseDelayUsecs(), 5000);
}

TEST_F(Configuration_test, GetJson)
{
    EXPECT_EQ(Configuration_test::configuration_.getJson(), DefaultConfiguration);
}

TEST_F(Configuration_test, AsJsonString)
{
    EXPECT_EQ(Configuration_test::configuration_.asJsonString(), DefaultConfiguration.dump());
}

