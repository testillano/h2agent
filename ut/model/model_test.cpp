#include <AdminData.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <AdminMatchingData.hpp>

const nlohmann::json MatchingConfiguration_FullMatching__Success = R"({ "algorithm": "FullMatching" })"_json;
const nlohmann::json MatchingConfiguration_FullMatchingRegexReplace__Success = R"({ "algorithm": "FullMatchingRegexReplace", "rgx":"regexp here", "fmt":"$1" })"_json;
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

class model_test : public ::testing::Test
{
public:
    h2agent::model::AdminData adata_;

    model_test() {
        ;
        //adata_.loadMatching(JsonMatching);
    }
};

TEST_F(model_test, LoadMatching)
{
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_FullMatching__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success2), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success3), h2agent::model::AdminMatchingData::Success);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__Success4), h2agent::model::AdminMatchingData::Success);

    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration__BadSchema), h2agent::model::AdminMatchingData::BadSchema);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_algorithm__BadSchema), h2agent::model::AdminMatchingData::BadSchema);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_uriPathQueryParametersFilter__BadSchema), h2agent::model::AdminMatchingData::BadSchema);

    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_FullMatching__BadContent2), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_FullMatchingRegexReplace__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent2), h2agent::model::AdminMatchingData::BadContent);
    EXPECT_EQ(model_test::adata_.loadMatching(MatchingConfiguration_PriorityMatchingRegex__BadContent3), h2agent::model::AdminMatchingData::BadContent);
}

