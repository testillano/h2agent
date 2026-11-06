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
        vault_.load("block", nlohmann::json::parse(R"({"book": {"title": "Human Action", "tags": ["economics", "praxeology"]}, "nested": "#{object:patterns}", "text": "#{code}"})"));
        vault_.load("numText", nlohmann::json("42"));
        vault_.load("negative", nlohmann::json(-7));
        vault_.load("ratio", nlohmann::json(2.5));
        vault_.load("jsonText", nlohmann::json(R"({"k": [1, 2]})"));
        vault_.load("badJson", nlohmann::json("{not json"));
        vault_.load("__app_barrier:001:x", nlohmann::json("done"));
        vault_.load("object:a", nlohmann::json("colon key"));
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

TEST_F(ProvisionReferences_test, TypedReferencesKeepOrConvertTheType) {
    nlohmann::json doc = nlohmann::json::parse(R"j({
      "responseCode": "#{integer:code}",
      "responseBody": {
        "object": "#{object:block}", "pointer": "#{object:block/book}", "array": "#{object:patterns/list}",
        "integer": "#{integer:numText}", "negative": "#{integer:negative}", "unsigned": "#{unsigned:code}",
        "float": "#{float:ratio}", "floatFromText": "#{float:numText}",
        "boolean": "#{boolean:flag}", "booleanFromNumber": "#{boolean:code}",
        "jsonFromText": "#{json:jsonText}", "jsonFromObject": "#{json:patterns}",
        "string": "#{string:code}", "stringInText": "c=#{string:code}", "text": "c=#{code}",
        "elements": ["#{integer:code}", 1, "#{object:patterns/list}"]
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
    EXPECT_EQ(body["array"], nlohmann::json::parse(R"(["a", "b"])"));  // a single node, no spread
    EXPECT_EQ(body["integer"], 42);
    EXPECT_EQ(body["negative"], -7);
    EXPECT_EQ(body["unsigned"], 201u);
    EXPECT_DOUBLE_EQ(body["float"].get<double>(), 2.5);
    EXPECT_DOUBLE_EQ(body["floatFromText"].get<double>(), 42.0);
    EXPECT_EQ(body["boolean"], false);
    EXPECT_EQ(body["booleanFromNumber"], true);
    EXPECT_EQ(body["jsonFromText"], nlohmann::json::parse(R"({"k": [1, 2]})"));
    EXPECT_EQ(body["jsonFromObject"]["msisdn"], "34[0-9]{9}");
    EXPECT_EQ(body["string"], "201");
    EXPECT_EQ(body["stringInText"], "c=201");  // 'string:' is text: allowed inside a text
    EXPECT_EQ(body["text"], "c=201");          // no prefix: unchanged
    EXPECT_EQ(body["elements"], nlohmann::json::parse(R"([201, 1, ["a", "b"]])"));
}

TEST_F(ProvisionReferences_test, ObjectWithoutPrefixIsStillItsJsonText) {
    nlohmann::json doc = nlohmann::json::parse(R"({"a": "#{patterns/list}"})");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["a"], R"(["a","b"])");
}

TEST_F(ProvisionReferences_test, TypedValuesAreNotScannedAgain) {
    nlohmann::json doc = nlohmann::json::parse(R"({"a": "#{object:block}", "b": "#{string:tpl}"})");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["a"]["nested"], "#{object:patterns}");
    EXPECT_EQ(doc["a"]["text"], "#{code}");
    EXPECT_EQ(doc["b"], "#{imsiPattern}");
}

TEST_F(ProvisionReferences_test, TypedReferenceAtRootReplacesTheWholeDocument) {
    nlohmann::json doc = nlohmann::json("#{object:patterns}");
    std::string warning;
    ASSERT_TRUE(resolveProvisionReferences(doc, &vault_, warning)) << warning;
    EXPECT_EQ(doc["msisdn"], "34[0-9]{9}");
}

TEST_F(ProvisionReferences_test, OnlyCastTypesAreRecognisedAsPrefixes) {
    nlohmann::json doc = nlohmann::json::parse(R"({"barrier": "#{__app_barrier:001:x}", "escaped": "#{string:object:a}",
                                                  "unknownPrefix": "#{obj:code}"})");
    std::string warning;
    EXPECT_FALSE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(doc["barrier"], "done");          // '__app_barrier:' is part of the key
    EXPECT_EQ(doc["escaped"], "colon key");     // the key 'object:a', read as text
    EXPECT_EQ(doc["unknownPrefix"], "#{obj:code}");  // 'obj' is no cast type: key 'obj:code', missing
    EXPECT_EQ(warning, "unresolved provision references kept literally (missing vault entries at provision time): #{obj:code}");
}

TEST_F(ProvisionReferences_test, InvalidTypedReferencesAreKeptLiterallyAndReported) {
    nlohmann::json doc = nlohmann::json::parse(R"j({
      "inText": "x#{object:block}", "twoRefs": "#{integer:code}#{integer:code}",
      "missing": "#{object:nope}", "missingPointer": "#{object:block/none}",
      "objectOfScalar": "#{object:code}", "integerOfObject": "#{integer:block}", "integerOfText": "#{integer:imsiPattern}",
      "booleanOfNull": "#{boolean:nothing}", "badJson": "#{json:badJson}", "empty": "#{object:}"
    })j");
    const nlohmann::json original = doc;
    std::string warning;
    EXPECT_FALSE(resolveProvisionReferences(doc, &vault_, warning));
    EXPECT_EQ(doc, original);  // everything kept literally
    EXPECT_EQ(warning,
              "unresolved provision references kept literally (missing vault entries at provision time): "
              "#{object:} #{object:block/none} #{object:nope}; "
              "typed provision references kept literally (they must be the whole string value): "
              "#{integer:code} #{object:block}; "
              "typed provision references kept literally (vault value not convertible to the cast type): "
              "#{boolean:nothing} #{integer:block} #{integer:imsiPattern} #{json:badJson} #{object:code}");

    nlohmann::json plain = nlohmann::json::parse(R"({"a": "#{object:block}"})");
    EXPECT_FALSE(resolveProvisionReferences(plain, nullptr, warning));  // no vault
    EXPECT_EQ(plain["a"], "#{object:block}");
}
