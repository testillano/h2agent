#include <nlohmann/json.hpp>

#include <ert/http2comm/Http2Client.hpp>

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>
#include <AdminClientEndpoint.hpp>
#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>

#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>

#include <ert/http2comm/Http.hpp>
#include <ert/metrics/Metrics.hpp>

#include <thread>
#include <vector>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


const nlohmann::json ProvisionExample = R"(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar?name=test",
  "responseCode": 200,
  "requestSchemaId": "myRequestSchemaId",
  "responseSchemaId": "myResponseSchemaId",
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
    },
    {
      "source": "value.foreignState",
      "target": "outState.POST./foo/bar"
    }
  ]
}
)"_json;

const nlohmann::json ProvisionExample2 = R"(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar?name=test",
  "responseCode": 200,
  "responseBody": {
    "foo": "bar"
  },
  "outState": "purge",
  "responseHeaders": {
    "content-type": "application/json",
    "x-version": "1.0.0"
  }
}
)"_json;

const nlohmann::json ProvisionExample3 = R"({"requestMethod": "GET", "requestUri": "/foo/bar", "responseCode": 200})"_json;

const nlohmann::json ProvisionBadSchema = R"(
{
  "foo": 1,
  "bar": "2"
}
)"_json;

const nlohmann::json FooBar = R"({"foo":"bar"})"_json;

const nlohmann::json FullMatching = R"({"algorithm":"FullMatching"})"_json;

const nlohmann::json MatchingIgnore = R"(
{
  "algorithm":"FullMatching",
  "uriPathQueryParameters": {
    "filter":"Ignore"
  }
}
)"_json;

const nlohmann::json MatchingPassBy = R"(
{
  "algorithm":"FullMatching",
  "uriPathQueryParameters": {
    "filter":"PassBy"
  }
}
)"_json;

const nlohmann::json MatchingRegexReplace = R"delim(
{
  "algorithm":"FullMatchingRegexReplace",
  "rgx":"(/app/v1/foo/bar/)([0-9]*)",
                                      "fmt":"$1"
}
)delim"_json;

const nlohmann::json MatchingRegexMatching = R"(
{
  "algorithm": "RegexMatching"
}
)"_json;

const nlohmann::json MatchingBadContent = R"({"algorithm":"FullMatching","rgx":"whatever"})"_json;

const nlohmann::json GeneralConfiguration = R"(
{
  "lazyClientConnection": false,
  "longTermFilesCloseDelayUsecs": 1000000,
  "shortTermFilesCloseDelayUsecs": 0
}
)"_json;

const nlohmann::json GlobalVariableExample = R"(
{
  "var1":"value1",
  "var1":"value1",
  "var1":"value1"
}
)"_json;

const nlohmann::json GlobalVariableExample2 = R"({"var1": "value1", "var2": "value2"})"_json;

const nlohmann::json GlobalVariableNOK1 = R"({"var1": 1})"_json;
const nlohmann::json GlobalVariableNOK2 = R"({"var1": {"foo":"bar"}})"_json;

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

const nlohmann::json SchemaExample2 = R"({"id": "myRequestsSchema", "schema": {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":false,"properties":{"foo":{"type":"string"}},"required":["foo"]}})"_json;

