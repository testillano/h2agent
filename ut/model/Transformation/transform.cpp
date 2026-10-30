#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fstream>
#include <ctime>
#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functions.hpp>

#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>
#include <AdminClientEndpointData.hpp>
#include <AdminClientProvisionData.hpp>
#include <AdminSchemas.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>
#include <Configuration.hpp>
#include <Vault.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <DataPart.hpp>

#include <ert/http2comm/Http2Headers.hpp>


// Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching = R"({ "algorithm": "FullMatching" })"_json;

// Provision configuration "template":
const nlohmann::json ProvisionConfiguration_GET = R"(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar/1?name=test",
  "responseCode": 200,
  "responseDelayMs": 0,
  "responseBody": {
    "bar": "bar-1",
    "remove-me": 0
  },
  "responseHeaders": {
    "content-type": "application/json",
    "x-version": "1.0.0"
  }
}
)"_json;

const std::string ProvisionConfiguration_GET_responseBodyAsString = "{\"bar\":\"bar-1\",\"remove-me\":0}";

const nlohmann::json Request = R"({"foo":1, "node1":{"node2":"value-of-node1-node2","delaymilliseconds":25}})"_json;

const nlohmann::json RequestSchema = R"(
{
  "id": "myRequestSchema",
  "schema": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": true,
    "properties": {
      "foo": {
        "type": "number"
      }
    },
    "required": [
      "foo"
    ]
  }
}
)"_json;

const nlohmann::json ResponseSchema = R"(
{
  "id": "myResponseSchema",
  "schema": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": true,
    "properties": {
      "bar": {
        "type": "string"
      }
    },
    "required": [
      "bar"
    ]
  }
}
)"_json;

const nlohmann::json SchemaExample = R"(
{
  "id": "SchemaExample",
  "schema": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["foo", "node1"],
    "properties": {
      "foo": {
        "type": "integer"
      },
      "node1": {
        "type": "object",
        "required": ["node2", "delaymilliseconds"],
        "properties": {
          "node2": {
            "type": "string"
          },
          "delaymilliseconds": {
            "type": "integer",
            "const": 25
          }
        }
      }
    }
  }
}
)"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCapture = R"delim(
{
  "from": "requestUri",
  "to": {"var": "uri_parts"},
  "filter": [{"RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)"
}]
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCaptureUnmatches = R"delim(
{
  "from": "requestUri",
  "to": {"var": "uri_parts"},
  "filter": [{"RegexCapture": "(/app/v2/foo/bar/)([a-z]*)(\\?name=proc)"}]
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCaptureWithVault = R"delim(
{
  "from": "requestUri",
  "to": {"vault": "g_uri_parts"},
  "filter": [{"RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)"}]
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexReplace = R"delim(
{
  "from": "requestUriPath",
  "to": "responseBodyText",
  "filter": [{"RegexReplace": {"rgx": "(/app/v1/foo/bar/)([0-9]*)", "fmt": "$2"}}]
}
)delim"_json;

// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemBadRegex = R"delim(
{"from": "requestUri", "to": "responseBodyText", "filter": [{"RegexCapture": "["}]}
)delim"_json;


// Transformation items to cover asString() for different types of sources:
const nlohmann::json TransformationAsStringSource1 = R"({ "source": "request.body", "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringSource2 = R"({ "source": "random.1.2", "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringSource3 = R"({ "source": "txtFile./tmp/foo", "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringSource4 = R"({ "source": "command.echo -n hi @{name}", "target": "response.body.string" })"_json;

// Transformation items to cover asString() for different types of targets:
const nlohmann::json TransformationAsStringTarget1 = R"({ "source": "value.foo", "target": "response.body.json.string" })"_json;
const nlohmann::json TransformationAsStringTarget2 = R"({ "source": "value.foo", "target": "outState.POST.@{uri}" })"_json;
const nlohmann::json TransformationAsStringTarget3 = R"({ "source": "value.foo", "target": "txtFile.@{filepath}" })"_json;

// Split filter test items:
const nlohmann::json TransformationItemSplitDefaults = R"(
{"from": {"value": "55011223"}, "to": "responseBodyText", "filter": [{"Split": {}}]}
)"_json;

const nlohmann::json TransformationItemSplitNumeric = R"(
{"from": {"value": "55011223"}, "to": "responseBodyText", "filter": [{"Split": {"numeric": true}}]}
)"_json;

const nlohmann::json TransformationItemSplitCustom = R"(
{
  "from": {"value": "AABBCC"},
  "to": "responseBodyText",
  "filter": [{"Split": {"size": 2, "count": 3, "sep": ":", "filler": "X", "numeric": false}}]
}
)"_json;

// Transformation items to cover asString() for different types of filters:
const nlohmann::json TransformationAsStringFilter1 = R"({ "source": "value.foo", "filter": { "Sum": 1 }, "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringFilter2 = R"({ "source": "value.foo", "filter": { "EqualTo": "@{bar}" }, "target": "response.body.string" })"_json;
// model::Transformation still loads grammar v1 (until phase [0.7b]): v1 copies of the items above.
const nlohmann::json TransformationAsStringFilter3 = R"delim({ "source": "request.uri", "target": "var.uri_parts", "filter": { "RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)" } })delim"_json;
const nlohmann::json TransformationAsStringFilter4 = R"delim({ "source": "request.uri.path", "target": "response.body.string", "filter": { "RegexReplace": { "rgx": "(/app/v1/foo/bar/)([0-9]*)", "fmt": "$2" } } })delim"_json;
const nlohmann::json TransformationAsStringFilter5 = R"({ "source": "value.55011223", "target": "response.body.string", "filter": { "Split": { "numeric": true } } })"_json;

// BaseConvert filter test items:
const nlohmann::json TransformationItemBaseConvertDecToHex = R"(
{
  "from": {"value": "255"},
  "to": "responseBodyText",
  "filter": [{"BaseConvert": {"in": 10, "out": 16}}]
}
)"_json;

const nlohmann::json TransformationItemBaseConvertDecToHexCapital = R"(
{
  "from": {"value": "255"},
  "to": "responseBodyText",
  "filter": [{"BaseConvert": {"in": 10, "out": 16, "capital": true}}]
}
)"_json;

const nlohmann::json TransformationItemBaseConvertHexToDec = R"(
{
  "from": {"value": "ff"},
  "to": "responseBodyText",
  "filter": [{"BaseConvert": {"in": 16, "out": 10}}]
}
)"_json;

const nlohmann::json TransformationItemBaseConvertDecToBin = R"(
{
  "from": {"value": "10"},
  "to": "responseBodyText",
  "filter": [{"BaseConvert": {"in": 10, "out": 2}}]
}
)"_json;




// XXXXXXXXXXXX client provision
// XXXXXXXXXXXX client endpoint

