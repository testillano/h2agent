#include <nlohmann/json.hpp>

#include <ert/http2comm/Http2Client.hpp>
#include <ert/http2comm/Http.hpp>

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>

#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>

#include <ert/metrics/Metrics.hpp>

#include <thread>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


const nlohmann::json ClientEndpointExample = R"(
{
  "id": "myServer",
  "host": "localhost",
  "port": 8000,
  "secure": false,
  "permit": true
}
)"_json;

const nlohmann::json ClientEndpointSecure = R"(
{
  "id": "mySecureServer",
  "host": "localhost",
  "port": 8443,
  "secure": true,
  "permit": true
}
)"_json;

const nlohmann::json ClientEndpointBadSchema = R"(
{
  "i_d": "myServer",
  "host": "localhost",
  "port": 8000
}
)"_json;

const nlohmann::json ClientEndpointEmptyId = R"(
{
  "id": "",
  "host": "localhost",
  "port": 8000
}
)"_json;

const nlohmann::json ClientProvisionExample = R"(
{
  "id": "myClientProvision",
  "endpoint": "myServer",
  "requestMethod": "GET",
  "requestUri": "/target/endpoint",
  "responseCode": 200
}
)"_json;

const nlohmann::json ClientProvisionBadSchema = R"(
{
  "foo": "bar"
}
)"_json;


class http2Client_test : public ::testing::Test
{
    std::thread tt_;

public:
    h2agent::http2::MyAdminHttp2Server* admin_http2_server_{};
    h2agent::http2::MyTrafficHttp2Server* traffic_http2_server_{};
    boost::asio::io_context *timers_io_context_{};
    h2agent::model::Configuration* configuration_{};
    h2agent::model::GlobalVariable* global_variable_{};
    h2agent::model::FileManager* file_manager_{};
    h2agent::model::SocketManager* socket_manager_{};
    h2agent::model::MockServerData* mock_server_events_data_{};
    h2agent::model::MockClientData* mock_client_events_data_{};
    ert::metrics::Metrics* metrics_{};

    std::shared_ptr<ert::http2comm::Http2Client> admin_client_{};

    http2Client_test() {
        timers_io_context_ = new boost::asio::io_context();
        configuration_ = new h2agent::model::Configuration();
        global_variable_ = new h2agent::model::GlobalVariable();
        file_manager_ = new h2agent::model::FileManager(timers_io_context_);
        socket_manager_ = new h2agent::model::SocketManager(timers_io_context_);
        metrics_ = new ert::metrics::Metrics();
        file_manager_->enableMetrics(metrics_, "test");
        socket_manager_->enableMetrics(metrics_, "test");
        mock_server_events_data_ = new h2agent::model::MockServerData();
        mock_client_events_data_ = new h2agent::model::MockClientData();

        admin_http2_server_ = new h2agent::http2::MyAdminHttp2Server("test_admin_server", 1);
        admin_http2_server_->setApiName("admin");
        admin_http2_server_->setApiVersion("v1");
        admin_http2_server_->setConfiguration(configuration_);
        admin_http2_server_->setGlobalVariable(global_variable_);
        admin_http2_server_->setFileManager(file_manager_);
        admin_http2_server_->setSocketManager(socket_manager_);
        admin_http2_server_->setMockServerData(mock_server_events_data_);
        admin_http2_server_->setMockClientData(mock_client_events_data_);
        admin_http2_server_->enableMetrics(metrics_);

        traffic_http2_server_ = new h2agent::http2::MyTrafficHttp2Server("test_traffic_server", 1, 1, timers_io_context_, -1);
        traffic_http2_server_->setApiName("app");
        traffic_http2_server_->setApiVersion("v1");
        traffic_http2_server_->setMockServerData(mock_server_events_data_);
        traffic_http2_server_->setMockClientData(mock_client_events_data_);
        traffic_http2_server_->setAdminData(admin_http2_server_->getAdminData());
        traffic_http2_server_->enableMetrics(metrics_);
        traffic_http2_server_->setGlobalVariable(global_variable_);

        admin_http2_server_->setHttp2Server(traffic_http2_server_);

        tt_ = std::thread([&] {
            boost::asio::io_context::work work(*timers_io_context_);
            timers_io_context_->run();
        });

        EXPECT_TRUE(metrics_->serve("127.0.0.1:8081"));
        EXPECT_EQ(admin_http2_server_->serve("127.0.0.1", "8075", "", "", 1, true), EXIT_SUCCESS);
        admin_client_ = std::make_shared<ert::http2comm::Http2Client>("myClient", "127.0.0.1", "8075", false);
    }

