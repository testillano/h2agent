#include <MockClientData.hpp>
#include <DataPart.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

class MockClientData_test : public ::testing::Test
{
public:
    h2agent::model::MockClientData data_{};

    std::string client_provision_id_;
    std::string previous_state_;
    std::string state_;
    nghttp2::asio_http2::header_map request_headers_, response_headers_;
    std::string request_body_;
    std::chrono::microseconds sending_timestamp_us_;
    std::chrono::microseconds reception_timestamp_us_;
    h2agent::model::DataPart response_body_data_part_;

    nlohmann::json real_event_;

    MockClientData_test() {
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

        // Two events per key, real and virtual for each, indexed by DELETE#/the/uri/111 and DELETE#/the/uri/222 respectively:
        // Client sequence/sequence will be ignored although being incoherent here (always 111):
        h2agent::model::DataKey key1("myClientEndpointId", "DELETE", "/the/uri/111");
        h2agent::model::DataKey key2("myClientEndpointId", "DELETE", "/the/uri/222");
        data_.loadEvent(key1, client_provision_id_, previous_state_, state_, sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);
        data_.loadEvent(key2, client_provision_id_, previous_state_, state_, sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);

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

TEST_F(MockClientData_test, ClearFails)
{
    bool somethingDeleted = false;
    bool success = false;

    // missing method: nothing with 'PUT':
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "PUT", "/the/uri", "1"));
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // missing uri: nothing with '/no/event/with/this/uri':
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "POST", "/no/event/with/this/uri", "1"));
    EXPECT_TRUE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong method/uri/number combinations ('checkSelection()' indirect testing):
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "POST", "", ""));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "", "/the/uri", ""));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "", "", "1"));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);

    // wrong string to number conversion ('string2uint64andSign()' indirect testing):
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "DELETE", "/the/uri/111", "invalid number"));
    EXPECT_FALSE(success);
    EXPECT_FALSE(somethingDeleted);
    success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "DELETE", "/the/uri/111", "")); // missing number
    EXPECT_TRUE(success);
    EXPECT_TRUE(somethingDeleted);
}

TEST_F(MockClientData_test, ClearEverything)
{
    bool somethingDeleted = false;
    bool success = data_.clear(somethingDeleted, h2agent::model::EventKey("", "", "", ""));
    EXPECT_TRUE(success);
    EXPECT_TRUE(somethingDeleted);
}

TEST_F(MockClientData_test, ClearSucceed)
{
    bool somethingDeleted = false;
    bool success = data_.clear(somethingDeleted, h2agent::model::EventKey("myClientEndpointId", "DELETE", "/the/uri/111", "1"));
    EXPECT_TRUE(success);
    EXPECT_TRUE(somethingDeleted);
}

TEST_F(MockClientData_test, AsJsonString)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/111", "1", ""), validQuery)); // normalize to have safer comparisons
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable
    std::uint64_t sendingTimestampUs = assertedJson["sendingTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = real_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;
    expectedJson["sendingTimestampUs"] = sendingTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
    EXPECT_TRUE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringBadKey)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("", "", "", "1", "/foo/bar"), validQuery));

    EXPECT_EQ(assertedJson.dump(), "[]");
    EXPECT_FALSE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringBadNumber)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/111", "invalid", ""), validQuery));

    EXPECT_EQ(assertedJson.dump(), "[]");
    EXPECT_FALSE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringNotFoundByKey)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/000", "", ""), validQuery));

    EXPECT_EQ(assertedJson.dump(), "[]");
    EXPECT_TRUE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringNotFoundByNumber)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/111", "30", ""), validQuery));

    EXPECT_EQ(assertedJson.dump(), "[]");
    EXPECT_TRUE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringNoNumber)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/111", "", ""), validQuery));

    //std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable
    //std::uint64_t sendingTimestampUs = assertedJson["sendingTimestampUs"]; // unpredictable

    //nlohmann::json expectedJson = real_event_;
    //expectedJson["receptionTimestampUs"] = receptionTimestampUs;
    //expectedJson["sendingTimestampUs"] = sendingTimestampUs;

    //EXPECT_EQ(assertedJson, expectedJson);
    //EXPECT_TRUE(validQuery);
}

