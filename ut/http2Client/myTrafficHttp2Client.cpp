/*
 Unit tests for MyTrafficHttp2Client that don't require real HTTP/2 connections.
 These tests focus on the business logic methods that can be tested in isolation.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <MyTrafficHttp2Client.hpp>
#include <AdminData.hpp>
#include <MockClientData.hpp>

#include <ert/metrics/Metrics.hpp>
#include <boost/asio.hpp>

namespace h2agent {
namespace test {

class MyTrafficHttp2ClientUnitTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<http2::MyTrafficHttp2Client> client_;
    std::unique_ptr<model::AdminData> admin_data_;

    void SetUp() override {
        client_ = std::make_unique<http2::MyTrafficHttp2Client>(
            "test_client", "127.0.0.1", "8080", false, &io_context_);
        admin_data_ = std::make_unique<model::AdminData>();
    }
};

// Constructor tests
TEST_F(MyTrafficHttp2ClientUnitTest, ConstructorInitializesCorrectly) {
    EXPECT_NE(client_->getMockClientData(), nullptr);
    EXPECT_EQ(client_->getAdminData(), nullptr);
    EXPECT_EQ(client_->getGeneralUniqueClientSequence(), 0);
}

TEST_F(MyTrafficHttp2ClientUnitTest, ConstructorWithSecureConnection) {
    auto secure_client = std::make_unique<http2::MyTrafficHttp2Client>(
        "secure_client", "localhost", "443", true, &io_context_);
    EXPECT_NE(secure_client->getMockClientData(), nullptr);
}

// AdminData tests
TEST_F(MyTrafficHttp2ClientUnitTest, SetAndGetAdminData) {
    EXPECT_EQ(client_->getAdminData(), nullptr);
    client_->setAdminData(admin_data_.get());
    EXPECT_EQ(client_->getAdminData(), admin_data_.get());
}

TEST_F(MyTrafficHttp2ClientUnitTest, SetAdminDataToNull) {
    client_->setAdminData(admin_data_.get());
    client_->setAdminData(nullptr);
    EXPECT_EQ(client_->getAdminData(), nullptr);
}

// MockClientData tests
TEST_F(MyTrafficHttp2ClientUnitTest, GetMockClientDataNotNull) {
    EXPECT_NE(client_->getMockClientData(), nullptr);
}

TEST_F(MyTrafficHttp2ClientUnitTest, SetMockClientData) {
    auto mock_data = new model::MockClientData();
    auto original = client_->getMockClientData();
    client_->setMockClientData(mock_data);
    EXPECT_EQ(client_->getMockClientData(), mock_data);
    EXPECT_NE(client_->getMockClientData(), original);
    // Note: original is leaked here but that's expected in this test
}

// GeneralUniqueClientSequence tests
TEST_F(MyTrafficHttp2ClientUnitTest, InitialSequenceIsZero) {
    EXPECT_EQ(client_->getGeneralUniqueClientSequence(), 0);
}

TEST_F(MyTrafficHttp2ClientUnitTest, IncrementSequence) {
    client_->incrementGeneralUniqueClientSequence();
    EXPECT_EQ(client_->getGeneralUniqueClientSequence(), 1);
}

TEST_F(MyTrafficHttp2ClientUnitTest, IncrementSequenceMultipleTimes) {
    for (int i = 0; i < 100; ++i) {
        client_->incrementGeneralUniqueClientSequence();
    }
    EXPECT_EQ(client_->getGeneralUniqueClientSequence(), 100);
}

TEST_F(MyTrafficHttp2ClientUnitTest, SequenceIsIndependentPerClient) {
    auto client2 = std::make_unique<http2::MyTrafficHttp2Client>(
        "test_client2", "127.0.0.1", "8081", false, &io_context_);

    client_->incrementGeneralUniqueClientSequence();
    client_->incrementGeneralUniqueClientSequence();

    EXPECT_EQ(client_->getGeneralUniqueClientSequence(), 2);
    EXPECT_EQ(client2->getGeneralUniqueClientSequence(), 0);
}

// Metrics tests
TEST_F(MyTrafficHttp2ClientUnitTest, EnableMetricsWithValidMetrics) {
    ert::metrics::Metrics metrics;
    // Should not throw
    EXPECT_NO_THROW(client_->enableMyMetrics(&metrics));
}

TEST_F(MyTrafficHttp2ClientUnitTest, EnableMetricsWithNull) {
    // Should not throw even with nullptr
    EXPECT_NO_THROW(client_->enableMyMetrics(nullptr));
}

} // namespace test
} // namespace h2agent