class Transform_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::common_resources_t common_resources_{};
    nlohmann::json server_provision_json_{};

    // transform attributes:
    // inputs:
    std::string in_state_{};
    std::string request_method_{};
    std::string request_uri_{};
    std::string request_uri_path_{};
    std::map<std::string, std::string> qmap_{};
    nghttp2::asio_http2::header_map request_headers_{};
    nlohmann::json request_body_{};
    h2agent::model::DataPart request_body_data_part_{};
    std::uint64_t general_unique_server_sequence_{};

    // outputs:
    unsigned int status_code_{};
    nghttp2::asio_http2::header_map response_headers_{};
    std::string response_body_{};
    unsigned int response_delay_ms_{};
    std::string out_state_{};
    std::string out_state_method_{};
    std::string out_state_uri_{};
    std::vector<std::pair<std::string, std::string>> client_provision_triggers_{};
    std::map<std::string, std::string> variables_{};

    Transform_test() {
        adata_.loadServerMatching(MatchingConfiguration_FullMatching);
        server_provision_json_ = ProvisionConfiguration_GET;

        in_state_ = "initial";
        request_method_ = "GET";
        request_uri_ = "/app/v1/foo/bar/1?name=test";
        request_uri_path_ = "/app/v1/foo/bar/1";
        qmap_ = h2agent::model::extractQueryParameters("name=test");
        request_headers_.emplace("x-version", nghttp2::asio_http2::header_value{"1.0.0"});
        request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        request_body_ = Request;
        general_unique_server_sequence_ = 74;

        auto loadResult = adata_.loadSchema(RequestSchema);
        loadResult = adata_.loadSchema(ResponseSchema);
        loadResult = adata_.loadSchema(SchemaExample);

        //////////////////////
        // COMMON RESOURCES //
        //////////////////////
        common_resources_.AdminDataPtr = &adata_;
        common_resources_.ConfigurationPtr = new h2agent::model::Configuration();
        common_resources_.VaultPtr = new h2agent::model::Vault();
        common_resources_.FileManagerPtr = new h2agent::model::FileManager(nullptr);
        common_resources_.SocketManagerPtr = new h2agent::model::SocketManager(nullptr);
        common_resources_.MockServerDataPtr = new h2agent::model::MockServerData();
        common_resources_.MockClientDataPtr = new h2agent::model::MockClientData();

        // Vault:
        common_resources_.VaultPtr->load("myVault","myVault_value");

        // Simulated event:
        h2agent::model::DataPart postRequestBodyDataPart;
        postRequestBodyDataPart.assign("{\"foo\":15}");
        std::string responseBody = "{\"bar\":25}";
        common_resources_.MockServerDataPtr->loadEvent(h2agent::model::DataKey("POST", "/app/v1/foo/bar/1"), "previous-state", "state", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()), 201, request_headers_ /* shared to simplify */, response_headers_ /* shared to simplify */, postRequestBodyDataPart, responseBody, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);

        // Simulated client event:
        h2agent::model::DataPart clientResponseBodyDataPart;
        clientResponseBodyDataPart.assign("{\"baz\":42}");
        std::string clientRequestBody = "{\"qux\":99}";
        nghttp2::asio_http2::header_map clientResponseHeaders;
        clientResponseHeaders.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        common_resources_.MockClientDataPtr->loadEvent(h2agent::model::DataKey("myEndpoint", "GET", "/api/v1/data"), "myProvisionId", "initial", "initial", now, now, 200, request_headers_, clientResponseHeaders, clientRequestBody, clientResponseBodyDataPart, 1 /* client sequence */, 1 /* sequence */, 0 /* request delay ms */, 3000 /* timeout ms */, true /* history */);
    }

    void provisionAndTransform(const std::string &requestBody) {

        //adata_.clearServerProvisions(); // class instance is created in every test
        EXPECT_EQ(adata_.getServerProvisionData().size(), 0);
        EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::Success);
        std::shared_ptr<h2agent::model::AdminServerProvision> provision = adata_.getServerProvisionData().find(in_state_, request_method_, request_uri_);
        ASSERT_TRUE(provision);
        EXPECT_EQ(adata_.getServerProvisionData().size(), 1);

        request_body_data_part_.assign(requestBody);
        provision->transform(request_uri_, request_uri_path_, qmap_, request_body_data_part_, request_headers_, general_unique_server_sequence_, status_code_, response_headers_, response_body_, response_delay_ms_, out_state_, out_state_method_, out_state_uri_, client_provision_triggers_, variables_);
    }

    ~Transform_test() {
        delete(common_resources_.ConfigurationPtr);
        delete(common_resources_.VaultPtr);
        delete(common_resources_.FileManagerPtr);
        delete(common_resources_.SocketManagerPtr);
        delete(common_resources_.MockServerDataPtr);
        delete(common_resources_.MockClientDataPtr);
    }
};

/////////////
// GENERAL //
/////////////
TEST_F(Transform_test, TransformationAsStringSource1)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringSource1));
    EXPECT_EQ(transformation.asString(), "SourceType: RequestBody | source_:  (empty: whole, path: node) | TargetType: ResponseBodyString");
}

TEST_F(Transform_test, TransformationAsStringSource2)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringSource2));
    EXPECT_EQ(transformation.asString(), "SourceType: Random | source_:  | source_i1_: 1 (Random min) | source_i2_: 2 (Random max) | TargetType: ResponseBodyString");
}

TEST_F(Transform_test, TransformationAsStringSource3)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringSource3));
    EXPECT_EQ(transformation.asString(), "SourceType: STxtFile | source_: /tmp/foo (path file) | TargetType: ResponseBodyString");
}

TEST_F(Transform_test, TransformationAsStringSource4)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringSource4));
    EXPECT_EQ(transformation.asString(), "SourceType: Command | source_: echo -n hi @{name} (shell command expression) | source variables: name | TargetType: ResponseBodyString");
}

TEST_F(Transform_test, TransformationAsStringTarget1)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringTarget1));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: foo | TargetType: ResponseBodyJson_String | target_:  (empty: whole, path: node)");
}

TEST_F(Transform_test, TransformationAsStringTarget2)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringTarget2));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: foo | TargetType: OutState | target_: POST (empty: current method, method: another) | target2_: @{uri}(empty: current uri, uri: another) | target2 variables: uri");
}

TEST_F(Transform_test, TransformationAsStringTarget3)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringTarget3));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: foo | TargetType: TTxtFile | target_: @{filepath} (path file) | target variables: filepath");
}

TEST_F(Transform_test, TransformationAsStringFilter1)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringFilter1));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: foo | TargetType: ResponseBodyString | FilterType: Sum | filter_number_type_: 1 (0: integer, 1: unsigned, 2: float) | filter_i_: 0 | filter_u_: 1 | filter_f_: 0");
}

TEST_F(Transform_test, TransformationAsStringFilter2)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringFilter2));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: foo | TargetType: ResponseBodyString | FilterType: EqualTo | filter_ @{bar} | filter variables: bar");
}

TEST_F(Transform_test, TransformationAsStringFilter3)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringFilter3));
    EXPECT_EQ(transformation.asString(), "SourceType: RequestUri | TargetType: TVar | target_: uri_parts | FilterType: RegexCapture | filter_ (/app/v1/foo/bar/)([0-9]*)(\\?name=test) (literal, although not actually needed, but useful to access & print on traces)");
}

TEST_F(Transform_test, TransformationAsStringFilter4)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringFilter4));
    EXPECT_EQ(transformation.asString(), "SourceType: RequestUriPath | TargetType: ResponseBodyString | FilterType: RegexReplace | filter_ $2 (fmt)");
}

TEST_F(Transform_test, TransformationAsStringFilter5)
{
    h2agent::model::Transformation transformation{};

    // Validations:
    EXPECT_TRUE(transformation.load(TransformationAsStringFilter5));
    EXPECT_EQ(transformation.asString(), "SourceType: Value | source_: 55011223 | TargetType: ResponseBodyString | FilterType: Split | size: 2 | count: 4 | sep: '.' | filler: '0' | numeric: true");
}

TEST_F(Transform_test, EmptyInStateAndOutStateBecomeDefault)
{
    // Build test provision:
    server_provision_json_["inState"] = "";
    server_provision_json_["outState"] = "";

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(in_state_, "initial");
    EXPECT_EQ(out_state_, "initial");
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsString)
{
    // Build test provision:
    server_provision_json_["responseBody"] = "hello";

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsNumberInteger)
{
    // Build test provision:
    server_provision_json_["responseBody"] = -123;

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "-123");
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsNumberUnsigned)
{
    // Build test provision:
    server_provision_json_["responseBody"] = 123;

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "123");
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsFloat)
{
    // Build test provision:
    server_provision_json_["responseBody"] = 3.14; // this validates the schema (float -> object)

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "3.140000");
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsBoolean)
{
    // Build test provision:
    server_provision_json_["responseBody"] = true; // this validates de schema (boolean -> object)

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "true");
}

TEST_F(Transform_test, ProvisionWithResponseBodyAsNull)
{
    // Build test provision:
    server_provision_json_["responseBody"] = nullptr;

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "null");
}

//////////////////////////////////////////////////
// TEST SCHEMAS VALIDATION (request & response) //
//////////////////////////////////////////////////
TEST_F(Transform_test, RequestNotValidated)
{
    // Build test provision:
    server_provision_json_["requestSchemaId"] = "myRequestSchema";
    server_provision_json_["responseSchemaId"] = "myResponseSchema";

    // Run transformation:
    request_body_["foo"] = "this is a string"; // must be number !
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 400); // transformations are interrupted when request is invalid: response body will be empty:
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, ResponseNotValidated)
{
    // Build test provision:
    server_provision_json_["requestSchemaId"] = "myRequestSchema";
    server_provision_json_["responseSchemaId"] = "myResponseSchema";

    // Run transformation:
    // Remove mandatory 'bar':
    auto path = nlohmann::json::json_pointer{"/responseBody"};
    // Get a reference to the 'responseBody':
    auto& rb = server_provision_json_[path];
    // Erase from the 'responseBody' node by key name: "bar" (response schema requires it !)
    rb.erase("bar");

    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 500); // transformations are NOT interrupted when response is invalid, but transformations are executed (before) and response body is respected:
    EXPECT_EQ(response_body_, "{\"remove-me\":0}");
}

///////////////////////////////////////////
// TEST SOURCES (target is not relevant) //
///////////////////////////////////////////
TEST_F(Transform_test, SourceRequestUri)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "requestUri", "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "/app/v1/foo/bar/1?name=test");
}

TEST_F(Transform_test, SourceRequestUriPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "requestUriPath", "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "/app/v1/foo/bar/1");
}

TEST_F(Transform_test, SourceRequestUriParam)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestUriParam": "name"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "test");
}

