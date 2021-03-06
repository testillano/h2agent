#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>
#include <AdminSchemaData.hpp>
#include <AdminSchemas.hpp>

// Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({"algorithm":"FullMatching"})"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({"algorithm":"FullMatchingRegexReplace","rgx":"([0-9]{3})-([a-z]{2})-foo-bar","fmt":"$1"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__Success = R"({"algorithm":"RegexMatching"})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success = R"({"algorithm":"FullMatching"})"_json; // default is Sort + Ampersand
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success2 = R"({"algorithm":"FullMatching","uriPathQueryParameters":{"filter":"Sort","separator":"Semicolon"}})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success3 = R"({"algorithm":"FullMatching","uriPathQueryParameters":{"filter":"PassBy"}})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success4 = R"({"algorithm":"FullMatching","uriPathQueryParameters":{"filter":"Ignore"}})"_json;

const nlohmann::json MatchingConfiguration__BadSchema = R"({"happy":true,"pi":3.141})"_json;
const nlohmann::json MatchingConfiguration_algorithm__BadSchema = R"({"algorithm":"unknown"})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__BadSchema = R"({"algorithm":"FullMatching","uriPathQueryParameters":"unknown"})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__BadSchema2 = R"({"algorithm":"FullMatching","uriPathQueryParameters":{"separator":"Semicolon"}})"_json;

const nlohmann::json MatchingConfiguration_FullMatching__BadContent = R"({"algorithm":"FullMatching","rgx":"whatever"})"_json;
const nlohmann::json MatchingConfiguration_FullMatching__BadContent2 = R"({"algorithm":"FullMatching","fmt":"whatever"})"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__BadContent = R"({"algorithm":"FullMatchingRegexReplace"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__BadContent = R"({"algorithm":"RegexMatching","rgx":"whatever"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__BadContent2 = R"({"algorithm":"RegexMatching","fmt":"whatever"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__BadContent3 = R"({"algorithm":"RegexMatching","rgx":"("})"_json;

// Provision configuration:
const nlohmann::json ProvisionConfiguration__Success = R"(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar/1?name=test",
  "responseCode": 200,
  "responseBody": {
    "foo": "bar-1"
  },
  "responseHeaders": {
    "content-type": "text/html",
    "x-version": "1.0.0"
  },
  "transform": [
    {
      "source": "random.10.30",
      "target": "response.body.json.integer./randomBetween10and30"
    }
  ],
  "requestSchemaId": "myRequestsSchema",
  "responseSchemaId": "myResponsesSchema"
}
)"_json;

const nlohmann::json ProvisionConfiguration__SuccessRegex = R"delim(
{
  "requestMethod": "GET",
  "requestUri": "(/foo/bar/)([0-9]{3})",
        "responseCode": 200
}
)delim"_json;

const nlohmann::json ProvisionConfiguration__SuccessArray = R"(
[
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/1",
    "responseCode": 200
  },
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/2",
    "responseCode": 200
  }
]
)"_json;

const nlohmann::json ProvisionConfiguration__BadSchema = R"({ "happy": true, "pi": 3.141 })"_json;
const nlohmann::json ProvisionConfiguration__BadContent = R"(
{
  "requestMethod": "GET",
  "requestUri": "bad regular expression due to the slash \\",
  "responseCode": 200
}
)"_json;
const nlohmann::json ProvisionConfiguration__BadContent2 = R"(
{
  "requestMethod": "GET",
  "requestUri": "/any/uri",
  "responseCode": 200,
  "requestSchemaId": ""
}
)"_json;
const nlohmann::json ProvisionConfiguration__BadContent3 = R"(
{
  "requestMethod": "GET",
  "requestUri": "/any/uri",
  "responseCode": 200,
  "responseSchemaId": ""
}
)"_json;

const nlohmann::json ProvisionConfiguration__BadContentArray = R"(
[
  {
    "requestMethod": "GET",
    "requestUri": "(/foo/bar/)[0-9]{3}",
    "responseCode": 200
  },
  {
    "requestMethod": "GET",
    "requestUri": "bad regular expression due to the slash \\",
    "responseCode": 200
  }
]
)"_json;

// Schema configuration:
const nlohmann::json SchemaConfiguration__Success = R"(
{
  "id": "myRequestsSchema",
  "schema": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": false,
    "properties": {
      "foo": {
        "type": "string"
      }
    },
    "required": [
      "foo"
    ]
  }
}
)"_json;

