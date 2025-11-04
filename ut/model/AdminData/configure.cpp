#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>
#include <AdminClientEndpointData.hpp>
#include <AdminClientProvisionData.hpp> // XXXXXXXXXXXXXXXXXXXXXXXX
#include <AdminSchemaData.hpp>
#include <AdminSchemas.hpp>
#include <Configuration.hpp>

// Server Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({"algorithm":"FullMatching"})"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({"algorithm":"FullMatchingRegexReplace","rgx":"([0-9]{3})-([a-z]{2})-foo-bar","fmt":"$1"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__Success = R"({"algorithm":"RegexMatching"})"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success = R"({"algorithm":"FullMatching"})"_json; // default is Sort + Ampersand
const nlohmann::json MatchingConfiguration_uriPathQueryParameters__SuccessDefault = R"({"algorithm":"FullMatching","uriPathQueryParameters":{"filter":"Sort","separator":"Ampersand"}})"_json;
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

// Server Provision configuration:
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

// Client Endpoint configuration:
const nlohmann::json ClientEndpointConfiguration__Success = R"(
{
  "id": "myServer",
  "host": "localhost",
  "port": 8000,
  "secure": true,
  "permit": false
}
)"_json;

const nlohmann::json ClientEndpointConfiguration__SuccessArray = R"(
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000
  },
  {
    "id": "myServer2",
    "host": "localhost2",
    "port": 8000
  }
]
)"_json;

const nlohmann::json ClientEndpointConfiguration__AcceptedArrayChangePort = R"(
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000
  },
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 9000
  }
]
)"_json;

const nlohmann::json ClientEndpointConfiguration__AcceptedArrayChangeSecure = R"(
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000,
    "secure": true
  },
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000,
    "secure": false
  }
]
)"_json;

const nlohmann::json ClientEndpointConfiguration__AcceptedArrayNoChanges = R"(
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000,
    "secure": false
  },
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000,
    "secure": false
  }
]
)"_json;

const nlohmann::json ClientEndpointConfiguration__BadSchema = R"({ "permit": true })"_json;
const nlohmann::json ClientEndpointConfiguration__BadSchema2 = R"(
{
  "id": "myServer",
  "host": "localhost",
  "port": 99999
}
)"_json;

const nlohmann::json ClientEndpointConfiguration__BadContent = R"(
{
  "id": "",
  "host": "localhost",
  "port": 8000
}
)"_json;

const nlohmann::json ClientEndpointConfiguration__BadContent2 = R"(
{
  "id": "myServer",
  "host": "",
  "port": 8000
}
)"_json;

const nlohmann::json ClientEndpointConfiguration__BadContentArray = R"(
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000
  },
  {
    "id": "",
    "host": "localhost2",
    "port": 8000
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

const nlohmann::json SchemaConfiguration__BadContent = R"(
{
  "id": "myRequestsSchema",
  "schema": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string"
      },
      "age": {
        "type": "integer"
      }
    },
    "required": [
      "name"
    ],
    "$ref": "external-schema.json"
  }
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
    "id": "myRequestsSchema",
    "schema": {
      "type": "object",
      "properties": {
        "name": {
          "type": "string"
        },
        "age": {
          "type": "integer"
        }
      },
      "required": [
        "name"
      ],
      "$ref": "external-schema.json"
    }
  }
]
)"_json;


class Configure_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::AdminSchema aschema_{};
    h2agent::model::common_resources_t common_resources_{}; // we won't execute provisions, but this is required by loadServerProvision
    h2agent::model::Configuration configuration_{}; // process static configuration

    Configure_test() {
        ;
        //adata_.loadServerMatching(JsonMatching);
        configuration_.setLazyClientConnection(true); // no client connection in unit-test
        common_resources_.AdminDataPtr = &adata_;
        common_resources_.ConfigurationPtr = &configuration_;
    }
};