TEST_F(Transform_test, SourceRequestUriParamUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestUriParam": "missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceRequestBody)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"foo\":1,\"node1\":{\"delaymilliseconds\":25,\"node2\":\"value-of-node1-node2\"}}");
}

TEST_F(Transform_test, SourceRequestBodyEmpty)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform("");

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, SourceRequestBodyPlainText)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    auto it = request_headers_.find("content-type"); // assigned in constructor to application/json:
    request_headers_.erase(it);
    request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"text/html"});
    // provision template dictates application/json on response headers, but we won't update to text/html (not actually needed)
    provisionAndTransform("hello");

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, SourceRequestBodyPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": "/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, SourceRequestBodyPathUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": "/missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceRequestHeader)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestHeader": "x-version"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1.0.0");
}

TEST_F(Transform_test, SourceRequestHeaderUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestHeader": "missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceResponseBody)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"responseBody": ""}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceResponseBodyPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"responseBody": "/bar"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "bar-1");
}

TEST_F(Transform_test, SourceResponseBodyPathUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"responseBody": "/missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceEraser)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "eraser", "to": {"responseBody": "/remove-me"}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\"}");
}

TEST_F(Transform_test, SourceEraserWholeJson)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "eraser", "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, SourceEraserVault)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "myVaultEntry_value"}, "to": {"vault": "myVaultEntry"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "myVaultEntry"}, "to": {"responseBody": "/vaultBeforeRemoval"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item3 = R"({ "from": "eraser", "to": {"vault": "myVaultEntry"} })"_json;
    const nlohmann::json item4 = R"({ "from": {"vault": "myVaultEntry"}, "to": {"responseBody": "/vaultAfterRemoval"}, "filter": [{"as": "object"}] })"_json; // will be skipped
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    expectedJson["vaultBeforeRemoval"] = "myVaultEntry_value";
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, SourceMath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"math": "1+1"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "2");
}

TEST_F(Transform_test, SourceRandom)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"random": [10, 10]}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "10"); // predictable random between 10 and 10, to ease test.
}

TEST_F(Transform_test, SourceRandomSet)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"randomset": ["hi", "hi", "hi"]}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hi"); // predictable to ease test.
}

TEST_F(Transform_test, SourceTimestamp)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"timestamp": "ms"}, "to": {"responseBody": "/timestampMs"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"timestamp": "us"}, "to": {"responseBody": "/timestampUs"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"timestamp": "ns"}, "to": {"responseBody": "/timestampNs"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"timestamp": "s"}, "to": {"responseBody": "/timestampS"}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    nlohmann::json responseBuilt{};
    bool success = h2agent::model::parseJsonContent(response_body_, responseBuilt);
    EXPECT_TRUE(success);

    // predictability:
    expectedJson["timestampMs"] = responseBuilt["timestampMs"];
    expectedJson["timestampUs"] = responseBuilt["timestampUs"];
    expectedJson["timestampNs"] = responseBuilt["timestampNs"];
    expectedJson["timestampS"] = responseBuilt["timestampS"];

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, SourceStrftime)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"strftime": "This month is %b."}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "This month is %b.", ltm);

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, buffer); // predictable to ease test (except if executed at 00:00 during month change !!).
}

TEST_F(Transform_test, SourceRecvseq)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "recvseq", "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, std::to_string(general_unique_server_sequence_)); // predictable to ease test.
}

TEST_F(Transform_test, SourceVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "myVariable_value"}, "to": {"var": "myVariable"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "myVariable"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myVariable_value");
}

TEST_F(Transform_test, SourceVariableUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"var": "myVariable"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceVault)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"vault": "myVault"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myVault_value");
}

TEST_F(Transform_test, SourceVaultUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"vault": "missingVault"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceServerEvent)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"serverEvent": "requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "15");
}

TEST_F(Transform_test, SourceServerEventPathUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"serverEvent": "requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceClientEvent)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"clientEvent": "clientEndpointId=myEndpoint&requestMethod=GET&requestUri=/api/v1/data&eventNumber=-1&eventPath=/responseBody/baz"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "42");
}

TEST_F(Transform_test, SourceClientEventPathUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"clientEvent": "clientEndpointId=myEndpoint&requestMethod=GET&requestUri=/api/v1/data&eventNumber=-1&eventPath=/responseBody/missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceClientEventEndpointUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"clientEvent": "clientEndpointId=unknownEndpoint&requestMethod=GET&requestUri=/api/v1/data&eventNumber=-1&eventPath=/responseBody/baz"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceValue)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "hello"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, SourceInState)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "inState", "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, in_state_);
}

TEST_F(Transform_test, SourceTxtFile)
{
    // Create temporary file:
    std::ofstream tmpfile("/tmp/h2agent.UT.Transform_test.SourceTxtFile.txt", std::ios::trunc);
    tmpfile << "example" << std::endl;
    tmpfile.close();

    // Build test provision:
    const nlohmann::json item = R"({ "from": {"txtFile": "/tmp/h2agent.UT.Transform_test.SourceTxtFile.txt"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "example");
}

TEST_F(Transform_test, SourceBinFile)
{
    // Create temporary file:
    std::ofstream tmpfile("/tmp/h2agent.UT.Transform_test.SourceBinFile.bin", std::ios::binary | std::ios::trunc);
    int value = 15;
    tmpfile.write(reinterpret_cast<char*>(&value), sizeof(int));
    tmpfile.close();

    // Build test provision:
    const nlohmann::json item = R"({ "from": {"binFile": "/tmp/h2agent.UT.Transform_test.SourceBinFile.bin"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    int responseValue{};
    memcpy(&responseValue, response_body_.data(), sizeof(int));
    EXPECT_EQ(responseValue, value);
}

TEST_F(Transform_test, SourceCommand)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"command": "echo -n foo"}, "to": "responseBodyText" })"_json;
    const nlohmann::json item2 = R"({ "from": {"math": "200+@{rc}"}, "to": "responseStatusCode" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200); // rc+200=200 (rc=0)
    EXPECT_EQ(response_body_, "foo");
}

TEST_F(Transform_test, SourceCommandFail)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"command": "unknowncommand"}, "to": {"var": "command-output"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"value": "@{command-output}@{rc}"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    //EXPECT_EQ(response_body_, "sh: 1: unknowncommand: not found\n127"); // if command would be 'unknowncommand 2>&1'
    EXPECT_EQ(response_body_, "127"); // command output is empty
}

// No way to test popen fail (return NULL) except for memory fault (infinite popen calls), so we leave the exception in
// source which stores rc=-1, but we cannot unit-test:
//TEST_F(Transform_test, SourceCommandPopenFail)

///////////////////////////////////////////
// TEST TARGETS (source is not relevant) //
///////////////////////////////////////////

TEST_F(Transform_test, TargetResponseBodyString)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "plain text"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "plain text");
}

TEST_F(Transform_test, TargetResponseBodyHexString)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "0xc0a8"}, "to": "responseBodyHex" })"_json;
    server_provision_json_["transform"].push_back(item);
    server_provision_json_["responseHeaders"]["content-type"] = "application/octet-stream";

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(ert::http2comm::headersAsString(response_headers_), "[content-type: application/octet-stream][x-version: 1.0.0]");
    EXPECT_EQ(response_body_, "\xC0\xA8");
}

// ////////////////// //
// Response Body Json //
// ////////////////// //
TEST_F(Transform_test, TargetResponseBodyJsonObjectFromString)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "hello"}, "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "\"hello\"");
}

TEST_F(Transform_test, TargetResponseBodyJsonObjectFromInteger)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"random": [10, 10]}, "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "10");
}

TEST_F(Transform_test, TargetResponseBodyJsonObjectFromUnsigned)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "recvseq", "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "74");
}

TEST_F(Transform_test, TargetResponseBodyJsonObjectFromFloat)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"math": "1/2"}, "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "0.5");
}

TEST_F(Transform_test, TargetResponseBodyJsonObjectFromBoolean)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": "/boo"}, "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform("{\"bar\":\"bar-1\",\"foo\":1,\"boo\":true}");

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "true");
}

TEST_F(Transform_test, TargetResponseBodyJsonObject)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": {"responseBody": ""}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"foo\":1,\"node1\":{\"delaymilliseconds\":25,\"node2\":\"value-of-node1-node2\"},\"remove-me\":0}"); // merged request & response
}

TEST_F(Transform_test, TargetResponseBodyJsonJsonstring)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "{\"bar\":\"bar-8\"}"}, "to": {"responseBody": ""}, "filter": [{"as": "json"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-8\",\"remove-me\":0}"); // merged request & response
}

TEST_F(Transform_test, TargetResponseBodyJsonJsonstringMalformed)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "{\"bar:\"bar-8\"}"}, "to": {"responseBody": ""}, "filter": [{"as": "json"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetResponseBodyJsonString)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "hello"}, "to": {"responseBody": ""}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "\"hello\"");
}