const nlohmann::json ClientEndpointExample = R"({"id": "myServer", "host": "localhost", "port": 8000, "secure": true, "permit": false})"_json;
const nlohmann::json ClientEndpointExampleNOK = R"({"i_d": "myServer", "host": "localhost", "port": 8000, "secure": true, "permit": false})"_json;
const nlohmann::json ClientEndpointExampleNOK2 = R"({"id": "", "host": "localhost", "port": 8000})"_json;




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
    h2agent::model::SocketManager* socket_manager_{};
    h2agent::model::MockServerData* mock_server_events_data_{};
    h2agent::model::MockClientData* mock_client_events_data_{};
    ert::metrics::Metrics* metrics_{};

    std::shared_ptr<ert::http2comm::Http2Client> traffic_client_{}, admin_client_{};

    http2Server_test() {
        timers_io_service_ = new boost::asio::io_service();
        configuration_ = new h2agent::model::Configuration();
        global_variable_ = new h2agent::model::GlobalVariable();
        file_manager_ = new h2agent::model::FileManager(timers_io_service_);
        socket_manager_ = new h2agent::model::SocketManager(timers_io_service_);
        metrics_ = new ert::metrics::Metrics();
        file_manager_->enableMetrics(metrics_);
        socket_manager_->enableMetrics(metrics_);
        mock_server_events_data_ = new h2agent::model::MockServerData();
        mock_client_events_data_ = new h2agent::model::MockClientData();

        admin_http2_server_ = new h2agent::http2::MyAdminHttp2Server(1);
        admin_http2_server_->setApiName("admin");
        admin_http2_server_->setApiVersion("v1");
        admin_http2_server_->setConfiguration(configuration_);
        admin_http2_server_->setGlobalVariable(global_variable_);
        admin_http2_server_->setFileManager(file_manager_);
        admin_http2_server_->setSocketManager(socket_manager_);
        admin_http2_server_->setMockServerData(mock_server_events_data_); // stored at administrative class to pass through created server provisions
        admin_http2_server_->setMockClientData(mock_client_events_data_); // stored at administrative class to pass through created client provisions
        admin_http2_server_->enableMetrics(metrics_);

        traffic_http2_server_ = new h2agent::http2::MyTrafficHttp2Server(2, 2, timers_io_service_, -1 /* no congestion control */);
        traffic_http2_server_->setApiName("app");
        traffic_http2_server_->setApiVersion("v1");
        traffic_http2_server_->setMockServerData(mock_server_events_data_);
        traffic_http2_server_->setMockClientData(mock_client_events_data_);
        traffic_http2_server_->setAdminData(admin_http2_server_->getAdminData()); // to retrieve mock behaviour configuration
        traffic_http2_server_->enableMyMetrics(metrics_);

        admin_http2_server_->setHttp2Server(traffic_http2_server_);

        // Timers thread:
        tt_ = std::thread([&] {
            boost::asio::io_service::work work(*timers_io_service_);
            timers_io_service_->run();
        });

        // Start prometheus server:
        EXPECT_TRUE(metrics_->serve("127.0.0.1:8080"));

        // Start server asynchronously:
        EXPECT_EQ(admin_http2_server_->serve("127.0.0.1", "8074", "", "", 1, true /* asynchronous */), EXIT_SUCCESS);
        admin_client_ = std::make_shared<ert::http2comm::Http2Client>("myClient", "127.0.0.1", "8074", false /* secure */);

        EXPECT_EQ(traffic_http2_server_->serve("127.0.0.1", "8000", "", "", 1, true /* asynchronous */), EXIT_SUCCESS);
        traffic_client_ = std::make_shared<ert::http2comm::Http2Client>("myClient", "127.0.0.1", "8000", false /* secure */);
    }

    ~http2Server_test() {
        delete(configuration_);
        delete(global_variable_);
        delete(file_manager_);
        delete(socket_manager_);
        delete(mock_server_events_data_);
        delete(mock_client_events_data_);
        admin_http2_server_->stop();
        traffic_http2_server_->stop();
        delete(timers_io_service_);
        delete(metrics_);
        delete(admin_http2_server_);
        delete(traffic_http2_server_);
    }

    void tearDown() {
        timers_io_service_->stop();
        tt_.join();
    }
};

TEST_F(http2Server_test, CheckMatchingWithDefault)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/server-matching", "", headers);

    EXPECT_EQ(response.body, "{\"algorithm\":\"FullMatching\"}");
    EXPECT_EQ(response.statusCode, 200);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    tearDown();
}

TEST_F(http2Server_test, CheckMatchingConfigurations)
{
    // Post configuration:
    nghttp2::asio_http2::header_map headersPost;
    nghttp2::asio_http2::header_map headersGet{};  // empty
    headersPost.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});

    std::vector<std::string> matchingBodies;
    matchingBodies.push_back(MatchingRegexReplace.dump());
    matchingBodies.push_back(MatchingRegexMatching.dump());
    matchingBodies.push_back(MatchingPassBy.dump());
    matchingBodies.push_back(MatchingIgnore.dump());
    std::string matchingBody;

    for (const auto &matchingBody: matchingBodies) {
        ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-matching", matchingBody, headersPost);
        EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }");
        EXPECT_EQ(response.statusCode, 201);

        // Check configuration:
        response = admin_client_->send("GET", "/admin/v1/server-matching", "", headersGet);
        EXPECT_EQ(response.body, matchingBody);
        EXPECT_EQ(response.statusCode, 200);
        auto rit = response.headers.find("content-type");
        EXPECT_TRUE(rit != response.headers.end());
        EXPECT_EQ(rit->second.value, "application/json");

        // Generate traffic:
        response = traffic_client_->send("GET", "/app/v1/foo/bar?name=test", "", headersGet);
        EXPECT_EQ(response.body, "");
        EXPECT_EQ(response.statusCode, 501);
    }

    // Case for bad content:
    matchingBody = MatchingBadContent.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-matching", matchingBody, headersPost);
    EXPECT_EQ(response.body, "{ \"result\":\"false\", \"response\": \"server-matching operation; invalid matching data received\" }");
    EXPECT_EQ(response.statusCode, 400);

    tearDown();
}

