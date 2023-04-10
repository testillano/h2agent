#include <keys.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


TEST(keys, dataKeyTwoPartsGetters) {
    h2agent::model::DataKey key("POST", "/foo/bar");
    EXPECT_TRUE(key.getClientEndpointId().empty());
    EXPECT_EQ(key.getMethod(), "POST");
    EXPECT_EQ(key.getUri(), "/foo/bar");
    EXPECT_EQ(key.getKey(), "POST#/foo/bar");
    EXPECT_EQ(key.getNKeys(), 2);
}

TEST(keys, dataKeyThreePartsGetters) {
    h2agent::model::DataKey key("myClientEndpointId", "POST", "/foo/bar");
    EXPECT_EQ(key.getClientEndpointId(), "myClientEndpointId");
    EXPECT_EQ(key.getMethod(), "POST");
    EXPECT_EQ(key.getUri(), "/foo/bar");
    EXPECT_EQ(key.getKey(), "myClientEndpointId#POST#/foo/bar");
    EXPECT_EQ(key.getNKeys(), 3);
}

TEST(keys, dataKeyTwoPartsPresence) {
    h2agent::model::DataKey key1("POST", "/foo/bar");
    EXPECT_FALSE(key1.empty());
    EXPECT_TRUE(key1.complete());
    EXPECT_TRUE(key1.checkSelection());

    h2agent::model::DataKey key2("", "/foo/bar");
    EXPECT_FALSE(key2.empty());
    EXPECT_FALSE(key2.complete());
    EXPECT_FALSE(key2.checkSelection());

    h2agent::model::DataKey key3("POST", "");
    EXPECT_FALSE(key3.empty());
    EXPECT_FALSE(key3.complete());
    EXPECT_FALSE(key3.checkSelection());

    h2agent::model::DataKey key4("", "");
    EXPECT_TRUE(key4.empty());
    EXPECT_FALSE(key4.complete());
    EXPECT_TRUE(key4.checkSelection());
}

TEST(keys, dataKeyThreePartsPresence) {
    h2agent::model::DataKey key1("myClientEndpointId", "POST", "/foo/bar");
    EXPECT_FALSE(key1.empty());
    EXPECT_TRUE(key1.complete());
    EXPECT_TRUE(key1.checkSelection());

    h2agent::model::DataKey key2("myClientEndpointId", "", "/foo/bar");
    EXPECT_FALSE(key2.empty());
    EXPECT_FALSE(key2.complete());
    EXPECT_FALSE(key2.checkSelection());

    h2agent::model::DataKey key3("myClientEndpointId", "POST", "");
    EXPECT_FALSE(key3.empty());
    EXPECT_FALSE(key3.complete());
    EXPECT_FALSE(key3.checkSelection());

    h2agent::model::DataKey key4("myClientEndpointId", "", "");
    EXPECT_FALSE(key4.empty());
    EXPECT_FALSE(key4.complete());
    EXPECT_FALSE(key4.checkSelection());

    h2agent::model::DataKey key5("", "POST", "/foo/bar");
    EXPECT_FALSE(key5.empty());
    EXPECT_FALSE(key5.complete());
    EXPECT_FALSE(key5.checkSelection());

    h2agent::model::DataKey key6("", "", "/foo/bar");
    EXPECT_FALSE(key6.empty());
    EXPECT_FALSE(key6.complete());
    EXPECT_FALSE(key6.checkSelection());

    h2agent::model::DataKey key7("", "POST", "");
    EXPECT_FALSE(key7.empty());
    EXPECT_FALSE(key7.complete());
    EXPECT_FALSE(key7.checkSelection());

    h2agent::model::DataKey key8("", "", "");
    EXPECT_TRUE(key8.empty());
    EXPECT_FALSE(key8.complete());
    EXPECT_TRUE(key8.checkSelection());
}

TEST(keys, dataKeyTwoPartsKeyToJson) {
    h2agent::model::DataKey key("POST", "/foo/bar");

    nlohmann::json asserted;
    key.keyToJson(asserted);

    nlohmann::json expected = R"({ "method": "POST", "uri": "/foo/bar" })"_json;
    EXPECT_EQ(asserted, expected);
}

TEST(keys, dataKeyThreePartsKeyToJson) {
    h2agent::model::DataKey key("myClientEndpointId", "POST", "/foo/bar");

    nlohmann::json asserted;
    key.keyToJson(asserted);

    nlohmann::json expected = R"({ "clientEndpointId": "myClientEndpointId", "method": "POST", "uri": "/foo/bar" })"_json;
    EXPECT_EQ(asserted, expected);
}

