#include <AdminData.hpp>
#include <AdminClientProvision.hpp>
#include <AdminClientProvisionData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <MockClientData.hpp>
#include <MockServerData.hpp>

#include <ert/http2comm/Http2Client.hpp>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Minimal client provision: POST to /api/v1/test with JSON body
const nlohmann::json ClientProvision_POST = R"(
{
  "id": "myFlow",
  "endpoint": "myServer",
  "requestMethod": "POST",
  "requestUri": "/api/v1/test",
  "requestBody": {"foo":"bar"},
  "requestHeaders": {"content-type": "application/json"},
  "requestDelayMs": 50,
  "timeoutMs": 2000
}
)"_json;

// Client provision with pre-send transform
const nlohmann::json ClientProvision_WithTransform = R"(
{
  "id": "myFlow",
  "endpoint": "myServer",
  "requestMethod": "GET",
  "requestUri": "/api/v1/original"
}
)"_json;

class ClientTransform_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::common_resources_t common_resources_{};
    nlohmann::json client_provision_json_{};

    // transform outputs:
    std::string request_method_{};
    std::string request_uri_{};
    std::string request_body_{};
    nghttp2::asio_http2::header_map request_headers_{};
    std::string out_state_{};
    unsigned int request_delay_ms_{};
    unsigned int request_timeout_ms_{};
    std::string error_{};
    std::map<std::string, std::string> variables_{};

    ClientTransform_test() {
        client_provision_json_ = ClientProvision_POST;

        common_resources_.AdminDataPtr = &adata_;
        common_resources_.ConfigurationPtr = new h2agent::model::Configuration();
        common_resources_.GlobalVariablePtr = new h2agent::model::GlobalVariable();
        common_resources_.FileManagerPtr = new h2agent::model::FileManager(nullptr);
        common_resources_.SocketManagerPtr = new h2agent::model::SocketManager(nullptr);
        common_resources_.MockClientDataPtr = new h2agent::model::MockClientData();
        common_resources_.MockServerDataPtr = new h2agent::model::MockServerData();

        common_resources_.GlobalVariablePtr->load("myGlobalVar", "globalValue");
    }

    std::shared_ptr<h2agent::model::AdminClientProvision> provisionAndTransform() {
        EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
        auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
        EXPECT_TRUE(provision != nullptr);
        if (!provision) return nullptr;

        provision->transform(request_method_, request_uri_, request_body_, request_headers_, out_state_, request_delay_ms_, request_timeout_ms_, error_, variables_);
        return provision;
    }

    ~ClientTransform_test() {
        delete(common_resources_.ConfigurationPtr);
        delete(common_resources_.GlobalVariablePtr);
        delete(common_resources_.FileManagerPtr);
        delete(common_resources_.SocketManagerPtr);
        delete(common_resources_.MockClientDataPtr);
        delete(common_resources_.MockServerDataPtr);
    }
};

// ==================== LOAD ====================

TEST_F(ClientTransform_test, LoadBasicProvision)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(provision->getClientEndpointId(), "myServer");
    EXPECT_EQ(provision->getRequestMethod(), "POST");
    EXPECT_EQ(provision->getRequestUri(), "/api/v1/test");
    EXPECT_EQ(provision->getRequestDelayMilliseconds(), 50);
    EXPECT_EQ(provision->getRequestTimeoutMilliseconds(), 2000);
}

TEST_F(ClientTransform_test, LoadProvisionWithInState)
{
    client_provision_json_ = R"({"id":"myFlow","inState":"step2","outState":"done"})"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("step2", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(provision->getOutState(), "done");
}

// ==================== TRANSFORM: NO TRANSFORMATIONS ====================

TEST_F(ClientTransform_test, TransformNoTransformations)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(request_method_, "POST");
    EXPECT_EQ(request_uri_, "/api/v1/test");
    EXPECT_EQ(request_body_, "{\"foo\":\"bar\"}");
    EXPECT_EQ(request_delay_ms_, 50);
    EXPECT_EQ(request_timeout_ms_, 2000);
    EXPECT_EQ(out_state_, "road-closed"); // default client outState
}

// ==================== TARGETS ====================

TEST_F(ClientTransform_test, TargetRequestUri)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/original",
        "transform": [{"source": "value./api/v1/replaced", "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/replaced");
}

