#include <nghttp2/asio_http2_server.h>

#include <DataPart.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

const nlohmann::json FooBarJson = R"({"foo":"bar"})"_json;
const std::string HelloWorld = "hello world !";
const std::string HelloWorldAsHexString = "0x68656c6c6f20776f726c642021";
const std::string MultipartAsHexString = "0x2d2d374d41345957786b54725a753067570d0a436f6e74656e742d547970653a206170706c69636174696f6e2f6a736f6e0d0a0d0a7b22666f6f223a22626172227d0d0a2d2d374d41345957786b54725a753067570d0a436f6e74656e742d547970653a206170706c69636174696f6e2f6f637465742d73747265616d0d0a0d0a268001260d0a2d2d374d41345957786b54725a753067572d2d";

const std::string IpAsHexString = "0xc0a80001";
const nlohmann::json MultipartJson = R"({"multipart.1":{"content":{"foo":"bar"},"headers":{"Content-Type":"application/json"}},"multipart.2":{"content":"0x26800126","headers":{"Content-Type":"application/octet-stream"}}})"_json;

class DataPart_test : public ::testing::Test
{
public:
    h2agent::model::DataPart dp_text_{};
    h2agent::model::DataPart dp_binary_{};
    h2agent::model::DataPart dp_json_{};
    h2agent::model::DataPart dp_multipart_{};

    DataPart_test() {
        dp_text_.assign(HelloWorld);
        dp_binary_.assignFromHex(IpAsHexString);
        dp_json_.assign(FooBarJson.dump());
        dp_multipart_.assign(FooBarJson.dump());
    }
};

TEST_F(DataPart_test, Constructors)
{
    std::string str = HelloWorld;

    h2agent::model::DataPart dp(str);
    EXPECT_EQ(dp, dp_text_);

    h2agent::model::DataPart dp2(std::move(str));
    EXPECT_EQ(dp2, dp_text_);

    h2agent::model::DataPart dp3(dp_text_);
    EXPECT_EQ(dp3, dp_text_);

    h2agent::model::DataPart dp4(std::move(dp_text_));
    EXPECT_EQ(dp4, dp3); // dp_text_ was moved
}

TEST_F(DataPart_test, GetStr)
{
    std::string str = HelloWorld;

    EXPECT_EQ(dp_text_.str(), str);

    h2agent::model::DataPart dp(std::move(str));
    EXPECT_EQ(str, "");
    EXPECT_EQ(dp.str(), HelloWorld);
}

TEST_F(DataPart_test, Operators)
{
    h2agent::model::DataPart dp = dp_text_;
    EXPECT_EQ(dp, dp_text_);
}

TEST_F(DataPart_test, DecodeText)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"text/plain"});
    dp_text_.decode(headers);
    EXPECT_EQ(dp_text_.getJson(), nlohmann::json(HelloWorld));
    EXPECT_FALSE(dp_text_.isJson());
}

TEST_F(DataPart_test, DecodeBinary)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/octet-stream"});
    dp_binary_.decode(headers);
    EXPECT_EQ(dp_binary_.getJson(), nlohmann::json(IpAsHexString));
    EXPECT_FALSE(dp_binary_.isJson());
}

TEST_F(DataPart_test, DecodeBinaryPrintable)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/octet-stream"});
    dp_binary_.assignFromHex(HelloWorldAsHexString);
    dp_binary_.decode(headers);
    EXPECT_EQ(dp_binary_.getJson(), nlohmann::json(HelloWorld));
    EXPECT_FALSE(dp_binary_.isJson());
}

TEST_F(DataPart_test, DecodeJson)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    dp_json_.decode(headers);
    EXPECT_EQ(dp_json_.getJson(), FooBarJson);
    EXPECT_TRUE(dp_json_.isJson());
}

TEST_F(DataPart_test, AsAsciiStringReadable)
{
    dp_binary_.assignFromHex(HelloWorldAsHexString);
    EXPECT_EQ(dp_binary_.asAsciiString(), HelloWorld);
}

TEST_F(DataPart_test, AsAsciiStringNonReadable)
{
    dp_binary_.assignFromHex(IpAsHexString);
    EXPECT_EQ(dp_binary_.asAsciiString(), "....");
}

TEST_F(DataPart_test, DecodeMultipart)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"multipart/related; boundary=7MA4YWxkTrZu0gW"});
    dp_multipart_.assignFromHex(MultipartAsHexString);
    dp_multipart_.decode(headers);
    EXPECT_EQ(dp_multipart_.getJson(), nlohmann::json(MultipartJson));
    EXPECT_TRUE(dp_multipart_.isJson());
}

TEST_F(DataPart_test, DecodeMultipartEmptyBoundary)
{
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"multipart/related; boundary="});
    dp_multipart_.assignFromHex(MultipartAsHexString);
    dp_multipart_.decode(headers);
    EXPECT_TRUE(dp_multipart_.getJson().is_null());
    EXPECT_FALSE(dp_multipart_.isJson());
}

// A content type announces json as 'application/json' or 'application/<x>+json' (RFC 6839),
// case-insensitive and with parameters ignored:
TEST_F(DataPart_test, DecodeJsonContentTypes)
{
    for (const char *ct : {
                "application/json", "application/json; charset=utf-8", "Application/JSON", "application/problem+json", " application/merge-patch+json ;q=1"
            }) {
        SCOPED_TRACE(ct);
        h2agent::model::DataPart dp(FooBarJson.dump());
        nghttp2::asio_http2::header_map headers;
        headers.emplace("content-type", nghttp2::asio_http2::header_value{ct});
        dp.decode(headers);
        EXPECT_TRUE(dp.isJson());
        EXPECT_EQ(dp.getJson(), FooBarJson);
        EXPECT_TRUE(dp.getJsonParseError().empty());
    }
}

// Announced as json but not parsed: the received text is kept, the error goes apart.
TEST_F(DataPart_test, DecodeInvalidJsonKeepsTheText)
{
    h2agent::model::DataPart dp(std::string("{\"foo\":"));
    nghttp2::asio_http2::header_map headers;
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    EXPECT_FALSE(dp.isDecoded());
    dp.decode(headers);
    EXPECT_TRUE(dp.isDecoded());
    EXPECT_FALSE(dp.isJson());
    EXPECT_EQ(dp.getJson(), nlohmann::json("{\"foo\":"));
    EXPECT_NE(dp.getJsonParseError().find("parse error"), std::string::npos);

    // assigning a new content clears it:
    dp.assign(FooBarJson.dump());
    EXPECT_TRUE(dp.getJsonParseError().empty());

    // not announced as json, or empty: no parse error
    h2agent::model::DataPart text(std::string("{\"foo\":"));
    nghttp2::asio_http2::header_map plain;
    plain.emplace("content-type", nghttp2::asio_http2::header_value{"text/plain"});
    text.decode(plain);
    EXPECT_TRUE(text.getJsonParseError().empty());
    h2agent::model::DataPart empty;
    empty.decode(headers);
    EXPECT_TRUE(empty.getJsonParseError().empty());
}
