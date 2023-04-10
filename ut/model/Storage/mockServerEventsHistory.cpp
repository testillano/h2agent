#include <MockServerEventsHistory.hpp>
#include <DataPart.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockServerEventsHistory_test : public ::testing::Test
{
public:
    h2agent::model::MockServerEventsHistory data_;

    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    h2agent::model::DataPart request_body_data_part_;
    std::chrono::microseconds reception_timestamp_us_;
    std::string response_body_;

    nlohmann::json real_event_;
    nlohmann::json virtual_event1_;
    nlohmann::json virtual_event2_;

    MockServerEventsHistory_test() : data_(h2agent::model::DataKey("DELETE", "/the/uri/222")) {

        // Example
        previous_state_ = "previous-state";
        state_ = "state";
        request_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        request_headers_.emplace("request-header1", nghttp2::asio_http2::header_value{"req-h1"});
        request_headers_.emplace("request-header2", nghttp2::asio_http2::header_value{"req-h2"});
        response_headers_.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
        response_headers_.emplace("response-header1", nghttp2::asio_http2::header_value{"res-h1"});
        response_headers_.emplace("response-header2", nghttp2::asio_http2::header_value{"res-h2"});
        request_body_data_part_.assign("{\"foo\":1}");
        reception_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        response_body_ = "{\"bar\":2}";

        // Three events, one real and two virtual, indexed by DELETE#/the/uri/222
        // Server sequence will be ignored although being incoherent here (always 111):
        data_.loadEvent(previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);
        data_.loadEvent(previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual1");
        data_.loadEvent(previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual2");

        real_event_ = R"(
        {
          "previousState": "previous-state",
          "receptionTimestampUs": 1653827182451112,
          "requestBody": {
            "foo": 1
          },
          "requestHeaders": {
            "content-type": "application/json",
            "request-header1": "req-h1",
            "request-header2": "req-h2"
          },
          "responseBody": {
            "bar": 2
          },
          "responseDelayMs": 20,
          "responseHeaders": {
            "content-type": "application/json",
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

TEST_F(MockServerEventsHistory_test, SizeKeyMethodAndUri)
{
    EXPECT_EQ(data_.size(), 3);
    EXPECT_EQ(data_.getKey().getKey(), "DELETE#/the/uri/222");
    EXPECT_EQ(data_.getKey().getMethod(), "DELETE");
    EXPECT_EQ(data_.getKey().getUri(), "/the/uri/222");
}

TEST_F(MockServerEventsHistory_test, RemoveVirtual1)
{
    // Remove the second (real, virtual1, virtual2 -> real, virtual2):
    data_.removeEvent(2, false);

    // Check content:
    EXPECT_EQ(data_.size(), 2);

    nlohmann::json assertedJson = data_.getJson();
    nlohmann::json expectedJson = R"(
    {
      "method": "DELETE",
      "events": [
      ],
      "uri": "/the/uri/222"
    }
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = assertedJson["events"][0]["receptionTimestampUs"];
    virtual_event2_["receptionTimestampUs"] = assertedJson["events"][1]["receptionTimestampUs"];

    expectedJson["events"].push_back(real_event_);
    expectedJson["events"].push_back(virtual_event2_);

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerEventsHistory_test, GetLastRegisteredRequestState)
{
    // Add real event (now it will be in fourth position), but with another state:
    data_.loadEvent(previous_state_, "latest_state", reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);

    // Check content (skip whole comparison):
    EXPECT_EQ(data_.size(), 4);
    EXPECT_EQ(data_.getLastRegisteredRequestState(), "latest_state");
}

TEST_F(MockServerEventsHistory_test, RemoveFirstUsingReverse)
{
    // Remove the first from tail (real, virtual1, virtual2 -> virtual1, virtual2):
    size_t currentSize = data_.size();
    data_.removeEvent(currentSize, true);

    // Check content
    EXPECT_EQ(data_.size(), currentSize - 1);

    nlohmann::json assertedJson = data_.getJson();
    nlohmann::json expectedJson = R"(
    {
      "method": "DELETE",
      "events": [
      ],
      "uri": "/the/uri/222"
    }
    )"_json;

    // Fix unpredictable timestamps:
    virtual_event1_["receptionTimestampUs"] = assertedJson["events"][0]["receptionTimestampUs"];
    virtual_event2_["receptionTimestampUs"] = assertedJson["events"][1]["receptionTimestampUs"];

    expectedJson["events"].push_back(virtual_event1_);
    expectedJson["events"].push_back(virtual_event2_);

    EXPECT_EQ(assertedJson, expectedJson);
}

