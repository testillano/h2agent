#include <MockServerKeyEvent.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockServerKeyEvent_test : public ::testing::Test
{
public:
    h2agent::model::MockServerKeyEvent data_{};

    // Example
    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::string response_body_;

    MockServerKeyEvent_test() {
        // Example
        previous_state_ = "previous-state";
        state_ = "state";
        request_headers_.emplace("request-header1", nghttp2::asio_http2::header_value{"req-h1"});
        request_headers_.emplace("request-header2", nghttp2::asio_http2::header_value{"req-h2"});
        response_headers_.emplace("response-header1", nghttp2::asio_http2::header_value{"res-h1"});
        response_headers_.emplace("response-header2", nghttp2::asio_http2::header_value{"res-h2"});
        request_body_ = "{\"foo\":1}";
        response_body_ = "{\"bar\":2}";

        data_.load(previous_state_, state_, request_headers_, request_body_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, "POST", "/the/uri");
    }
};

TEST_F(MockServerKeyEvent_test, load)
{
    EXPECT_EQ(data_.getState(), state_);
}

TEST_F(MockServerKeyEvent_test, getHeaders)
{
    for(auto suffix: {
                "1", "2"
            }) {
        auto it = data_.getHeaders().find(std::string("request-header") + suffix);
        EXPECT_TRUE(it != data_.getHeaders().end());
        EXPECT_EQ(it->second.value, std::string("req-h") + suffix);
    }
}

TEST_F(MockServerKeyEvent_test, getBody)
{
    EXPECT_EQ(data_.getBody(), request_body_);
}

TEST_F(MockServerKeyEvent_test, getJson)
{
    std::stringstream ss;

    nlohmann::json eventJson{};
    eventJson["previousState"] = previous_state_;
    eventJson["receptionTimestampMs"] = data_.getJson()["receptionTimestampMs"]; // unpredictable
    eventJson["requestBody"] = nlohmann::json::parse(request_body_);
    eventJson["requestHeaders"] = nlohmann::json::parse("{\"request-header1\":\"req-h1\",\"request-header2\":\"req-h2\"}");
    eventJson["responseBody"] = nlohmann::json::parse(response_body_);
    eventJson["responseDelayMs"] = 20;
    eventJson["responseHeaders"] = nlohmann::json::parse("{\"response-header1\":\"res-h1\",\"response-header2\":\"res-h2\"}");
    eventJson["responseStatusCode"] = 201;
    eventJson["serverSequence"] = 111;
    eventJson["state"] = state_;
    eventJson["virtualOrigin"] = nlohmann::json::parse("{\"method\":\"POST\",\"uri\":\"/the/uri\"}");
    EXPECT_EQ(data_.getJson(), eventJson);
}

