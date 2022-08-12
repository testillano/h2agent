#include <nlohmann/json.hpp>

#include <ert/http2comm/Http2Connection.hpp>
#include <ert/http2comm/Http2Client.hpp>

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>

#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <MockServerEventsData.hpp>

//#include <ert/tracing/Logger.hpp>
#include <ert/metrics/Metrics.hpp>

#include <thread>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


const nlohmann::json ProvisionExample = R"(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar?name=test",
  "responseCode": 200,
  "responseBody": {
    "foo": "bar"
  },
  "responseHeaders": {
    "content-type": "application/json",
    "x-version": "1.0.0"
  },
  "transform": [
    {
      "source": "value.hello",
      "target": "response.body.json.object./text"
    }
  ]
}
)"_json;

const nlohmann::json MatchingExample = R"(
{
  "algorithm":"FullMatching",
  "uriPathQueryParameters": {
    "filter":"Ignore"
  }
}
)"_json;

const nlohmann::json GlobalVariableExample = R"(
{
  "var1":"value1",
  "var1":"value1",
  "var1":"value1"
}
)"_json;

const nlohmann::json SchemaExample = R"(
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


class http2Server_test : public ::testing::Test
{
    std::thread tt_;

public:
    h2agent::http2::MyAdminHttp2Server* admin_http2_server_{};
    h2agent::http2::MyTrafficHttp2Server* traffic_http2_server_{};
    boost::asio::io_service *timers_io_service_{};
    h2agent::model::Configuration* configuration_{};
    h2agent::model::GlobalVariable* global_variable_{};
    h2agent::model::FileManager* file_manager_{};
    h2agent::model::MockServerEventsData* mock_server_events_data_{};
    ert::metrics::Metrics* metrics_{};

    std::shared_ptr<ert::http2comm::Http2Connection> traffic_client_connection_{}, admin_client_connection_{};
    std::shared_ptr<ert::http2comm::Http2Client> traffic_client_{}, admin_client_{};

    http2Server_test() {
        timers_io_service_ = new boost::asio::io_service();
        configuration_ = new h2agent::model::Configuration();
        global_variable_ = new h2agent::model::GlobalVariable();
        file_manager_ = new h2agent::model::FileManager(timers_io_service_);
        metrics_ = new ert::metrics::Metrics();
        file_manager_->enableMetrics(metrics_);
        mock_server_events_data_ = new h2agent::model::MockServerEventsData();

        admin_http2_server_ = new h2agent::http2::MyAdminHttp2Server(1);
        admin_http2_server_->setApiName("admin");
        admin_http2_server_->setApiVersion("v1");
        admin_http2_server_->setConfiguration(configuration_);
        admin_http2_server_->setGlobalVariable(global_variable_);
        admin_http2_server_->setFileManager(file_manager_);
        admin_http2_server_->setMockServerEventsData(mock_server_events_data_); // stored at administrative class to pass through created server provisions
        admin_http2_server_->enableMetrics(metrics_);

        traffic_http2_server_ = new h2agent::http2::MyTrafficHttp2Server(1, timers_io_service_);
        traffic_http2_server_->setApiName("app");
        traffic_http2_server_->setApiVersion("v1");
        traffic_http2_server_->setMockServerEventsData(mock_server_events_data_);
        traffic_http2_server_->setAdminData(admin_http2_server_->getAdminData()); // to retrieve mock behaviour configuration
        traffic_http2_server_->enableMetrics(metrics_);

        admin_http2_server_->setHttp2Server(traffic_http2_server_);

        // Timers thread:
        tt_ = std::thread([&] {
            boost::asio::io_service::work work(*timers_io_service_);
            timers_io_service_->run();
        });

        // Start prometheus server:
        metrics_->serve("127.0.0.1:880");

        // Start server asynchronously:
        admin_http2_server_->serve("127.0.0.1", "8074", "", "", 1, true /* asynchronous */);
        admin_client_connection_ = std::make_shared<ert::http2comm::Http2Connection>("127.0.0.1", "8074", false /* secure */);
        admin_client_ = std::make_shared<ert::http2comm::Http2Client>(admin_client_connection_);
        admin_client_connection_->waitToBeConnected();

        traffic_http2_server_->serve("127.0.0.1", "8000", "", "", 1, true /* asynchronous */);
        traffic_client_connection_ = std::make_shared<ert::http2comm::Http2Connection>("127.0.0.1", "8000", false /* secure */);
        traffic_client_ = std::make_shared<ert::http2comm::Http2Client>(traffic_client_connection_);
        traffic_client_connection_->waitToBeConnected();
    }