TEST_F(http2Server_test, CheckMatchingWhenProvisionExists)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionExample.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);
    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and server provision data received\" }");
    EXPECT_EQ(response.statusCode, 201);

    // Post configuration:
    nghttp2::asio_http2::header_map headersPost;
    headersPost.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string matchingBody = MatchingRegexMatching.dump();
    response = admin_client_->send("POST", "/admin/v1/server-matching", matchingBody, headersPost);
    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }");
    EXPECT_EQ(response.statusCode, 201);

    tearDown();
}

TEST_F(http2Server_test, CheckProvision)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/server-provision", "", headers);

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
    response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and server provision data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send("GET", "/admin/v1/server-provision", "", headers);
    EXPECT_EQ(response.body, std::string("[") + provisionBody + std::string("]"));
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, CheckProvision2)
{
    // Configure:
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionExample2.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and server provision data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    tearDown();
}

TEST_F(http2Server_test, CheckProvisionBadSchema)
{
    // Configure:
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionBadSchema.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"false\", \"response\": \"server-provision operation; invalid schema\" }");
    EXPECT_EQ(response.statusCode, 400);
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    tearDown();
}

TEST_F(http2Server_test, CheckGlobalVariable)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/global-variable", "", headers);

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
    response = admin_client_->send("POST", "/admin/v1/global-variable", globalVariableBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"global-variable operation; valid schema and global variables received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send("GET", "/admin/v1/global-variable", "", headers);
    EXPECT_EQ(response.body, globalVariableBody);
    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

TEST_F(http2Server_test, CheckSchema)
{
    // Check current configuration:
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/schema", "", headers);

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
    response = admin_client_->send("POST", "/admin/v1/schema", schemaBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"schema operation; valid schema and schema data received\" }");
    EXPECT_EQ(response.statusCode, 201);
    // Check headers
    // EXPECT_EQ(ert::http2comm::headersAsString(response.headers), expected); // unpredictable (date header added by nghttp2)
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    // Check current configuration again:
    response = admin_client_->send("GET", "/admin/v1/schema", "", headers);
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
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and server provision data received\" }");
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
    response = traffic_client_->send("GET", "/app/v1/foo/bar?name=test", "", headers);

    EXPECT_EQ(response.body, "{\"foo\":\"bar\",\"text\":\"hello\"}");
    EXPECT_EQ(response.statusCode, 200);
    // Check headers
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    if (it != response.headers.end()) {
        EXPECT_EQ(it->second.value, "application/json");
    }

    it = response.headers.find("x-version");
    EXPECT_TRUE(it != response.headers.end());
    if (it != response.headers.end()) {
        EXPECT_EQ(it->second.value, "1.0.0");
    }

    tearDown();
}

TEST_F(http2Server_test, ProvisionedEvent2)
{
    /////////////////////
    // Admin provision //
    /////////////////////
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string provisionBody = ProvisionExample2.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

    EXPECT_EQ(response.body, "{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema and server provision data received\" }");
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
    response = traffic_client_->send("GET", "/app/v1/foo/bar?name=test", "", headers);

    EXPECT_EQ(response.body, "{\"foo\":\"bar\"}");
    EXPECT_EQ(response.statusCode, 200);
    // Check headers
    it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    if (it != response.headers.end()) {
        EXPECT_EQ(it->second.value, "application/json");
    }

    it = response.headers.find("x-version");
    EXPECT_TRUE(it != response.headers.end());
    if (it != response.headers.end()) {
        EXPECT_EQ(it->second.value, "1.0.0");
    }

    tearDown();
}

TEST_F(http2Server_test, UnprovisionedEvent)
{
    nghttp2::asio_http2::header_map headers;
    //headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    ert::http2comm::Http2Client::response response = traffic_client_->send("POST", "/app/v1/non/provisioned/uri", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 501);

    tearDown();
}