TEST_F(Configure_test, LoadMatching)
{
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.getServerMatchingData().getJson(), MatchingConfiguration_FullMatching__Success);
    EXPECT_EQ(Configure_test::adata_.getServerMatchingData().getSchema().getJson(), h2agent::adminSchemas::server_matching);

    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_FullMatchingRegexReplace__Success), h2agent::model::AdminServerMatchingData::Success);
    h2agent::model::AdminServerMatchingData::AlgorithmType algorithm = Configure_test::adata_.getServerMatchingData().getAlgorithm(); // FullMatchingRegexReplace
    EXPECT_EQ(algorithm,  h2agent::model::AdminServerMatchingData::FullMatchingRegexReplace);
    std::string fmt = Configure_test::adata_.getServerMatchingData().getFmt(); // $1
    EXPECT_EQ(fmt, "$1");
    std::regex rgx = Configure_test::adata_.getServerMatchingData().getRgx(); // ([0-9]{3})-([a-z]{2})-foo-bar
    std::string result = std::regex_replace ("123-ab-foo-bar", rgx, fmt);
    EXPECT_EQ(result, "123");

    //EXPECT_EQ(Configure_test::adata_.getServerMatchingData().getRgx(), re);

    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__SuccessDefault), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.getServerMatchingData().getUriPathQueryParametersFilter(), h2agent::model::AdminServerMatchingData::Sort);
    EXPECT_EQ(Configure_test::adata_.getServerMatchingData().getUriPathQueryParametersSeparator(), h2agent::model::AdminServerMatchingData::Ampersand);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__Success2), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__Success3), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__Success4), h2agent::model::AdminServerMatchingData::Success);

    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_algorithm__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__BadSchema), h2agent::model::AdminServerMatchingData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_uriPathQueryParameters__BadSchema2), h2agent::model::AdminServerMatchingData::BadSchema);

    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_FullMatching__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_FullMatching__BadContent2), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_FullMatchingRegexReplace__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__BadContent), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__BadContent2), h2agent::model::AdminServerMatchingData::BadContent);
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__BadContent3), h2agent::model::AdminServerMatchingData::BadContent);
}

TEST_F(Configure_test, LoadProvisionSuccess)
{
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__Success, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    nlohmann::json jarray = nlohmann::json::array();
    jarray.push_back(ProvisionConfiguration__Success);
    EXPECT_EQ(Configure_test::adata_.getServerProvisionData().asJsonString(), jarray.dump());
    //EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());


    // two ordered provisions:
    nlohmann::json anotherProvision = ProvisionConfiguration__Success;
    anotherProvision["requestUri"] = std::string(ProvisionConfiguration__Success["requestUri"]) + "_bis";
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(anotherProvision, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    jarray.push_back(anotherProvision);
    EXPECT_EQ(Configure_test::adata_.getServerProvisionData().asJsonString(true /* ordered */), jarray.dump());
    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());

    // provision array
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__SuccessArray, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());
}

TEST_F(Configure_test, LoadProvisionFail)
{
    // Bad schema
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__BadSchema, common_resources_), h2agent::model::AdminServerProvisionData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.getServerProvisionData().asJsonString(), "[]");
    EXPECT_EQ(Configure_test::adata_.getServerProvisionData().getSchema().getJson(), h2agent::adminSchemas::server_provision);
    EXPECT_FALSE(Configure_test::adata_.clearServerProvisions());

    // Bad content due to bad regex (using RegexMatching):
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__BadContent, common_resources_), h2agent::model::AdminServerProvisionData::BadContent);
    // Bad content with empty request schema id:
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__BadContent2, common_resources_), h2agent::model::AdminServerProvisionData::BadContent);
    // Bad content with empty response schema id:
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__BadContent3, common_resources_), h2agent::model::AdminServerProvisionData::BadContent);
    EXPECT_FALSE(Configure_test::adata_.clearServerProvisions());

    // Bad content array:
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__BadContentArray, common_resources_), h2agent::model::AdminServerProvisionData::BadContent);
    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());
}