    ~http2Server_test() {
        delete(configuration_);
        delete(global_variable_);
        delete(file_manager_);
        delete(mock_server_events_data_);
        admin_http2_server_->stop();
        traffic_http2_server_->stop();
        delete(timers_io_service_);
    }

    void tearDown() {
        timers_io_service_->stop();
        tt_.join();
    }
};

TEST_F(http2Server_test, CheckMatching)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/server-matching", "", headers);

    EXPECT_EQ(response.body, "{\"algorithm\":\"FullMatching\"}");
    EXPECT_EQ(response.statusCode, 200);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Configure:
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string matchingBody = MatchingExample.dump();
    response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/server-matching", matchingBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/server-matching", "", headers);
    EXPECT_EQ(response.body, matchingBody);
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, CheckProvision)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/server-provision", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Configure:
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionExample.dump();
    response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and provision data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/server-provision", "", headers);
    EXPECT_EQ(response.body, std::string("[") + provisionBody + std::string("]"));
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, CheckGlobalVariable)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/global-variable", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Configure:
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string globalVariableBody = GlobalVariableExample.dump();
    response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/global-variable", globalVariableBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"global-variable operation; valid schema and global variables received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/global-variable", "", headers);
    EXPECT_EQ(response.body, globalVariableBody);
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, CheckSchema)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/schema", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Configure:
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string schemaBody = SchemaExample.dump();
    response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/schema", schemaBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"schema operation; valid schema and schema data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send(ert::http2comm::Http2Client::Method::GET, "/admin/v1/schema", "", headers);
    EXPECT_EQ(response.body, std::string("[") + schemaBody + std::string("]"));
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, ProvisionedEvent)
{
    /////////////////////
    // Admin provision //
    /////////////////////
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionExample.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and provision data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    /////////////
    // Traffic //
    /////////////
    headers.clear();
    response = traffic_client_->send(ert::http2comm::Http2Client::Method::GET, "/app/v1/foo/bar?name=test", "", headers);

    EXPECT_EQ(response.body, "{\"foo\":\"bar\",\"text\":\"hello\"}");
    EXPECT_EQ(response.statusCode, 200);
    // Check headers
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");
    it = response.headers.find("x-version");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "1.0.0");

    tearDown();
}

TEST_F(http2Server_test, UnprovisionedEvent)
{
    nghttp2::asio_http2::header_map headers;
    //headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    ert::http2comm::Http2Client::response response = traffic_client_->send(ert::http2comm::Http2Client::Method::POST, "/app/v1/non/provisioned/uri", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 501);

    tearDown();
}

TEST_F(http2Server_test, InvalidContentType)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"text/html"});
    std::string provisionBody = ProvisionExample.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{\"cause\":\"UNSUPPORTED_MEDIA_TYPE\"}");
    EXPECT_EQ(response.statusCode, 415);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/problem+json");

    tearDown();
}

TEST_F(http2Server_test, MissingOperation)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    ert::http2comm::Http2Client::response response = admin_client_->send(ert::http2comm::Http2Client::Method::POST, "/admin/v1", "whatever", headers);

    EXPECT_EQ(response.body, "{ \"result\":\"false\", \"response\": \"no operation provided\" }");
    EXPECT_EQ(response.statusCode, 400);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    tearDown();
}

