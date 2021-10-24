#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


const nlohmann::json ServerMatchingJson = R"({"algorithm":"FullMatching"})"_json;
const nlohmann::json ServerProvisionJson = R"({"requestMethod": "GET", "requestUri": "/foo/bar", "responseCode": 200})"_json;

class jsonSchema_test : public ::testing::Test
{
public:
    h2agent::jsonschema::JsonSchema json_schema_;
};

TEST_F(jsonSchema_test, validSchema1)
{
    ASSERT_TRUE(jsonSchema_test::json_schema_.setSchema(h2agent::adminSchemas::server_matching));
    ASSERT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getSchema(), h2agent::adminSchemas::server_matching);
    ASSERT_TRUE(jsonSchema_test::json_schema_.validate(ServerMatchingJson));
}

TEST_F(jsonSchema_test, validSchema2)
{
    ASSERT_TRUE(jsonSchema_test::json_schema_.setSchema(h2agent::adminSchemas::server_provision));
    ASSERT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getSchema(), h2agent::adminSchemas::server_provision);
    ASSERT_TRUE(jsonSchema_test::json_schema_.validate(ServerProvisionJson));
}

TEST_F(jsonSchema_test, invalidSchema)
{
    nlohmann::json not_an_schema{};
    ASSERT_FALSE(jsonSchema_test::json_schema_.setSchema(not_an_schema));
    ASSERT_FALSE(jsonSchema_test::json_schema_.isAvailable());
}