TEST_F(Configure_test, FindProvisionRegex)
{
    // Bad content only happens for RegexMatching:
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__SuccessRegex, common_resources_), h2agent::model::AdminServerProvisionData::Success);

    EXPECT_TRUE(Configure_test::adata_.getServerProvisionData().findRegexMatching("initial", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().findRegexMatching("missing", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().findRegexMatching("initial", "POST", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().findRegexMatching("initial", "GET", "/foo/bar/12345") != nullptr);
    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());
}

TEST_F(Configure_test, FindProvision)
{
    // Bad content only happens for RegexMatching:
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__Success, common_resources_), h2agent::model::AdminServerProvisionData::Success);

    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().find("missing", "GET", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().find("initial", "POST", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(Configure_test::adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=missing") != nullptr);

    auto provision = Configure_test::adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    EXPECT_TRUE(provision != nullptr);

    EXPECT_EQ(provision->getResponseBodyAsString(), "{\"foo\":\"bar-1\"}");
    EXPECT_EQ(provision->getRequestSchema(), nullptr);
    EXPECT_EQ(provision->getResponseSchema(), nullptr);

    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions());
}

TEST_F(Configure_test, Delete)
{
    // Bad content only happens for RegexMatching:
    EXPECT_EQ(Configure_test::adata_.loadServerMatching(MatchingConfiguration_RegexMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Configure_test::adata_.loadServerProvision(ProvisionConfiguration__Success, common_resources_), h2agent::model::AdminServerProvisionData::Success);

    EXPECT_TRUE(Configure_test::adata_.clearServerProvisions()); // 200
    EXPECT_FALSE(Configure_test::adata_.clearServerProvisions()); // 204
}

TEST_F(Configure_test, LoadClientEndpointSuccess)
{
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__Success, common_resources_), h2agent::model::AdminClientEndpointData::Success);
    nlohmann::json jarray = nlohmann::json::array();
    jarray.push_back(ClientEndpointConfiguration__Success);
    EXPECT_EQ(Configure_test::adata_.getClientEndpointData().asJsonString(), jarray.dump());
    //EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());

    // two client endpoints:
    nlohmann::json anotherClientEndpoint = ClientEndpointConfiguration__Success;
    anotherClientEndpoint["id"] = std::string(ClientEndpointConfiguration__Success["id"]) + "_bis";
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(anotherClientEndpoint, common_resources_), h2agent::model::AdminClientEndpointData::Success);
    jarray.push_back(anotherClientEndpoint);
    // Reverse array to compare (not ordered map):
    nlohmann::json rjarray = nlohmann::json::array();
    for (auto it = jarray.rbegin(); it != jarray.rend(); it++) {
        rjarray.push_back(*it);
    }
    EXPECT_EQ(Configure_test::adata_.getClientEndpointData().asJsonString(), rjarray.dump());
    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());

    // client endpoint array
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__SuccessArray, common_resources_), h2agent::model::AdminClientEndpointData::Success);
    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());

    // client endpoint accepted array
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__AcceptedArrayChangePort, common_resources_), h2agent::model::AdminClientEndpointData::Accepted);
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__AcceptedArrayChangeSecure, common_resources_), h2agent::model::AdminClientEndpointData::Accepted);
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__AcceptedArrayNoChanges, common_resources_), h2agent::model::AdminClientEndpointData::Success);
    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());
}

TEST_F(Configure_test, LoadClientEndpointFail)
{
    // Bad schema
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__BadSchema, common_resources_), h2agent::model::AdminClientEndpointData::BadSchema);
    EXPECT_EQ(Configure_test::adata_.getClientEndpointData().asJsonString(), "[]");
    EXPECT_EQ(Configure_test::adata_.getClientEndpointData().getSchema().getJson(), h2agent::adminSchemas::client_endpoint);
    // Bad Schema due to invalid port:
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__BadSchema2, common_resources_), h2agent::model::AdminClientEndpointData::BadSchema);
    EXPECT_FALSE(Configure_test::adata_.clearClientEndpoints());

    // Bad content due to empty id:
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__BadContent, common_resources_), h2agent::model::AdminClientEndpointData::BadContent);
    // Bad content due to empty host:
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__BadContent2, common_resources_), h2agent::model::AdminClientEndpointData::BadContent);
    EXPECT_FALSE(Configure_test::adata_.clearClientEndpoints());

    // Bad content array:
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__BadContentArray, common_resources_), h2agent::model::AdminClientEndpointData::BadContent);
    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());
}

