#include <functions.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>

const std::string QueryParametersExampleDefault = "foo=foo_value&bar=bar_value";
const std::string QueryParametersExampleSemicolon = "foo=foo_value;bar=bar_value";
const std::string QueryParametersExampleBadKey = "foo=foo_value&bar=bar_value&foo=foo_value2";

class http2Server_test : public ::testing::Test
{
public:
    std::map<std::string, std::string> qmap_amp_{};
    std::map<std::string, std::string> qmap_scl_{};

    nghttp2::asio_http2::header_map headers_{};

    nlohmann::json json_doc_{};

    http2Server_test() {
        qmap_amp_ = h2agent::http2::extractQueryParameters(QueryParametersExampleDefault);
        qmap_scl_ = h2agent::http2::extractQueryParameters(QueryParametersExampleSemicolon, ';');

        headers_.insert(std::pair<std::string, nghttp2::asio_http2::header_value>("content-type", {"application/json; charset=utf-8", false}));
        headers_.insert(std::pair<std::string, nghttp2::asio_http2::header_value>("content-length", {std::to_string(200), false}));

        json_doc_ = R"({ "happy": true, "pi": 3.141 })"_json;
    }
};

TEST_F(http2Server_test, QueryParameters)
{

    for (auto qmap: {
                http2Server_test::qmap_amp_, http2Server_test::qmap_scl_
            }) {
        EXPECT_EQ(qmap.size(), 2);
        for (auto var: {
                    "foo", "bar"
                }) {
            auto it = qmap.find(var);
            ASSERT_TRUE(it != qmap.end());
            EXPECT_EQ(it->second, std::string(var) + "_value");
        }

        std::string qmap_str = h2agent::http2::sortQueryParameters(qmap);
        EXPECT_EQ(qmap_str, "bar=bar_value&foo=foo_value");
        qmap_str = h2agent::http2::sortQueryParameters(qmap, ';');
        EXPECT_EQ(qmap_str, "bar=bar_value;foo=foo_value");
    }

    // Bad key
    auto qmap_bkey = h2agent::http2::extractQueryParameters(QueryParametersExampleBadKey);
    EXPECT_EQ(qmap_bkey.size(), 0);
}

TEST_F(http2Server_test, jsonContentSerialization)
{
    std::string json_str = http2Server_test::json_doc_.dump();
    std::ofstream jfile;
    std::string jfilePath = "/tmp/h2agent.ut.example.json";
    jfile.open (jfilePath);
    jfile << json_str;
    jfile.close();

    std::string content;
    if (h2agent::http2::getFileContent(jfilePath, content)) {
        EXPECT_EQ(json_str, content);
    }

    nlohmann::json jsonDoc;
    if (h2agent::http2::parseJsonContent(content, jsonDoc)) {
        EXPECT_EQ(http2Server_test::json_doc_, jsonDoc);
    }

    // Error cases:
    if (!h2agent::http2::parseJsonContent("{\"not a json\":}", jsonDoc, true /* write exception */)) {
        EXPECT_EQ(jsonDoc.dump(), "\"Json content parse error: [json.exception.parse_error.101] parse error at line 1, column 15: syntax error while parsing value - unexpected '}'; expected '[', '{', or a literal | exception id: 101 | byte position of error: 15\"");
    }

    EXPECT_FALSE(h2agent::http2::getFileContent("/this/is/not/a/valid/file", content));
}