TEST_F(MockClientData_test, AsJsonStringWithEventPath)
{
    bool validQuery = false;
    nlohmann::json assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("myClientEndpointId", "DELETE", "/the/uri/111", "1", "/requestBody"), validQuery)); // normalize to have safer comparisons

    nlohmann::json::json_pointer p("/requestBody");
    nlohmann::json expectedJson = real_event_[p];

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockClientData_test, Summary)
{
    nlohmann::json assertedJson = nlohmann::json::parse(data_.summary()); // normalize to have safer comparisons
    nlohmann::json expectedJson = R"(
    {
      "displayedKeys": {
        "amount": 2,
        "list": [
          {
            "amount": 1,
            "clientEndpointId": "myClientEndpointId",
            "method": "DELETE",
            "uri": "/the/uri/222"
          },
          {
            "amount": 1,
            "clientEndpointId": "myClientEndpointId",
            "method": "DELETE",
            "uri": "/the/uri/111"
          }
        ]
      },
      "totalEvents": 2,
      "totalKeys": 2
    }
    )"_json;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockClientData_test, GetMockClientEvent)
{
    nlohmann::json assertedJson = data_.getEvent(h2agent::model::EventKey("myClientEndpointId", "DELETE", "/the/uri/222", "-1"))->getJson(); // last for uri '/the/uri/222'
    std::uint64_t receptionTimestampUs = assertedJson["receptionTimestampUs"]; // unpredictable
    std::uint64_t sendingTimestampUs = assertedJson["sendingTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = real_event_;
    expectedJson["receptionTimestampUs"] = receptionTimestampUs;
    expectedJson["sendingTimestampUs"] = sendingTimestampUs;

    EXPECT_EQ(assertedJson, expectedJson);
}

TEST_F(MockClientData_test, GetMockClientEventIncompleteKey)
{
    auto ptr = data_.getEvent(h2agent::model::EventKey("myClientEndpointId", "DELETE", "", ""));
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(MockClientData_test, GetMockClientEventInvalidNumber)
{
    auto ptr = data_.getEvent(h2agent::model::EventKey("myClientEndpointId", "DELETE", "/the/uri/222", "invalid"));
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(MockClientData_test, GetJsonOrAllAsJsonString)
{
    nlohmann::json assertedJson = data_.getJson();
    std::uint64_t receptionTimestampUs_0_0 = assertedJson[0]["events"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t sendingTimestampUs_0_0 = assertedJson[0]["events"][0]["sendingTimestampUs"]; // unpredictable
    std::uint64_t receptionTimestampUs_1_0 = assertedJson[1]["events"][0]["receptionTimestampUs"]; // unpredictable
    std::uint64_t sendingTimestampUs_1_0 = assertedJson[1]["events"][0]["sendingTimestampUs"]; // unpredictable

    nlohmann::json expectedJson = R"(
    [
      {
        "clientEndpointId": "myClientEndpointId",
        "method": "DELETE",
        "events": [
        ],
        "uri": "/the/uri/222"
      },
      {
        "clientEndpointId": "myClientEndpointId",
        "method": "DELETE",
        "events": [
        ],
        "uri": "/the/uri/111"
      }
    ]
    )"_json;

    // Fix unpredictable timestamps:
    real_event_["receptionTimestampUs"] = receptionTimestampUs_0_0;
    real_event_["sendingTimestampUs"] = sendingTimestampUs_0_0;
    expectedJson[0]["events"].push_back(real_event_);

    real_event_["receptionTimestampUs"] = receptionTimestampUs_1_0;
    real_event_["sendingTimestampUs"] = sendingTimestampUs_1_0;
    expectedJson[1]["events"].push_back(real_event_);

    EXPECT_EQ(assertedJson, expectedJson);

    bool validQuery = false;
    assertedJson = nlohmann::json::parse(data_.asJsonString(h2agent::model::EventLocationKey("", "", "", "", ""), validQuery));
    EXPECT_EQ(assertedJson, expectedJson);
    EXPECT_TRUE(validQuery);
}

TEST_F(MockClientData_test, FindLastRegisteredRequestState)
{
    h2agent::model::DataKey key("myClientEndpointId", "PUT", "/the/put/uri");
    data_.loadEvent(key, client_provision_id_, previous_state_, "most_recent_state", sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, true /* history */);

    std::string latestState;
    data_.findLastRegisteredRequestState(key, latestState);
    EXPECT_EQ(latestState, "most_recent_state");

    data_.findLastRegisteredRequestState(h2agent::model::DataKey("myClientEndpointId", "DELETE", "/the/uri/222"), latestState);
    EXPECT_EQ(latestState, "state");
}

TEST_F(MockClientData_test, OverwriteLastEvent)
{
    h2agent::model::DataKey key("myClientEndpointId", "PUT", "/the/put/uri");
    data_.loadEvent(key, client_provision_id_, previous_state_, "last_recent_state", sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 22 /* request delay ms */, 2222 /* timestamp ms */, true /* history */);
    data_.loadEvent(key, client_provision_id_, previous_state_, "most_recent_state", sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 20 /* request delay ms */, 2000 /* timestamp ms */, false /* FALSE HISTORY */);

    std::string latestState;
    data_.findLastRegisteredRequestState(key, latestState);
    EXPECT_EQ(latestState, "most_recent_state");
}

TEST_F(MockClientData_test, DefaultStateInitial)
{
    h2agent::model::DataKey key("myClientEndpointId", "PUT", "/the/put/uri");
    data_.loadEvent(key, client_provision_id_, previous_state_, "", sending_timestamp_us_, reception_timestamp_us_, 201, request_headers_, response_headers_, request_body_, response_body_data_part_, 111 /* client sequence */, 100 /* sequence */, 22 /* request delay ms */, 2222 /* timestamp ms */, true /* history */);

    std::string latestState;
    data_.findLastRegisteredRequestState(key, latestState);
    EXPECT_EQ(latestState, "initial");
}

