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
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
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

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCapture = R"delim(
{
  "source": "request.uri",
  "target": "var.uri_parts",
  "filter": {
    "RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)"
}
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCaptureUnmatches = R"delim(
{
  "source": "request.uri",
  "target": "var.uri_parts",
  "filter": {
    "RegexCapture": "(/app/v2/foo/bar/)([a-z]*)(\\?name=proc)"
}
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexCaptureWithGlobalVariable = R"delim(
{
  "source": "request.uri",
  "target": "globalVar.g_uri_parts",
  "filter": {
    "RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)"
}
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemRegexReplace = R"delim(
{
  "source": "request.uri.path",
  "target": "response.body.string",
  "filter": {
    "RegexReplace": {
      "rgx": "(/app/v1/foo/bar/)([0-9]*)",
      "fmt": "$2"
    }
  }
}
)delim"_json;

// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json TransformationItemBadRegex = R"delim(
{
  "source": "request.uri",
  "target": "response.body.string",
  "filter": {
    "RegexCapture": "["
  }
}
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

// Transformation items to cover asString() for different types of filters:
const nlohmann::json TransformationAsStringFilter1 = R"({ "source": "value.foo", "filter": { "Sum": 1 }, "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringFilter2 = R"({ "source": "value.foo", "filter": { "EqualTo": "@{bar}" }, "target": "response.body.string" })"_json;
const nlohmann::json TransformationAsStringFilter3 = TransformationItemRegexCapture;
const nlohmann::json TransformationAsStringFilter4 = TransformationItemRegexReplace;




// XXXXXXXXXXXX client provision
// XXXXXXXXXXXX client endpoint

class Transform_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::common_resources_t common_resources_{};
    std::shared_ptr<h2agent::model::AdminSchema> request_schema_{};
    std::shared_ptr<h2agent::model::AdminSchema> response_schema_{};
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
    bool check_schemas_{}; // no need to put requestSchemaId or responseSchemaId on provision:
    // we will use our schemas when needed.

    // outputs:
    unsigned int status_code_{};
    nghttp2::asio_http2::header_map response_headers_{};
    std::string response_body_{};
    unsigned int response_delay_ms_{};
    std::string out_state_{};
    std::string out_state_method_{};
    std::string out_state_uri_{};

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

        request_schema_ = std::make_shared<h2agent::model::AdminSchema>();
        request_schema_->load(RequestSchema);
        response_schema_ = std::make_shared<h2agent::model::AdminSchema>();
        response_schema_->load(ResponseSchema);

        //////////////////////
        // COMMON RESOURCES //
        //////////////////////
        common_resources_.ConfigurationPtr = new h2agent::model::Configuration();
        common_resources_.GlobalVariablePtr = new h2agent::model::GlobalVariable();
        common_resources_.FileManagerPtr = new h2agent::model::FileManager(nullptr);
        common_resources_.MockServerDataPtr = new h2agent::model::MockServerData();

        // Global variables:
        common_resources_.GlobalVariablePtr->load("myGlobalVariable","myGlobalVariable_value");

        // Simulated event:
        h2agent::model::DataPart postRequestBodyDataPart;
        postRequestBodyDataPart.assign("{\"foo\":15}");
        std::string responseBody = "{\"bar\":25}";
        common_resources_.MockServerDataPtr->loadEvent(h2agent::model::DataKey("POST", "/app/v1/foo/bar/1"), "previous-state", "state", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()), 201, request_headers_ /* shared to simplify */, response_headers_ /* shared to simplify */, postRequestBodyDataPart, responseBody, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);
    }

    void provisionAndTransform(const std::string &requestBody) {

        //adata_.clearServerProvisions(); // class instance is created in every test
        EXPECT_EQ(adata_.getServerProvisionData().size(), 0);
        EXPECT_EQ(adata_.loadServerProvision(server_provision_json_, common_resources_), h2agent::model::AdminServerProvisionData::Success);
        std::shared_ptr<h2agent::model::AdminServerProvision> provision = adata_.getServerProvisionData().find(in_state_, request_method_, request_uri_);
        ASSERT_TRUE(provision);
        EXPECT_EQ(adata_.getServerProvisionData().size(), 1);

        request_body_data_part_.assign(requestBody);
        provision->transform(request_uri_, request_uri_path_, qmap_, request_body_data_part_, request_headers_, general_unique_server_sequence_, status_code_, response_headers_, response_body_, response_delay_ms_, out_state_, out_state_method_, out_state_uri_, (check_schemas_ ? request_schema_:nullptr), (check_schemas_ ? response_schema_:nullptr));
    }

    ~Transform_test() {
        delete(common_resources_.ConfigurationPtr);
        delete(common_resources_.GlobalVariablePtr);
        delete(common_resources_.FileManagerPtr);
        delete(common_resources_.MockServerDataPtr);
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
    // Run transformation:
    check_schemas_ = true;
    request_body_["foo"] = "this is a string"; // must be number !
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 400); // transformations are interrupted when request is invalid: response body will be empty:
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, ResponseNotValidated)
{
    // Run transformation:
    check_schemas_ = true;

    // Remove mandatory 'bar':
    auto path = nlohmann::json_pointer<nlohmann::json> {"/responseBody"};
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
    const nlohmann::json item = R"({ "source": "request.uri", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.uri.path", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.uri.param.name", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.uri.param.missing", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body./foo", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body./missing", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.header.x-version", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.header.missing", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "response.body", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "response.body./bar", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "response.body./missing", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "eraser", "target": "response.body.json.object./remove-me" })"_json;
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
    const nlohmann::json item = R"({ "source": "eraser", "target": "response.body.json.object" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "");
}