TEST_F(Transform_test, TargetResponseBodyJsonInteger)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "-15"}, "to": {"responseBody": ""}, "filter": [{"as": "integer"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "-15");
}

TEST_F(Transform_test, TargetResponseBodyJsonUnsigned)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "15"}, "to": {"responseBody": ""}, "filter": [{"as": "unsigned"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "15");
}

TEST_F(Transform_test, TargetResponseBodyJsonFloat)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "3.14"}, "to": {"responseBody": ""}, "filter": [{"as": "float"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "3.14");
}

TEST_F(Transform_test, TargetResponseBodyJsonBoolean)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "non-empty-string-means-true"}, "to": {"responseBody": ""}, "filter": [{"as": "boolean"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "true");
}

// ///////////////////////////// //
// Response Body Json WITH PATHS //
// ///////////////////////////// //

TEST_F(Transform_test, TargetResponseBodyJsonObjectPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": {"responseBody": "/target"}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":{\"foo\":1,\"node1\":{\"delaymilliseconds\":25,\"node2\":\"value-of-node1-node2\"}}}"); // merged request & response
}

TEST_F(Transform_test, TargetResponseBodyJsonJsonstringPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "{\"bar\":\"bar-8\"}"}, "to": {"responseBody": "/target"}, "filter": [{"as": "json"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":{\"bar\":\"bar-8\"}}"); // merged request & response
}

TEST_F(Transform_test, TargetResponseBodyJsonStringPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "hello"}, "to": {"responseBody": "/target"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":\"hello\"}");
}

TEST_F(Transform_test, TargetResponseBodyJsonStringMissingPathException) // json_pointer with missing path
{
    // A body path which is not a json pointer is rejected by the schema (v1 failed at runtime):
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": {"responseBody": "target"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::BadSchema);
}

// Setting json field with binary data may raise an exception:
// echo -en '\x80\x01' | curl --http2-prior-knowledge -i -H 'content-type:application/octet-stream' -X GET "<traffic url>/uri" --data-binary @-
TEST_F(Transform_test, TargetResponseBodyJsonStringPathExceptionDueToBinary)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"requestBody": ""}, "to": {"responseBody": "/target"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    auto it = request_headers_.find("content-type"); // assigned in constructor to application/json:
    request_headers_.erase(it);
    request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/octet-stream"});
    //const char buffer[] = { '\x80', '\x01' };
    //const int buffer_size = sizeof(buffer);
    //std::string data(buffer, buffer_size);
    //provisionAndTransform(data);
    provisionAndTransform("\x80\x01");

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, TargetResponseBodyJsonIntegerPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "-15"}, "to": {"responseBody": "/target"}, "filter": [{"as": "integer"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":-15}");
}

TEST_F(Transform_test, TargetResponseBodyJsonUnsignedPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "15"}, "to": {"responseBody": "/target"}, "filter": [{"as": "unsigned"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":15}");
}

TEST_F(Transform_test, TargetResponseBodyJsonFloatPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "3.14"}, "to": {"responseBody": "/target"}, "filter": [{"as": "float"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":3.14}");
}

TEST_F(Transform_test, TargetResponseBodyJsonBooleanPath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "non-empty-string-means-true"}, "to": {"responseBody": "/target"}, "filter": [{"as": "boolean"}] })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":true}");
}

TEST_F(Transform_test, TargetResponseHeader)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "1.2.3"}, "to": {"responseHeader": "version"} })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
    auto it = response_headers_.find("version");
    EXPECT_TRUE(it != response_headers_.end());
    EXPECT_EQ(it->second.value, "1.2.3");
}

TEST_F(Transform_test, TargetResponseStatusCode)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "201"}, "to": "responseStatusCode" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 201);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetResponsedelayMs)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "125"}, "to": "responseDelayMs" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
    EXPECT_EQ(response_delay_ms_, 125);
}

TEST_F(Transform_test, TargetVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "hello"}, "to": {"var": "varname"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "varname"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, TargetVault)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "hello"}, "to": {"vault": "vaultname"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "vaultname"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, TargetServerEvent) // 'eraser' is the only compatible source
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "any"}, "to": {"serverEvent": "requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo"} })"_json;
    const nlohmann::json item2 = R"({ "from": "eraser", "to": {"serverEvent": "requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo"} })"_json;
    const nlohmann::json item3 = R"({ "from": "eraser", "to": {"serverEvent": "requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo"} })"_json;
    const nlohmann::json item4 = R"({ "from": {"serverEvent": "requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1); // invalid (skipped): only "eraser" is a compatible source for serverEvent target.
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3); // invalid: missing requestMethod
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetOutState)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "out-state"}, "to": "outState" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
    EXPECT_EQ(out_state_, "out-state");
}

TEST_F(Transform_test, TargetOutStateForeign)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": {"value": "foreign-state"}, "to": {"foreignOutState": "POST", "uri": "/foreign/uri"} })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
    EXPECT_EQ(out_state_, "foreign-state"); // this is because provision transformation sends here the foreign-state but it is replaced later by provision state in own event
    EXPECT_EQ(out_state_method_, "POST");
    EXPECT_EQ(out_state_uri_, "/foreign/uri");
}

TEST_F(Transform_test, TargetTxtFile)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": "example2"}, "to": {"txtFile": "/tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"txtFile": "/tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt"}, "to": "responseBodyText" })"_json;
    const nlohmann::json item3 = R"({ "from": "eraser", "to": {"txtFile": "/tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt"} })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "example2");
}

TEST_F(Transform_test, TargetBinFile)
{
    // Create temporary file:
    std::ofstream tmpfile("/tmp/h2agent.UT.Transform_test.SourceBinFile.bin", std::ios::binary | std::ios::trunc);
    int value = 35;
    tmpfile.write(reinterpret_cast<char*>(&value), sizeof(int));
    tmpfile.close();

    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"binFile": "/tmp/h2agent.UT.Transform_test.SourceBinFile.bin"}, "to": {"binFile": "/tmp/h2agent.UT.Transform_test.SourceBinFile2.bin"} })"_json;
    const nlohmann::json item2 = R"({ "from": "eraser", "to": {"binFile": "/tmp/h2agent.UT.Transform_test.SourceBinFile.bin"} })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);

    std::ifstream tmpfile2("/tmp/h2agent.UT.Transform_test.SourceBinFile2.bin", std::ios::binary); // Abrir archivo para lectura en modo binario
    int value2{};
    tmpfile2.read(reinterpret_cast<char*>(&value2), sizeof(value2));
    tmpfile2.close();
    EXPECT_EQ(value2, value);
}

TEST_F(Transform_test, TargetUDPSocket)
{
    // Socket path:
    std::string socketPath = "/tmp/h2agent.UT.Transform_test.TargetUDPSocket";
    // Build test provision:
    nlohmann::json item = R"({ "from": {"value": "hello"} })"_json;
    item["to"] = {{"udpSocket", socketPath}, {"delayMs", 0}};

    server_provision_json_["transform"].push_back(item);

    // Prepare UDP socket for reception:
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(sockfd < 0);

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, socketPath.c_str());

    unlink(socketPath.c_str()); // just in case, it exists

    int bindrc = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un));
    if (bindrc < 0) close(sockfd);
    ASSERT_FALSE(bindrc < 0);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    // Check socket content:
    char buffer[32];
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
    ASSERT_TRUE(bytesRead > 0);
    buffer[bytesRead] = '\0';
    std::string msg;
    msg.assign(buffer);
    EXPECT_EQ(msg, "hello");
}

TEST_F(Transform_test, TargetBreak)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"value": ""}, "to": "break" })"_json; // this break is not triggered (empty value)
    const nlohmann::json item2 = R"({ "from": {"value": "hello"}, "to": "responseBodyText" })"_json;
    const nlohmann::json item3 = R"({ "from": {"value": "non-empty"}, "to": "break" })"_json;
    const nlohmann::json item4 = R"({ "from": {"value": "201"}, "to": "responseStatusCode" })"_json; // not reached (previous break is non-empty-triggered)
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, TargetClientProvision)
{
    // Build test provision with clientProvision trigger (source as conditional gate):
    const nlohmann::json item1 = R"({ "from": {"value": "1"}, "to": {"clientProvision": "myFlow", "inState": "initial"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"value": "go"}, "to": {"clientProvision": "anotherFlow", "inState": "step2"} })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations: triggers collected
    ASSERT_EQ(client_provision_triggers_.size(), 2);
    EXPECT_EQ(client_provision_triggers_[0].first, "myFlow");
    EXPECT_EQ(client_provision_triggers_[0].second, "initial");
    EXPECT_EQ(client_provision_triggers_[1].first, "anotherFlow");
    EXPECT_EQ(client_provision_triggers_[1].second, "step2");
}

