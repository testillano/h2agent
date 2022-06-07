#include <MockServerKeyEvents.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockServerKeyEvents_test : public ::testing::Test
{
public:
    h2agent::model::MockServerKeyEvents data_{};

    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::chrono::microseconds reception_timestamp_us_;
    std::string response_body_;

    nlohmann::json real_event_;
    nlohmann::json virtual_event1_;
    nlohmann::json virtual_event2_;

    MockServerKeyEvents_test() {
        // Example
        previous_state_ = "previous-state";
        state_ = "state";
        request_headers_.emplace("request-header1", nghttp2::asio_http2::header_value{"req-h1"});
        request_headers_.emplace("request-header2", nghttp2::asio_http2::header_value{"req-h2"});
        response_headers_.emplace("response-header1", nghttp2::asio_http2::header_value{"res-h1"});
        response_headers_.emplace("response-header2", nghttp2::asio_http2::header_value{"res-h2"});
        request_body_ = "{\"foo\":1}";
        reception_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        response_body_ = "{\"bar\":2}";

        // Three events, one real and two virtual, indexed by DELETE#/the/uri/222
        // Server sequence will be ignored although being incoherent here (always 111):
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */);
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual1");
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual2");

        real_event_ = R"(
        {
          "previousState": "previous-state",
          "receptionTimestampUs": 1653827182451112,
          "requestBody": {
            "foo": 1
          },
          "requestHeaders": {
            "request-header1": "req-h1",
            "request-header2": "req-h2"
          },
          "responseBody": {
            "bar": 2
          },
          "responseDelayMs": 20,
          "responseHeaders": {
            "response-header1": "res-h1",
            "response-header2": "res-h2"
          },
          "responseStatusCode": 201,
          "serverSequence": 111,
          "state": "state"
        }
        )"_json;

        virtual_event1_ = real_event_;
        virtual_event1_["virtualOrigin"] = R"({"method":"POST","uri": "/the/uri/which/causes/virtual1"})"_json;
        virtual_event2_ = real_event_;
        virtual_event2_["virtualOrigin"] = R"({"method":"POST","uri": "/the/uri/which/causes/virtual2"})"_json;
    }
};

TEST_F(MockServerKeyEvents_test, calculateMockServerKeyEventsKey)
{
    h2agent::model::mock_server_events_key_t key;
    h2agent::model::calculateMockServerKeyEventsKey(key, "POST", "/the/uri");

    EXPECT_EQ(key, "POST#/the/uri");
}


TEST_F(MockServerKeyEvents_test, sizeKeyMethodAndUri)
{
    EXPECT_EQ(data_.size(), 3);
    EXPECT_EQ(data_.getKey(), "DELETE#/the/uri/222");
    EXPECT_EQ(data_.getMethod(), "DELETE");
    EXPECT_EQ(data_.getUri(), "/the/uri/222");
}

TEST_F(MockServerKeyEvents_test, removeVirtual1)
{
    // Remove the second (real, virtual1, virtual2 -> real, virtual2):
    data_.removeMockServerKeyEvent(2, false);

    // Check content:
    EXPECT_EQ(data_.size(), 2);

    nlohmann::json assertedJson = data_.getJson();
    nlohmann::json expectedJson = R"(
    {
      "method": "DELETE",
      "requests": [
      ],
      "uri": "/the/uri/222"
    }
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = assertedJson["requests"][0]["receptionTimestampUs"];
    virtual_event2_["receptionTimestampUs"] = assertedJson["requests"][1]["receptionTimestampUs"];

    expectedJson["requests"].push_back(real_event_);
    expectedJson["requests"].push_back(virtual_event2_);

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerKeyEvents_test, getLastRegisteredRequestState)
{
    // Add real event (now it will be in fourth position), but with another state:
    data_.loadRequest(previous_state_, "latest_state", "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */);

    // Check content (skip whole comparison):
    EXPECT_EQ(data_.size(), 4);
    EXPECT_EQ(data_.getLastRegisteredRequestState(), "latest_state");
}

TEST_F(MockServerKeyEvents_test, removeFirstUsingReverse)
{
    // Remove the first from tail (real, virtual1, virtual2 -> virtual1, virtual2):
    size_t currentSize = data_.size();
    data_.removeMockServerKeyEvent(currentSize, true);

    // Check content
    EXPECT_EQ(data_.size(), currentSize - 1);

    nlohmann::json assertedJson = data_.getJson();
    nlohmann::json expectedJson = R"(
    {
      "method": "DELETE",
      "requests": [
      ],
      "uri": "/the/uri/222"
    }
    )"_json;

    // Fix unpredictable timestamps:
    virtual_event1_["receptionTimestampUs"] = assertedJson["requests"][0]["receptionTimestampUs"];
    virtual_event2_["receptionTimestampUs"] = assertedJson["requests"][1]["receptionTimestampUs"];

    expectedJson["requests"].push_back(virtual_event1_);
    expectedJson["requests"].push_back(virtual_event2_);

    EXPECT_EQ(assertedJson, expectedJson);
}