TEST_F(Transform_test, SourceEraserGlobalVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "source": "value.myGlobalVar_value", "target": "globalVar.myGlobalVar" })"_json;
    const nlohmann::json item2 = R"({ "source": "globalVar.myGlobalVar", "target": "response.body.json.object./globalVariableBeforeRemoval" })"_json;
    const nlohmann::json item3 = R"({ "source": "eraser", "target": "globalVar.myGlobalVar" })"_json;
    const nlohmann::json item4 = R"({ "source": "globalVar.myGlobalVar", "target": "response.body.json.object./globalVariableAfterRemoval" })"_json; // will be skipped
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    expectedJson["globalVariableBeforeRemoval"] = "myGlobalVar_value";
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, SourceMath)
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "math.1+1", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "random.10.10", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "randomset.hi|hi|hi", "target": "response.body.string" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "timestamp.ms", "target": "response.body.json.object./timestampMs" })"_json;
    const nlohmann::json item2 = R"({ "source": "timestamp.us", "target": "response.body.json.object./timestampUs" })"_json;
    const nlohmann::json item3 = R"({ "source": "timestamp.ns", "target": "response.body.json.object./timestampNs" })"_json;
    const nlohmann::json item4 = R"({ "source": "timestamp.s", "target": "response.body.json.object./timestampS" })"_json;
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
    const nlohmann::json item = R"({ "source": "strftime.This month is %b.", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "recvseq", "target": "response.body.string" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "value.myVariable_value", "target":"var.myVariable" })"_json;
    const nlohmann::json item2 = R"({ "source": "var.myVariable", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "var.myVariable", "target": "response.body.string" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

TEST_F(Transform_test, SourceGlobalVariable)
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "globalVar.myGlobalVariable", "target": "response.body.string" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myGlobalVariable_value");
}

