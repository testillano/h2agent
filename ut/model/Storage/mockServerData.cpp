#include <MockServerData.hpp>
#include <DataPart.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

class MockServerData_test : public ::testing::Test
{
public:
    h2agent::model::MockServerData data_{};

    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    h2agent::model::DataPart request_body_data_part_;
    std::chrono::microseconds reception_timestamp_us_;
    std::string response_body_;

    nlohmann::json real_event_;
    nlohmann::json virtual_event_;

    MockServerData_test() {
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

        // Two events per key, real and virtual for each, indexed by DELETE#/the/uri/111 and DELETE#/the/uri/222 respectively:
        // Server sequence will be ignored although being incoherent here (always 111):
        h2agent::model::DataKey key1("DELETE", "/the/uri/111");
        h2agent::model::DataKey key2("DELETE", "/the/uri/222");
        data_.loadEvent(key1, previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);
        data_.loadEvent(key1, previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual");
        data_.loadEvent(key2, previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);
        data_.loadEvent(key2, previous_state_, state_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual");

        real_event_ = R"(
        {
          "previousState": "previous-state",
          "receptionTimestampUs": 1653827182451878,
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
          "serverSequence":111,
          "state": "state"
        }
        )"_json;

        virtual_event_ = real_event_;
        virtual_event_["virtualOrigin"] = R"({"method":"POST","uri": "/the/uri/which/causes/virtual"})"_json;
    }
};

TEST_F(MockServerData_test, ClearFails)
{
    bool somethingDeleted = false;
    bool success = false;

    // missing method: nothing with 'PUT':
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("PUT", "/the/uri", "1"));
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // missing uri: nothing with '/no/event/with/this/uri':
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("POST", "/no/event/with/this/uri", "1"));
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong method/uri/number combinations ('checkSelection()' indirect testing):
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("POST", "", ""));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("", "/the/uri", ""));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("", "", "1"));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong string to number conversion ('string2uint64andSign()' indirect testing):
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("DELETE", "/the/uri/111", "invalid number"));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("DELETE", "/the/uri/111", "-invalid number"));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong input: method/uri/number empty
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("", "", ""));
    EXPECT_TRUE(success);
    EXPECT_TRUE(somethingDeleted);
}

TEST_F(MockServerData_test, AsJsonString)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("DELETE", "/the/uri/111", "1", ""), validQuery)); // normalize to have safer comparisons
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = real_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerData_test, AsJsonStringWithEventPath)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("DELETE", "/the/uri/111", "1", "/requestBody"), validQuery)); // normalize to have safer comparisons

    nlohmann::json::json_pointer p("/requestBody");
    nlohmann::json expectedJson = real_event_[p];

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerData_test, Summary)
{
    nlohmann::json assertedJson = nlohmann::json::parse(data_.summary()); // normalize to have safer comparisons
    nlohmann::json expectedJson = R"(
    {
      "displayedKeys": {
        "amount": 2,
        "list": [
          {
            "amount": 2,
            "method": "DELETE",
            "uri": "/the/uri/222"
          },
          {
            "amount": 2,
            "method": "DELETE",
            "uri": "/the/uri/111"
          }
        ]
      },
      "totalEvents": 4,
      "totalKeys": 2
    }
    )"_json;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerData_test, GetMockServerEvent)
{
    nlohmann::json assertedJson = data_.getEvent(h2agent::model::EventKey("DELETE", "/the/uri/222", "-1"))->getJson(); // last for uri '/the/uri/222'
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = virtual_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerData_test, GetJson)
{
    nlohmann::json assertedJson = data_.getJson();
    std::uint64_t receptionTimestampUs_0_0 = assertedJson[0]["events"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_0_1 = assertedJson[0]["events"][1]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_1_0 = assertedJson[1]["events"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_1_1 = assertedJson[1]["events"][1]["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = R"(
    [
      {
        "method": "DELETE",
        "events": [
        ],
        "uri": "/the/uri/222"
      },
      {
        "method": "DELETE",
        "events": [
        ],
        "uri": "/the/uri/111"
      }
    ]
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = receptionTimestampUs_0_0;
    virtual_event_["receptionTimestampUs"] = receptionTimestampUs_0_1;
    expectedJson[0]["events"].push_back(real_event_);
    expectedJson[0]["events"].push_back(virtual_event_);

    real_event_["receptionTimestampUs"] = receptionTimestampUs_1_0;
    virtual_event_["receptionTimestampUs"] = receptionTimestampUs_1_1;
    expectedJson[1]["events"].push_back(real_event_);
    expectedJson[1]["events"].push_back(virtual_event_);

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerData_test, FindLastRegisteredRequestState)
{
    h2agent::model::DataKey key("PUT", "/the/put/uri");
    data_.loadEvent(key, previous_state_, "most_recent_state", reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_data_part_, response_body_, 111 /* server sequence */, 20 /* response delay ms */, true /* history */);

    std::string latestState;
    data_.findLastRegisteredRequestState(key, latestState);
    EXPECT_EQ(latestState, "most_recent_state");

    data_.findLastRegisteredRequestState(h2agent::model::DataKey("DELETE", "/the/uri/222"), latestState);
    EXPECT_EQ(latestState, "state");
}