TEST_F(Transform_test, TargetClientProvisionWithEraser)
{
    // When eraser source is used, trigger is skipped (conditional gate):
    const nlohmann::json item = R"({ "from": "eraser", "to": {"clientProvision": "myFlow", "inState": "initial"} })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations: no triggers (eraser = empty = skip)
    ASSERT_EQ(client_provision_triggers_.size(), 0);
}

TEST_F(Transform_test, TargetClientProvisionSkippedOnEmptyVar)
{
    // When source variable does not exist, trigger is skipped:
    const nlohmann::json item = R"({ "from": {"var": "nonExistent"}, "to": {"clientProvision": "myFlow", "inState": "initial"} })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations: no triggers (empty var = skip)
    ASSERT_EQ(client_provision_triggers_.size(), 0);
}

TEST_F(Transform_test, TargetClientProvisionMissingInState)
{
    // clientProvision without inState is rejected by the schema:
    const nlohmann::json item = R"({ "from": {"value": "1"}, "to": {"clientProvision": "myFlow"} })"_json;
    server_provision_json_["transform"].push_back(item);

    EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::BadSchema);
}

///////////////////////////////////////////////////
// TEST FILTERS (source/target are not relevant) //
///////////////////////////////////////////////////

TEST_F(Transform_test, FilterIncompatibleWithEraser)
{
    // Build test provision:
    const nlohmann::json item = R"({ "from": "eraser", "to": {"responseBody": "/remove-me"}, "filter": [{"ConditionVar": "myVault"}, {"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(TransformationItemBadRegex);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterRegexCapture)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"var": "uri_parts"}, "to": {"responseBody": "/uri_parts"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "uri_parts.1"}, "to": {"responseBody": "/uri_parts.1"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"var": "uri_parts.2"}, "to": {"responseBody": "/uri_parts.2"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"var": "uri_parts.3"}, "to": {"responseBody": "/uri_parts.3"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(TransformationItemRegexCapture);
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    expectedJson["uri_parts"] = "/app/v1/foo/bar/1?name=test";
    expectedJson["uri_parts.1"] = "/app/v1/foo/bar/";
    expectedJson["uri_parts.2"] = "1";
    expectedJson["uri_parts.3"] = "?name=test";
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, FilterRegexCaptureWithVault)
{
    // Build test provision:
    // RegexCapture on vault stores captures as JSON object: {"0": "full", "1": "group1", ...}
    // Access individual captures via json pointer path: vault.g_uri_parts./1
    const nlohmann::json item1 = R"({ "from": {"vault": "g_uri_parts"}, "to": {"responseBody": "/g_uri_parts"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "g_uri_parts", "path": "/1"}, "to": {"responseBody": "/g_uri_parts.1"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"vault": "g_uri_parts", "path": "/2"}, "to": {"responseBody": "/g_uri_parts.2"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"vault": "g_uri_parts", "path": "/3"}, "to": {"responseBody": "/g_uri_parts.3"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(TransformationItemRegexCaptureWithVault);
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    expectedJson["g_uri_parts"] = {{"0", "/app/v1/foo/bar/1?name=test"}, {"1", "/app/v1/foo/bar/"}, {"2", "1"}, {"3", "?name=test"}};
    expectedJson["g_uri_parts.1"] = "/app/v1/foo/bar/";
    expectedJson["g_uri_parts.2"] = "1";
    expectedJson["g_uri_parts.3"] = "?name=test";
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, FilterRegexCaptureUnmatches)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "from": {"var": "uri_parts"}, "to": {"responseBody": "/uri_parts"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "uri_parts.1"}, "to": {"responseBody": "/uri_parts.1"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"var": "uri_parts.2"}, "to": {"responseBody": "/uri_parts.2"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"var": "uri_parts.3"}, "to": {"responseBody": "/uri_parts.3"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(TransformationItemRegexCaptureUnmatches);
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString); // all "variable source" transformation items are skipped
}

TEST_F(Transform_test, FilterRegexCaptureWrongRegex)
{
    // Build test provision:
    server_provision_json_["transform"].push_back(TransformationItemBadRegex);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterRegexReplace)
{
    // Build test provision:
    server_provision_json_["transform"].push_back(TransformationItemRegexReplace);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, FilterAppend)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "from": "inState",
      "to": "responseBodyText",
      "filter": [{"Append": " state, and variable is: @{myVault}"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "initial state, and variable is: myVault_value");
}

TEST_F(Transform_test, FilterPrepend)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "from": "inState",
      "to": "responseBodyText",
      "filter": [{"Prepend": "@{myVault} variable value, and state is: "}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myVault_value variable value, and state is: initial");
}

TEST_F(Transform_test, FilterSum)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {"from": {"value": "-3"}, "to": {"var": "accum"}, "filter": [{"Sum": -5}]}
    )"_json;
    const nlohmann::json item2 = R"(
    {"from": {"var": "accum"}, "to": "responseBodyText", "filter": [{"Sum": 9}]}
    )"_json;
    const nlohmann::json item3 = R"(
    {"from": {"value": "200.5"}, "to": "responseStatusCode", "filter": [{"Sum": 0.5}]}
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 201);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, FilterMultiply)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {"from": {"value": "10"}, "to": {"var": "accum"}, "filter": [{"Multiply": 2}]}
    )"_json;
    const nlohmann::json item2 = R"(
    {"from": {"var": "accum"}, "to": {"var": "accum"}, "filter": [{"Multiply": -0.5}]}
    )"_json;
    const nlohmann::json item3 = R"(
    {"from": {"var": "accum"}, "to": "responseBodyText", "filter": [{"Multiply": -1}]}
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "10");
}

TEST_F(Transform_test, FilterEqualTo)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "myVault_value"},
      "to": "responseBodyText",
      "filter": [{"EqualTo": "@{myVault}"}]
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "from": {"value": "this is different"},
      "to": "responseBodyText",
      "filter": [{"EqualTo": "@{myVault}"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myVault_value");
}

TEST_F(Transform_test, FilterDifferentFrom)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "myVault_value"},
      "to": "responseBodyText",
      "filter": [{"DifferentFrom": "@{myVault}"}]
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "from": {"value": "this is different"},
      "to": "responseBodyText",
      "filter": [{"DifferentFrom": "@{myVault}"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "this is different");
}

TEST_F(Transform_test, FilterConditionVar)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {"from": {"value": "hi"}, "to": "responseBodyText", "filter": [{"ConditionVar": "myVault"}]}
    )"_json;
    const nlohmann::json item2 = R"(
    {"from": {"value": "bye"}, "to": "responseBodyText", "filter": [{"ConditionVar": "missingVar"}]}
    )"_json;
    const nlohmann::json item3 = R"(
    {
      "from": {"value": "201"},
      "to": "responseStatusCode",
      "filter": [{"ConditionVar": "!missingVar"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3); // also test negated condition variable

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 201);
    EXPECT_EQ(response_body_, "hi");
}

