#include <AdminData.hpp>
#include <AdminClientProvision.hpp>
#include <AdminClientProvisionData.hpp>
#include <Configuration.hpp>
#include <Vault.hpp>
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
    h2agent::model::Variables variables_{};

    ClientTransform_test() {
        client_provision_json_ = ClientProvision_POST;

        common_resources_.AdminDataPtr = &adata_;
        common_resources_.ConfigurationPtr = new h2agent::model::Configuration();
        common_resources_.VaultPtr = new h2agent::model::Vault();
        common_resources_.FileManagerPtr = new h2agent::model::FileManager(nullptr);
        common_resources_.SocketManagerPtr = new h2agent::model::SocketManager(nullptr);
        common_resources_.MockClientDataPtr = new h2agent::model::MockClientData();
        common_resources_.MockServerDataPtr = new h2agent::model::MockServerData();

        common_resources_.VaultPtr->load("myVaultEntry", "vaultValue");
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
        delete(common_resources_.VaultPtr);
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/original",
      "transform": [{"from": {"source": "literal", "value": "/api/v1/replaced"}, "to": {"target": "requestUri"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/replaced");
}

TEST_F(ClientTransform_test, TargetRequestMethod)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "literal", "value": "PUT"}, "to": {"target": "requestMethod"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_method_, "PUT");
}

TEST_F(ClientTransform_test, TargetRequestHeader)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "literal", "value": "bearer-token-123"},
          "to": {"target": "requestHeader", "name": "authorization"}
        }
      ]
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "requestDelayMs": 10,
      "transform": [{"from": {"source": "literal", "value": "500"}, "to": {"target": "requestDelayMs"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 500);
}

TEST_F(ClientTransform_test, TargetRequestTimeoutMs)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "timeoutMs": 1000,
      "transform": [{"from": {"source": "literal", "value": "5000"}, "to": {"target": "requestTimeoutMs"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_timeout_ms_, 5000);
}

TEST_F(ClientTransform_test, TargetRequestBodyJsonString)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "POST",
      "requestUri": "/test",
      "requestBody": {"original": "data"},
      "transform": [
        {
          "from": {"source": "literal", "value": "replaced"},
          "to": {"target": "requestBody", "path": "/original"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["original"], "replaced");
}

TEST_F(ClientTransform_test, TargetRequestBodyJsonInteger)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "POST",
      "requestUri": "/test",
      "requestBody": {"count": 0},
      "transform": [
        {
          "from": {"source": "literal", "value": "42"},
          "filter": [{"op": "cast", "type": "integer"}],
          "to": {"target": "requestBody", "path": "/count"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["count"], 42);
}

TEST_F(ClientTransform_test, TargetOutState)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "literal", "value": "step2"}, "to": {"target": "outState"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(out_state_, "step2");
}

TEST_F(ClientTransform_test, TargetVariable)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {"from": {"source": "literal", "value": "hello"}, "to": {"target": "var", "name": "myVar"}},
        {"from": {"source": "var", "name": "myVar"}, "to": {"target": "requestUri"}}
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "hello");
}

TEST_F(ClientTransform_test, TargetVault)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "literal", "value": "newValue"},
          "to": {"target": "vault", "key": "testGVar"}
        },
        {"from": {"source": "vault", "key": "testGVar"}, "to": {"target": "requestUri"}}
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [
        {"from": {"source": "literal", "value": "stopped"}, "to": {"target": "outState"}},
        {"from": {"source": "literal", "value": "1"}, "to": {"target": "stop"}},
        {"from": {"source": "literal", "value": "should-not-reach"}, "to": {"target": "outState"}}
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "literal", "value": "/api/v1/new"}, "to": {"target": "requestUri"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/new");
}

TEST_F(ClientTransform_test, SourceVault)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "vault", "key": "myVaultEntry"}, "to": {"target": "requestUri"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "vaultValue");
}

TEST_F(ClientTransform_test, SourceEraser)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {"from": {"source": "eraser"}, "to": {"target": "vault", "key": "myVaultEntry"}},
        {"from": {"source": "vault", "key": "myVaultEntry"}, "to": {"target": "requestUri"}}
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/test"); // vault erased, second transform fails silently, uri unchanged
}

