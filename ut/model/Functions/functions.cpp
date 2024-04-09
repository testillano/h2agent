#include <functions.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>

const std::string QueryParametersExampleDefault = "foo=foo_value&bar=bar_value";
const std::string QueryParametersExampleSemicolon = "foo=foo_value;bar=bar_value";
const std::string QueryParametersExampleBadKey = "foo=foo_value&bar=bar_value&foo=foo_value2";
const std::string QueryParametersExampleEncoded = "requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&foo=foo_value";

class functions_test : public ::testing::Test
{
public:
    std::map<std::string, std::string> qmap_amp_{};
    std::map<std::string, std::string> qmap_scl_{};
    std::map<std::string, std::string> qmap_enc_{};

    nghttp2::asio_http2::header_map headers_{};

    nlohmann::json json_doc_{};

    functions_test() {
        qmap_amp_ = h2agent::model::extractQueryParameters(QueryParametersExampleDefault);
        qmap_scl_ = h2agent::model::extractQueryParameters(QueryParametersExampleSemicolon, ';');
        qmap_enc_ = h2agent::model::extractQueryParameters(QueryParametersExampleEncoded);

        headers_.insert(std::pair<std::string, nghttp2::asio_http2::header_value>("content-type", {"application/json; charset=utf-8", false}));
        headers_.insert(std::pair<std::string, nghttp2::asio_http2::header_value>("content-length", {std::to_string(200), false}));

        json_doc_ = R"({ "happy": true, "pi": 3.141 })"_json;
    }
};

TEST_F(functions_test, calculateStringKey_k1k2k3)
{
    std::string key;
    h2agent::model::calculateStringKey(key, "initial", "POST", "/foo/bar");
    EXPECT_EQ(key, "initial#POST#/foo/bar");
}

TEST_F(functions_test, calculateStringKey_k1k2)
{
    std::string key;
    h2agent::model::calculateStringKey(key, "POST", "/foo/bar");
    EXPECT_EQ(key, "POST#/foo/bar");
}

TEST_F(functions_test, aggregateKeyPart_TwoArguments)
{
    std::string key;
    h2agent::model::aggregateKeyPart(key, "POST", "/foo/bar");
    EXPECT_EQ(key, "POST#/foo/bar");
}

TEST_F(functions_test, aggregateKeyPart_OneArguments)
{
    std::string key = "POST#/foo/bar";
    h2agent::model::aggregateKeyPart(key, "myClientEndpoint");
    EXPECT_EQ(key, "myClientEndpoint#POST#/foo/bar");
}

TEST_F(functions_test, FooBarQueryParameters)
{

    for (auto qmap: {
                functions_test::qmap_amp_, functions_test::qmap_scl_
            }) {
        EXPECT_EQ(qmap.size(), 2);
        for (auto var: {
                    "foo", "bar"
                }) {
            auto it = qmap.find(var);
            ASSERT_TRUE(it != qmap.end());
            EXPECT_EQ(it->second, std::string(var) + "_value");
        }

        std::string qmap_str = h2agent::model::sortQueryParameters(qmap);
        EXPECT_EQ(qmap_str, "bar=bar_value&foo=foo_value");
        qmap_str = h2agent::model::sortQueryParameters(qmap, ';');
        EXPECT_EQ(qmap_str, "bar=bar_value;foo=foo_value");
    }
}

TEST_F(functions_test, BadQueryParameterKey)
{
    // Bad key
    auto qmap_bkey = h2agent::model::extractQueryParameters(QueryParametersExampleBadKey);
    EXPECT_EQ(qmap_bkey.size(), 0);
}

TEST_F(functions_test, EncodedQueryParameter)
{
    EXPECT_EQ(qmap_enc_.size(), 2);
    auto it = qmap_enc_.find("foo");
    ASSERT_TRUE(it != qmap_enc_.end());
    EXPECT_EQ(it->second, "foo_value");

    it = qmap_enc_.find("requestUri");
    ASSERT_TRUE(it != qmap_enc_.end());
    EXPECT_EQ(it->second, "/app/v1/foo/bar?id=5&name=test");

    std::string qmap_str = h2agent::model::sortQueryParameters(qmap_enc_);
    EXPECT_EQ(qmap_str, "foo=foo_value&requestUri=%2Fapp%2Fv1%2Ffoo%2Fbar%3Fid%3D5%26name%3Dtest");
}

