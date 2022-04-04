#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <AdminMatchingData.hpp>
#include <AdminProvisionData.hpp>
#include <AdminSchemas.hpp>

// Matching configuration:
const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({ "algorithm": "FullMatching" })"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({ "algorithm": "FullMatchingRegexReplace", "rgx":"([0-9]{3})-([a-z]{2})-foo-bar", "fmt":"$1"})"_json;
const nlohmann::json MatchingConfiguration_PriorityMatchingRegex__Success = R"({ "algorithm": "PriorityMatchingRegex" })"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__Success = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"SortAmpersand" })"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__Success2 = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"SortSemicolon" })"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__Success3 = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"PassBy" })"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__Success4 = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"Ignore" })"_json;

const nlohmann::json MatchingConfiguration__BadSchema = R"({ "happy": true, "pi": 3.141 })"_json;
const nlohmann::json MatchingConfiguration_algorithm__BadSchema = R"({ "algorithm": "unknown" })"_json;
const nlohmann::json MatchingConfiguration_uriPathQueryParametersFilter__BadSchema = R"({ "algorithm": "FullMatching", "uriPathQueryParametersFilter":"unknown" })"_json;

const nlohmann::json MatchingConfiguration_FullMatching__BadContent = R"({ "algorithm": "FullMatching", "rgx":"whatever" })"_json;
const nlohmann::json MatchingConfiguration_FullMatching__BadContent2 = R"({ "algorithm": "FullMatching", "fmt":"whatever" })"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__BadContent = R"({ "algorithm": "FullMatchingRegexReplace" })"_json;
const nlohmann::json MatchingConfiguration_PriorityMatchingRegex__BadContent = R"({ "algorithm": "PriorityMatchingRegex", "rgx":"whatever" })"_json;
const nlohmann::json MatchingConfiguration_PriorityMatchingRegex__BadContent2 = R"({ "algorithm": "PriorityMatchingRegex", "fmt":"whatever" })"_json;
const nlohmann::json MatchingConfiguration_PriorityMatchingRegex__BadContent3 = R"({ "algorithm": "PriorityMatchingRegex", "rgx":"(" })"_json;

// Provision configuration:
const nlohmann::json ProvisionConfiguration__Success = R"(
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
      "source": "general.random.10.30",
      "target": "response.body.integer./generalRandomBetween10and30"
    }
  ]
}
)"_json;

const nlohmann::json ProvisionConfiguration__SuccessRegex = R"delim(
{
  "requestMethod": "GET",
  "requestUri": "(/foo/bar/)([0-9]{3})",
        "responseCode": 200
}
)delim"_json;

const nlohmann::json ProvisionConfiguration__SuccessArray = R"(
[
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/1",
    "responseCode": 200
  },
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/2",
    "responseCode": 200
  }
]
)"_json;

const nlohmann::json ProvisionConfiguration__BadSchema = R"({ "happy": true, "pi": 3.141 })"_json;
const nlohmann::json ProvisionConfiguration__BadContent = R"(
{
  "requestMethod": "GET",
  "requestUri": "bad regular expression due to the slash \\",
  "responseCode": 200
}
)"_json;

const nlohmann::json ProvisionConfiguration__BadContentArray = R"(
[
  {
    "requestMethod": "GET",
    "requestUri": "(/foo/bar/)[0-9]{3}",
    "responseCode": 200
  },
  {
    "requestMethod": "GET",
    "requestUri": "bad regular expression due to the slash \\",
    "responseCode": 200
  }
]
)"_json;

class AdminData_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_;

    AdminData_test() {
        ;
        //adata_.loadMatching(JsonMatching);
    }
};