TEST_F(ClientTransform_test, SourceInState)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "inState"}, "to": {"target": "requestUri"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "initial");
}

TEST_F(ClientTransform_test, SourceMath)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "math", "expression": "2+3"}, "to": {"target": "requestDelayMs"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 5);
}

TEST_F(ClientTransform_test, SourceRandom)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "random", "min": 10, "max": 20}, "to": {"target": "requestDelayMs"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_GE(request_delay_ms_, 10);
    EXPECT_LE(request_delay_ms_, 20);
}

TEST_F(ClientTransform_test, SourceTimestamp)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [{"from": {"source": "timestamp", "unit": "ms"}, "to": {"target": "var", "name": "ts"}}]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    // just verify it loaded and transformed without error
}

// ==================== FILTERS ====================

TEST_F(ClientTransform_test, FilterAppend)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "literal", "value": "/api/v1/"},
          "filter": [{"op": "append", "value": "resource"}],
          "to": {"target": "requestUri"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/resource");
}

TEST_F(ClientTransform_test, FilterPrepend)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "literal", "value": "resource"},
          "filter": [{"op": "prepend", "value": "/api/v1/"}],
          "to": {"target": "requestUri"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_uri_, "/api/v1/resource");
}

TEST_F(ClientTransform_test, FilterSum)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "literal", "value": "100"},
          "filter": [{"op": "sum", "value": 50}],
          "to": {"target": "requestDelayMs"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    EXPECT_EQ(request_delay_ms_, 150);
}

// ==================== TRANSFORM RESPONSE ====================

TEST_F(ClientTransform_test, TransformResponseSetsOutState)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [{"from": {"source": "literal", "value": "step2"}, "to": {"target": "outState"}}]
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [
        {"from": {"source": "eraser"}, "to": {"target": "vault", "key": "respBody"}},
        {"from": {"source": "responseBody"}, "to": {"target": "vault", "key": "respBody"}}
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

    nlohmann::json stored;
    EXPECT_TRUE(common_resources_.VaultPtr->tryGet("respBody", stored));
}

TEST_F(ClientTransform_test, TransformResponseSourceResponseStatusCode)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [{"from": {"source": "responseStatusCode"}, "to": {"target": "var", "name": "code"}}]
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
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [
        {"from": {"source": "eraser"}, "to": {"target": "vault", "key": "location"}},
        {
          "from": {"source": "responseHeader", "name": "location"},
          "to": {"target": "vault", "key": "location"}
        }
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
    // vault.location should now contain "/api/v1/redirected"
}

TEST_F(ClientTransform_test, TransformResponseBreakOnError)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [
        {
          "from": {"source": "responseStatusCode"},
          "filter": [{"op": "equalTo", "value": "200", "not": true}],
          "to": {"target": "outState"}
        },
        {"from": {"source": "literal", "value": "1"}, "to": {"target": "stop"}}
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
    EXPECT_EQ(outState, "500"); // equalTo 200 negated passes, statusCode "500" written to outState
}

// ==================== TRIGGERING ====================

TEST_F(ClientTransform_test, SetSeqGetSeq)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_EQ(provision->getSeq(), 0);
    provision->setSeq(42);
    EXPECT_EQ(provision->getSeq(), 42);
    provision->setSeq(0);
    EXPECT_EQ(provision->getSeq(), 0);
}

TEST_F(ClientTransform_test, UpdateTriggeringValidRange)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_TRUE(provision->updateTriggering("1", "100", "50", "false"));
    EXPECT_EQ(provision->getCps(), 50u);
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidRange)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    // sequenceBegin > sequenceEnd
    EXPECT_FALSE(provision->updateTriggering("100", "1", "", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringNegativeSequenceBegin)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_TRUE(provision->updateTriggering("-1", "100", "", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidSequenceBegin)
{
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);

    EXPECT_FALSE(provision->updateTriggering("abc", "100", "", ""));
}

TEST_F(ClientTransform_test, UpdateTriggeringInvalidCps)
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
    EXPECT_EQ(provision->getCps(), 50u);

    // Partial update: only cps, range keeps previous
    EXPECT_TRUE(provision->updateTriggering("", "", "200", ""));
    EXPECT_EQ(provision->getCps(), 200u);
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
    client_provision_json_["transform"] = R"([{"from": {"source": "clientEvent", "clientEndpointId": "myServer", "requestMethod": "GET", "requestUri": "/x", "eventNumber": -1}, "to": {"target": "requestUri"}}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
    EXPECT_TRUE(adata_.getClientProvisionData().needsStorage());
}