TEST(keys, eventKeyDataKeyConstructorTwoPartsGetters) {
    h2agent::model::DataKey key("POST", "/foo/bar");
    h2agent::model::EventKey ekey(key, "1");
    EXPECT_TRUE(ekey.getClientEndpointId().empty());
    EXPECT_EQ(ekey.getMethod(), "POST");
    EXPECT_EQ(ekey.getUri(), "/foo/bar");
    EXPECT_EQ(ekey.getKey(), "POST#/foo/bar");
    EXPECT_EQ(ekey.getNKeys(), 2);
}

TEST(keys, eventKeyTwoPartsGetters) {
    h2agent::model::EventKey ekey("POST", "/foo/bar", "1");
    EXPECT_TRUE(ekey.getClientEndpointId().empty());
    EXPECT_EQ(ekey.getMethod(), "POST");
    EXPECT_EQ(ekey.getUri(), "/foo/bar");
    EXPECT_EQ(ekey.getKey(), "POST#/foo/bar");
    EXPECT_EQ(ekey.getNKeys(), 2);
}

TEST(keys, eventKeyThreePartsGetters) {
    h2agent::model::EventKey ekey("myClientEndpointId", "POST", "/foo/bar", "1");
    EXPECT_EQ(ekey.getClientEndpointId(), "myClientEndpointId");
    EXPECT_EQ(ekey.getMethod(), "POST");
    EXPECT_EQ(ekey.getUri(), "/foo/bar");
    EXPECT_EQ(ekey.getKey(), "myClientEndpointId#POST#/foo/bar");
    EXPECT_EQ(ekey.getNKeys(), 3);
}

TEST(keys, eventKeyTwoPartsCheckPositiveNumber) {
    h2agent::model::EventKey ekey("POST", "/foo/bar", "1");
    EXPECT_EQ(ekey.getNumber(), "1");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_TRUE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 1);
    EXPECT_FALSE(ekey.reverse());
}

TEST(keys, eventKeyTwoPartsCheckNegativeNumber) {
    h2agent::model::EventKey ekey("POST", "/foo/bar", "-1");
    EXPECT_EQ(ekey.getNumber(), "-1");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_TRUE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 1);
    EXPECT_TRUE(ekey.reverse());
}

TEST(keys, eventKeyTwoPartsCheckInvalidNumber) {
    h2agent::model::EventKey ekey("POST", "/foo/bar", "invalid");
    EXPECT_EQ(ekey.getNumber(), "invalid");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_FALSE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 0);
    EXPECT_FALSE(ekey.reverse());
}

TEST(keys, eventKeyThreePartsCheckPositiveNumber) {
    h2agent::model::EventKey ekey("myClientEnpointId", "POST", "/foo/bar", "invalid");
    EXPECT_EQ(ekey.getNumber(), "invalid");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_FALSE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 0);
    EXPECT_FALSE(ekey.reverse());
}

TEST(keys, eventKeyThreePartsCheckNegativeNumber) {
    h2agent::model::EventKey ekey("myClientEnpointId", "POST", "/foo/bar", "invalid");
    EXPECT_EQ(ekey.getNumber(), "invalid");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_FALSE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 0);
    EXPECT_FALSE(ekey.reverse());
}

TEST(keys, eventKeyThreePartsCheckInvalidNumber) {
    h2agent::model::EventKey ekey("myClientEnpointId", "POST", "/foo/bar", "invalid");
    EXPECT_EQ(ekey.getNumber(), "invalid");
    EXPECT_TRUE(ekey.hasNumber());
    EXPECT_FALSE(ekey.validNumber());
    EXPECT_EQ(ekey.getUNumber(), 0);
    EXPECT_FALSE(ekey.reverse());
}