TEST_F(functions_test, JsonContentSerialization)
{
    std::string json_str = functions_test::json_doc_.dump();
    std::ofstream jfile;
    std::string jfilePath = "/tmp/h2agent.ut.example.json";
    jfile.open (jfilePath);
    jfile << json_str;
    jfile.close();

    std::string content;
    if (h2agent::model::getFileContent(jfilePath, content)) {
        EXPECT_EQ(json_str, content);
    }

    nlohmann::json jsonDoc;
    if (h2agent::model::parseJsonContent(content, jsonDoc)) {
        EXPECT_EQ(functions_test::json_doc_, jsonDoc);
    }

    // Error cases:
    if (!h2agent::model::parseJsonContent("{\"not a json\":}", jsonDoc, true /* write exception */)) {
        EXPECT_EQ(jsonDoc.dump(), "\"Json content parse error: [json.exception.parse_error.101] parse error at line 1, column 15: syntax error while parsing value - unexpected '}'; expected '[', '{', or a literal | exception id: 101 | byte position of error: 15\"");
    }

    EXPECT_FALSE(h2agent::model::getFileContent("/this/is/not/a/valid/file", content));
}

TEST_F(functions_test, PrintableStringAsHex)
{
    std::string output;

    EXPECT_TRUE(h2agent::model::asHexString("hello world !", output));
    EXPECT_EQ(output, "0x68656c6c6f20776f726c642021");
}

TEST_F(functions_test, NonPrintableStringAsHex)
{
    std::string output;

    EXPECT_FALSE(h2agent::model::asHexString("helló", output));
    EXPECT_EQ(output, "0x68656c6cc3b3");
}

TEST_F(functions_test, EmptyInputStringAsHex) {
    std::string output;

    EXPECT_TRUE(h2agent::model::asHexString("", output));
    EXPECT_EQ(output, "0x");
}

TEST_F(functions_test, ValidHexStringToOctetStream)
{
    std::string hexIP = "c0a80001"; // i.e.: 192.168.0.1
    std::string input, output;

    EXPECT_TRUE(h2agent::model::fromHexString(hexIP, output));
    // Transform output again:
    EXPECT_FALSE(h2agent::model::asHexString(output, input));
    hexIP.insert(0, "0x"); // asHexString always adds 0x prefix
    EXPECT_EQ(input, hexIP);
}

TEST_F(functions_test, ValidHexStringToOctetStreamWith0xPrefix)
{
    std::string hexIP = "0xc0a80001"; // i.e.: 192.168.0.1
    std::string input, output;

    EXPECT_TRUE(h2agent::model::fromHexString(hexIP, output));
    // Transform output again:
    EXPECT_FALSE(h2agent::model::asHexString(output, input));
    EXPECT_EQ(input, hexIP);
}

TEST_F(functions_test, InvalidHexStringToOctetStreamError1)
{
    std::string output;

    EXPECT_FALSE(h2agent::model::fromHexString("0x68656c6c6fr0", output)); // 'r' is not hex symbol
    EXPECT_EQ(output, "hello"); // decodes until fail
}

TEST_F(functions_test, InvalidHexStringToOctetStreamError2)
{
    std::string output;

    EXPECT_FALSE(h2agent::model::fromHexString("0x68656c6c6f0r", output)); // 'r' is not hex symbol
    EXPECT_EQ(output, "hello"); // decodes until fail
}

TEST_F(functions_test, InvalidHexStringOddSize)
{
    std::string output;

    EXPECT_FALSE(h2agent::model::fromHexString("0x686", output));
}