TEST_F(Transform_test, SourceGlobalVariableUnknown)
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "globalVar.missingGlobalVariable", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "serverEvent.requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "serverEvent.requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/missing", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.hello", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "inState", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "txtFile./tmp/h2agent.UT.Transform_test.SourceTxtFile.txt", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "binFile./tmp/h2agent.UT.Transform_test.SourceBinFile.bin", "target": "response.body.string" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "command.echo -n foo", "target": "response.body.string" })"_json;
    const nlohmann::json item2 = R"({ "source": "math.200+@{rc}", "target": "response.statusCode" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "command.unknowncommand", "target": "var.command-output" })"_json;
    const nlohmann::json item2 = R"({ "source": "value.@{command-output}@{rc}", "target": "response.body.string" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
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
    const nlohmann::json item = R"({ "source": "value.plain text", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.0xc0a8", "target": "response.body.hexstring" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.hello", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "random.10.10", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "recvseq", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "math.1/2", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body./boo", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.json.object" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.{\"bar\":\"bar-8\"}", "target": "response.body.json.jsonstring" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.{\"bar:\"bar-8\"}", "target": "response.body.json.jsonstring" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.hello", "target": "response.body.json.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.-15", "target": "response.body.json.integer" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.15", "target": "response.body.json.unsigned" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.3.14", "target": "response.body.json.float" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.non-empty-string-means-true", "target": "response.body.json.boolean" })"_json;
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
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.json.object./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.{\"bar\":\"bar-8\"}", "target": "response.body.json.jsonstring./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.hello", "target": "response.body.json.string./target" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "{\"bar\":\"bar-1\",\"remove-me\":0,\"target\":\"hello\"}");
}

TEST_F(Transform_test, TargetResponseBodyJsonStringMissingPathException) // json_pointer with missing path
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.json.string.target" })"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform("hello");

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, ProvisionConfiguration_GET_responseBodyAsString);
}

// Setting json field with binary data may raise an exception:
// echo -en '\x80\x01' | curl --http2-prior-knowledge -i -H 'content-type:application/octet-stream' -X GET "$TRAFFIC_URL/uri" --data-binary @-
TEST_F(Transform_test, TargetResponseBodyJsonStringPathExceptionDueToBinary)
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "request.body", "target": "response.body.json.string./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.-15", "target": "response.body.json.integer./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.15", "target": "response.body.json.unsigned./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.3.14", "target": "response.body.json.float./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.non-empty-string-means-true", "target": "response.body.json.boolean./target" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.1.2.3", "target": "response.header.version" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.201", "target": "response.statusCode" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.125", "target": "response.delayMs" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "value.hello", "target": "var.varname" })"_json;
    const nlohmann::json item2 = R"({ "source": "var.varname", "target": "response.body.string" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "hello");
}

TEST_F(Transform_test, TargetGlobalVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "source": "value.hello", "target": "globalVar.globalvarname" })"_json;
    const nlohmann::json item2 = R"({ "source": "globalVar.globalvarname", "target": "response.body.string" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "value.any", "target": "serverEvent.requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo" })"_json;
    const nlohmann::json item2 = R"({ "source": "eraser", "target": "serverEvent.requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo" })"_json;
    const nlohmann::json item3 = R"({ "source": "eraser", "target": "serverEvent.requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo" })"_json;
    const nlohmann::json item4 = R"({ "source": "serverEvent.requestMethod=POST&requestUri=/app/v1/foo/bar/1&eventNumber=-1&eventPath=/requestBody/foo", "target": "response.body.string" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.out-state", "target": "outState" })"_json;
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
    const nlohmann::json item = R"({ "source": "value.foreign-state", "target": "outState.POST./foreign/uri" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "value.example2", "target": "txtFile./tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt" })"_json;
    const nlohmann::json item2 = R"({ "source": "txtFile./tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt", "target": "response.body.string" })"_json;
    const nlohmann::json item3 = R"({ "source": "eraser", "target": "txtFile./tmp/h2agent.UT.Transform_test.SourceTxtFile2.txt" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "binFile./tmp/h2agent.UT.Transform_test.SourceBinFile.bin", "target": "binFile./tmp/h2agent.UT.Transform_test.SourceBinFile2.bin" })"_json;
    const nlohmann::json item2 = R"({ "source": "eraser", "target": "binFile./tmp/h2agent.UT.Transform_test.SourceBinFile.bin" })"_json;
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