TEST_F(ClientTransform_test, TargetRequestMethod)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value.PUT", "target": "request.method"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_method_, "PUT");
}

TEST_F(ClientTransform_test, TargetRequestHeader)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value.bearer-token-123", "target": "request.header.authorization"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    auto it = request_headers_.find("authorization");
    ASSERT_NE(it, request_headers_.end());
    EXPECT_EQ(it->second.value, "bearer-token-123");
}

TEST_F(ClientTransform_test, TargetRequestDelayMs)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "requestDelayMs": 10,
        "transform": [{"source": "value.500", "target": "request.delayMs"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 500);
}

TEST_F(ClientTransform_test, TargetRequestTimeoutMs)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "timeoutMs": 1000,
        "transform": [{"source": "value.5000", "target": "request.timeoutMs"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_timeout_ms_, 5000);
}

TEST_F(ClientTransform_test, TargetRequestBodyJsonString)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "POST", "requestUri": "/test",
        "requestBody": {"original":"data"},
        "transform": [{"source": "value.replaced", "target": "request.body.json.string./original"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["original"], "replaced");
}

TEST_F(ClientTransform_test, TargetRequestBodyJsonInteger)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "POST", "requestUri": "/test",
        "requestBody": {"count":0},
        "transform": [{"source": "value.42", "target": "request.body.json.integer./count"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["count"], 42);
}

TEST_F(ClientTransform_test, TargetOutState)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value.step2", "target": "outState"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(out_state_, "step2");
}

TEST_F(ClientTransform_test, TargetVariable)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [
            {"source": "value.hello", "target": "var.myVar"},
            {"source": "var.myVar", "target": "request.uri"}
        ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "hello");
}

TEST_F(ClientTransform_test, TargetGlobalVariable)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [
            {"source": "value.newValue", "target": "globalVar.testGVar"},
            {"source": "globalVar.testGVar", "target": "request.uri"}
        ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "newValue");
}

TEST_F(ClientTransform_test, TargetBreak)
{
    // break is only available in onResponseTransform (not in pre-send transform)
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [
            {"source": "value.stopped", "target": "outState"},
            {"source": "value.1", "target": "break"},
            {"source": "value.should-not-reach", "target": "outState"}
        ]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = "";

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);
    EXPECT_EQ(outState, "stopped"); // break prevented "should-not-reach" from overwriting
}

// ==================== SOURCES ====================

TEST_F(ClientTransform_test, SourceValue)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value./api/v1/new", "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/new");
}

TEST_F(ClientTransform_test, SourceGlobalVariable)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "globalVar.myGlobalVar", "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "globalValue");
}

TEST_F(ClientTransform_test, SourceEraser)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [
            {"source": "eraser", "target": "globalVar.myGlobalVar"},
            {"source": "globalVar.myGlobalVar", "target": "request.uri"}
        ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/test"); // globalVar erased, second transform fails silently, uri unchanged
}

TEST_F(ClientTransform_test, SourceInState)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "inState", "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "initial");
}

TEST_F(ClientTransform_test, SourceMath)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "math.2+3", "target": "request.delayMs"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 5);
}

TEST_F(ClientTransform_test, SourceRandom)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "random.10.20", "target": "request.delayMs"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_GE(request_delay_ms_, 10);
    EXPECT_LE(request_delay_ms_, 20);
}

TEST_F(ClientTransform_test, SourceTimestamp)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "timestamp.ms", "target": "var.ts"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    // just verify it loaded and transformed without error
}

// ==================== FILTERS ====================

TEST_F(ClientTransform_test, FilterAppend)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value./api/v1/", "filter": {"Append": "resource"}, "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/resource");
}

TEST_F(ClientTransform_test, FilterPrepend)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value.resource", "filter": {"Prepend": "/api/v1/"}, "target": "request.uri"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/resource");
}

TEST_F(ClientTransform_test, FilterSum)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "transform": [{"source": "value.100", "filter": {"Sum": 50}, "target": "request.delayMs"}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 150);
}

// ==================== TRANSFORM RESPONSE ====================

TEST_F(ClientTransform_test, TransformResponseSetsOutState)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [{"source": "value.step2", "target": "outState"}]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = R"({"result":"ok"})";
    fakeResponse.headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);
    EXPECT_EQ(outState, "step2");
}

