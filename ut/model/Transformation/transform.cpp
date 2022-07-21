#include <iostream>
#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functions.hpp>

#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>
#include <AdminSchemas.hpp>
#include <MockServerEventsData.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>

#include <ert/http2comm/Http2Headers.hpp>


// Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({ "algorithm": "FullMatching" })"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({ "algorithm": "FullMatchingRegexReplace", "rgx":"([0-9]{3})-([a-z]{2})-foo-bar", "fmt":"$1"})"_json;
const nlohmann::json MatchingConfiguration_RegexMatching__Success = R"({ "algorithm": "RegexMatching" })"_json;
//const nlohmann::json MatchingConfiguration_uriPathQueryParameters__Success4 = R"({ "algorithm": "FullMatching", "uriPathQueryParameters":{"filter":"Ignore"} })"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json ProvisionConfiguration_Sources = R"delim(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar/1?name=test",
  "responseCode": 200,
  "responseBody": {
    "foo": "bar-1",
    "remove-me": 0
  },
  "responseHeaders": {
    "content-type": "text/html",
    "x-version": "1.0.0"
  },
  "transform": [
    {
      "source": "request.uri",
      "target": "response.body.json.string./requestUri"
    },
    {
      "source": "request.uri.path",
      "target": "response.body.json.string./requestUriPath"
    },
    {
      "source": "request.uri.param.name",
      "target": "response.body.json.string./requestUriQueryParam"
    },
    {
      "source": "request.body",
      "target": "response.body.json.object./requestBody"
    },
    {
      "source": "request.body./missing/path",
      "target": "response.body.json.object./notprocessed"
    },
    {
      "source": "response.body",
      "target": "response.body.json.object./responseBody"
    },
    {
      "source": "response.body./missing/path",
      "target": "response.body.json.object./notprocessed"
    },
    {
      "source": "request.header.x-version",
      "target": "response.body.json.object./requestHeader"
    },
    {
      "source": "request.header.missing-header",
      "target": "response.body.json.object./notprocessed"
    },
    {
      "source": "eraser",
      "target": "response.body.json.object./responseBody/remove-me"
    },
    {
      "source": "random.20.30",
      "target": "response.body.json.string./random"
    },
    {
      "source": "randomset.rock|paper|scissors",
      "target": "response.body.json.string./randomset"
    },
    {
      "source": "timestamp.s",
      "target": "response.body.json.string./unix_s"
    },
    {
      "source": "timestamp.ms",
      "target": "response.body.json.string./unix_ms"
    },
    {
      "source": "timestamp.us",
      "target": "response.body.json.string./unix_us"
    },
    {
      "source": "timestamp.ns",
      "target": "response.body.json.string./unix_ns"
    },
    {
      "source": "strftime.Now it's %I:%M%p.",
      "target": "response.body.json.string./strftime"
    },
    {
      "source": "recvseq",
      "target": "response.body.json.integer./recvseq"
    },
    {
      "source": "value.myvarvalue",
      "target": "var.myvar"
    },
    {
      "source": "var.myvar",
      "target": "response.body.json.string./myvar"
    },
    {
      "source": "var.missing-var",
      "target": "response.body.json.string./notprocessed"
    },
    {
      "source": "inState",
      "target": "response.body.json.string./instate"
    },
    {
      "source": "value.myglobalvarvalue",
      "target": "globalVar.myglobalvar"
    },
    {
      "source": "globalVar.myglobalvar",
      "target": "response.body.json.string./myglobalvar"
    },
    {
      "source": "eraser",
      "target": "globalVar.myglobalvar"
    },
    { "source": "value.POST", "target": "var.persistEvent.method" },
    { "source": "value./app/v1/foo/bar/1", "target": "var.persistEvent.uri" },
    { "source": "value.-1", "target": "var.persistEvent.number" },
    { "source": "value./body/node1/node2", "target": "var.persistEvent.path" },
    {
      "source": "event.persistEvent",
      "target": "response.body.json.object./event"
    },
    {
      "source": "math.1+2+3+5+8",
      "target": "response.body.json.integer./math-calculation"
    },
    {
      "source": "eraser",
      "target": "txtFile./tmp/h2agent.ut.@{myvar}.txt"
    }
  ]
}
)delim"_json;

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json ProvisionConfiguration_Filters = R"delim(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar/2?name=test",
  "responseCode": 200,
  "responseBody": {
    "foo": "bar-1"
  },
  "responseHeaders": {
    "content-type": "text/html",
    "x-version": "1.0.0"
  },
  "transform": [
    {
      "source": "request.body",
      "target": "response.body.json.object"
    },
    {
      "source": "request.uri",
      "target": "response.body.json.string./nodeA/nodeB/nodeS",
      "filter": {
        "RegexCapture": "(/app/v1/foo/bar/)([0-9]*)(\\?name=test)"
      }
    },
    {
      "source": "request.uri.path",
      "target": "response.body.json.integer./nodeA/nodeI",
      "filter": {
        "RegexReplace": {
          "rgx": "(/app/v1/foo/bar/)([0-9]*)",
          "fmt": "$2"
        }
      }
    },
    {
      "source": "value.headervalue",
      "target": "response.header.my-header"
    },
    {
      "source": "request.uri.param.name",
      "target": "response.header.response-header-field-name",
      "filter": {
        "Append": ".mysuffix"
      }
    },
    {
      "source": "value.fieldvalue",
      "target": "response.body.json.string./field"
    },
    {
      "source": "request.header.x-version",
      "target": "var.x-value"
    },
    {
      "source": "var.x-value",
      "target": "response.body.json.string./x-version"
    },
    {
      "source": "random.-3.-3",
      "target": "response.body.json.string./random-will-be-2",
      "filter": {
        "Sum": 5
      }
    },
    {
      "source": "timestamp.ns",
      "target": "response.body.json.unsigned./zeroed",
      "filter": {
        "Multiply": 0
      }
    },
    {
      "source": "strftime.predictable",
      "target": "response.body.json.string./time",
      "filter": {
        "Prepend": "Now it's "
      }
    },
    {
      "source": "value.10",
      "target": "response.body.json.integer./one",
      "filter": {
        "Multiply": 0.1
      }
    },
    {
      "source": "inState",
      "target": "response.body.json.string./instate",
      "filter": {
        "Append": " state"
      }
    },
    {
      "source": "value.new-state-for-POST",
      "target": "outState.POST./this/uri"
    },
    {
      "source": "value.1",
      "target": "var.transfer-500-to-status-code"
    },
    {
      "source": "value.500",
      "target": "response.statusCode",
      "filter": { "ConditionVar" : "transfer-500-to-status-code" }
    },
    {
      "source": "request.uri.path",
      "target": "response.body.json.string./captureBarIdFromURI",
      "filter": {
        "RegexReplace": {
          "rgx": "/app/v1/foo/bar/([0-9]*)",
          "fmt": "$1"
        }
      }
    }
  ]
}
)delim"_json;