TEST_F(Transform_test, OnFilterFail)
{
    // ConditionVar with onFilterFail: if myVault exists -> "found", else -> "missing"
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "found"},
      "to": "responseBodyText",
      "filter": [{"ConditionVar": "myVault"}],
      "onFilterFail": [{"from": {"value": "missing"}, "to": "responseBodyText"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    // myVault exists (pre-loaded in fixture), so "found"
    EXPECT_EQ(response_body_, "found");
}

TEST_F(Transform_test, OnFilterFailTriggered)
{
    // ConditionVar on non-existent var: filter fails, onFilterFail executes
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "found"},
      "to": "responseBodyText",
      "filter": [{"ConditionVar": "nonExistent"}],
      "onFilterFail": [{"from": {"value": "fallback"}, "to": "responseBodyText"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(response_body_, "fallback");
}

TEST_F(Transform_test, OnFilterFailWithEqualTo)
{
    // EqualTo filter with onFilterFail
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "hello"},
      "to": "responseBodyText",
      "filter": [{"EqualTo": "world"}],
      "onFilterFail": [{"from": {"value": "not_equal"}, "to": "responseBodyText"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    // "hello" != "world", so fallback
    EXPECT_EQ(response_body_, "not_equal");
}

TEST_F(Transform_test, OnFilterFailNested)
{
    // Nested onFilterFail: if/else-if/else — tests deep recursion
    const nlohmann::json item1 = R"(
    {
      "from": {"value": "hello"},
      "to": "responseBodyText",
      "filter": [{"EqualTo": "a"}],
      "onFilterFail": [
        {
          "from": {"value": "hello"},
          "to": "responseBodyText",
          "filter": [{"EqualTo": "b"}],
          "onFilterFail": [{"from": {"value": "default"}, "to": "responseBodyText"}]
        }
      ]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    // "hello" != "a" -> fallback: "hello" != "b" -> fallback: "default"
    EXPECT_EQ(response_body_, "default");
}

TEST_F(Transform_test, FilterJsonConstraintSucceed)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "from": {"requestBody": ""},
      "to": "responseBodyText",
      "filter": [{"JsonConstraint": {"foo": 1}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, FilterJsonConstraintFail)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "from": {"requestBody": ""},
      "to": "responseBodyText",
      "filter": [{"JsonConstraint": {"foo": 2}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "JsonConstraint FAILED: expected value for key 'foo' differs regarding validated source");
}

// When target is a variable, a failed constraint builds empty variable: we will use condition var (negated) to put status code 400
// And we will set response string with validation report stored in <varname>.fail
TEST_F(Transform_test, FilterJsonConstraintFailAndTargetVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {
      "from": {"requestBody": ""},
      "to": {"var": "expectedBody"},
      "filter": [{"JsonConstraint": {"foo": 2}}]
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "from": {"value": "400"},
      "to": "responseStatusCode",
      "filter": [{"ConditionVar": "!expectedBody"}]
    }
    )"_json;
    const nlohmann::json item3 = R"({ "from": {"var": "expectedBody.fail"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 400);
    EXPECT_EQ(response_body_, "JsonConstraint FAILED: expected value for key 'foo' differs regarding validated source");
}

TEST_F(Transform_test, FilterSchemaId)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "from": {"requestBody": ""},
      "to": "responseBodyText",
      "filter": [{"SchemaId": "SchemaExample"}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Now must be OK:

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");

    // Now we want to invalidate:

    // Remove mandatory "foo" at 'request_body_':
    nlohmann::json badDocument = request_body_;
    badDocument.erase("foo");
    adata_.clearServerProvisions();
    provisionAndTransform(badDocument.dump());

    // Validations:
    EXPECT_EQ(response_body_, "At  of {\"node1\":{\"delaymilliseconds\":25,\"node2\":\"value-of-node1-node2\"}} - required property 'foo' not found in object\n");
}


// ==================== NEEDS STORAGE ====================

TEST_F(Transform_test, NeedsStorageFalseForBasicProvision)
{
    EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    auto provision = adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_FALSE(provision->needsStorage());
    EXPECT_FALSE(adata_.getServerProvisionData().needsStorage());
}

TEST_F(Transform_test, NeedsStorageTrueForServerEventSource)
{
    server_provision_json_["transform"] = R"([{"from": {"serverEvent": "requestBody"}, "to": {"responseBody": "/result"}, "filter": [{"as": "string"}]}])"_json;
    EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    auto provision = adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
    EXPECT_TRUE(adata_.getServerProvisionData().needsStorage());
}

TEST_F(Transform_test, NeedsStorageTrueForServerEventToPurgeTarget)
{
    server_provision_json_["transform"] = R"([{"from": "eraser", "to": {"serverEvent": "purge"}}])"_json;
    EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    auto provision = adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
}

/////////////////////////////////////
// SCOPED VARIABLE CHAIN PROPAGATION //
/////////////////////////////////////
TEST_F(Transform_test, ScopedVarPropagatesAcrossOutStateChain)
{
    // Link 1: set var.captured from request body, outState -> "step2"
    nlohmann::json link1 = ProvisionConfiguration_GET;
    link1["inState"] = "initial";
    link1["outState"] = "step2";
    link1["transform"] = R"([{"from": {"requestBody": "/foo"}, "to": {"var": "captured"}}])"_json;

    EXPECT_EQ(adata_.loadServerProvision(link1, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    auto provision1 = adata_.getServerProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision1);

    request_body_data_part_.assign(R"({"foo":"hello"})");
    provision1->transform(request_uri_, request_uri_path_, qmap_, request_body_data_part_, request_headers_, general_unique_server_sequence_, status_code_, response_headers_, response_body_, response_delay_ms_, out_state_, out_state_method_, out_state_uri_, client_provision_triggers_, variables_);

    EXPECT_EQ(variables_["captured"], "hello");
    EXPECT_EQ(out_state_, "step2");

    // Link 2: read var.captured into response body (same variables_ map = chain propagation)
    nlohmann::json link2 = ProvisionConfiguration_GET;
    link2["inState"] = "step2";
    link2["transform"] = R"([{"from": {"var": "captured"}, "to": "responseBodyText"}])"_json;

    EXPECT_EQ(adata_.loadServerProvision(link2, common_resources_), h2agent::model::AdminServerProvisionData::Success);
    auto provision2 = adata_.getServerProvisionData().find("step2", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision2);

    request_body_data_part_.assign("{}");
    provision2->transform(request_uri_, request_uri_path_, qmap_, request_body_data_part_, request_headers_, general_unique_server_sequence_, status_code_, response_headers_, response_body_, response_delay_ms_, out_state_, out_state_method_, out_state_uri_, client_provision_triggers_, variables_);

    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, FilterSplitDefaults)
{
    // Build test provision:
    server_provision_json_["transform"].push_back(TransformationItemSplitDefaults);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations (size=2, count=4, sep=".", numeric=false):
    // "55011223" -> groups "55","01","12","23" -> "55.01.12.23"
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "55.01.12.23");
}

TEST_F(Transform_test, FilterSplitNumeric)
{
    // Build test provision:
    server_provision_json_["transform"].push_back(TransformationItemSplitNumeric);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations (numeric=true strips leading zeros):
    // "55011223" -> groups "55","01","12","23" -> "55.1.12.23"
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "55.1.12.23");
}

TEST_F(Transform_test, FilterSplitCustom)
{
    // Build test provision:
    server_provision_json_["transform"].push_back(TransformationItemSplitCustom);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations (size=2, count=3, sep=":", filler="X"):
    // "AABBCC" -> groups "AA","BB","CC" -> "AA:BB:CC"
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "AA:BB:CC");
}

TEST_F(Transform_test, FilterSplitPadding)
{
    // Source shorter than size*count -> left-pad with filler
    const nlohmann::json item = R"(
    {
      "from": {"value": "123"},
      "to": "responseBodyText",
      "filter": [{"Split": {"size": 2, "count": 4, "numeric": true}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // "123" padded to "00000123" -> groups "00","00","01","23" -> numeric: "0.0.1.23"
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "0.0.1.23");
}

TEST_F(Transform_test, FilterSplitTruncation)
{
    // Source longer than size*count -> take last size*count chars
    const nlohmann::json item = R"(
    {
      "from": {"value": "987655011223"},
      "to": "responseBodyText",
      "filter": [{"Split": {"numeric": true}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // "987655011223" (12 chars) truncated to last 8: "55011223" -> "55.1.12.23"
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "55.1.12.23");
}

TEST_F(Transform_test, FilterBaseConvertDecToHex)
{
    server_provision_json_["transform"].push_back(TransformationItemBaseConvertDecToHex);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "ff");
}

TEST_F(Transform_test, FilterBaseConvertDecToHexCapital)
{
    server_provision_json_["transform"].push_back(TransformationItemBaseConvertDecToHexCapital);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "FF");
}

TEST_F(Transform_test, FilterBaseConvertHexToDec)
{
    server_provision_json_["transform"].push_back(TransformationItemBaseConvertHexToDec);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "255");
}

TEST_F(Transform_test, FilterBaseConvertDecToBin)
{
    server_provision_json_["transform"].push_back(TransformationItemBaseConvertDecToBin);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1010");
}

TEST_F(Transform_test, FilterBaseConvertZero)
{
    const nlohmann::json item = R"(
    {
      "from": {"value": "0"},
      "to": "responseBodyText",
      "filter": [{"BaseConvert": {"in": 10, "out": 16}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "0");
}

TEST_F(Transform_test, FilterBaseConvertInvalidSource)
{
    const nlohmann::json item = R"(
    {
      "from": {"value": "not_a_number"},
      "to": "responseBodyText",
      "filter": [{"BaseConvert": {"in": 10, "out": 16}}]
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);
    provisionAndTransform(request_body_.dump());
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "not_a_number");
}

TEST_F(Transform_test, SourceRequestHeaders)
{
    const nlohmann::json item = R"(
    {"from": "requestHeaders", "to": {"responseBody": "/hdrs"}, "filter": [{"as": "object"}]}
    )"_json;
    server_provision_json_["transform"].push_back(item);
    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    ASSERT_TRUE(body.contains("hdrs"));
    const auto &hdrs = body["hdrs"];
    EXPECT_TRUE(hdrs.is_array());
    EXPECT_GE(hdrs.size(), 2);
    bool found_ct = false, found_xv = false;
    for (const auto &h : hdrs) {
        if (h["name"] == "content-type" && h["value"] == "application/json") found_ct = true;
        if (h["name"] == "x-version" && h["value"] == "1.0.0") found_xv = true;
    }
    EXPECT_TRUE(found_ct);
    EXPECT_TRUE(found_xv);
}

TEST_F(Transform_test, SourceRequestHeadersDuplicateKeys)
{
    request_headers_.emplace("cookie", nghttp2::asio_http2::header_value{"session=abc"});
    request_headers_.emplace("cookie", nghttp2::asio_http2::header_value{"lang=en"});

    const nlohmann::json item = R"(
    {"from": "requestHeaders", "to": {"responseBody": "/hdrs"}, "filter": [{"as": "object"}]}
    )"_json;
    server_provision_json_["transform"].push_back(item);
    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    ASSERT_TRUE(body.contains("hdrs"));
    const auto &hdrs = body["hdrs"];
    EXPECT_TRUE(hdrs.is_array());
    int cookie_count = 0;
    for (const auto &h : hdrs) {
        if (h["name"] == "cookie") cookie_count++;
    }
    EXPECT_EQ(cookie_count, 2);
}

// Regression (CT filter_test test_023): a request without regular headers yields
// an EMPTY headers array, which must still be validated (v1 RequestHeaders never
// fails), so the constraint fails and the fallback rules answer 400.
static const nlohmann::json HeadersJsonConstraintTransform = R"([
  {
    "from": "requestHeaders",
    "to": {"var": "validatedHeaders"},
    "filter": [{"JsonConstraint": [{"name": "x-mandatory", "value": "required-value"}]}]
  },
  {
    "from": {"value": "400"},
    "to": "responseStatusCode",
    "filter": [{"ConditionVar": "!validatedHeaders"}]
  },
  {
    "from": {"var": "validatedHeaders.fail"},
    "to": "responseBodyText",
    "filter": [{"ConditionVar": "!validatedHeaders"}]
  }
])"_json;

