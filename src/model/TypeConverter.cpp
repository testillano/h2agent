/*
 ___________________________________________
|    _     ___                        _     |
|   | |   |__ \                      | |    |
|   | |__    ) |__ _  __ _  ___ _ __ | |_   |
|   | '_ \  / // _` |/ _` |/ _ \ '_ \| __|  |  HTTP/2 AGENT FOR MOCK TESTING
|   | | | |/ /| (_| | (_| |  __/ | | | |_   |  Version 0.0.z
|   |_| |_|____\__,_|\__, |\___|_| |_|\__|  |  https://github.com/testillano/h2agent
|                     __/ |                 |
|                    |___/                  |
|___________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include <ert/tracing/Logger.hpp>

#include <TypeConverter.hpp>


namespace h2agent
{
namespace model
{

void searchReplaceAll(std::string& str,
                      const std::string& from,
                      const std::string& to)
{
    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String to replace all: %s | from: %s | to: %s", str.c_str(), from.c_str(), to.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
    std::string::size_type pos = 0u;
    while((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

void searchReplaceValueVariables(const std::map<std::string, std::string> &varmap, std::string &source) {

    static std::string token_op("@{");
    static std::string token_cl("}");

    if (source.find(token_op) == std::string::npos) return;

    for(auto it = varmap.begin(); it != varmap.end(); it++) {
        searchReplaceAll(source, token_op + it->first + token_cl, it->second);
    }
}

void TypeConverter::setStringReplacingVariables(const std::string &str, const std::map<std::string, std::string> variables) {

    setString(str);
    searchReplaceValueVariables(variables, s_value_);
}

const std::string &TypeConverter::getString(bool &success) {

    success = true;

    if (native_type_ == NativeType::Object) {
        success = false;
    }
    else if (native_type_ == NativeType::Integer) s_value_ = std::to_string(i_value_);
    else if (native_type_ == NativeType::Unsigned) s_value_ = std::to_string(u_value_);
    else if (native_type_ == NativeType::Float) s_value_ = std::to_string(f_value_);
    else if (native_type_ == NativeType::Boolean) s_value_ = b_value_ ? "true":"false";

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get string representation for source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return s_value_;
}

std::int64_t TypeConverter::getInteger(bool &success) {

    success = true;

    if (native_type_ == NativeType::Object) {
        success = false;
    }
    else if (native_type_ == NativeType::String) {
        try {
            i_value_ = std::stoll(s_value_);
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to long long integer: %s", s_value_.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
            success = false;
        }
    }
    else if (native_type_ == NativeType::Unsigned) i_value_ = std::int64_t(u_value_);
    else if (native_type_ == NativeType::Float) i_value_ = std::int64_t(f_value_);
    else if (native_type_ == NativeType::Boolean) i_value_ = std::int64_t(b_value_ ? 1:0);

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get integer representation for source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return i_value_;
}

std::uint64_t TypeConverter::getUnsigned(bool &success) {

    success = true;

    if (native_type_ == NativeType::Object) {
        success = false;
    }
    else if (native_type_ == NativeType::String) {
        try {
            u_value_ = std::stoull(s_value_);
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to unsigned long long integer: %s", s_value_.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
            success = false;
        }
    }
    else if (native_type_ == NativeType::Integer) u_value_ = std::uint64_t(i_value_);
    else if (native_type_ == NativeType::Float) u_value_ = std::uint64_t(f_value_);
    else if (native_type_ == NativeType::Boolean) u_value_ = std::uint64_t(b_value_ ? 1:0);

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get unsigned integer representation for source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return u_value_;
}

double TypeConverter::getFloat(bool &success) {

    success = true;

    if (native_type_ == NativeType::Object) {
        success = false;
    }
    else if (native_type_ == NativeType::String) {
        try {
            f_value_ = std::stod(s_value_);
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to float number: %s", s_value_.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
            success = false;
        }
    }
    else if (native_type_ == NativeType::Integer) f_value_ = double(i_value_);
    else if (native_type_ == NativeType::Unsigned) f_value_ = double(u_value_);
    else if (native_type_ == NativeType::Boolean) f_value_ = double(b_value_ ? 1:0);

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get float number representation for source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return f_value_;
}

bool TypeConverter::getBoolean(bool &success) {

    success = true;

    if (native_type_ == NativeType::Object) {
        success = false;
    }
    else if (native_type_ == NativeType::String) b_value_ = (s_value_.empty() ? false:true);
    else if (native_type_ == NativeType::Integer) b_value_ = ((i_value_ != (std::int64_t)0) ? true:false);
    else if (native_type_ == NativeType::Unsigned) b_value_ = ((u_value_ != (std::uint64_t)0) ? true:false);
    else if (native_type_ == NativeType::Float) b_value_ = ((f_value_ != (double)0) ? true:false);

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get boolean representation for source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return b_value_;
}

const nlohmann::json &TypeConverter::getObject(bool &success) {

    success = (native_type_ == NativeType::Object);

    LOGDEBUG(
    if (!success) {
    std::string msg = ert::tracing::Logger::asString("Unable to get json object from source:\n%s", asString().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    }
    );

    return j_value_;
}

void TypeConverter::clear() {
    s_value_ = "";
    i_value_ = 0;
    u_value_ = 0;
    f_value_ = 0;
    b_value_ = false;
    j_value_.clear();
    native_type_ = NativeType::String;
}

bool TypeConverter::setObject(const nlohmann::json &jsonSource, const std::string &path) {
    clear();
    try {
        nlohmann::json::json_pointer j_ptr(path);
        j_value_ = jsonSource[j_ptr];
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
    }

    if (j_value_.empty()) return false; // null extracted (path not found)

    native_type_ = NativeType::Object; // assumed

    if (j_value_.is_string()) {
        s_value_ = j_value_;
        native_type_ = NativeType::String;
    }
    else if (j_value_.is_number_integer()) {
        i_value_ = j_value_;
        native_type_ = NativeType::Integer;
    }
    else if (j_value_.is_number_unsigned()) {
        u_value_ = j_value_;
        native_type_ = NativeType::Unsigned;
    }
    else if (j_value_.is_number_float()) {
        f_value_ = j_value_;
        native_type_ = NativeType::Float;
    }
    else if (j_value_.is_boolean()) {
        b_value_ = j_value_;
        native_type_ = NativeType::Boolean;
    }

    return true;
}

std::string s_value_; // string
std::int64_t i_value_; // integer
std::uint64_t u_value_; // unsigned integer
double f_value_; // float number
bool b_value_; // boolean
nlohmann::json j_value_; // json object



std::string TypeConverter::asString() {

    bool success;
    std::stringstream ss;
    ss << "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): " << getNativeType()
       << "|  String: " << s_value_
       << " | Integer: " << i_value_
       << " | Unsigned integer: " << u_value_
       << " | Float number: " << f_value_
       << " | Boolean: " << (b_value_ ? "true":"false")
       << " | Object: " << j_value_.dump();

    return ss.str();
}

}
}
