#include <nghttp2/asio_http2_server.h>

#include <DataPart.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

const nlohmann::json FooBarJson = R"({"foo":"bar"})"_json;
const std::string HelloWorld = "hello world !";
const std::string HelloWorldAsHexString = "0x68656c6c6f20776f726c642021";
const std::string IpAsHexString = "0xc0a80001";

class DataPart_test : public ::testing::Test
{
public:
    h2agent::model::DataPart dp_text_{};
    h2agent::model::DataPart dp_binary_{};
    h2agent::model::DataPart dp_json_{};

    DataPart_test() {
        dp_text_.assign(HelloWorld);
        dp_binary_.assignFromHex(IpAsHexString);
        dp_json_.assign(FooBarJson.dump());
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