TEST_F(Transform_test, SourceRequestHeadersEmptyJsonConstraintFails)
{
    request_headers_.clear();
    server_provision_json_["transform"] = HeadersJsonConstraintTransform;
    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 400);
    EXPECT_EQ(variables_["validatedHeaders"], "");
    EXPECT_NE(variables_["validatedHeaders.fail"].find("JsonConstraint FAILED"), std::string::npos);
    EXPECT_NE(response_body_.find("JsonConstraint FAILED"), std::string::npos);
}

TEST_F(Transform_test, SourceRequestHeadersEmptyJsonConstraintFailsOverStaleChainVariable)
{
    // Exact CT scenario: the previous request on the same key left the chain
    // variable validated ("1"); it must be overwritten, not kept.
    request_headers_.clear();
    variables_["validatedHeaders"] = "1";
    server_provision_json_["transform"] = HeadersJsonConstraintTransform;
    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 400);
    EXPECT_EQ(variables_["validatedHeaders"], "");
}

TEST_F(Transform_test, SourceRequestHeadersJsonConstraintSucceeds)
{
    request_headers_.emplace("x-mandatory", nghttp2::asio_http2::header_value{"required-value"});
    server_provision_json_["transform"] = HeadersJsonConstraintTransform;
    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(variables_["validatedHeaders"], "1");
}

// --- Vault with path navigation ---

TEST_F(Transform_test, TargetVaultWithPath)
{
    // Store a value at a specific path within a vault entry
    const nlohmann::json item1 = R"({ "from": {"value": "alice"}, "to": {"vault": "user", "path": "/name"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"value": "30"}, "to": {"vault": "user", "path": "/age"} })"_json;
    const nlohmann::json item3 = R"({ "from": {"vault": "user"}, "to": {"responseBody": "/result"}, "filter": [{"as": "object"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["result"]["name"], "alice");
    EXPECT_EQ(body["result"]["age"], "30");
}

TEST_F(Transform_test, SourceVaultWithPath)
{
    // Pre-load a JSON object, then read a subfield via path
    common_resources_.VaultPtr->load("config", nlohmann::json({{"host","localhost"},{"port",8080}}));

    const nlohmann::json item = R"({ "from": {"vault": "config", "path": "/host"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "localhost");
}

TEST_F(Transform_test, SourceVaultWithPathNumeric)
{
    // Read a numeric subfield — should come as json dump
    common_resources_.VaultPtr->load("config", nlohmann::json({{"port",8080}}));

    const nlohmann::json item = R"({ "from": {"vault": "config", "path": "/port"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "8080");
}