class Transform_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_{};
    h2agent::model::MockServerEventsData *events_data_{};
    h2agent::model::GlobalVariable *global_variable_{};
    h2agent::model::FileManager *file_manager_{};

    Transform_test() {
        // Reserve memory for storage data and global variables, just in case they are used:
        events_data_ = new h2agent::model::MockServerEventsData();
        global_variable_ = new h2agent::model::GlobalVariable();
        file_manager_ = new h2agent::model::FileManager(nullptr);
    }

    ~Transform_test() {
        delete(events_data_);
        delete(global_variable_);
        delete(file_manager_);
    }
};

TEST_F(Transform_test, TransformWithSources) // test different sources
{

    EXPECT_EQ(Transform_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Transform_test::adata_.loadProvision(ProvisionConfiguration_Sources), h2agent::model::AdminServerProvisionData::Success);

    std::shared_ptr<h2agent::model::AdminServerProvision> provision = adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision);
    //auto provision = Transform_test::adata_.getProvisionData().findRegexMatching("initial", "GET", "/app/v1/foo/bar/1?name=test");
    provision->setMockServerEventsData(events_data_); // could be used by event source
    provision->setGlobalVariable(global_variable_);
    provision->setFileManager(file_manager_);

    // Simulate event on transformation:
    std::string requestUri = "/app/v1/foo/bar/1?name=test";
    std::string requestUriPath = "/app/v1/foo/bar/1";
    std::map<std::string, std::string> qmap = h2agent::http2::extractQueryParameters("name=test");
    const nlohmann::json request = R"({"node1":{"node2":"value-of-node1-node2","delaymilliseconds":25}})"_json;

    std::string requestBody = request.dump();
    nghttp2::asio_http2::header_map requestHeaders;
    requestHeaders.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    requestHeaders.emplace("x-version", nghttp2::asio_http2::header_value{"1.0.0"});
    std::uint64_t generalUniqueServerSequence = 74;
    // outputs:
    unsigned int statusCode = 0;
    nghttp2::asio_http2::header_map headers;
    std::string responseBody;
    unsigned int responseDelayMs = 0;
    std::string outState;
    std::string outStateMethod;
    std::string outStateUri;

    provision->transform(requestUri, requestUriPath, qmap, requestBody, requestHeaders, generalUniqueServerSequence, statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod, outStateUri, nullptr, nullptr);

    EXPECT_TRUE(Transform_test::adata_.clearProvisions());

    EXPECT_EQ(statusCode, 200);
    EXPECT_EQ(ert::http2comm::headersAsString(headers), "[content-type: text/html][x-version: 1.0.0]");
    nlohmann::json assertedJson = nlohmann::json::parse(responseBody);
    nlohmann::json expectedJson = R"(
    {
      "foo": "bar-1",
      "instate": "initial",
      "myglobalvar": "myglobalvarvalue",
      "myvar": "myvarvalue",
      "random": "20",
      "randomset": "scissors",
      "recvseq": 74,
      "remove-me": 0,
      "requestUriQueryParam":"test",
      "requestBody": {
        "node1": {
          "delaymilliseconds": 25,
          "node2": "value-of-node1-node2"
        }
      },
      "requestHeader": "1.0.0",
      "requestUri": "/app/v1/foo/bar/1?name=test",
      "requestUriPath": "/app/v1/foo/bar/1",
      "responseBody": {
        "foo": "bar-1"
      },
      "strftime": "Now it's 02:56AM.",
      "unix_ms": "1653872192363",
      "unix_us": "1653872192363705",
      "unix_ns": "1653872192363705636",
      "unix_s": "1653872192",
      "math-calculation": 19
    }
    )"_json;
    for(auto i: { "random", "randomset", "strftime", "unix_ms", "unix_us", "unix_ns", "unix_s" }) {
        expectedJson[i] = assertedJson[i];
    }
    EXPECT_EQ(assertedJson, expectedJson);
    EXPECT_EQ(responseDelayMs, 0);
    EXPECT_EQ(outState, "initial");
    EXPECT_EQ(outStateMethod, "");
    EXPECT_EQ(outStateUri, "");
}