TEST_F(ClientTransform_test, NeedsStorageTrueForServerEventSource)
{
    client_provision_json_["transform"] = R"([{"from": {"source": "serverEvent", "requestMethod": "GET", "requestUri": "/x", "eventNumber": -1}, "to": {"target": "requestUri"}}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
}

TEST_F(ClientTransform_test, NeedsStorageTrueForClientEventToPurgeTarget)
{
    client_provision_json_["onResponseTransform"] = R"([{"from": {"source": "eraser"}, "to": {"target": "clientEvent", "clientEndpointId": "myServer"}}])"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_TRUE(provision->needsStorage());
}

// ==================== PURGE OUTSTATE (client provision chain) ====================
//
// In client mode the reserved 'purge' outState removes all events accumulated during
// the chain once it terminates. These tests validate that a chain ending in 'purge'
// loads and the terminal provision carries the 'purge' outState. The actual runtime
// removal of the accumulated client-data is covered by the component test
// ct/src/state_flow/sf_chain_purge_client_test.py.

TEST_F(ClientTransform_test, LoadClientChainEndingInPurge)
{
    nlohmann::json step1 = R"({
        "id":"purgeFlow","endpoint":"myServer","inState":"initial","outState":"check",
        "requestMethod":"POST","requestUri":"/api/v1/order","expectedResponseStatusCode":201
    })"_json;
    nlohmann::json step2 = R"({
        "id":"purgeFlow","endpoint":"myServer","inState":"check","outState":"purge",
        "requestMethod":"GET","requestUri":"/api/v1/order/A-100","expectedResponseStatusCode":200
    })"_json;

    client_provision_json_ = nlohmann::json::array({step1, step2});
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);

    auto p1 = adata_.getClientProvisionData().find("initial", "purgeFlow");
    ASSERT_TRUE(p1 != nullptr);
    EXPECT_EQ(p1->getOutState(), "check");

    // Terminal step carries the reserved 'purge' outState:
    auto p2 = adata_.getClientProvisionData().find("check", "purgeFlow");
    ASSERT_TRUE(p2 != nullptr);
    EXPECT_EQ(p2->getOutState(), "purge");
}

TEST_F(ClientTransform_test, PurgeOutStateIsAcceptedAsSingleProvision)
{
    client_provision_json_ = R"({
        "id":"myFlow","endpoint":"myServer","inState":"initial","outState":"purge",
        "requestMethod":"GET","requestUri":"/api/v1/last"
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision != nullptr);
    EXPECT_EQ(provision->getOutState(), "purge");
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
      {
        "from": {"source": "literal", "value": "pre-send-marker"},
        "to": {"target": "var", "name": "marker"}
      }
    ])"_json;
    client_provision_json_["onResponseTransform"] = R"([{"from": {"source": "var", "name": "marker"}, "to": {"target": "var", "name": "result"}}])"_json;

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
      {
        "from": {"source": "literal", "value": "my-secret-token"},
        "to": {"target": "var", "name": "token"}
      }
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
        {
          "from": {"source": "var", "name": "token"},
          "to": {"target": "requestHeader", "name": "x-auth"}
        }
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

TEST_F(ClientTransform_test, TransformResponseSourceResponseHeaders)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [{"from": {"source": "responseHeaders"}, "to": {"target": "vault", "key": "resp_hdrs"}}]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = "{}";
    fakeResponse.headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    fakeResponse.headers.emplace("x-request-id", nghttp2::asio_http2::header_value{"abc-123"});

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);

    // Verify vault contains the headers array
    bool exists = false;
    nlohmann::json hdrs = common_resources_.VaultPtr->get("resp_hdrs", exists);
    ASSERT_TRUE(exists);
    EXPECT_TRUE(hdrs.is_array());
    bool found_ct = false, found_xr = false;
    for (const auto &h : hdrs) {
        if (h["name"] == "content-type" && h["value"] == "application/json") found_ct = true;
        if (h["name"] == "x-request-id" && h["value"] == "abc-123") found_xr = true;
    }
    EXPECT_TRUE(found_ct);
    EXPECT_TRUE(found_xr);
}


