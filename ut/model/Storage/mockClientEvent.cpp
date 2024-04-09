#include <MockClientEvent.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockClientEvent_test : public ::testing::Test
{
public:
    h2agent::model::MockClientEvent data_{};

    // Example
    std::string client_provision_id_;
    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::chrono::microseconds sending_timestamp_us_;
    std::chrono::microseconds reception_timestamp_us_;
    h2agent::model::DataPart response_body_;

    MockClientEvent_test() {
        // Example
        client_provision_id_ = "scenario1";
        previous_state_ = "previous-state";
        state_ = "state";
        request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        request_headers_.emplace("request-header1", nghttp2::asio_http2::header_value{"req-h1"});
        request_headers_.emplace("request-header2", nghttp2::asio_http2::header_value{"req-h2"});
        response_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        response_headers_.emplace("response-header1", nghttp2::asio_http2::header_value{"res-h1"});
        response_headers_.emplace("response-header2", nghttp2::asio_http2::header_value{"res-h2"});
        request_body_ = "{\"foo\":1}";
        sending_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        reception_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        response_body_.assign("{\"bar\":2}");
        response_body_.decode(response_headers_);

        data_.load(client_provision_id_, previous_state_, state_, sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */);
    }
};

TEST_F(MockClientEvent_test, Load)
{
    EXPECT_EQ(data_.getState(), state_);
}

TEST_F(MockClientEvent_test, GetResponseHeaders)
{
    for(auto suffix: {
                "1", "2"
            }) {
        auto it = data_.getResponseHeaders().find(std::string("response-header") + suffix);
        EXPECT_TRUE(it != data_.getResponseHeaders().end());
        EXPECT_EQ(it->second.value, std::string("res-h") + suffix);
    }

    auto it = data_.getResponseHeaders().find("content-type");
    EXPECT_TRUE(it != data_.getResponseHeaders().end());
    EXPECT_EQ(it->second.value, "application/json");
}

TEST_F(MockClientEvent_test, GetResponseBody)
{
    EXPECT_EQ(data_.getResponseBody(), response_body_.getJson());
}

TEST_F(MockClientEvent_test, GetJson)
{
    std::stringstream ss;

    nlohmann::json eventJson{};
    eventJson["clientProvisionId"] = client_provision_id_;
    eventJson["previousState"] = previous_state_;
    eventJson["sendingTimestampUs"] = data_.getJson()["sendingTimestampUs"]; // unpredictable
    eventJson["receptionTimestampUs"] = data_.getJson()["receptionTimestampUs"]; // unpredictable
    eventJson["requestBody"] = nlohmann::json::parse(request_body_);
    eventJson["requestHeaders"] = nlohmann::json::parse("{\"content-type\":\"application/json\",\"request-header1\":\"req-h1\",\"request-header2\":\"req-h2\"}");
    eventJson["responseBody"] = nlohmann::json::parse(response_body_.str());
    eventJson["requestDelayMs"] = 20;
    eventJson["timeoutMs"] = 2000;
    eventJson["responseHeaders"] = nlohmann::json::parse("{\"content-type\":\"application/json\",\"response-header1\":\"res-h1\",\"response-header2\":\"res-h2\"}");
    eventJson["responseStatusCode"] = 201;
    eventJson["clientSequence"] = 111;
    eventJson["sequence"] = 100;
    eventJson["state"] = state_;
    EXPECT_EQ(data_.getJson(), eventJson);
}