TEST_F(Transform_test, TransformWithSourcesAndFilters)
{
    EXPECT_EQ(Transform_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminServerMatchingData::Success);
    EXPECT_EQ(Transform_test::adata_.loadProvision(ProvisionConfiguration_Filters), h2agent::model::AdminServerProvisionData::Success);

    std::shared_ptr<h2agent::model::AdminServerProvision> provision = adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/2?name=test");
    ASSERT_TRUE(provision);
    //auto provision = Transform_test::adata_.getProvisionData().findRegexMatching("initial", "GET", "/app/v1/foo/bar/1?name=test");
    provision->setMockServerEventsData(events_data_); // could be used by event source
    provision->setGlobalVariable(global_variable_);

    // Simulate event on transformation:
    std::string requestUri = "/app/v1/foo/bar/2?name=test";
    std::string requestUriPath = "/app/v1/foo/bar/2";
    std::map<std::string, std::string> qmap = h2agent::http2::extractQueryParameters("name=test");
    EXPECT_EQ(qmap.size(), 1);
    EXPECT_EQ(qmap["name"], "test");
    const nlohmann::json request = R"({"node1":{"node2":"value-of-node1-node2","delaymilliseconds":25}})"_json;

    std::string requestBody = request.dump();
    nghttp2::asio_http2::header_map requestHeaders;
    requestHeaders.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    requestHeaders.emplace("x-version", nghttp2::asio_http2::header_value{"1.0.0"});
    std::uint64_t generalUniqueServerSequence = 74;
    // outputs:
    unsigned int statusCode = 0;
    nghttp2::asio_http2::header_map headers;
    std::string responseBody;
    unsigned int responseDelayMs = 0;
    std::string outState;
    std::string outStateMethod;
    std::string outStateUri;

    provision->transform(requestUri, requestUriPath, qmap, requestBody, requestHeaders, generalUniqueServerSequence, statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod, outStateUri, nullptr, nullptr);

    EXPECT_TRUE(Transform_test::adata_.clearProvisions());

    EXPECT_EQ(statusCode, 500);
    EXPECT_EQ(ert::http2comm::headersAsString(headers), "[content-type: text/html][my-header: headervalue][response-header-field-name: test.mysuffix][x-version: 1.0.0]");
    nlohmann::json assertedJson = nlohmann::json::parse(responseBody);
    // add original responseBody, which is joined (merge by default)
    nlohmann::json expectedJson = R"(
    {
      "foo": "bar-1",
      "field": "fieldvalue",
      "node1": {
        "delaymilliseconds": 25,
        "node2": "value-of-node1-node2"
      },
      "nodeA": {
        "nodeI": 2,
        "nodeB": {
          "nodeS": "/app/v1/foo/bar/2?name=test"
        }
      },
      "x-version": "1.0.0",
      "random-will-be-2": "2",
      "zeroed": 0,
      "time": "Now it's predictable",
      "one": 1,
      "instate": "initial state",
      "captureBarIdFromURI": "2"
    }
    )"_json;
    EXPECT_EQ(assertedJson, expectedJson);
    EXPECT_EQ(responseDelayMs, 0);
    //EXPECT_EQ(outState, "7.400000");
    EXPECT_EQ(outState, "new-state-for-POST");
    EXPECT_EQ(outStateMethod, "POST");
    EXPECT_EQ(outStateUri, "/this/uri");
}