TEST_F(Transform_test, TargetBreak)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "source": "value.", "target": "break" })"_json; // this break is not triggered (empty value)
    const nlohmann::json item2 = R"({ "source": "value.hello", "target": "response.body.string" })"_json;
    const nlohmann::json item3 = R"({ "source": "value.non-empty", "target": "break" })"_json;
    const nlohmann::json item4 = R"({ "source": "value.201", "target": "response.statusCode" })"_json; // not reached (previous break is non-empty-triggered)
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

///////////////////////////////////////////////////
// TEST FILTERS (source/target are not relevant) //
///////////////////////////////////////////////////

TEST_F(Transform_test, FilterIncompatibleWithEraser)
{
    // Build test provision:
    const nlohmann::json item = R"({ "source": "eraser", "filter": { "ConditionVar": "myGlobalVariable" }, "target": "response.body.json.object./remove-me" })"_json;
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
    const nlohmann::json item1 = R"({ "source": "var.uri_parts", "target": "response.body.json.string./uri_parts" })"_json;
    const nlohmann::json item2 = R"({ "source": "var.uri_parts.1", "target": "response.body.json.string./uri_parts.1" })"_json;
    const nlohmann::json item3 = R"({ "source": "var.uri_parts.2", "target": "response.body.json.string./uri_parts.2" })"_json;
    const nlohmann::json item4 = R"({ "source": "var.uri_parts.3", "target": "response.body.json.string./uri_parts.3" })"_json;
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

TEST_F(Transform_test, FilterRegexCaptureWithGlobalVariable)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "source": "globalVar.g_uri_parts", "target": "response.body.json.string./g_uri_parts" })"_json;
    const nlohmann::json item2 = R"({ "source": "globalVar.g_uri_parts.1", "target": "response.body.json.string./g_uri_parts.1" })"_json;
    const nlohmann::json item3 = R"({ "source": "globalVar.g_uri_parts.2", "target": "response.body.json.string./g_uri_parts.2" })"_json;
    const nlohmann::json item4 = R"({ "source": "globalVar.g_uri_parts.3", "target": "response.body.json.string./g_uri_parts.3" })"_json;
    server_provision_json_["transform"].push_back(TransformationItemRegexCaptureWithGlobalVariable);
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);
    server_provision_json_["transform"].push_back(item4);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);

    nlohmann::json expectedJson = ProvisionConfiguration_GET["responseBody"];
    expectedJson["g_uri_parts"] = "/app/v1/foo/bar/1?name=test";
    expectedJson["g_uri_parts.1"] = "/app/v1/foo/bar/";
    expectedJson["g_uri_parts.2"] = "1";
    expectedJson["g_uri_parts.3"] = "?name=test";
    EXPECT_EQ(response_body_, expectedJson.dump());
}

TEST_F(Transform_test, FilterRegexCaptureUnmatches)
{
    // Build test provision:
    const nlohmann::json item1 = R"({ "source": "var.uri_parts", "target": "response.body.json.string./uri_parts" })"_json;
    const nlohmann::json item2 = R"({ "source": "var.uri_parts.1", "target": "response.body.json.string./uri_parts.1" })"_json;
    const nlohmann::json item3 = R"({ "source": "var.uri_parts.2", "target": "response.body.json.string./uri_parts.2" })"_json;
    const nlohmann::json item4 = R"({ "source": "var.uri_parts.3", "target": "response.body.json.string./uri_parts.3" })"_json;
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
      "source": "inState",
      "target": "response.body.string",
      "filter": {
        "Append": " state, and variable is: @{myGlobalVariable}"
      }
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "initial state, and variable is: myGlobalVariable_value");
}

