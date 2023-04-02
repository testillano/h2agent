#include <MockClientEventsHistory.hpp>
#include <DataPart.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockClientEventsHistory_test : public ::testing::Test
{
public:
    h2agent::model::MockClientEventsHistory data_;

    std::string client_provision_id_;
    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::chrono::microseconds reception_timestamp_us_;
    std::chrono::microseconds sending_timestamp_us_;
    h2agent::model::DataPart response_body_data_part_;

    nlohmann::json real_event_;

    MockClientEventsHistory_test() : data_(h2agent::model::DataKey("myClientEndpoint", "DELETE", "/the/uri/222")) {
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
        response_body_data_part_.assign("{\"bar\":2}");

        // Two equal events indexed by myClientEndpoint#DELETE#/the/uri/222
        // Client sequence & sequence will be ignored although being incoherent here (always 111, 100):
        data_.loadEvent(client_provision_id_, previous_state_, state_, sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);
        data_.loadEvent(client_provision_id_, previous_state_, state_, sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);

        real_event_ = R"(
        {
          "clientProvisionId": "scenario1",
          "previousState": "previous-state",
          "sendingTimestampUs": 1653827182451000,
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
          "requestDelayMs": 20,
          "timeoutMs": 2000,
          "responseHeaders": {
            "content-type": "application/json",
            "response-header1": "res-h1",
            "response-header2": "res-h2"
          },
          "responseStatusCode": 201,
          "clientSequence": 111,
          "sequence": 100,
          "state": "state"
        }
        )"_json;
    }
};

TEST_F(MockClientEventsHistory_test, SizeKeyMethodAndUri)
{
    EXPECT_EQ(data_.size(), 2);
    EXPECT_EQ(data_.getKey().getKey(), "myClientEndpoint#DELETE#/the/uri/222");
    EXPECT_EQ(data_.getKey().getClientEndpointId(), "myClientEndpoint");
    EXPECT_EQ(data_.getKey().getMethod(), "DELETE");
    EXPECT_EQ(data_.getKey().getUri(), "/the/uri/222");
}

TEST_F(MockClientEventsHistory_test, GetLastRegisteredRequestState)
{
    // Add real event (now it will be in second position), but with another state:
    data_.loadEvent(client_provision_id_, previous_state_, "latest_state", sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);

    // Check content (skip whole comparison):
    EXPECT_EQ(data_.size(), 3);
    EXPECT_EQ(data_.getLastRegisteredRequestState(), "latest_state");
}

TEST_F(MockClientEventsHistory_test, RemoveFirstUsingReverse)
{
    // Remove the first from tail:
    size_t currentSize = data_.size();
    data_.removeEvent(currentSize, true);

    // Check content
    EXPECT_EQ(data_.size(), currentSize - 1);

    nlohmann::json assertedJson = data_.getJson();
    nlohmann::json expectedJson = R"(
    {
      "clientEndpointId": "myClientEndpoint",
      "method": "DELETE",
      "events": [
      ],
      "uri": "/the/uri/222"
    }
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = assertedJson["events"][0]["receptionTimestampUs"];
    real_event_["sendingTimestampUs"] = assertedJson["events"][0]["sendingTimestampUs"];

    expectedJson["events"].push_back(real_event_);

    EXPECT_EQ(assertedJson, expectedJson);
}