TEST_F(Configure_test, FindClientEndpoint)
{
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__Success, common_resources_), h2agent::model::AdminClientEndpointData::Success);

    auto clientEndpoint = Configure_test::adata_.getClientEndpointData().find("myServer");
    EXPECT_TRUE(clientEndpoint != nullptr);

    EXPECT_EQ(clientEndpoint->getKey(), "myServer");
    EXPECT_EQ(clientEndpoint->getHost(), "localhost");
    EXPECT_EQ(clientEndpoint->getPort(), 8000);
    EXPECT_EQ(clientEndpoint->getSecure(), true);
    EXPECT_EQ(clientEndpoint->getPermit(), false);
    EXPECT_EQ(clientEndpoint->asJson(), ClientEndpointConfiguration__Success);

    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints());
}

TEST_F(Configure_test, ConnectClientEndpoint)
{
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__Success, common_resources_), h2agent::model::AdminClientEndpointData::Success);

    auto clientEndpoint = Configure_test::adata_.getClientEndpointData().find("myServer");
    EXPECT_TRUE(clientEndpoint != nullptr);

    // From scratch: false
    clientEndpoint->connect();
    nlohmann::json expectedJson = ClientEndpointConfiguration__Success;
    expectedJson["status"] = "Closed";
    EXPECT_EQ(clientEndpoint->asJson(), expectedJson);

    // From scratch: true (client endpoint asJson is the same: no connection takes place)
    clientEndpoint->connect(true);
    EXPECT_EQ(clientEndpoint->asJson(), expectedJson);
}

TEST_F(Configure_test, DeleteClientEndpoint)
{
    EXPECT_EQ(Configure_test::adata_.loadClientEndpoint(ClientEndpointConfiguration__Success, common_resources_), h2agent::model::AdminClientEndpointData::Success);

    EXPECT_TRUE(Configure_test::adata_.clearClientEndpoints()); // 200
    EXPECT_FALSE(Configure_test::adata_.clearClientEndpoints()); // 204
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

    // Bad content with the schema is not a schema
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__BadContent), h2agent::model::AdminSchemaData::BadContent);
    EXPECT_FALSE(Configure_test::adata_.clearSchemas());

    // Bad content array:
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__BadContentArray), h2agent::model::AdminSchemaData::BadContent);
    EXPECT_TRUE(Configure_test::adata_.clearSchemas());
}

TEST_F(Configure_test, FindSchema)
{
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__Success), h2agent::model::AdminSchemaData::Success);

    EXPECT_TRUE(Configure_test::adata_.getSchemaData().find("myRequestsSchema") != nullptr);
    EXPECT_TRUE(Configure_test::adata_.clearSchemas());
}

TEST_F(Configure_test, FindMissingSchema)
{
    EXPECT_TRUE(Configure_test::adata_.getSchemaData().find("myRequestsSchema") == nullptr);
}

TEST_F(Configure_test, ValidateSchema)
{
    EXPECT_EQ(Configure_test::adata_.loadSchema(SchemaConfiguration__Success), h2agent::model::AdminSchemaData::Success);

    auto schemaData = Configure_test::adata_.getSchemaData().find("myRequestsSchema");
    EXPECT_TRUE(schemaData != nullptr);

    std::string error{};
    EXPECT_TRUE(schemaData->validate(R"({"foo":"bar"})"_json, error));
EXPECT_TRUE(error.empty());
}

