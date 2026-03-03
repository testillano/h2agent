/*
 Unit tests for MyAdminHttp2Server that don't require real HTTP/2 connections.
 These tests focus on the business logic methods that can be tested in isolation.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>
#include <AdminData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace h2agent {
namespace test {

class MyAdminHttp2ServerUnitTest : public ::testing::Test {
protected:
    std::unique_ptr<http2::MyAdminHttp2Server> admin_server_;
    std::unique_ptr<http2::MyTrafficHttp2Server> traffic_server_;
    boost::asio::io_context io_context_;
    std::unique_ptr<model::Configuration> configuration_;
    std::unique_ptr<model::GlobalVariable> global_variable_;
    std::unique_ptr<model::FileManager> file_manager_;
    std::unique_ptr<model::SocketManager> socket_manager_;
    std::unique_ptr<model::MockServerData> mock_server_data_;
    std::unique_ptr<model::MockClientData> mock_client_data_;

    void SetUp() override {
        admin_server_ = std::make_unique<http2::MyAdminHttp2Server>("test_admin", 1);
        traffic_server_ = std::make_unique<http2::MyTrafficHttp2Server>(
            "test_traffic", 1, 1, &io_context_, -1);

        configuration_ = std::make_unique<model::Configuration>();
        global_variable_ = std::make_unique<model::GlobalVariable>();
        file_manager_ = std::make_unique<model::FileManager>(&io_context_);
        socket_manager_ = std::make_unique<model::SocketManager>(&io_context_);
        mock_server_data_ = std::make_unique<model::MockServerData>();
        mock_client_data_ = std::make_unique<model::MockClientData>();

        admin_server_->setConfiguration(configuration_.get());
        admin_server_->setGlobalVariable(global_variable_.get());
        admin_server_->setFileManager(file_manager_.get());
        admin_server_->setSocketManager(socket_manager_.get());
        admin_server_->setMockServerData(mock_server_data_.get());
        admin_server_->setMockClientData(mock_client_data_.get());
        admin_server_->setHttp2Server(traffic_server_.get());
    }
};

// ============================================================================
// Tests for serverMatching()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ServerMatchingFullMatching) {
    nlohmann::json config = R"({"algorithm":"FullMatching"})"_json;
    std::string log;
    int result = admin_server_->serverMatching(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "server-matching operation; valid schema and matching data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerMatchingRegexReplace) {
    nlohmann::json config = R"delim({"algorithm":"FullMatchingRegexReplace","rgx":"(/app/v1/foo/)([0-9]*)","fmt":"$1"})delim"_json;
    std::string log;
    int result = admin_server_->serverMatching(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "server-matching operation; valid schema and matching data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerMatchingRegexMatching) {
    nlohmann::json config = R"({"algorithm":"RegexMatching"})"_json;
    std::string log;
    int result = admin_server_->serverMatching(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "server-matching operation; valid schema and matching data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerMatchingInvalidSchema) {
    nlohmann::json config = R"({"foo":"bar"})"_json;
    std::string log;
    int result = admin_server_->serverMatching(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "server-matching operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerMatchingInvalidContent) {
    nlohmann::json config = R"({"algorithm":"FullMatching","rgx":"whatever"})"_json;
    std::string log;
    int result = admin_server_->serverMatching(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "server-matching operation; invalid matching data received");
}

// ============================================================================
// Tests for serverProvision()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ServerProvisionValid) {
    nlohmann::json config = R"({"requestMethod": "GET", "requestUri": "/foo/bar", "responseCode": 200})"_json;
    std::string log;
    int result = admin_server_->serverProvision(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "server-provision operation; valid schema and server provision data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerProvisionInvalidSchema) {
    nlohmann::json config = R"({"requestUri": "/foo/bar", "responseCode": 200})"_json;
    std::string log;
    int result = admin_server_->serverProvision(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "server-provision operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerProvisionWithResponseBody) {
    nlohmann::json config = R"({
        "requestMethod": "POST",
        "requestUri": "/api/test",
        "responseCode": 201,
        "responseBody": {"status": "created"}
    })"_json;
    std::string log;
    int result = admin_server_->serverProvision(config, log);

    EXPECT_EQ(result, 201);
}

TEST_F(MyAdminHttp2ServerUnitTest, ServerProvisionWithHeaders) {
    nlohmann::json config = R"({
        "requestMethod": "GET",
        "requestUri": "/api/data",
        "responseCode": 200,
        "responseHeaders": {"content-type": "application/json", "x-custom": "value"}
    })"_json;
    std::string log;
    int result = admin_server_->serverProvision(config, log);

    EXPECT_EQ(result, 201);
}

// ============================================================================
// Tests for globalVariable()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableValid) {
    nlohmann::json config = R"({"var1": "value1", "var2": "value2"})"_json;
    std::string log;
    int result = admin_server_->globalVariable(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "global-variable operation; valid schema and global variables received");
}

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableInvalidNonString) {
    nlohmann::json config = R"({"var1": 123})"_json;
    std::string log;
    int result = admin_server_->globalVariable(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "global-variable operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableInvalidObject) {
    nlohmann::json config = R"({"var1": {"nested": "object"}})"_json;
    std::string log;
    int result = admin_server_->globalVariable(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "global-variable operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableSingleVar) {
    nlohmann::json config = R"({"singleVar": "singleValue"})"_json;
    std::string log;
    int result = admin_server_->globalVariable(config, log);

    EXPECT_EQ(result, 201);
}

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableEmptyValue) {
    nlohmann::json config = R"({"emptyVar": ""})"_json;
    std::string log;
    int result = admin_server_->globalVariable(config, log);

    EXPECT_EQ(result, 201);
}

// ============================================================================
// Tests for schema()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, SchemaValid) {
    nlohmann::json config = R"({
        "id": "mySchema",
        "schema": {
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {"foo": {"type": "string"}}
        }
    })"_json;
    std::string log;
    int result = admin_server_->schema(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "schema operation; valid schema and schema data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, SchemaInvalid) {
    nlohmann::json config = R"({"id": "mySchema"})"_json;
    std::string log;
    int result = admin_server_->schema(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "schema operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, SchemaWithRequired) {
    nlohmann::json config = R"({
        "id": "requiredSchema",
        "schema": {
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {"name": {"type": "string"}},
            "required": ["name"]
        }
    })"_json;
    std::string log;
    int result = admin_server_->schema(config, log);

    EXPECT_EQ(result, 201);
}

// ============================================================================
// Tests for clientEndpoint()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ClientEndpointValidSingle) {
    nlohmann::json config = R"({"id": "myServer", "host": "localhost", "port": 8000, "secure": false})"_json;
    std::string log;
    int result = admin_server_->clientEndpoint(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "client-endpoint operation; valid schema and client endpoint data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientEndpointValidArray) {
    nlohmann::json config = R"([
        {"id": "server1", "host": "localhost", "port": 8000},
        {"id": "server2", "host": "localhost", "port": 8001}
    ])"_json;
    std::string log;
    int result = admin_server_->clientEndpoint(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "client-endpoint operation; valid schemas and client endpoints data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientEndpointInvalidSchema) {
    nlohmann::json config = R"({"i_d": "wrong_field", "host": "localhost"})"_json;
    std::string log;
    int result = admin_server_->clientEndpoint(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "client-endpoint operation; invalid schema");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientEndpointInvalidContent) {
    nlohmann::json config = R"({"id": "", "host": "localhost", "port": 8000})"_json;
    std::string log;
    int result = admin_server_->clientEndpoint(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "client-endpoint operation; invalid client endpoint data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientEndpointSecure) {
    nlohmann::json config = R"({"id": "secureServer", "host": "localhost", "port": 443, "secure": true})"_json;
    std::string log;
    int result = admin_server_->clientEndpoint(config, log);

    EXPECT_EQ(result, 201);
}

// ============================================================================
// Tests for clientProvision()
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ClientProvisionValid) {
    // First create an endpoint
    nlohmann::json endpoint = R"({"id": "myEndpoint", "host": "localhost", "port": 8000})"_json;
    std::string log;
    admin_server_->clientEndpoint(endpoint, log);

    nlohmann::json config = R"({
        "id": "myProvision",
        "endpoint": "myEndpoint",
        "requestMethod": "GET",
        "requestUri": "/test"
    })"_json;
    int result = admin_server_->clientProvision(config, log);

    EXPECT_EQ(result, 201);
    EXPECT_EQ(log, "client-provision operation; valid schema and client provision data received");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientProvisionInvalidSchema) {
    nlohmann::json config = R"({})"_json;  // missing required "id"
    std::string log;
    int result = admin_server_->clientProvision(config, log);

    EXPECT_EQ(result, 400);
    EXPECT_EQ(log, "client-provision operation; invalid schema");
}

// ============================================================================
// Tests for getters/setters
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ConfigurationGetterSetter) {
    EXPECT_EQ(admin_server_->getConfiguration(), configuration_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, GlobalVariableGetterSetter) {
    EXPECT_EQ(admin_server_->getGlobalVariable(), global_variable_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, FileManagerGetterSetter) {
    EXPECT_EQ(admin_server_->getFileManager(), file_manager_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, SocketManagerGetterSetter) {
    EXPECT_EQ(admin_server_->getSocketManager(), socket_manager_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, MockServerDataGetterSetter) {
    EXPECT_EQ(admin_server_->getMockServerData(), mock_server_data_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, MockClientDataGetterSetter) {
    EXPECT_EQ(admin_server_->getMockClientData(), mock_client_data_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, Http2ServerGetterSetter) {
    EXPECT_EQ(admin_server_->getHttp2Server(), traffic_server_.get());
}

TEST_F(MyAdminHttp2ServerUnitTest, AdminDataGetter) {
    EXPECT_NE(admin_server_->getAdminData(), nullptr);
}

TEST_F(MyAdminHttp2ServerUnitTest, DebugClientProvisionSchema) {
    // Check if schema is available
    auto& schema = admin_server_->getAdminData()->getClientProvisionData().getSchema();
    std::cout << "Schema available: " << (schema.isAvailable() ? "yes" : "no") << std::endl;

    // Try to validate
    nlohmann::json valid = R"({"id": "test"})"_json;
    nlohmann::json invalid = R"({})"_json;
    std::string error;

    bool validResult = schema.validate(valid, error);
    std::cout << "Valid JSON result: " << (validResult ? "passed" : "failed") << " error: " << error << std::endl;

    error.clear();
    bool invalidResult = schema.validate(invalid, error);
    std::cout << "Invalid JSON result: " << (invalidResult ? "passed" : "failed") << " error: " << error << std::endl;

    EXPECT_TRUE(schema.isAvailable());
}

// ============================================================================
// Tests for client data configuration
// ============================================================================

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationDefault) {
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationAfterDiscard) {
    admin_server_->discardClientData();
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":false,\"storeEventsKeyHistory\":true}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationAfterDiscardHistory) {
    admin_server_->discardClientDataKeyHistory();
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":false}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationAfterDisablePurge) {
    admin_server_->disableClientPurge();
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":false,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationAllDisabled) {
    admin_server_->discardClientData();
    admin_server_->discardClientDataKeyHistory();
    admin_server_->disableClientPurge();
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":false,\"storeEvents\":false,\"storeEventsKeyHistory\":false}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationToggleDiscard) {
    admin_server_->discardClientData(true);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":false,\"storeEventsKeyHistory\":true}");

    admin_server_->discardClientData(false);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationToggleHistory) {
    admin_server_->discardClientDataKeyHistory(true);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":false}");

    admin_server_->discardClientDataKeyHistory(false);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

TEST_F(MyAdminHttp2ServerUnitTest, ClientDataConfigurationTogglePurge) {
    admin_server_->disableClientPurge(true);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":false,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");

    admin_server_->disableClientPurge(false);
    EXPECT_EQ(admin_server_->clientDataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

} // namespace test
} // namespace h2agent
