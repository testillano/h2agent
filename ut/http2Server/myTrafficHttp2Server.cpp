/*
 Unit tests for MyTrafficHttp2Server that don't require real HTTP/2 connections.
 These tests focus on the business logic methods that can be tested in isolation.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <MyTrafficHttp2Server.hpp>
#include <AdminData.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>
#include <GlobalVariable.hpp>

#include <boost/asio.hpp>

namespace h2agent {
namespace test {

class MyTrafficHttp2ServerUnitTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<http2::MyTrafficHttp2Server> server_;
    std::unique_ptr<model::AdminData> admin_data_;
    std::unique_ptr<model::MockServerData> mock_server_data_;
    std::unique_ptr<model::MockClientData> mock_client_data_;
    std::unique_ptr<model::GlobalVariable> global_variable_;

    void SetUp() override {
        server_ = std::make_unique<http2::MyTrafficHttp2Server>(
            "test_server", 1, 1, &io_context_, -1);

        admin_data_ = std::make_unique<model::AdminData>();
        mock_server_data_ = std::make_unique<model::MockServerData>();
        mock_client_data_ = std::make_unique<model::MockClientData>();
        global_variable_ = std::make_unique<model::GlobalVariable>();

        server_->setAdminData(admin_data_.get());
        server_->setMockServerData(mock_server_data_.get());
        server_->setMockClientData(mock_client_data_.get());
        server_->setGlobalVariable(global_variable_.get());
    }
};

// Test dataConfigurationAsJsonString with default values
TEST_F(MyTrafficHttp2ServerUnitTest, DataConfigurationDefault) {
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

// Test dataConfigurationAsJsonString after discardData
TEST_F(MyTrafficHttp2ServerUnitTest, DataConfigurationAfterDiscardData) {
    server_->discardData();
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":false,\"storeEventsKeyHistory\":true}");
}

// Test dataConfigurationAsJsonString after discardDataKeyHistory
TEST_F(MyTrafficHttp2ServerUnitTest, DataConfigurationAfterDiscardKeyHistory) {
    server_->discardDataKeyHistory();
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":false}");
}

// Test dataConfigurationAsJsonString after disablePurge
TEST_F(MyTrafficHttp2ServerUnitTest, DataConfigurationAfterDisablePurge) {
    server_->disablePurge();
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":false,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

// Test dataConfigurationAsJsonString with all disabled
TEST_F(MyTrafficHttp2ServerUnitTest, DataConfigurationAllDisabled) {
    server_->discardData();
    server_->discardDataKeyHistory();
    server_->disablePurge();
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":false,\"storeEvents\":false,\"storeEventsKeyHistory\":false}");
}

// Test configurationAsJsonString with default values
TEST_F(MyTrafficHttp2ServerUnitTest, ConfigurationDefault) {
    EXPECT_EQ(server_->configurationAsJsonString(),
              "{\"preReserveRequestBody\":true,\"receiveRequestBody\":true}");
}

// Test configurationAsJsonString after setReceiveRequestBody(false)
TEST_F(MyTrafficHttp2ServerUnitTest, ConfigurationReceiveRequestBodyFalse) {
    server_->setReceiveRequestBody(false);
    EXPECT_EQ(server_->configurationAsJsonString(),
              "{\"preReserveRequestBody\":true,\"receiveRequestBody\":false}");
}

// Test configurationAsJsonString after setPreReserveRequestBody(false)
TEST_F(MyTrafficHttp2ServerUnitTest, ConfigurationPreReserveRequestBodyFalse) {
    server_->setPreReserveRequestBody(false);
    EXPECT_EQ(server_->configurationAsJsonString(),
              "{\"preReserveRequestBody\":false,\"receiveRequestBody\":true}");
}

// Test configurationAsJsonString with both false
TEST_F(MyTrafficHttp2ServerUnitTest, ConfigurationBothFalse) {
    server_->setReceiveRequestBody(false);
    server_->setPreReserveRequestBody(false);
    EXPECT_EQ(server_->configurationAsJsonString(),
              "{\"preReserveRequestBody\":false,\"receiveRequestBody\":false}");
}

// Test preReserveRequestBody default
TEST_F(MyTrafficHttp2ServerUnitTest, PreReserveRequestBodyDefault) {
    EXPECT_TRUE(server_->preReserveRequestBody());
}

// Test preReserveRequestBody after setting false
TEST_F(MyTrafficHttp2ServerUnitTest, PreReserveRequestBodySetFalse) {
    server_->setPreReserveRequestBody(false);
    EXPECT_FALSE(server_->preReserveRequestBody());
}

// Test getters/setters for AdminData
TEST_F(MyTrafficHttp2ServerUnitTest, AdminDataGetterSetter) {
    EXPECT_EQ(server_->getAdminData(), admin_data_.get());

    model::AdminData other_admin_data;
    server_->setAdminData(&other_admin_data);
    EXPECT_EQ(server_->getAdminData(), &other_admin_data);
}

// Test getters/setters for MockServerData
TEST_F(MyTrafficHttp2ServerUnitTest, MockServerDataGetterSetter) {
    EXPECT_EQ(server_->getMockServerData(), mock_server_data_.get());

    model::MockServerData other_mock_data;
    server_->setMockServerData(&other_mock_data);
    EXPECT_EQ(server_->getMockServerData(), &other_mock_data);
}

// Test getters/setters for MockClientData
TEST_F(MyTrafficHttp2ServerUnitTest, MockClientDataGetterSetter) {
    EXPECT_EQ(server_->getMockClientData(), mock_client_data_.get());

    model::MockClientData other_mock_data;
    server_->setMockClientData(&other_mock_data);
    EXPECT_EQ(server_->getMockClientData(), &other_mock_data);
}

// Test getters/setters for GlobalVariable
TEST_F(MyTrafficHttp2ServerUnitTest, GlobalVariableGetterSetter) {
    EXPECT_EQ(server_->getGlobalVariable(), global_variable_.get());

    model::GlobalVariable other_global_var;
    server_->setGlobalVariable(&other_global_var);
    EXPECT_EQ(server_->getGlobalVariable(), &other_global_var);
}

// Test responseDelayMs with no variable set (returns zero)
TEST_F(MyTrafficHttp2ServerUnitTest, ResponseDelayMsNoVariable) {
    auto delay = server_->responseDelayMs(12345);
    EXPECT_EQ(delay.count(), 0);
}

// Test responseDelayMs with valid variable
TEST_F(MyTrafficHttp2ServerUnitTest, ResponseDelayMsWithVariable) {
    global_variable_->add("__core.response-delay-ms.12345", "500");
    auto delay = server_->responseDelayMs(12345);
    EXPECT_EQ(delay.count(), 500);
}

// Test responseDelayMs with invalid (non-numeric) variable
TEST_F(MyTrafficHttp2ServerUnitTest, ResponseDelayMsInvalidVariable) {
    global_variable_->add("__core.response-delay-ms.12345", "not_a_number");
    auto delay = server_->responseDelayMs(12345);
    EXPECT_EQ(delay.count(), 0);
}

// Test responseDelayMs with out-of-range variable
TEST_F(MyTrafficHttp2ServerUnitTest, ResponseDelayMsOutOfRange) {
    global_variable_->add("__core.response-delay-ms.12345", "99999999999999999999999999999");
    auto delay = server_->responseDelayMs(12345);
    EXPECT_EQ(delay.count(), 0);
}

// Test discardData toggle
TEST_F(MyTrafficHttp2ServerUnitTest, DiscardDataToggle) {
    // Default: storing enabled
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");

    // Discard
    server_->discardData(true);
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":false,\"storeEventsKeyHistory\":true}");

    // Re-enable
    server_->discardData(false);
    EXPECT_EQ(server_->dataConfigurationAsJsonString(),
              "{\"purgeExecution\":true,\"storeEvents\":true,\"storeEventsKeyHistory\":true}");
}

} // namespace test
} // namespace h2agent