TEST_F(http2Server_test, StorageIndicators)
{
    // All enabled by default:
    EXPECT_EQ(traffic_http2_server_->dataConfigurationAsJsonString(), "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");

    // Storage disabled (general implies history disable):
    traffic_http2_server_->discardData();
    traffic_http2_server_->discardDataKeyHistory();
    EXPECT_EQ(traffic_http2_server_->dataConfigurationAsJsonString(), "{\"purgeExecution\":true,\"storeEvents\":false,\"storeEventsKeyHistory\":false}");

    // Also disable purge:
    traffic_http2_server_->disablePurge();
    EXPECT_EQ(traffic_http2_server_->dataConfigurationAsJsonString(), "{\"purgeExecution\":false,\"storeEvents\":false,\"storeEventsKeyHistory\":false}");

    tearDown();
}

TEST_F(http2Server_test, ServerMatchingOK)
{
    nlohmann::json configuration = R"({"algorithm":"FullMatching"})"_json;
std::string log;

bool result = admin_http2_server_->serverMatching(configuration, log);

EXPECT_TRUE(result);
EXPECT_EQ(log, "server-matching operation; valid schema and matching data received");

tearDown();
}

TEST_F(http2Server_test, ServerMatchingBadSchema)
{
    nlohmann::json configuration = R"({"foo":"bar"})"_json;
    std::string log;

    bool result = admin_http2_server_->serverMatching(configuration, log);

    EXPECT_FALSE(result);
    EXPECT_EQ(log, "server-matching operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, ServerProvision)
{
    nlohmann::json configuration = R"({"requestMethod": "GET", "requestUri": "/foo/bar", "responseCode": 200})"_json;
    std::string log;

    bool result = admin_http2_server_->serverProvision(configuration, log);

    EXPECT_TRUE(result);
    EXPECT_EQ(log, "server-provision operation; valid schema and provision data received");

    tearDown();
}

TEST_F(http2Server_test, ServerProvisionBadSchema)
{
    nlohmann::json configuration = R"({"requestUri": "/foo/bar", "responseCode": 200})"_json;
    std::string log;

    bool result = admin_http2_server_->serverProvision(configuration, log);

    EXPECT_FALSE(result);
    EXPECT_EQ(log, "server-provision operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariable)
{
    nlohmann::json configuration = R"({"var1": "value1", "var2": "value2"})"_json;
    std::string log;

    bool result = admin_http2_server_->globalVariable(configuration, log);

    EXPECT_TRUE(result);
    EXPECT_EQ(log, "global-variable operation; valid schema and global variables received");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariableNOK1)
{
    nlohmann::json configuration = R"({"var1": 1})"_json;
    std::string log;

    bool result = admin_http2_server_->globalVariable(configuration, log);

    EXPECT_FALSE(result);
    EXPECT_EQ(log, "global-variable operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariableNOK2)
{
    nlohmann::json configuration = R"({"var1": {"foo":"bar"}})"_json;
    std::string log;

    bool result = admin_http2_server_->globalVariable(configuration, log);

    EXPECT_FALSE(result);
    EXPECT_EQ(log, "global-variable operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, Schema)
{
    nlohmann::json configuration = R"({"id": "myRequestsSchema", "schema": {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":false,"properties":{"foo":{"type":"string"}},"required":["foo"]}})"_json;
    std::string log;

    bool result = admin_http2_server_->schema(configuration, log);

    EXPECT_TRUE(result);
    EXPECT_EQ(log, "schema operation; valid schema and schema data received");

    tearDown();
}

TEST_F(http2Server_test, SchemaNOK)
{
    nlohmann::json configuration = R"({"id": "myRequestsSchema"})"_json;
    std::string log;

    bool result = admin_http2_server_->schema(configuration, log);

    EXPECT_FALSE(result);
    EXPECT_EQ(log, "schema operation; invalid schema");

    tearDown();
}
