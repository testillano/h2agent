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
        vault_.load("block", nlohmann::json::parse(R"({"book": {"title": "Human Action", "tags": ["economics", "praxeology"]}, "nested": {"#include": "code"}, "text": "#{code}"})"));
        vault_.load("flag", nlohmann::json(false));
        vault_.load("nothing", nlohmann::json(nullptr));
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

TEST_F(ProvisionReferences_test, IncludesKeepTheJsonTypeOfTheVaultValue) {
    nlohmann::json doc = nlohmann::json::parse(R"j({
      "responseCode": {"#include": "code"},
      "responseBody": {
        "object": {"#include": "block"}, "pointer": {"#include": "block/book"},
        "array": {"#include": "patterns/list"}, "string": {"#include": "imsiPattern"},
        "boolean": {"#include": "flag"}, "null": {"#include": "nothing"},
        "elements": [{"#include": "code"}, 1, {"#include": "patterns/list/0"}],
        "text": "c=#{code}"
      }
    })j");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["responseCode"], 201);
    const nlohmann::json &body = doc["responseBody"];
    nlohmann::json block;
    ASSERT_TRUE(vault_.tryGet("block", block));
    EXPECT_EQ(body["object"], block);
    EXPECT_EQ(body["pointer"], nlohmann::json::parse(R"({"title": "Human Action", "tags": ["economics", "praxeology"]})"));
    EXPECT_EQ(body["array"], nlohmann::json::parse(R"(["a", "b"])"));  // no spread
    EXPECT_EQ(body["string"], "([0-9]{15})");
    EXPECT_EQ(body["boolean"], false);
    EXPECT_TRUE(body["null"].is_null());
    EXPECT_EQ(body["elements"], nlohmann::json::parse(R"([201, 1, "a"])"));
    EXPECT_EQ(body["text"], "c=201");  // #{} unchanged
}

TEST_F(ProvisionReferences_test, IncludedValuesAreNotScannedAgain) {
    nlohmann::json doc = nlohmann::json::parse(R"({"a": {"#include": "block"}, "b": {"#include": "tpl"}})");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["a"]["nested"], nlohmann::json::parse(R"({"#include": "code"})"));
    EXPECT_EQ(doc["a"]["text"], "#{code}");
    EXPECT_EQ(doc["b"], "#{imsiPattern}");
}

TEST_F(ProvisionReferences_test, IncludeAtRootReplacesTheWholeDocument) {
    nlohmann::json doc = nlohmann::json::parse(R"({"#include": "patterns"})");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["msisdn"], "34[0-9]{9}");
}

TEST_F(ProvisionReferences_test, UndefinedIncludesAreKeptLiterallyAndReported) {
    nlohmann::json doc = nlohmann::json::parse(R"([{"a": {"#include": "nope"}, "b": {"#include": "block/missing"}, "c": "#{nope}"},
                                                  {"d": {"#include": ""}, "e": {"#include": "code"}}])");
    std::string warning;
    EXPECT_FALSE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(warning, "unresolved provision references kept literally (missing vault entries at provision time): #{nope}; "
              R"(reserved key '#include' ignored, undefined vault entry (kept literally): {"#include":""} {"#include":"block/missing"} {"#include":"nope"})");
    EXPECT_EQ(doc, nlohmann::json::parse(R"([{"a": {"#include": "nope"}, "b": {"#include": "block/missing"}, "c": "#{nope}"},
                                            {"d": {"#include": ""}, "e": 201}])"));

    nlohmann::json plain = nlohmann::json::parse(R"({"a": {"#include": "code"}})");
    EXPECT_FALSE(resolveProvisionReferences(plain, nullptr, warning));  // no vault
    EXPECT_EQ(plain["a"], nlohmann::json::parse(R"({"#include": "code"})"));
}

TEST_F(ProvisionReferences_test, ObjectsWhichAreNotIncludesAreKeptAsRegularObjectsAndReported) {
    nlohmann::json doc = nlohmann::json::parse(R"({"x": {"#include": "code", "other": "#{code}"}, "y": [{"#include": 5}],
                                                  "z": {"#include": {"inner": {"#include": "code"}}}})");
    std::string warning;
    EXPECT_FALSE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(warning, "reserved key '#include' ignored, not the only key or not a string (kept literally): "
              R"({"#include":"code","other":"#{code}"} {"#include":5} {"#include":{"inner":{"#include":"code"}}})");
    // Regular objects: their children are still resolved
    EXPECT_EQ(doc, nlohmann::json::parse(R"({"x": {"#include": "code", "other": "201"}, "y": [{"#include": 5}],
                                            "z": {"#include": {"inner": 201}}})"));
}

TEST_F(ProvisionReferences_test, IncludeKeyOnlyMattersAsObjectKey) {
    const nlohmann::json original = nlohmann::json::parse(R"({"a": "#include", "b": ["#include", "code"], "c": {"#includes": "code"}, "d": {"#Include": "code"}})");
    nlohmann::json doc = original;
    std::string warning;
    EXPECT_TRUE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(doc, original);
}