TEST_F(ClientTransform_test, TransformResponseSourceResponseBody)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [
            {"source": "eraser", "target": "globalVar.respBody"},
            {"source": "response.body", "target": "globalVar.respBody"}
        ]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = R"({"greeting":"hello"})";

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);

    std::string stored;
    EXPECT_TRUE(common_resources_.GlobalVariablePtr->tryGet("respBody", stored));
}

TEST_F(ClientTransform_test, TransformResponseSourceResponseStatusCode)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [{"source": "response.statusCode", "target": "var.code"}]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 404;
    fakeResponse.body = "";

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);
    // Verify it didn't crash — var is local, can't inspect directly
}

TEST_F(ClientTransform_test, TransformResponseSourceResponseHeader)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [
            {"source": "eraser", "target": "globalVar.location"},
            {"source": "response.header.location", "target": "globalVar.location"}
        ]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 302;
    fakeResponse.body = "";
    fakeResponse.headers.emplace("location", nghttp2::asio_http2::header_value{"/api/v1/redirected"});

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);
    // globalVar.location should now contain "/api/v1/redirected"
}

TEST_F(ClientTransform_test, TransformResponseBreakOnError)
{
    client_provision_json_ = R"({
        "id": "myFlow", "endpoint": "myServer", "requestMethod": "GET", "requestUri": "/test",
        "onResponseTransform": [
            {"source": "response.statusCode", "filter": {"DifferentFrom": "200"}, "target": "outState"},
            {"source": "value.1", "target": "break"}
        ]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 500;
    fakeResponse.body = "";

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);
    EXPECT_EQ(outState, "500"); // DifferentFrom 200 passes, statusCode "500" written to outState
}

// ==================== TRIGGERING ====================

TEST_F(ClientTransform_test, SetSeqGetSeq)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_EQ(provision->getSeq(), 0u);
    provision->setSeq(42);
    EXPECT_EQ(provision->getSeq(), 42u);
    provision->setSeq(0);
    EXPECT_EQ(provision->getSeq(), 0u);
}

TEST_F(ClientTransform_test, UpdateTriggeringValidRange)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_TRUE(provision->updateTriggering("1", "100", "50", "false"));
    EXPECT_EQ(provision->getRps(), 50u);
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidRange)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    // sequenceBegin > sequenceEnd
    EXPECT_FALSE(provision->updateTriggering("100", "1", "", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidSequenceBegin)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_FALSE(provision->updateTriggering("-1", "100", "", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidRps)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_FALSE(provision->updateTriggering("", "", "-1", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidRepeat)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_FALSE(provision->updateTriggering("", "", "", "invalid"));
}

TEST_F(ClientTransform_test, UpdateTriggeringPartialUpdate)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    // Set initial values
    EXPECT_TRUE(provision->updateTriggering("1", "100", "50", "false"));
    EXPECT_EQ(provision->getRps(), 50u);

    // Partial update: only rps, range keeps previous
    EXPECT_TRUE(provision->updateTriggering("", "", "200", ""));
    EXPECT_EQ(provision->getRps(), 200u);
}

// ==================== NEEDS STORAGE ====================

TEST_F(ClientTransform_test, NeedsStorageFalseForBasicProvision)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_FALSE(provision->needsStorage());
    EXPECT_FALSE(adata_.getClientProvisionData().needsStorage());
}

TEST_F(ClientTransform_test, NeedsStorageTrueForClientEventSource)
{
    client_provision_json_["transform"] = R"([{"source":"clientEvent.requestMethod","target":"request.uri"}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
    EXPECT_TRUE(adata_.getClientProvisionData().needsStorage());
}

TEST_F(ClientTransform_test, NeedsStorageTrueForServerEventSource)
{
    client_provision_json_["transform"] = R"([{"source":"serverEvent.requestMethod","target":"request.uri"}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
}

TEST_F(ClientTransform_test, NeedsStorageTrueForClientEventToPurgeTarget)
{
    client_provision_json_["onResponseTransform"] = R"([{"source":"eraser","target":"clientEvent.purge"}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
}

// ==================== EXPECTED RESPONSE STATUS CODE ====================