TEST(keys, eventKeyTwoPartsPresence) {
    h2agent::model::EventKey ekey1("POST", "/foo/bar", "1");
    EXPECT_FALSE(ekey1.empty());
    EXPECT_TRUE(ekey1.complete());
    EXPECT_TRUE(ekey1.checkSelection());

    h2agent::model::DataKey ekey2("POST", "/foo/bar", "");
    EXPECT_FALSE(ekey2.empty());
    EXPECT_FALSE(ekey2.complete());
    EXPECT_FALSE(ekey2.checkSelection());

    h2agent::model::DataKey ekey3("", "", "1");
    EXPECT_FALSE(ekey3.empty());
    EXPECT_FALSE(ekey3.complete());
    EXPECT_FALSE(ekey3.checkSelection());

    h2agent::model::DataKey ekey4("", "", "");
    EXPECT_TRUE(ekey4.empty());
    EXPECT_FALSE(ekey4.complete());
    EXPECT_TRUE(ekey4.checkSelection());
}

TEST(keys, eventKeyThreePartsPresence) {
    h2agent::model::EventKey ekey1("myClientEndpointId", "POST", "/foo/bar", "1");
    EXPECT_FALSE(ekey1.empty());
    EXPECT_TRUE(ekey1.complete());
    EXPECT_TRUE(ekey1.checkSelection());

    h2agent::model::EventKey ekey2("myClientEndpointId", "POST", "/foo/bar", "");
    EXPECT_FALSE(ekey2.empty());
    EXPECT_FALSE(ekey2.complete());
    EXPECT_TRUE(ekey2.checkSelection());

    h2agent::model::EventKey ekey3("", "", "", "1");
    EXPECT_FALSE(ekey3.empty());
    EXPECT_FALSE(ekey3.complete());
    EXPECT_FALSE(ekey3.checkSelection());

    h2agent::model::EventKey ekey4("", "", "", "");
    EXPECT_TRUE(ekey4.empty());
    EXPECT_FALSE(ekey4.complete());
    EXPECT_TRUE(ekey4.checkSelection());
}
TEST(keys, eventKeyTwoPartsAsString) {
    h2agent::model::EventKey ekey("POST", "/foo/bar", "1");
    EXPECT_EQ(ekey.asString(), std::string("requestMethod: POST | requestUri: /foo/bar | eventNumber: 1"));
}

TEST(keys, eventKeyThreePartsAsString) {
    h2agent::model::EventKey ekey("myClientEndpointId", "POST", "/foo/bar", "1");
    EXPECT_EQ(ekey.asString(), std::string("clientEndpointId: myClientEndpointId | requestMethod: POST | requestUri: /foo/bar | eventNumber: 1"));
}

TEST(keys, eventLocationKeyTwoPartsGetters) {
    h2agent::model::EventLocationKey elkey("POST", "/foo/bar", "1", "/requestBody");
    EXPECT_TRUE(elkey.getClientEndpointId().empty());
    EXPECT_EQ(elkey.getMethod(), "POST");
    EXPECT_EQ(elkey.getUri(), "/foo/bar");
    EXPECT_EQ(elkey.getKey(), "POST#/foo/bar");
    EXPECT_EQ(elkey.getNKeys(), 2);
    EXPECT_EQ(elkey.getNumber(), "1");
    EXPECT_EQ(elkey.getPath(), "/requestBody");
}

TEST(keys, eventLocationKeyThreePartsGetters) {
    h2agent::model::EventLocationKey elkey("myClientEndpointId", "POST", "/foo/bar", "1", "/requestBody");
    EXPECT_EQ(elkey.getClientEndpointId(), "myClientEndpointId");
    EXPECT_EQ(elkey.getMethod(), "POST");
    EXPECT_EQ(elkey.getUri(), "/foo/bar");
    EXPECT_EQ(elkey.getKey(), "myClientEndpointId#POST#/foo/bar");
    EXPECT_EQ(elkey.getNKeys(), 3);
    EXPECT_EQ(elkey.getNumber(), "1");
    EXPECT_EQ(elkey.getPath(), "/requestBody");
}

TEST(keys, eventLocationKeyTwoPartsPresence) {
    h2agent::model::EventLocationKey elkey1("myClientEndpointId", "POST", "/foo/bar", "1", "/requestBody");
    EXPECT_TRUE(elkey1.checkSelection());

    h2agent::model::EventLocationKey elkey2("myClientEndpointId", "POST", "/foo/bar", "1", "");
    EXPECT_TRUE(elkey2.checkSelection());

    h2agent::model::EventLocationKey elkey3("myClientEndpointId", "POST", "/foo/bar", "", "/requestBody");
    EXPECT_FALSE(elkey3.checkSelection());

    h2agent::model::EventLocationKey elkey4("myClientEndpointId", "POST", "/foo/bar", "", "");
    EXPECT_TRUE(elkey4.checkSelection());
}
