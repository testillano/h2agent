#include <TypeConverter.hpp>

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class TypeConverter_test : public ::testing::Test
{
public:
    h2agent::model::TypeConverter tconv_{};
    std::map<std::string, std::string> vars_{};
    nlohmann::json json_{};

    TypeConverter_test() {
        vars_["var1"] = "value1";
        vars_["var2"] = "value2";
        json_ = R"({
            "path_to_basics": {
              "string": "hello",
              "integer": -111,
              "unsigned": 111,
              "float": 3.14,
              "boolean": true
            },
            "path_to_object": {
              "foo": 1,
              "bar": 2
            }
          })"_json;
    }
};

TEST_F(TypeConverter_test, searchReplaceAll)
{
    std::string source = "var=@{var}; var=@{var}";
    std::string expected = "var=value; var=value";

    std::string result = source;
    h2agent::model::searchReplaceAll(result, "@{var}", "value");

    EXPECT_EQ(result, expected);
}

TEST_F(TypeConverter_test, searchReplaceValueVariables)
{
    std::string source = "var1=@{var1}; var2=@{var2}; var1var2=@{var1}@{var2}";
    std::string expected = "var1=value1; var2=value2; var1var2=value1value2";

    std::string result = source;
    h2agent::model::searchReplaceValueVariables(vars_, result);

    EXPECT_EQ(result, expected);
}

TEST_F(TypeConverter_test, setString)
{
    std::string value = "hello";
    tconv_.setString(value);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, value);
    EXPECT_TRUE(success);

    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, 0);
    EXPECT_FALSE(success);

    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, 0);
    EXPECT_FALSE(success);

    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, 0);
    EXPECT_FALSE(success);

    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    res_object = tconv_.getObject(success);
    EXPECT_TRUE(res_object.empty());
    EXPECT_FALSE(success);

    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 0 | STRING: hello | Integer: 0 | Unsigned: 0 | Float: 0 | Boolean: true | Object: null";
    EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setStringReplacingVariables)
{
    std::string value = "@{var1}";
    tconv_.setStringReplacingVariables(value, vars_);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "value1");
    EXPECT_TRUE(success);

    // The rest of checkings are redundant regarding 'setString' test

    // asString() requires updating avery member through getters !
    // TypeConverter class representation:
    //std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 0 | STRING: value1 | Integer: 0 | Unsigned: 0 | Float: 0 | Boolean: true | Object: null";
    //EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setInteger)
{
    std::int64_t value = -111;
    tconv_.setInteger(value);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "-111");
    EXPECT_TRUE(success);

    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, value);
    EXPECT_TRUE(success);

    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, (std::uint64_t)value);
    EXPECT_TRUE(success);

    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, (double)value);
    EXPECT_TRUE(success);

    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    res_object = tconv_.getObject(success);
    EXPECT_TRUE(res_object.empty());
    EXPECT_FALSE(success);

    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 1 | String: -111 | INTEGER: -111 | Unsigned: 18446744073709551505 | Float: -111 | Boolean: true | Object: null";
    EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setUnsigned)
{
    std::uint64_t value = 111;
    tconv_.setUnsigned(value);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "111");
    EXPECT_TRUE(success);

    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, (std::int64_t)value);
    EXPECT_TRUE(success);

    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, value);
    EXPECT_TRUE(success);

    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, (double)value);
    EXPECT_TRUE(success);

    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    res_object = tconv_.getObject(success);
    EXPECT_TRUE(res_object.empty());
    EXPECT_FALSE(success);

    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 2 | String: 111 | Integer: 111 | UNSIGNED: 111 | Float: 111 | Boolean: true | Object: null";
    EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setFloat)
{
    double value = 3.14;
    tconv_.setFloat(value);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "3.140000");
    EXPECT_TRUE(success);

    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, (std::int64_t)value);
    EXPECT_TRUE(success);

    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, (std::uint64_t)value);
    EXPECT_TRUE(success);

    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, value);
    EXPECT_TRUE(success);

    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    res_object = tconv_.getObject(success);
    EXPECT_TRUE(res_object.empty());
    EXPECT_FALSE(success);

    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 3 | String: 3.140000 | Integer: 3 | Unsigned: 3 | FLOAT: 3.14 | Boolean: true | Object: null";
    EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setBoolean)
{
    bool value = true;
    tconv_.setBoolean(value);

    bool success;

    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "true");
    EXPECT_TRUE(success);

    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, 1);
    EXPECT_TRUE(success);

    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, 1);
    EXPECT_TRUE(success);

    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, 1);
    EXPECT_TRUE(success);

    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    res_object = tconv_.getObject(success);
    EXPECT_TRUE(res_object.empty());
    EXPECT_FALSE(success);

    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 4 | String: true | Integer: 1 | Unsigned: 1 | Float: 1 | BOOLEAN: true | Object: null";
    EXPECT_EQ(tconv_.asString(), str);
}

TEST_F(TypeConverter_test, setObject)
{
    bool success;

    // We won't test all the combinations because it is done in tests above

    tconv_.setObject(json_, "/path_to_basics/string");
    std::string res_string = tconv_.getString(success);
    EXPECT_EQ(res_string, "hello");
    EXPECT_TRUE(success);

    tconv_.setObject(json_, "/path_to_basics/integer");
    std::int64_t res_integer = tconv_.getInteger(success);
    EXPECT_EQ(res_integer, -111);
    EXPECT_TRUE(success);

    tconv_.setObject(json_, "/path_to_basics/unsigned");
    std::uint64_t res_unsigned = tconv_.getUnsigned(success);
    EXPECT_EQ(res_unsigned, 111);
    EXPECT_TRUE(success);

    tconv_.setObject(json_, "/path_to_basics/float");
    double res_float = tconv_.getFloat(success);
    EXPECT_EQ(res_float, 3.14);
    EXPECT_TRUE(success);

    tconv_.setObject(json_, "/path_to_basics/boolean");
    bool res_boolean = tconv_.getBoolean(success);
    EXPECT_EQ(res_boolean, true);
    EXPECT_TRUE(success);

    nlohmann::json res_object;
    tconv_.setObject(json_, "/path_to_object");
    res_object = tconv_.getObject(success);
    EXPECT_EQ(res_object.dump(), "{\"bar\":2,\"foo\":1}");
    EXPECT_TRUE(success);
    // TypeConverter class representation:
    std::string str = "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): 5 | String:  | Integer: 0 | Unsigned: 0 | Float: 0 | Boolean: false | OBJECT: {\"bar\":2,\"foo\":1}";
    EXPECT_EQ(tconv_.asString(), str);
}

