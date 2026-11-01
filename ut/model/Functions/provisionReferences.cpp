#include <string>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <ProvisionReferences.hpp>
#include <Vault.hpp>

using h2agent::model::resolveProvisionReferences;

class ProvisionReferences_test : public ::testing::Test
{
public:
    h2agent::model::Vault vault_{};

    ProvisionReferences_test() {
        vault_.load("imsiPattern", nlohmann::json("([0-9]{15})"));
        vault_.load("patterns", nlohmann::json::parse(R"({"msisdn": "34[0-9]{9}", "list": ["a", "b"]})"));
        vault_.load("code", nlohmann::json(201));
        vault_.load("tpl", nlohmann::json("#{imsiPattern}"));
    }
};

TEST_F(ProvisionReferences_test, ResolvesEveryStringValueOnce) {
    nlohmann::json doc = nlohmann::json::parse(R"j([
      {"requestUri": "/app/#{patterns/list/1}", "responseBody": {"n": "#{code}", "keep": "@{runtime}", "#{key}": 1},
       "transform": [{"from": {"source": "requestUriPath"}, "filter": [{"op": "regexCapture", "rgx": "/imsi-#{imsiPattern}/#{patterns/msisdn}"}],
                      "to": {"target": "var", "name": "x"}}]},
      {"requestUri": "/literal/#{tpl} and #{unclosed"}
    ])j");
    std::string error;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, error)) << error;
    EXPECT_EQ(doc[0]["requestUri"], "/app/b");
    EXPECT_EQ(doc[0]["responseBody"]["n"], "201");                // non-string: compact json text
    EXPECT_EQ(doc[0]["responseBody"]["keep"], "@{runtime}");      // runtime references untouched
    EXPECT_TRUE(doc[0]["responseBody"].contains("#{key}"));        // object keys are not scanned
    EXPECT_EQ(doc[0]["transform"][0]["filter"][0]["rgx"], "/imsi-([0-9]{15})/34[0-9]{9}");
    EXPECT_EQ(doc[1]["requestUri"], "/literal/#{imsiPattern} and #{unclosed");  // single pass; no '}' kept
}

TEST_F(ProvisionReferences_test, MissingReferencesAreKeptLiterallyAndReported) {
    nlohmann::json doc = nlohmann::json::parse(R"({"a": "x#{nope}y", "b": "#{patterns/missing}", "c": "#{nope}-#{code}", "d": "#{}"})");
    std::string warning;
    EXPECT_FALSE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(warning, "unresolved provision references kept literally (missing vault entries at provision time): #{} #{nope} #{patterns/missing}");
    EXPECT_EQ(doc, nlohmann::json::parse(R"({"a": "x#{nope}y", "b": "#{patterns/missing}", "c": "#{nope}-201", "d": "#{}"})"));

    nlohmann::json plain = nlohmann::json::parse(R"({"a": "#{imsiPattern}"})");
    EXPECT_FALSE(resolveProvisionReferences(plain, nullptr, warning));  // no vault: every reference is unresolved
    EXPECT_EQ(plain["a"], "#{imsiPattern}");
}

TEST_F(ProvisionReferences_test, DocumentWithoutReferencesIsUnchanged) {
    const nlohmann::json original = nlohmann::json::parse(R"({"a": "x#y{z}", "b": [1, true, null], "c": "#"})");
    nlohmann::json doc = original;
    std::string error;
    EXPECT_TRUE(resolveProvisionReferences(doc, &vault_, error));
    EXPECT_EQ(doc, original);
}