TEST_F(AdminData_test, LoadMatching)
{
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.getMatchingData().getJson(), MatchingConfiguration_FullMatching__Success);
    EXPECT_EQ(AdminData_test::adata_.getMatchingData().getSchema().getJson(), h2agent::adminSchemas::server_matching);

    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__Success), h2agent::model::AdminMatchingData::Success);
    h2agent::model::AdminMatchingData::AlgorithmType algorithm = AdminData_test::adata_.getMatchingData().getAlgorithm(); // FullMatchingRegexReplace
    EXPECT_EQ(algorithm,  h2agent::model::AdminMatchingData::FullMatchingRegexReplace);
    std::string fmt = AdminData_test::adata_.getMatchingData().getFmt(); // $1
    EXPECT_EQ(fmt, "$1");
    std::regex rgx = AdminData_test::adata_.getMatchingData().getRgx(); // ([0-9]{3})-([a-z]{2})-foo-bar
    std::string result = std::regex_replace ("123-ab-foo-bar", rgx, fmt);
    EXPECT_EQ(result, "123");

    //EXPECT_EQ(AdminData_test::adata_.getMatchingData().getRgx(), re);

    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.getMatchingData().getUriPathQueryParametersFilter(), h2agent::model::AdminMatchingData::SortAmpersand);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success2), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success3), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success4), h2agent::model::AdminMatchingData::Success);

    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration__BadSchema), h2agent::model::AdminMatchingData::BadSchema);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_algorithm__BadSchema), h2agent::model::AdminMatchingData::BadSchema);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__BadSchema), h2agent::model::AdminMatchingData::BadSchema);

    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent2), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent2), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent3), h2agent::model::AdminMatchingData::BadContent);
}

TEST_F(AdminData_test, LoadProvisionSuccess)
{
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__Success), h2agent::model::AdminProvisionData::Success);
    nlohmann::json jarray = nlohmann::json::array();
    jarray.push_back(ProvisionConfiguration__Success);
    EXPECT_EQ(AdminData_test::adata_.getProvisionData().asJsonString(), jarray.dump());
    //EXPECT_TRUE(AdminData_test::adata_.clearProvisions());

    // two ordered provisions:
    nlohmann::json anotherProvision = ProvisionConfiguration__Success;
    anotherProvision["requestUri"] = std::string(ProvisionConfiguration__Success["requestUri"]) + "_bis";
    EXPECT_EQ(AdminData_test::adata_.loadProvision(anotherProvision), h2agent::model::AdminProvisionData::Success);
    jarray.push_back(anotherProvision);
    EXPECT_EQ(AdminData_test::adata_.getProvisionData().asJsonString(true /* ordered */), jarray.dump());
    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());

    // provision array
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__SuccessArray), h2agent::model::AdminProvisionData::Success);
    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());
}

TEST_F(AdminData_test, LoadProvisionFail)
{
    // Bad schema
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__BadSchema), h2agent::model::AdminProvisionData::BadSchema);
    EXPECT_EQ(AdminData_test::adata_.getProvisionData().asJsonString(), "null");
    EXPECT_EQ(AdminData_test::adata_.getProvisionData().getSchema().getJson(), h2agent::adminSchemas::server_provision);
    EXPECT_FALSE(AdminData_test::adata_.clearProvisions());

    // Bad content only happens for PriorityMatchingRegex:
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__BadContent), h2agent::model::AdminProvisionData::BadContent);
    EXPECT_FALSE(AdminData_test::adata_.clearProvisions());

    // Bad content array:
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__BadContentArray), h2agent::model::AdminProvisionData::BadContent);
    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());
}

TEST_F(AdminData_test, FindProvisionRegex)
{
    // Bad content only happens for PriorityMatchingRegex:
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__SuccessRegex), h2agent::model::AdminProvisionData::Success);

    EXPECT_TRUE(AdminData_test::adata_.getProvisionData().findWithPriorityMatchingRegex("initial", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().findWithPriorityMatchingRegex("missing", "GET", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().findWithPriorityMatchingRegex("initial", "POST", "/foo/bar/123") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().findWithPriorityMatchingRegex("initial", "GET", "/foo/bar/12345") != nullptr);
    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());
}

TEST_F(AdminData_test, FindProvision)
{
    // Bad content only happens for PriorityMatchingRegex:
    EXPECT_EQ(AdminData_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(AdminData_test::adata_.loadProvision(ProvisionConfiguration__Success), h2agent::model::AdminProvisionData::Success);

    EXPECT_TRUE(AdminData_test::adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().find("missing", "GET", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().find("initial", "POST", "/app/v1/foo/bar/1?name=test") != nullptr);
    EXPECT_FALSE(AdminData_test::adata_.getProvisionData().find("initial", "GET", "/app/v1/foo/bar/1?name=missing") != nullptr);
    EXPECT_TRUE(AdminData_test::adata_.clearProvisions());
}
