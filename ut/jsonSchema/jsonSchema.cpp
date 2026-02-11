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
const nlohmann::json ClientEndpointJson = R"({"id": "myClientEndpoint", "host": "localhost", "port": 8000, "secure": false, "permit": true})"_json;
const nlohmann::json ClientEndpointJson_nok = R"({"host": "localhost", "port": 8000, "secure": false, "permit": true})"_json; // mandatory missing: id


class jsonSchema_test : public ::testing::Test
{
public:
    h2agent::jsonschema::JsonSchema json_schema_{};
};

TEST_F(jsonSchema_test, InvalidSchema)
{
    nlohmann::json not_an_schema;
    EXPECT_FALSE(jsonSchema_test::json_schema_.setJson(not_an_schema));
    EXPECT_FALSE(jsonSchema_test::json_schema_.isAvailable());
}

TEST_F(jsonSchema_test, CheckSchemaMatching)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::server_matching));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::server_matching);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ServerMatchingJson, error));
    EXPECT_TRUE(error.empty());
}

TEST_F(jsonSchema_test, CheckSchemaServerProvision)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::server_provision));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::server_provision);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ServerProvisionJson, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(ServerProvisionJson_nok, error));
    EXPECT_EQ(error, "At  of {\"requestUri\":\"/foo/bar\",\"responseCode\":200} - required property 'requestMethod' not found in object\n");
}

TEST_F(jsonSchema_test, CheckSchemaSchema)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::schema));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::schema);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(SchemaJson, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(SchemaJson_nok, error));
    EXPECT_EQ(error, "At  of {\"id\":\"myRequestsSchema\"} - required property 'schema' not found in object\n");
}

TEST_F(jsonSchema_test, CheckGlobalVariableSchema)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::global_variable));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::global_variable);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(GlobalVariableJson, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(GlobalVariableJson_nok, error));
    EXPECT_EQ(error, "At /var1 of 1 - no subschema has succeeded, but one of them is required to validate\n");
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(GlobalVariableJson_nok2, error));
    EXPECT_EQ(error, "At /var1 of {\"foo\":\"bar\"} - no subschema has succeeded, but one of them is required to validate\n");
}

TEST_F(jsonSchema_test, CheckSchemaClientEndpoint)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::client_endpoint));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::client_endpoint);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ClientEndpointJson, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(ClientEndpointJson_nok, error));
    EXPECT_EQ(error, "At  of {\"host\":\"localhost\",\"permit\":true,\"port\":8000,\"secure\":false} - required property 'id' not found in object\n");
}

/*
TEST_F(jsonSchema_test, CheckSchemaClientProvision)
{
    EXPECT_TRUE(jsonSchema_test::json_schema_.setJson(h2agent::adminSchemas::client_provision));
    EXPECT_TRUE(jsonSchema_test::json_schema_.isAvailable());
    EXPECT_EQ(jsonSchema_test::json_schema_.getJson(), h2agent::adminSchemas::client_provision);
    std::string error{};
    EXPECT_TRUE(jsonSchema_test::json_schema_.validate(ClientProvisionJson, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(jsonSchema_test::json_schema_.validate(ClientProvisionJson_nok, error));
    EXPECT_EQ(error, "xxxxxxxxx");
}
*/
