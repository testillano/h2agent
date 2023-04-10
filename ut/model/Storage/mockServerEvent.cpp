#include <MockServerEvent.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockServerEvent_test : public ::testing::Test
{
public:
    h2agent::model::MockServerEvent data_{};

    // Example
    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    h2agent::model::DataPart request_body_;
    std::chrono::microseconds reception_timestamp_us_;
    std::string response_body_;

    MockServerEvent_test() {
        // Example
        previous_state_ = "previous-state";
        state_ = "state";
        request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        request_headers_.emplace("request-header1", nghttp2::asio_http2::header_value{"req-h1"});
        request_headers_.emplace("request-header2", nghttp2::asio_http2::header_value{"req-h2"});
        response_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        response_headers_.emplace("response-header1", nghttp2::asio_http2::header_value{"res-h1"});
        response_headers_.emplace("response-header2", nghttp2::asio_http2::header_value{"res-h2"});
        request_body_.assign("{\"foo\":1}");
        request_body_.decode(request_headers_);
        reception_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        response_body_ = "{\"bar\":2}";

        data_.load(previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, "POST", "/the/uri");
    }
};

TEST_F(MockServerEvent_test, Load)
{
    EXPECT_EQ(data_.getState(), state_);
}

TEST_F(MockServerEvent_test, GetRequestHeaders)
{
    for(auto suffix: {
                "1", "2"
            }) {
        auto it = data_.getRequestHeaders().find(std::string("request-header") + suffix);
        EXPECT_TRUE(it != data_.getRequestHeaders().end());
        EXPECT_EQ(it->second.value, std::string("req-h") + suffix);
    }

    auto it = data_.getRequestHeaders().find("content-type");
    EXPECT_TRUE(it != data_.getRequestHeaders().end());
    EXPECT_EQ(it->second.value, "application/json");
}

TEST_F(MockServerEvent_test, GetRequestBody)
{
    EXPECT_EQ(data_.getRequestBody(), request_body_.getJson());
}

TEST_F(MockServerEvent_test, GetJson)
{
    std::stringstream ss;

    nlohmann::json eventJson{};
    eventJson["previousState"] = previous_state_;
    eventJson["receptionTimestampUs"] = data_.getJson()["receptionTimestampUs"]; // unpredictable
    eventJson["requestBody"] = nlohmann::json::parse(request_body_.str());
    eventJson["requestHeaders"] = nlohmann::json::parse("{\"content-type\":\"application/json\",\"request-header1\":\"req-h1\",\"request-header2\":\"req-h2\"}");
    eventJson["responseBody"] = nlohmann::json::parse(response_body_);
    eventJson["responseDelayMs"] = 20;
    eventJson["responseHeaders"] = nlohmann::json::parse("{\"content-type\":\"application/json\",\"response-header1\":\"res-h1\",\"response-header2\":\"res-h2\"}");
    eventJson["responseStatusCode"] = 201;
    eventJson["serverSequence"] = 111;
    eventJson["state"] = state_;
    eventJson["virtualOrigin"] = nlohmann::json::parse("{\"method\":\"POST\",\"uri\":\"/the/uri\"}");
    EXPECT_EQ(data_.getJson(), eventJson);
}