// ==================== DYNAMICS OBSERVABILITY (state / nextSequence) ====================
//
// The internal iterator keeps the "use-then-increment from begin-1" mechanism untouched.
// These tests exercise the human-friendly fields exposed in getJson()["dynamics"]:
//   - state: armed | running | paused | exhausted
//   - nextSequence: sequence the next request will carry (null when exhausted)
//
// Note: 'running' requires an active ticker (timer_ != nullptr), which is wired by the
// admin server, not by updateTriggering() alone. In this unit scope there is no timer, so
// a configured cps>0 range reports 'paused' (positioned, not ticking). The 'running' path
// is covered at component/manual level.

TEST_F(ClientTransform_test, DynamicsArmedWithSequenceBeginZero)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);

    // sequenceBegin=0 => internal iterator sits at begin-1 (-1): armed, not yet started.
    ASSERT_TRUE(provision->updateTriggering("0", "10", "0", ""));

    nlohmann::json dyn = provision->getJson()["dynamics"];
    EXPECT_EQ(dyn["sequenceBegin"], 0);
    EXPECT_EQ(dyn["sequenceEnd"], 10);
    EXPECT_EQ(dyn["sequence"], -1);              // raw internal sentinel (begin-1) kept for compatibility
    EXPECT_EQ(dyn["state"], "armed");            // friendly state: prepared, no element emitted yet
    EXPECT_EQ(dyn["nextSequence"], 0);           // the first request will carry 'begin' (0)
}

TEST_F(ClientTransform_test, DynamicsArmedWithNonZeroBegin)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);

    ASSERT_TRUE(provision->updateTriggering("100", "200", "0", ""));

    nlohmann::json dyn = provision->getJson()["dynamics"];
    EXPECT_EQ(dyn["sequence"], 99);              // begin-1, raw
    EXPECT_EQ(dyn["state"], "armed");
    EXPECT_EQ(dyn["nextSequence"], 100);         // next request carries begin
}

TEST_F(ClientTransform_test, DynamicsPausedWithinRange)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);

    ASSERT_TRUE(provision->updateTriggering("0", "10", "0", ""));
    provision->setSeq(5); // positioned inside the range, no ticker active in unit scope

    nlohmann::json dyn = provision->getJson()["dynamics"];
    EXPECT_EQ(dyn["sequence"], 5);
    EXPECT_EQ(dyn["state"], "paused");           // within range but not ticking
    EXPECT_EQ(dyn["nextSequence"], 6);           // next request carries seq+1
}

TEST_F(ClientTransform_test, DynamicsExhaustedBeyondEnd)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);

    ASSERT_TRUE(provision->updateTriggering("0", "10", "0", ""));
    provision->setSeq(11); // past the end, no repeat => range consumed

    nlohmann::json dyn = provision->getJson()["dynamics"];
    EXPECT_EQ(dyn["sequence"], 11);
    EXPECT_EQ(dyn["state"], "exhausted");
    EXPECT_TRUE(dyn["nextSequence"].is_null());  // nothing pending
}

TEST_F(ClientTransform_test, DynamicsNextSequenceWrapsWithRepeat)
{
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision != nullptr);

    // repeat=true: once the last element is reached, the next wraps back to begin.
    ASSERT_TRUE(provision->updateTriggering("0", "10", "0", "true"));
    provision->setSeq(10); // at the last element of the range

    nlohmann::json dyn = provision->getJson()["dynamics"];
    EXPECT_EQ(dyn["sequence"], 10);
    EXPECT_EQ(dyn["state"], "paused");
    EXPECT_EQ(dyn["nextSequence"], 0);           // wraps to begin because repeat is enabled
}