TEST_F(functions_test, AsAsciiString)
{
    std::string mixed = "c030"; // first byte is not readable (0xc0), second one is '0' (0x30), so readable
    std::string data, output;

    EXPECT_TRUE(h2agent::model::fromHexString(mixed, data));
    EXPECT_FALSE(h2agent::model::asAsciiString(data, output));
    EXPECT_EQ(output, ".0");
}

TEST_F(functions_test, EmptyAsAsciiString)
{
    std::string output;

    EXPECT_FALSE(h2agent::model::asAsciiString("", output));
    EXPECT_EQ(output, "<null>");
}

TEST_F(functions_test, JsonConstraintSuccess)
{
    std::string failReport{};
    nlohmann::json received = R"({"pi":3.141,"happy":true,"name":"Niels","nothing":null,"answer":{"everything":42},"list":[1,0,2],"object":{"currency":"USD","value":42.99}})"_json;
    nlohmann::json expected = R"({"pi":3.141,"name":"Niels","object":{"value":42.99}})"_json;

    EXPECT_TRUE(h2agent::model::jsonConstraint(received, expected, failReport));
    EXPECT_TRUE(failReport.empty());
}

TEST_F(functions_test, JsonConstraintFail)
{
    std::string failReport{};
    nlohmann::json received = R"({"pi":3.141,"happy":true,"name":"Niels","nothing":null,"answer":{"everything":42},"list":[1,0,2],"object":{"currency":"USD","value":42.99}})"_json;
    nlohmann::json expected = R"({"pi":3.141,"name":"Niels","object":{"value":42.99,"MISSING":3}})"_json;

    EXPECT_FALSE(h2agent::model::jsonConstraint(received, expected, failReport));
    EXPECT_EQ(failReport, "JsonConstraint FAILED: expected key 'MISSING' is missing in validated source");
}

TEST_F(functions_test, JsonConstraintFailByValue)
{
    std::string failReport{};
    nlohmann::json received = R"({"pi":3.141})"_json;
    nlohmann::json expected = R"({"pi":3.1})"_json;

    EXPECT_FALSE(h2agent::model::jsonConstraint(received, expected, failReport));
    EXPECT_EQ(failReport, "JsonConstraint FAILED: expected value for key 'pi' differs regarding validated source");
}

TEST_F(functions_test, String2uint64andSignError)
{
    std::uint64_t output = 0;
    bool negative = false;

    EXPECT_FALSE(h2agent::model::string2uint64andSign("bad_input", output, negative));
}

TEST_F(functions_test, String2uint64andSignPositive)
{
    std::uint64_t output = 0;
    bool negative = false;

    EXPECT_TRUE(h2agent::model::string2uint64andSign("1770", output, negative));
    EXPECT_EQ(output, 1770);
    EXPECT_FALSE(negative);
}

TEST_F(functions_test, String2uint64andSignNegative)
{
    std::uint64_t output = 0;
    bool negative = false;

    EXPECT_TRUE(h2agent::model::string2uint64andSign("-1770", output, negative));
    EXPECT_EQ(output, 1770);
    EXPECT_TRUE(negative);
}

TEST_F(functions_test, FixMetricsNameEmptyString) {
    std::string input = "";
    std::string expected_output = "";
    ASSERT_EQ(expected_output, h2agent::model::fixMetricsName(input));
}

TEST_F(functions_test, FixMetricsNameOnlyValidCharacters) {
    std::string input = "valid:chars_0123";
    std::string expected_output = "valid:chars_0123";
    ASSERT_EQ(expected_output, h2agent::model::fixMetricsName(input));
}

TEST_F(functions_test, FixMetricsNameInvalidCharacters) {
    std::string input = "invalid!chars#";
    std::string expected_output = "invalid_chars_";
    ASSERT_EQ(expected_output, h2agent::model::fixMetricsName(input));
}

TEST_F(functions_test, FixMetricsNameMixedCharacters) {
    std::string input = "mixed!:_chars#0123";
    std::string expected_output = "mixed_:_chars_0123";
    ASSERT_EQ(expected_output, h2agent::model::fixMetricsName(input));
}