TEST_F(Transform_test, FilterPrepend)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "source": "inState",
      "target": "response.body.string",
      "filter": {
        "Prepend": "@{myGlobalVariable} variable value, and state is: "
      }
    }
    )"_json;
    server_provision_json_["transform"].push_back(item);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myGlobalVariable_value variable value, and state is: initial");
}

TEST_F(Transform_test, FilterSum)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {
      "source": "value.-3",
      "target": "var.accum",
      "filter": {
        "Sum": -5
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "var.accum",
      "target": "response.body.string",
      "filter": {
        "Sum": 9
      }
    }
    )"_json;
    const nlohmann::json item3 = R"(
    {
      "source": "value.200.5",
      "target": "response.statusCode",
      "filter": {
        "Sum": 0.5
      }
    }
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
    {
      "source": "value.10",
      "target": "var.accum",
      "filter": {
        "Multiply": 2
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "var.accum",
      "target": "var.accum",
      "filter": {
        "Multiply": -0.5
      }
    }
    )"_json;
    const nlohmann::json item3 = R"(
    {
      "source": "var.accum",
      "target": "response.body.string",
      "filter": {
        "Multiply": -1
      }
    }
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
      "source": "value.myGlobalVariable_value",
      "target": "response.body.string",
      "filter": {
        "EqualTo": "@{myGlobalVariable}"
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "value.this is different",
      "target": "response.body.string",
      "filter": {
        "EqualTo": "@{myGlobalVariable}"
      }
    }
    )"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 200);
    EXPECT_EQ(response_body_, "myGlobalVariable_value");
}

TEST_F(Transform_test, FilterDifferentFrom)
{
    // Build test provision:
    const nlohmann::json item1 = R"(
    {
      "source": "value.myGlobalVariable_value",
      "target": "response.body.string",
      "filter": {
        "DifferentFrom": "@{myGlobalVariable}"
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "value.this is different",
      "target": "response.body.string",
      "filter": {
        "DifferentFrom": "@{myGlobalVariable}"
      }
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
    {
      "source": "value.hi",
      "target": "response.body.string",
      "filter": {
        "ConditionVar": "myGlobalVariable"
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "value.bye",
      "target": "response.body.string",
      "filter": {
        "ConditionVar": "missingVar"
      }
    }
    )"_json;
    const nlohmann::json item3 = R"(
    {
      "source": "value.201",
      "target": "response.statusCode",
      "filter": {
        "ConditionVar": "!missingVar"
      }
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

TEST_F(Transform_test, FilterJsonConstraintSucceed)
{
    // Build test provision:
    const nlohmann::json item = R"(
    {
      "source": "request.body",
      "target": "response.body.string",
      "filter": {
        "JsonConstraint": {
          "foo": 1
        }
      }
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
      "source": "request.body",
      "target": "response.body.string",
      "filter": {
        "JsonConstraint": {
          "foo": 2
        }
      }
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
      "source": "request.body",
      "target": "var.expectedBody",
      "filter": {
        "JsonConstraint": {
          "foo": 2
        }
      }
    }
    )"_json;
    const nlohmann::json item2 = R"(
    {
      "source": "value.400",
      "target": "response.statusCode",
      "filter": {
        "ConditionVar": "!expectedBody"
      }
    }
    )"_json;
    const nlohmann::json item3 = R"({ "source": "var.expectedBody.fail", "target": "response.body.string" })"_json;
    server_provision_json_["transform"].push_back(item1);
    server_provision_json_["transform"].push_back(item2);
    server_provision_json_["transform"].push_back(item3);

    // Run transformation:
    provisionAndTransform(request_body_.dump());

    // Validations:
    EXPECT_EQ(status_code_, 400);
    EXPECT_EQ(response_body_, "JsonConstraint FAILED: expected value for key 'foo' differs regarding validated source");
}