const nlohmann::json SchemaConfiguration__SuccessArray = R"(
[
  {
    "id": "myRequestsSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "foo": {
          "type": "string"
        }
      },
      "required": [
        "foo"
      ]
    }
  },
  {
    "id": "myResponsesSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "bar": {
          "type": "number"
        }
      },
      "required": [
        "bar"
      ]
    }
  }
]
)"_json;

const nlohmann::json SchemaConfiguration__BadSchema = R"({ "happy": true, "pi": 3.141 })"_json;
/*
const nlohmann::json SchemaConfiguration__BadContent = R"(
{
  "id": "myRequestsSchema",
  "schema": {} <- TODO: find a invalid schema definition
}
)"_json;

const nlohmann::json SchemaConfiguration__BadContentArray = R"(
[
  {
    "id": "myRequestsSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "foo": {
          "type": "string"
        }
      },
      "required": [
        "foo"
      ]
    }
  },
  {
    "id": "myResponsesSchema",
    "schema": {} <- TODO: find a invalid schema definition
  }
]
)"_json;
*/

class Configure_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::AdminSchema aschema_{};

    Configure_test() {
        ;
        //adata_.loadMatching(JsonMatching);
    }
};

TEST_F(Configure_test, LoadMatching)
{
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.getMatchingData().getJson(), MatchingConfiguration_FullMatching__Success);
    EXPECT_EQ(Configure_test::adata_.getMatchingData().getSchema().getJson(), h2agent::adminSchemas::server_matching);

    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__Success), h2agent::model::AdminServerMatchingData::Success);
    h2agent::model::AdminServerMatchingData::AlgorithmType algorithm = Configure_test::adata_.getMatchingData().getAlgorithm(); // FullMatchingRegexReplace
    EXPECT_EQ(algorithm,  h2agent::model::AdminServerMatchingData::FullMatchingRegexReplace);
    std::string fmt = Configure_test::adata_.getMatchingData().getFmt(); // $1
    EXPECT_EQ(fmt, "$1");
    std::regex rgx = Configure_test::adata_.getMatchingData().getRgx(); // ([0-9]{3})-([a-z]{2})-foo-bar
    std::string result = std::regex_replace ("123-ab-foo-bar", rgx, fmt);
    EXPECT_EQ(result, "123");

    //EXPECT_EQ(Configure_test::adata_.getMatchingData().getRgx(), re);

    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.getMatchingData().getUriPathQueryParametersFilter(), h2agent::model::AdminServerMatchingData::Sort);
    EXPECT_EQ(Configure_test::adata_.getMatchingData().getUriPathQueryParametersSeparator(), h2agent::model::AdminServerMatchingData::Ampersand);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__Success2), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__Success3), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__Success4), h2agent::model::AdminServerMatchingData::Success);

    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_algorithm__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParameters__BadSchema2), h2agent::model::AdminServerMatchingData::BadSchema);

    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent2), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__BadContent2), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__BadContent3), h2agent::model::AdminServerMatchingData::BadContent);
}

TEST_F(Configure_test, LoadProvisionSuccess)
{
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__Success), h2agent::model::AdminServerProvisionData::Success);
    nlohmann::json jarray = nlohmann::json::array();
    jarray.push_back(ProvisionConfiguration__Success);
    EXPECT_EQ(Configure_test::adata_.getProvisionData().asJsonString(), jarray.dump());
    //EXPECT_TRUE(Configure_test::adata_.clearProvisions());

    // two ordered provisions:
    nlohmann::json anotherProvision = ProvisionConfiguration__Success;
    anotherProvision["requestUri"] = std::string(ProvisionConfiguration__Success["requestUri"]) + "_bis";
    EXPECT_EQ(Configure_test::adata_.loadProvision(anotherProvision), h2agent::model::AdminServerProvisionData::Success);
    jarray.push_back(anotherProvision);
    EXPECT_EQ(Configure_test::adata_.getProvisionData().asJsonString(true /* ordered */), jarray.dump());
    EXPECT_TRUE(Configure_test::adata_.clearProvisions());

    // provision array
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__SuccessArray), h2agent::model::AdminServerProvisionData::Success);
    EXPECT_TRUE(Configure_test::adata_.clearProvisions());
}

