#include <iostream>
#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functions.hpp>

#include <AdminMatchingData.hpp>
#include <AdminProvisionData.hpp>
#include <AdminSchemas.hpp>

// Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({ "algorithm": "FullMatching" })"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({ "algorithm": "FullMatchingRegexReplace", "rgx":"([0-9]{3})-([a-z]{2})-foo-bar", "fmt":"$1"})"_json;
const nlohmann::json MatchingConfiguration_PriorityMatchingRegex__Success = R"({ "algorithm": "PriorityMatchingRegex" })"_json;
//const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__Success4 = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"Ignore" })"_json;

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
      "target": "response.body.string./requestUri"
    },
    {
      "source": "request.uri.path",
      "target": "response.body.string./requestUriPath"
    },
    {
      "source": "request.uri.param.name",
      "target": "response.body.string./requestUriQueryParam"
    },
    {
      "source": "request.body",
      "target": "response.body.object./requestBody"
    },
    {
      "source": "request.body./missing/path",
      "target": "response.body.object./notprocessed"
    },
    {
      "source": "response.body",
      "target": "response.body.object./responseBody"
    },
    {
      "source": "response.body./missing/path",
      "target": "response.body.object./notprocessed"
    },
    {
      "source": "request.header.x-version",
      "target": "response.body.object./requestHeader"
    },
    {
      "source": "request.header.missing-header",
      "target": "response.body.object./notprocessed"
    },
    {
      "source": "eraser",
      "target": "response.body.object./responseBody/remove-me"
    },
    {
      "source": "general.random.20.30",
      "target": "response.body.string./random"
    },
    {
      "source": "general.randomset.rock|paper|scissors",
      "target": "response.body.string./randomset"
    },
    {
      "source": "general.timestamp.s",
      "target": "response.body.string./unix_s"
    },
    {
      "source": "general.timestamp.ms",
      "target": "response.body.string./unix_ms"
    },
    {
      "source": "general.timestamp.ns",
      "target": "response.body.string./unix_ns"
    },
    {
      "source": "general.strftime.Now it's %I:%M%p.",
      "target": "response.body.string./strftime"
    },
    {
      "source": "general.recvseq",
      "target": "response.body.integer./recvseq"
    },
    {
      "source": "value.myvarvalue",
      "target": "var.myvar"
    },
    {
      "source": "var.myvar",
      "target": "response.body.string./myvar"
    },
    {
      "source": "var.missing-var",
      "target": "response.body.string./notprocessed"
    },
    {
      "source": "inState",
      "target": "response.body.string./instate"
    }
  ]
}
)delim"_json;

/*
    { "source": "value.POST", "target": "var.persistEvent.method" },
    { "source": "value./app/v1/foo/bar/1", "target": "var.persistEvent.uri" },
    { "source": "value.-1", "target": "var.persistEvent.number" },
    { "source": "value./body/node1/node2", "target": "var.persistEvent.path" },
    {
      "source": "event.persistEvent",
      "target": "response.body.object./event"
    }
*/

// https://www.geeksforgeeks.org/raw-string-literal-c/
// We extend delimiters to 'foo(' and ')foo' because internal regex have also parentheses:
const nlohmann::json ProvisionConfiguration_Filters = R"delim(
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/foo/bar/1?name=test",
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
      "source": "request.uri",
      "target": "response.body.integer./nodeA/nodeB",
      "filter": {
        "RegexCapture": "([0-9]*)"
      }
    },
    {
      "source": "request.uri.path",
      "target": "response.body.integer./nodeA/nodeB/nodeC",
      "filter": {
        "RegexReplace": {
          "rgx": "([0-9]*)",
          "fmt": "$1"
        }
      }
    },
    {
      "source": "request.uri.param.querypepe",
      "target": "response.header.response-header-field-name",
      "filter": {
        "Append": ".mysuffix"
      }
    },
    {
      "source": "request.body",
      "target": "response.body.object"
    },
    {
      "source": "request.header.request-header-field-name",
      "target": "var.node3#x-value",
      "filter": {
        "Sum": -9
      }
    },
    {
      "source": "general.random.-3.4",
      "target": "outState",
      "filter": {
        "Sum": 5
      }
    },
    {
      "source": "general.timestamp.ns",
      "target": "outState",
      "filter": {
        "Multiply": -1
      }
    },
    {
      "source": "general.strftime.Now it's %I:%M%p.",
      "target": "outState",
      "filter": {
        "Sum": 5
      }
    },
    {
      "source": "general.recvseq",
      "target": "outState",
      "filter": {
        "Multiply": 0.1
      }
    },
    {
      "source": "var.variable.name",
      "target": "outState",
      "filter": {
        "Sum": 5
      }
    },
    {
      "source": "inState",
      "target": "outState",
      "filter": {
        "Sum": 5
      }
    },
    {
      "source": "value.500",
      "target": "response.statusCode",
      "filter": { "ConditionVar" : "transfer-500-to-status-code" }
    }
  ]
}
)delim"_json;

class AdminData_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_;

    AdminData_test() {
      ;
    }
};

TEST_F(AdminData_test, TransformWithSources) // test different sources
{

    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration_Sources), h2agent::model::AdminProvisionData::Success);

    std::shared_ptr<h2agent::model::AdminProvision> provision = adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test");
    ASSERT_TRUE(provision);
    //auto provision = AdminData_test::adata_.getProvisionData().findWithPriorityMatchingRegex("initial", "GET", "/app/v1/foo/bar/1?name=test");

    std::string requestUri = "/app/v1/foo/bar/1?name=test";
    std::string requestUriPath = "/app/v1/foo/bar/1";
    std::map<std::string, std::string> qmap = h2agent::http2server::extractQueryParameters(requestUri);
    const nlohmann::json request = R"({ "node1": { "node2": "value-of-node1-node2", "delaymilliseconds": 25 } })"_json;


    std::string requestBody = request.dump();
    nghttp2::asio_http2::header_map requestHeaders;
    requestHeaders.emplace("Content-Type", nghttp2::asio_http2::header_value{"application/json"});
    requestHeaders.emplace("x-version", nghttp2::asio_http2::header_value{"1.0.0"});
    std::uint64_t generalUniqueServerSequence = 74;
    // outputs:
    unsigned int statusCode;
    nghttp2::asio_http2::header_map headers;
    std::string responseBody;
    unsigned int responseDelayMs;
    std::string outState;
    std::string outStateMethod;

    provision->transform(requestUri, requestUriPath, qmap, requestBody, requestHeaders, generalUniqueServerSequence, statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod);


    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());
}