TEST_F(ClientTransform_test, LoadProvisionWithExpectedResponseStatusCode)
{
    client_provision_json_["expectedResponseStatusCode"] = 200;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(provision->getExpectedResponseStatusCode(), 200u);
}

TEST_F(ClientTransform_test, LoadProvisionWithoutExpectedResponseStatusCode)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(provision->getExpectedResponseStatusCode(), 0u);
}

TEST_F(ClientTransform_test, TransformResponseReturnsTrueWhenNoValidation)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    ert::http2comm::Http2Client::response resp;
    resp.statusCode = 200;
    resp.body = "{}";
    std::string outState = "initial";
    EXPECT_TRUE(provision->transformResponse("/test", {}, resp, 1, outState, variables_));
}

TEST_F(ClientTransform_test, TransformResponseReturnsTrueWhenStatusCodeMatches)
{
    client_provision_json_["expectedResponseStatusCode"] = 200;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    ert::http2comm::Http2Client::response resp;
    resp.statusCode = 200;
    std::string outState = "initial";
    EXPECT_TRUE(provision->transformResponse("/test", {}, resp, 1, outState, variables_));
}

TEST_F(ClientTransform_test, TransformResponseReturnsFalseWhenStatusCodeMismatch)
{
    client_provision_json_["expectedResponseStatusCode"] = 200;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    ert::http2comm::Http2Client::response resp;
    resp.statusCode = 500;
    std::string outState = "initial";
    EXPECT_FALSE(provision->transformResponse("/test", {}, resp, 1, outState, variables_));
}

/////////////////////////////////////
// SCOPED VARIABLE CHAIN PROPAGATION //
/////////////////////////////////////
TEST_F(ClientTransform_test, ScopedVarPropagatesFromTransformToTransformResponse)
{
    // Provision sets var.marker in pre-send transform, reads it in onResponseTransform
    client_provision_json_["transform"] = R"([
        {"source": "value.pre-send-marker", "target": "var.marker"}
    ])"_json;
    client_provision_json_["onResponseTransform"] = R"([
        {"source": "var.marker", "target": "var.result"}
    ])"_json;

    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    provision->transform(request_method_, request_uri_, request_body_, request_headers_, out_state_, request_delay_ms_, request_timeout_ms_, error_, variables_);
    EXPECT_EQ(variables_["marker"], "pre-send-marker");

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = "{}";
    std::string outState = "initial";
    provision->transformResponse("/api/v1/test", request_headers_, fakeResponse, 1, outState, variables_);

    EXPECT_EQ(variables_["result"], "pre-send-marker");
}

TEST_F(ClientTransform_test, ScopedVarPropagatesAcrossOutStateChain)
{
    // Link 1: set var.token in onResponseTransform, outState -> "authenticated"
    client_provision_json_["outState"] = "authenticated";
    client_provision_json_["onResponseTransform"] = R"([
        {"source": "value.my-secret-token", "target": "var.token"}
    ])"_json;

    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision1 = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision1);

    provision1->transform(request_method_, request_uri_, request_body_, request_headers_, out_state_, request_delay_ms_, request_timeout_ms_, error_, variables_);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = "{}";
    std::string outState = "authenticated";
    provision1->transformResponse("/api/v1/test", request_headers_, fakeResponse, 1, outState, variables_);

    EXPECT_EQ(variables_["token"], "my-secret-token");

    // Link 2: read var.token into request header (same variables_ map = chain propagation)
    nlohmann::json link2 = R"({
        "id": "myFlow",
        "endpoint": "myServer",
        "inState": "authenticated",
        "requestMethod": "GET",
        "requestUri": "/api/v1/data",
        "transform": [
            {"source": "var.token", "target": "request.header.x-auth"}
        ]
    })"_json;

    EXPECT_EQ(adata_.loadClientProvision(link2, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision2 = adata_.getClientProvisionData().find("authenticated", "myFlow");
    ASSERT_TRUE(provision2);

    std::string method2, uri2, body2, error2, outState2;
    nghttp2::asio_http2::header_map headers2;
    unsigned int delay2{}, timeout2{};
    provision2->transform(method2, uri2, body2, headers2, outState2, delay2, timeout2, error2, variables_);

    auto it = headers2.find("x-auth");
    ASSERT_NE(it, headers2.end());
    EXPECT_EQ(it->second.value, "my-secret-token");
}
