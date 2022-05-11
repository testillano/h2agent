#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


const nlohmann::json ServerMatchingJson = R"({"algorithm":"FullMatching"})"_json;
const nlohmann::json ServerMatchingJson_nok = R"({"foo":"bar"})"_json; // mandatory missing: algorithm
const nlohmann::json ServerProvisionJson = R"({"requestMethod": "GET", "requestUri": "/foo/bar", "responseCode": 200})"_json;
const nlohmann::json ServerProvisionJson_nok = R"({"requestUri": "/foo/bar", "responseCode": 200})"_json; // mandatory missing: requestMethod
const nlohmann::json SchemaJson = R"({"id": "myRequestsSchema", "schema": {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":false,"properties":{"foo":{"type":"string"}},"required":["foo"]}})"_json;
const nlohmann::json SchemaJson_nok = R"({"id": "myRequestsSchema"})"_json; // mandatory missing: schema
const nlohmann::json GlobalVariableJson = R"({"var1": "value1", "var2": "value2"})"_json;
const nlohmann::json GlobalVariableJson_nok = R"({"var1": 1})"_json; // non-string value
const nlohmann::json GlobalVariableJson_nok2 = R"({"var1": {"foo":"bar"}})"_json; // non-string value



class jsonSchema_test : public ::testing::Test
{
public:
    h2agent::jsonschema::JsonSchema json_schema_;
};

TEST_F(jsonSchema_test, invalidSchema)
{
    nlohmann::json not_an_schema{};
    EXPECT_FALSE(jsonSchema_test::json_schema_.setJson(not_an_schema));
    EXPECT_FALSE(jsonSchema_test::json_schema_.isAvailable());
}

TEST_F(jsonSchema_test, checkSchemaMatching)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::server_matching));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::server_matching);
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ServerMatchingJson));
}

TEST_F(jsonSchema_test, checkSchemaProvision)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::server_provision));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::server_provision);
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ServerProvisionJson));
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(ServerProvisionJson_nok));
}

TEST_F(jsonSchema_test, checkSchemaSchema)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::schema));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::schema);
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(SchemaJson));
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(SchemaJson_nok));
}

TEST_F(jsonSchema_test, checkGlobalVariablesSchema)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::server_data_global));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::server_data_global);
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(GlobalVariableJson));
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(GlobalVariableJson_nok));
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(GlobalVariableJson_nok2));
}