    ~http2Client_test() {
        delete(configuration_);
        delete(global_variable_);
        delete(file_manager_);
        delete(socket_manager_);
        delete(mock_server_events_data_);
        delete(mock_client_events_data_);
        admin_http2_server_->stop();
        traffic_http2_server_->stop();
        delete(timers_io_context_);
        delete(metrics_);
        delete(admin_http2_server_);
        delete(traffic_http2_server_);
    }

    void tearDown() {
        timers_io_context_->stop();
        tt_.join();
    }
};

// Client Endpoint Tests

TEST_F(http2Client_test, ClientEndpointGetEmpty)
{
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/client-endpoint", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointPost)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string body = ClientEndpointExample.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/client-endpoint", body, headers);

    EXPECT_EQ(response.statusCode, 201);
    auto it = response.headers.find("content-type");
    EXPECT_TRUE(it != response.headers.end());
    EXPECT_EQ(it->second.value, "application/json");

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointPostArray)
{
    nlohmann::json array{};
    array.push_back(ClientEndpointExample);
    array.push_back(ClientEndpointSecure);

    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/client-endpoint", array.dump(), headers);

    EXPECT_EQ(response.statusCode, 201);

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointPostBadSchema)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string body = ClientEndpointBadSchema.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/client-endpoint", body, headers);

    EXPECT_EQ(response.statusCode, 400);

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointPostEmptyId)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string body = ClientEndpointEmptyId.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/client-endpoint", body, headers);

    EXPECT_EQ(response.statusCode, 400);

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointDelete)
{
    // First create
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    admin_client_->send("POST", "/admin/v1/client-endpoint", ClientEndpointExample.dump(), headers);

    // Then delete
    ert::http2comm::Http2Client::response response = admin_client_->send("DELETE", "/admin/v1/client-endpoint", "", headers);
    EXPECT_EQ(response.statusCode, 200);

    // Verify empty
    response = admin_client_->send("GET", "/admin/v1/client-endpoint", "", headers);
    EXPECT_EQ(response.statusCode, 204);

    tearDown();
}

// Client Provision Tests

TEST_F(http2Client_test, ClientProvisionGetEmpty)
{
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/client-provision", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);

    tearDown();
}

TEST_F(http2Client_test, ClientProvisionPostBadSchema)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    std::string body = ClientProvisionBadSchema.dump();
    ert::http2comm::Http2Client::response response = admin_client_->send("POST", "/admin/v1/client-provision", body, headers);

    EXPECT_EQ(response.statusCode, 400);

    tearDown();
}

TEST_F(http2Client_test, ClientProvisionDelete)
{
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("DELETE", "/admin/v1/client-provision", "", headers);

    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

// Client Data Tests

TEST_F(http2Client_test, ClientDataGetEmpty)
{
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("GET", "/admin/v1/client-data", "", headers);

    EXPECT_EQ(response.body, "");
    EXPECT_EQ(response.statusCode, 204);

    tearDown();
}

TEST_F(http2Client_test, ClientDataDelete)
{
    nghttp2::asio_http2::header_map headers;
    ert::http2comm::Http2Client::response response = admin_client_->send("DELETE", "/admin/v1/client-data", "", headers);

    EXPECT_EQ(response.statusCode, 200);

    tearDown();
}

// Direct API Tests

TEST_F(http2Client_test, ClientEndpointDirectAPI)
{
    std::string log;
    int result = admin_http2_server_->clientEndpoint(ClientEndpointExample, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "client-endpoint operation; valid schema and client endpoint data received");

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointDirectAPIArray)
{
    nlohmann::json array{};
    array.push_back(ClientEndpointExample);
    array.push_back(ClientEndpointSecure);

    std::string log;
    int result = admin_http2_server_->clientEndpoint(array, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::CREATED);
    EXPECT_EQ(log, "client-endpoint operation; valid schemas and client endpoints data received");

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointDirectAPIBadSchema)
{
    std::string log;
    int result = admin_http2_server_->clientEndpoint(ClientEndpointBadSchema, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; invalid schema");

    tearDown();
}

TEST_F(http2Client_test, ClientEndpointDirectAPIEmptyId)
{
    std::string log;
    int result = admin_http2_server_->clientEndpoint(ClientEndpointEmptyId, log);

    EXPECT_EQ(result, ert::http2comm::ResponseCode::BAD_REQUEST);
    EXPECT_EQ(log, "client-endpoint operation; invalid client endpoint data received");

    tearDown();
}