TEST_F(http2Server_test, InvalidContentType)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"text/html"});
    std::string provisionBody = ProvisionExample.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/server-provision", provisionBody, headers);

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
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1", "whatever", headers);

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
    std::string log;
    int result = admin_http2_server_->serverMatching(FullMatching, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "server-matching operation; valid schema and matching data received");

    tearDown();
}

TEST_F(http2Server_test, ServerMatchingBadSchema)
{
    std::string log;
    int result = admin_http2_server_->serverMatching(FooBar, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "server-matching operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, ServerProvision)
{
    std::string log;
    int result = admin_http2_server_->serverProvision(ProvisionExample3, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "server-provision operation; valid schema and server provision data received");

    tearDown();
}

TEST_F(http2Server_test, ServerProvisionBadSchema)
{
    std::string log;
    int result = admin_http2_server_->serverProvision(ProvisionBadSchema, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "server-provision operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariable)
{
    std::string log;
    int result = admin_http2_server_->globalVariable(GlobalVariableExample2, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "global-variable operation; valid schema and global variables received");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariableNOK1)
{
    std::string log;
    int result = admin_http2_server_->globalVariable(GlobalVariableNOK1, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "global-variable operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, GlobalVariableNOK2)
{
    std::string log;
    int result = admin_http2_server_->globalVariable(GlobalVariableNOK2, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "global-variable operation; invalid schema");

    tearDown();
}

TEST_F(http2Server_test, Schema)
{
    std::string log;
    int result = admin_http2_server_->schema(SchemaExample2, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "schema operation; valid schema and schema data received");

    tearDown();
}

TEST_F(http2Server_test, ClientEndpoint)
{
    nlohmann::json array{};
    array.push_back(ClientEndpointExample);
    array.push_back(ClientEndpointExample);

    std::string log;
    int result;

    result = admin_http2_server_->clientEndpoint(ClientEndpointExample, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "client-endpoint operation; valid schema and client endpoint data received");

    result = admin_http2_server_->clientEndpoint(array, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "client-endpoint operation; valid schemas and client endpoints data received");

    tearDown();
}

TEST_F(http2Server_test, ClientEndpointBadSchema)
{
    nlohmann::json array{};
    array.push_back(ClientEndpointExampleNOK);
    array.push_back(ClientEndpointExampleNOK);

    std::string log;
    int result;

    result = admin_http2_server_->clientEndpoint(ClientEndpointExampleNOK, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; invalid schema");

    result = admin_http2_server_->clientEndpoint(array, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; detected one invalid schema");

    tearDown();
}

TEST_F(http2Server_test, ClientEndpointBadContent)
{
    nlohmann::json array{};
    array.push_back(ClientEndpointExampleNOK2);
    array.push_back(ClientEndpointExampleNOK2);

    std::string log;
    int result;

    result = admin_http2_server_->clientEndpoint(ClientEndpointExampleNOK2, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; invalid client endpoint data received");

    result = admin_http2_server_->clientEndpoint(array, log);
    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; detected one invalid client endpoint data received");

    tearDown();
}

TEST_F(http2Server_test, GetConfiguration)
{
    EXPECT_EQ(admin_http2_server_->getConfiguration()->getJson(), GeneralConfiguration);

    tearDown();
}

TEST_F(http2Server_test, GetFileManager)
{
    nlohmann::json expectedJson{}; // nothing managed
    EXPECT_EQ(admin_http2_server_->getFileManager()->getJson(), expectedJson);

    tearDown();
}

TEST_F(http2Server_test, GetHttp2Server)
{
    EXPECT_EQ(admin_http2_server_->getHttp2Server(), traffic_http2_server_);

    tearDown();
}

TEST_F(http2Server_test, CheckServerConfiguration)
{
    traffic_http2_server_->setReceiveRequestBody(true);
    traffic_http2_server_->setPreReserveRequestBody(true);
    EXPECT_EQ(traffic_http2_server_->configurationAsJsonString(), "{\"preReserveRequestBody\":true,\"receiveRequestBody\":true}");
    traffic_http2_server_->setReceiveRequestBody(false);
    traffic_http2_server_->setPreReserveRequestBody(false);
    EXPECT_EQ(traffic_http2_server_->configurationAsJsonString(), "{\"preReserveRequestBody\":false,\"receiveRequestBody\":false}");

    tearDown();
}