TEST_F(Configure_test, LoadProvisionFail)
{
    // Bad schema
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__BadSchema), h2agent::model::AdminServerProvisionData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.getProvisionData().asJsonString(), "[]");
    EXPECT_EQ(Configure_test::adata_.getProvisionData().getSchema().getJson(), h2agent::adminSchemas::server_provision);
    EXPECT_FALSE(Configure_test::adata_.clearProvisions());

    // Bad content due to bad regex (using RegexMatching):
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__BadContent), h2agent::model::AdminServerProvisionData::BadContent);
    // Bad content with empty request schema id:
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__BadContent2), h2agent::model::AdminServerProvisionData::BadContent);
    // Bad content with empty response schema id:
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__BadContent3), h2agent::model::AdminServerProvisionData::BadContent);

    EXPECT_FALSE(Configure_test::adata_.clearProvisions());

    // Bad content array:
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__BadContentArray), h2agent::model::AdminServerProvisionData::BadContent);
    EXPECT_TRUE(Configure_test::adata_.clearProvisions());
}

TEST_F(Configure_test, FindProvisionRegex)
{
    // Bad content only happens for RegexMatching:
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__SuccessRegex), h2agent::model::AdminServerProvisionData::Success);

    EXPECT_TRUE(Configure_test::adata_.getProvisionData().findRegexMatching("initial", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getProvisionData().findRegexMatching("missing", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getProvisionData().findRegexMatching("initial", "POST", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getProvisionData().findRegexMatching("initial", "GET", "/foo/bar/12345") != nullptr);
    EXPECT_TRUE(Configure_test::adata_.clearProvisions());
}

TEST_F(Configure_test, FindProvision)
{
    // Bad content only happens for RegexMatching:
    EXPECT_EQ(Configure_test::adata_.loadMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadProvision(ProvisionConfiguration__Success), h2agent::model::AdminServerProvisionData::Success);

    EXPECT_FALSE(Configure_test::adata_.getProvisionData().find("missing", "GET", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getProvisionData().find("initial", "POST", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=missing") != nullptr);

    auto provision = Configure_test::adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    EXPECT_TRUE(provision != nullptr);

    EXPECT_EQ(provision->getResponseBodyString(), "{\"foo\":\"bar-1\"}");
    EXPECT_EQ(provision->getRequestSchemaId(), "myRequestsSchema");
    EXPECT_EQ(provision->getResponseSchemaId(), "myResponsesSchema");

    EXPECT_TRUE(Configure_test::adata_.clearProvisions());
}

TEST_F(Configure_test, LoadSchemaSuccess)
{
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__Success), h2agent::model::AdminSchemaData::Success);
    nlohmann::json jarray = nlohmann::json::array();
    jarray.push_back(SchemaConfiguration__Success);
    EXPECT_EQ(Configure_test::adata_.getSchemaData().asJsonString(), jarray.dump());
    //EXPECT_TRUE(Configure_test::adata_.clearSchemas());

    // schema array
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__SuccessArray), h2agent::model::AdminSchemaData::Success);
    EXPECT_TRUE(Configure_test::adata_.clearSchemas());
}

TEST_F(Configure_test, LoadSchemaFail)
{
    // Bad schema
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__BadSchema), h2agent::model::AdminSchemaData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.getSchemaData().asJsonString(), "[]");
    EXPECT_EQ(Configure_test::adata_.getSchemaData().getSchema().getJson(), h2agent::adminSchemas::schema);
    EXPECT_FALSE(Configure_test::adata_.clearSchemas());

//    // Bad content with the schema is not a schema
//    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__BadContent), h2agent::model::AdminSchemaData::BadContent);
//    EXPECT_FALSE(Configure_test::adata_.clearSchemas());

//    // Bad content array:
//    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__BadContentArray), h2agent::model::AdminSchemaData::BadContent);
//    EXPECT_TRUE(Configure_test::adata_.clearSchemas());
}

TEST_F(Configure_test, FindSchema)
{
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__Success), h2agent::model::AdminSchemaData::Success);

    EXPECT_TRUE(Configure_test::adata_.getSchemaData().find("myRequestsSchema") != nullptr);
    EXPECT_TRUE(Configure_test::adata_.clearSchemas());
}

TEST_F(Configure_test, ValidateSchema)
{
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__Success), h2agent::model::AdminSchemaData::Success);

    auto schemaData = Configure_test::adata_.getSchemaData().find("myRequestsSchema");
    EXPECT_TRUE(schemaData != nullptr);

    EXPECT_TRUE(schemaData->validate(R"({"foo":"bar"})"_json));
}