TEST_F(Transform_test, SourceVaultWithPathMissing)
{
    // Path does not exist — transform should be skipped
    common_resources_.VaultPtr->load("config", nlohmann::json({{"host","localhost"}}));

    const nlohmann::json item = R"({ "from": {"vault": "config", "path": "/missing"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetVaultJsonObject)
{
    // Store a JSON object from request body into vault, then read a subfield
    const nlohmann::json item1 = R"({ "from": {"requestBody": ""}, "to": {"vault": "captured"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "captured", "path": "/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, TargetVaultJsonObjectTyped)
{
    // vault.key.json.object stores source as native JSON object
    const nlohmann::json item1 = R"({ "from": {"requestBody": ""}, "to": {"vault": "obj"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "obj", "path": "/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, TargetVaultJsonObjectTypedWithPath)
{
    // vault.key.json.object./path stores object at sub-path
    const nlohmann::json item1 = R"({ "from": {"requestBody": ""}, "to": {"vault": "container", "path": "/nested"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "container", "path": "/nested/foo"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "1");
}

TEST_F(Transform_test, TargetVaultJsonObjectTypedFailsOnString)
{
    // vault.key.json.object should skip when source is a plain string (not an object)
    const nlohmann::json item1 = R"({ "from": {"value": "just-a-string"}, "to": {"vault": "obj"}, "filter": [{"as": "object"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "obj"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    // Transform was skipped, vault.obj does not exist, response body unchanged
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetVaultJsonJsonString)
{
    // vault.key.json.jsonstring parses a JSON-encoded string into a native object
    const nlohmann::json item1 = R"({ "from": {"value": "{\"volume\":100,\"time\":30}"}, "to": {"vault": "parsed"}, "filter": [{"as": "json"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "parsed", "path": "/volume"}, "to": {"responseBody": "/vol"}, "filter": [{"as": "unsigned"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"vault": "parsed", "path": "/time"}, "to": {"responseBody": "/t"}, "filter": [{"as": "unsigned"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["vol"], 100);
    EXPECT_EQ(body["t"], 30);
}

TEST_F(Transform_test, TargetVaultJsonJsonStringWithPath)
{
    // vault.key.json.jsonstring./path parses and stores at sub-path
    const nlohmann::json item1 = R"({ "from": {"value": "{\"a\":1}"}, "to": {"vault": "container", "path": "/data"}, "filter": [{"as": "json"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "container", "path": "/data/a"}, "to": {"responseBody": "/result"}, "filter": [{"as": "integer"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["result"], 1);
}

TEST_F(Transform_test, TargetVaultJsonJsonStringInvalidJson)
{
    // vault.key.json.jsonstring should skip when source is not valid JSON
    const nlohmann::json item1 = R"({ "from": {"value": "not-valid-json"}, "to": {"vault": "parsed"}, "filter": [{"as": "json"}] })"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "parsed"}, "to": "responseBodyText" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    // Transform was skipped, vault.parsed does not exist, response body unchanged
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, TargetVaultJsonJsonStringFromVar)
{
    // Typical flow: serverEvent → var (string) → vault.json.jsonstring → vault path navigation
    const nlohmann::json item1 = R"({ "from": {"value": "{\"name\":\"alice\",\"age\":30}"}, "to": {"var": "rawJson"} })"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "rawJson"}, "to": {"vault": "user"}, "filter": [{"as": "json"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"vault": "user", "path": "/name"}, "to": {"responseBody": "/userName"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"vault": "user", "path": "/age"}, "to": {"responseBody": "/userAge"}, "filter": [{"as": "unsigned"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["userName"], "alice");
    EXPECT_EQ(body["userAge"], 30);
}

TEST_F(Transform_test, FilterStrptimeBasic)
{
    // Parse a date string into epoch seconds
    const nlohmann::json item1 = R"({
      "from": {"value": "2026-03-30T16:00:00"},
      "to": {"responseBody": "/epoch"},
      "filter": [{"Strptime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}, {"as": "integer"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    // 2026-03-30T16:00:00 UTC = 1774886400
    EXPECT_EQ(body["epoch"], 1774886400);
}

TEST_F(Transform_test, FilterStrptimeMilliseconds)
{
    const nlohmann::json item1 = R"({
      "from": {"value": "2026-03-30T16:00:00"},
      "to": {"responseBody": "/epoch_ms"},
      "filter": [{"Strptime": {"fmt": "%Y-%m-%dT%H:%M:%S", "unit": "ms"}}, {"as": "integer"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["epoch_ms"], 1774886400000);
}

TEST_F(Transform_test, FilterStrptimeInvalidFormat)
{
    // Invalid date string should skip the transform
    const nlohmann::json item1 = R"({
      "from": {"value": "not-a-date"},
      "to": {"responseBody": "/epoch"},
      "filter": [{"Strptime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}, {"as": "integer"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterStrftimeBasic)
{
    // Format epoch seconds into a date string
    const nlohmann::json item1 = R"({
      "from": {"value": "1774886400"},
      "to": {"responseBody": "/date"},
      "filter": [{"Strftime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}, {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["date"], "2026-03-30T16:00:00");
}

TEST_F(Transform_test, FilterStrftimeFromMilliseconds)
{
    const nlohmann::json item1 = R"({
      "from": {"value": "1774886400000"},
      "to": {"responseBody": "/date"},
      "filter": [{"Strftime": {"fmt": "%Y-%m-%dT%H:%M:%S", "unit": "ms"}}, {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["date"], "2026-03-30T16:00:00");
}

TEST_F(Transform_test, FilterStrptimeStrftimeRoundTrip)
{
    // Parse → Sum 1 hour → Format back
    const nlohmann::json item1 = R"({
      "from": {"value": "2026-03-30T16:00:00"},
      "to": {"var": "epoch"},
      "filter": [{"Strptime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}]
    })"_json;
    const nlohmann::json item2 = R"({"from": {"var": "epoch"}, "to": {"var": "epoch"}, "filter": [{"Sum": 3600}]})"_json;
    const nlohmann::json item3 = R"({
      "from": {"var": "epoch"},
      "to": {"responseBody": "/future"},
      "filter": [{"Strftime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}, {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["future"], "2026-03-30T17:00:00");
}

TEST_F(Transform_test, FilterStrftimeNonNumericSource)
{
    // Strftime with a non-numeric source should skip
    const nlohmann::json item1 = R"({
      "from": {"value": "not-a-number"},
      "to": {"responseBody": "/date"},
      "filter": [{"Strftime": {"fmt": "%Y-%m-%dT%H:%M:%S"}}, {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterRegexKeyBasic)
{
    // Source is a JSON object with dynamic keys; RegexKey finds the matching one
    common_resources_.VaultPtr->load("data", nlohmann::json({
        {"user-1234-abc", nlohmann::json({{"name", "alice"}, {"age", 30}})},
        {"user-5678-def", nlohmann::json({{"name", "bob"}, {"age", 25}})}
    }));

    const nlohmann::json item1 = R"({"from": {"vault": "data"}, "to": {"vault": "found"}, "filter": [{"RegexKey": "user-5678-.*"}]})"_json;
    const nlohmann::json item2 = R"({
      "from": {"vault": "found", "path": "/name"},
      "to": {"responseBody": "/result"},
      "filter": [{"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["result"], "bob");
}

TEST_F(Transform_test, FilterRegexKeyFromRequestBody)
{
    // RegexKey on request body subpath
    const nlohmann::json item1 = R"({
      "from": {"requestBody": "/node1"},
      "to": {"vault": "match"},
      "filter": [{"RegexKey": "node.*"}]
    })"_json;
    const nlohmann::json item2 = R"({"from": {"vault": "match"}, "to": "responseBodyText"})"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "value-of-node1-node2");
}

TEST_F(Transform_test, FilterRegexKeyNoMatch)
{
    common_resources_.VaultPtr->load("data", nlohmann::json({{"keyA", 1}, {"keyB", 2}}));

    const nlohmann::json item1 = R"({
      "from": {"vault": "data"},
      "to": {"responseBody": "/val"},
      "filter": [{"RegexKey": "noMatch.*"}, {"as": "integer"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterRegexKeyNotAnObject)
{
    // Source is a string, not an object — filter should be skipped
    const nlohmann::json item1 = R"({"from": {"value": "just-a-string"}, "to": {"vault": "result"}, "filter": [{"RegexKey": ".*"}]})"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, FilterRegexKeyStringValue)
{
    // Matched key has a string value (not object)
    common_resources_.VaultPtr->load("cfg", nlohmann::json({{"host-prod", "10.0.0.1"}, {"host-dev", "127.0.0.1"}}));

    const nlohmann::json item1 = R"({
      "from": {"vault": "cfg"},
      "to": {"responseBody": "/addr"},
      "filter": [{"RegexKey": "host-prod"}, {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item1);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["addr"], "10.0.0.1");
}

TEST_F(Transform_test, FilterRegexKeyCaptureGroupsVault)
{
    // RegexKey with capture groups: value goes to vault, key+groups go to variables
    common_resources_.VaultPtr->load("data", nlohmann::json({
        {"imsi-5550110000000:rule1", nlohmann::json({{"qos", 15}})},
        {"imsi-5550220000000:rule2", nlohmann::json({{"qos", 9}})}
    }));

    const nlohmann::json item1 = R"delim({
      "from": {"vault": "data"},
      "to": {"vault": "matched"},
      "filter": [{"RegexKey": "imsi-([0-9]+):(rule1)"}]
    })delim"_json;
    const nlohmann::json item2 = R"({ "from": {"vault": "matched", "path": "/qos"}, "to": {"responseBody": "/qos"}, "filter": [{"as": "integer"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"var": "matched.0"}, "to": {"responseBody": "/key"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"var": "matched.1"}, "to": {"responseBody": "/imsi"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item5 = R"({ "from": {"var": "matched.2"}, "to": {"responseBody": "/rule"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);
    server_provision_json_["transform"].push_back(item5);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["qos"], 15);
    EXPECT_EQ(body["key"], "imsi-5550110000000:rule1");
    EXPECT_EQ(body["imsi"], "5550110000000");
    EXPECT_EQ(body["rule"], "rule1");
}

TEST_F(Transform_test, FilterRegexKeyCaptureGroupsVar)
{
    // RegexKey with var target: value as string in target, key+groups in .0/.1/.2
    common_resources_.VaultPtr->load("cfg", nlohmann::json({
        {"host-prod-eu", "10.0.0.1"},
        {"host-dev-us", "127.0.0.1"}
    }));

    const nlohmann::json item1 = R"delim({
      "from": {"vault": "cfg"},
      "to": {"var": "result"},
      "filter": [{"RegexKey": "host-(prod)-(eu)"}]
    })delim"_json;
    const nlohmann::json item2 = R"({ "from": {"var": "result"}, "to": {"responseBody": "/value"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item3 = R"({ "from": {"var": "result.0"}, "to": {"responseBody": "/key"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item4 = R"({ "from": {"var": "result.1"}, "to": {"responseBody": "/env"}, "filter": [{"as": "string"}] })"_json;
    const nlohmann::json item5 = R"({ "from": {"var": "result.2"}, "to": {"responseBody": "/region"}, "filter": [{"as": "string"}] })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);
    server_provision_json_["transform"].push_back(item5);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json body = nlohmann::json::parse(response_body_);
    EXPECT_EQ(body["value"], "10.0.0.1");
    EXPECT_EQ(body["key"], "host-prod-eu");
    EXPECT_EQ(body["env"], "prod");
    EXPECT_EQ(body["region"], "eu");
}

TEST_F(Transform_test, FilterSizeJsonArray)
{
    const nlohmann::json item = R"({
      "from": {"requestBody": "/node1/node2"},
      "to": {"responseBody": "/count"},
      "filter": ["Size", {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item);

    // node1.node2 is "value-of-node1-node2" (string, 20 chars)
    // Use a body with an array instead:
    nlohmann::json body;
    body["items"] = nlohmann::json::array({1, 2, 3, 4, 5});
    const nlohmann::json item2 = R"({
      "from": {"requestBody": "/items"},
      "to": {"responseBody": "/count"},
      "filter": ["Size", {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].clear();
    server_provision_json_["transform"].push_back(item2);

    provisionAndTransform(body.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json resp = nlohmann::json::parse(response_body_);
    EXPECT_EQ(resp["count"], "5");
}

TEST_F(Transform_test, FilterSizeJsonObject)
{
    nlohmann::json body;
    body["data"] = nlohmann::json({{"a", 1}, {"b", 2}, {"c", 3}});
    const nlohmann::json item = R"({
      "from": {"requestBody": "/data"},
      "to": {"responseBody": "/count"},
      "filter": ["Size", {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item);

    provisionAndTransform(body.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json resp = nlohmann::json::parse(response_body_);
    EXPECT_EQ(resp["count"], "3");
}

TEST_F(Transform_test, FilterSizeString)
{
    const nlohmann::json item = R"({
      "from": {"value": "hello"},
      "to": {"responseBody": "/count"},
      "filter": ["Size", {"as": "string"}]
    })"_json;
    server_provision_json_["transform"].push_back(item);

    provisionAndTransform(request_body_.dump());

    EXPECT_EQ(status_code_, 200);
    nlohmann::json resp = nlohmann::json::parse(response_body_);
    EXPECT_EQ(resp["count"], "5");
}