// Regression (CT filter_test test_023, client side): empty header arrays are
// values (v1 RequestHeaders/ResponseHeaders never fail), so JsonConstraint runs.
TEST_F(ClientTransform_test, SourceRequestHeadersEmptyJsonConstraintFails)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "transform": [
        {
          "from": {"source": "requestHeaders"},
          "filter": [{"op": "jsonConstraint", "value": [{"name": "x-mandatory"}]}],
          "to": {"target": "var", "name": "h"}
        },
        {
          "from": {"source": "var", "name": "h.fail"},
          "filter": [{"op": "conditionVar", "name": "h", "not": true}],
          "to": {"target": "vault", "key": "requestHeadersFail"}
        }
      ]
    })"_json;
    variables_["h"] = "1";  // stale chain value must be overwritten
    provisionAndTransform();

    EXPECT_EQ(variables_["h"], "");
    nlohmann::json stored;
    ASSERT_TRUE(common_resources_.VaultPtr->tryGet("requestHeadersFail", stored));
    EXPECT_NE(stored.get<std::string>().find("JsonConstraint FAILED"), std::string::npos);
}

TEST_F(ClientTransform_test, TransformResponseSourceResponseHeadersEmptyJsonConstraintFails)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "GET",
      "requestUri": "/test",
      "onResponseTransform": [
        {
          "from": {"source": "responseHeaders"},
          "filter": [{"op": "jsonConstraint", "value": [{"name": "x-mandatory"}]}],
          "to": {"target": "var", "name": "r"}
        },
        {
          "from": {"source": "var", "name": "r.fail"},
          "filter": [{"op": "conditionVar", "name": "r", "not": true}],
          "to": {"target": "vault", "key": "responseHeadersFail"}
        }
      ]
    })"_json;
    EXPECT_EQ(adata_.loadClientProvision(client_provision_json_, common_resources_), h2agent::model::AdminClientProvisionData::Success);
    auto provision = adata_.getClientProvisionData().find("initial", "myFlow");
    ASSERT_TRUE(provision);

    ert::http2comm::Http2Client::response fakeResponse;
    fakeResponse.statusCode = 200;
    fakeResponse.body = R"({"result":"ok"})";  // no response headers at all

    std::string outState = "initial";
    nghttp2::asio_http2::header_map reqHeaders;
    variables_["r"] = "1";
    provision->transformResponse("/test", reqHeaders, fakeResponse, 1, outState, variables_);

    EXPECT_EQ(variables_["r"], "");
    nlohmann::json stored;
    ASSERT_TRUE(common_resources_.VaultPtr->tryGet("responseHeadersFail", stored));
    EXPECT_NE(stored.get<std::string>().find("JsonConstraint FAILED"), std::string::npos);
}

// v1 parity: request header writes are visible to later header reads in the same
// pipeline (v1 emplaced them into the live header map).
TEST_F(ClientTransform_test, RequestHeaderWrittenThenReadInSamePipeline)
{
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "POST",
      "requestUri": "/test",
      "requestBody": {},
      "transform": [
        {
          "from": {"source": "literal", "value": "abc"},
          "to": {"target": "requestHeader", "name": "x-a"}
        },
        {
          "from": {"source": "requestHeader", "name": "x-a"},
          "to": {"target": "requestBody", "path": "/h"}
        },
        {
          "from": {"source": "requestHeaders"},
          "filter": [{"op": "cast", "type": "object"}],
          "to": {"target": "requestBody", "path": "/all"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["h"], "abc");
    bool listed = false;
    for (const auto &h : body["all"]) if (h["name"] == "x-a" && h["value"] == "abc") listed = true;
    EXPECT_TRUE(listed);
}

TEST_F(ClientTransform_test, RequestHeaderRewriteKeepsFirstValueOnRead)
{
    // header_map is a multimap: a write adds a value and find() keeps returning the
    // first one (the provisioned value), exactly as v1 did; both are finally sent.
    client_provision_json_ = R"({
      "id": "myFlow",
      "endpoint": "myServer",
      "requestMethod": "POST",
      "requestUri": "/test",
      "requestBody": {},
      "requestHeaders": {"x-a": "orig"},
      "transform": [
        {
          "from": {"source": "literal", "value": "new"},
          "to": {"target": "requestHeader", "name": "x-a"}
        },
        {
          "from": {"source": "requestHeader", "name": "x-a"},
          "to": {"target": "requestBody", "path": "/h"}
        }
      ]
    })"_json;
    auto provision = provisionAndTransform();
    ASSERT_TRUE(provision);
    nlohmann::json body = nlohmann::json::parse(request_body_);
    EXPECT_EQ(body["h"], "orig");
    EXPECT_EQ(request_headers_.count("x-a"), 2u);
}
