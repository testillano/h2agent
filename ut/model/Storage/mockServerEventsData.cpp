#include <MockServerEventsData.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

class MockServerEventsData_test : public ::testing::Test
{
public:
    h2agent::model::MockServerEventsData data_{};

    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::chrono::microseconds reception_timestamp_us_;
    std::string response_body_;

    nlohmann::json real_event_;
    nlohmann::json virtual_event_;

    MockServerEventsData_test() {
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

        // Two events per key, real and virtual for each, indexed by DELETE#/the/uri/111 and DELETE#/the/uri/222 respectively:
        // Server sequence will be ignored although being incoherent here (always 111):
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/111", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */);
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/111", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual");
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */);
        data_.loadRequest(previous_state_, state_, "DELETE", "/the/uri/222", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */, "POST", "/the/uri/which/causes/virtual");

        real_event_ = R"(
        {
          "previousState": "previous-state",
          "receptionTimestampUs": 1653827182451878,
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
          "serverSequence":111,
          "state": "state"
        }
        )"_json;

        virtual_event_ = real_event_;
        virtual_event_["virtualOrigin"] = R"({"method":"POST","uri": "/the/uri/which/causes/virtual"})"_json;
    }
};

/*
TEST_F(MockServerEventsData_test, string2uint64andSign)
{
    std::uint64_t output = 0;
    bool negative = false;

    EXPECT_FALSE(data_.string2uint64andSign("bad_input", output, negative);
}
*/

TEST_F(MockServerEventsData_test, clearFails)
{
    bool somethingDeleted = false;
    bool success = false;

    // missing method: nothing with 'PUT':
    success = data_.clear(somethingDeleted, "PUT", "/the/uri", "1");
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // missing uri: nothing with '/no/event/with/this/uri':
    success = data_.clear(somethingDeleted, "POST", "/no/event/with/this/uri", "1");
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong method/uri/number combinations ('checkSelection()' indirect testing):
    success = data_.clear(somethingDeleted, "POST", "", "");
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, "", "/the/uri", "");
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, "", "", "1");
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong string to number conversion ('string2uint64andSign()' indirect testing):
    success = data_.clear(somethingDeleted, "DELETE", "/the/uri/111", "invalid number");
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, "DELETE", "/the/uri/111", "-invalid number");
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong input: method/uri/number empty
    success = data_.clear(somethingDeleted, "", "", "");
    EXPECT_TRUE(success);
    EXPECT_TRUE(somethingDeleted);
}

TEST_F(MockServerEventsData_test, asJsonString)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString("DELETE", "/the/uri/111", "1", validQuery)); // normalize to have safer comparisons
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = real_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerEventsData_test, summary)
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

TEST_F(MockServerEventsData_test, getMockServerKeyEvent)
{
    nlohmann::json assertedJson = data_.getMockServerKeyEvent("DELETE", "/the/uri/222", "-1")->getJson(); // last for uri '/the/uri/222'
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = virtual_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerEventsData_test, getJson)
{
    nlohmann::json assertedJson = data_.getJson();
    std::uint64_t receptionTimestampUs_0_0 = assertedJson[0]["requests"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_0_1 = assertedJson[0]["requests"][1]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_1_0 = assertedJson[1]["requests"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_1_1 = assertedJson[1]["requests"][1]["receptionTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = R"(
    [
      {
        "method": "DELETE",
        "requests": [
        ],
        "uri": "/the/uri/222"
      },
      {
        "method": "DELETE",
        "requests": [
        ],
        "uri": "/the/uri/111"
      }
    ]
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = receptionTimestampUs_0_0;
    virtual_event_["receptionTimestampUs"] = receptionTimestampUs_0_1;
    expectedJson[0]["requests"].push_back(real_event_);
    expectedJson[0]["requests"].push_back(virtual_event_);

    real_event_["receptionTimestampUs"] = receptionTimestampUs_1_0;
    virtual_event_["receptionTimestampUs"] = receptionTimestampUs_1_1;
    expectedJson[1]["requests"].push_back(real_event_);
    expectedJson[1]["requests"].push_back(virtual_event_);

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockServerEventsData_test, findLastRegisteredRequestState)
{
    data_.loadRequest(previous_state_, "most_recent_state", "PUT", "/the/put/uri", request_headers_, request_body_, reception_timestamp_us_, 201, response_headers_, response_body_, 111 /* server sequence */, 20 /* reponse delay ms */, true /* history */);

    std::string latestState;
    data_.findLastRegisteredRequestState("PUT", "/the/put/uri", latestState);
    EXPECT_EQ(latestState, "most_recent_state");

    data_.findLastRegisteredRequestState("DELETE", "/the/uri/222", latestState);
    EXPECT_EQ(latestState, "state");
}

TEST_F(MockServerEventsData_test, loadRequestsSchema)
{
    nlohmann::json schema = R"(
    {
      "$schema": "http://json-schema.org/draft-07/schema#",

      "type": "object",
      "additionalProperties": false,
      "patternProperties": {
        "^.*$": {
          "anyOf": [
            {
              "type": "string"
            }
          ]
        }
      }
    }
    )"_json;

    EXPECT_TRUE(data_.loadRequestsSchema(schema));

    nlohmann::json invalidSchema{};
    EXPECT_FALSE(data_.loadRequestsSchema(invalidSchema));
}
