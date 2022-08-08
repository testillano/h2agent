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

#include <cinttypes> // PRIi64, etc.

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
        std::string msg = ert::tracing::Logger::asString("String source to 'search/replace all': %s | from: %s | to: %s", str.c_str(), from.c_str(), to.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
    std::string::size_type pos = 0u;
    while((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String result of 'search/replace all': %s", str.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
}

void replaceVariables(std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars, const std::unordered_map<std::string,std::string> &gvars) {

    if (patterns.empty()) return;
    if (vars.empty() && gvars.empty()) return;

    std::map<std::string,std::string>::const_iterator it;
    std::unordered_map<std::string,std::string>::const_iterator git;

    for (auto pit = patterns.begin(); pit != patterns.end(); pit++) {

        // local var has priority over a global var with the same name
        if (!vars.empty()) {
            it = vars.find(pit->second);
            if (it != vars.end()) {
                searchReplaceAll(str, pit->first, it->second);
                continue; // all is done
            }
        }

        if (!gvars.empty()) { // this is much more efficient that find() == end() below
            git = gvars.find(pit->second);
            if (git != gvars.end()) {
                searchReplaceAll(str, pit->first, git->second);
            }
        }
    }
}

void TypeConverter::setString(const std::string &str) {
    clear();
    s_value_ = str;
    native_type_ = NativeType::String;
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("String value: %s", s_value_.c_str()), ERT_FILE_LOCATION));
}

void TypeConverter::setInteger(const std::int64_t i) {
    clear();
    i_value_ = i;
    native_type_ = NativeType::Integer;
    LOGDEBUG(std::string fmt = std::string("Integer value: %") + PRIi64; ert::tracing::Logger::debug(ert::tracing::Logger::asString(fmt.c_str(), i_value_), ERT_FILE_LOCATION));
}

void TypeConverter::setUnsigned(const std::uint64_t u) {
    clear();
    u_value_ = u;
    native_type_ = NativeType::Unsigned;
    LOGDEBUG(std::string fmt = std::string("Unsigned value: %") + PRIu64; ert::tracing::Logger::debug(ert::tracing::Logger::asString(fmt.c_str(), u_value_), ERT_FILE_LOCATION));
}

void TypeConverter::setFloat(const double f) {
    clear();
    f_value_ = f;
    native_type_ = NativeType::Float;
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Float value: %lf", f_value_), ERT_FILE_LOCATION));
}

void TypeConverter::setBoolean(bool boolean) {
    clear();
    b_value_ = boolean;
    native_type_ = NativeType::Boolean;
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Boolean value: %s", b_value_ ? "true":"false"), ERT_FILE_LOCATION));
}

void TypeConverter::setStringReplacingVariables(const std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars, const std::unordered_map<std::string,std::string> &gvars) {

    setString(str);
    replaceVariables(s_value_, patterns, vars, gvars);
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
        std::string msg;
    if (success) {
    msg = ert::tracing::Logger::asString("String value: %s", s_value_.c_str());
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get string representation for source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
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
        std::string msg;
    if (success) {
    std::string fmt = std::string("Integer value: %") + PRIi64;
        msg = ert::tracing::Logger::asString(fmt.c_str(), i_value_);
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get integer representation for source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
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
        std::string msg;
    if (success) {
    std::string fmt = std::string("Unsigned value: %") + PRIu64;
        msg = ert::tracing::Logger::asString(fmt.c_str(), u_value_);
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get unsigned integer representation for source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
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
        std::string msg;
    if (success) {
    msg = ert::tracing::Logger::asString("Float value: %lf", f_value_);
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get float number representation for source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
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
        std::string msg;
    if (success) {
    msg = ert::tracing::Logger::asString("Boolean value: %s", b_value_ ? "true":"false");
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get boolean representation for source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    return b_value_;
}

const nlohmann::json &TypeConverter::getObject(bool &success) {

    success = (native_type_ == NativeType::Object);

    LOGDEBUG(
        std::string msg;
    if (success) {
    msg = ert::tracing::Logger::asString("Json object value: %s", j_value_.dump().c_str());
    }
    else {
        msg = ert::tracing::Logger::asString("Unable to get json object from source: %s", asString().c_str());
    }

    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
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

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Json path: %s | Json object: %s", path.c_str(), jsonSource.dump().c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    if (path.empty()) {
        j_value_ = jsonSource;
    }
    else {
        try {
            nlohmann::json::json_pointer j_ptr(path);
            // operator[] aborts in debug compilation when the json pointer path is not found.
            // It is safer to use at() method which has "bounds checking":
            // j_value_ = jsonSource[j_ptr];
            j_value_ = jsonSource.at(j_ptr);
            if (j_value_.empty()) return false; // null extracted (path not found)
        }
        catch (std::exception& e)
        {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            return false;
        }
    }

    if (j_value_.is_object()) {
        native_type_ = NativeType::Object;
    }
    else if (j_value_.is_string()) {
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
    else {
        ert::tracing::Logger::error("Unrecognized json pointer value format", ERT_FILE_LOCATION); // this shouldn't happen as all the possible formats are checked above
        return false;
    }

    return true;
}

std::string TypeConverter::asString() {

    std::stringstream ss;
    ss << "NativeType (String = 0, Integer, Unsigned, Float, Boolean, Object): " << getNativeType()
       << " | " << ((getNativeType() == NativeType::String) ? "STRING":"String") << ": " << s_value_
       << " | " << ((getNativeType() == NativeType::Integer) ? "INTEGER":"Integer") << ": " << i_value_
       << " | " << ((getNativeType() == NativeType::Unsigned) ? "UNSIGNED":"Unsigned") << ": " << u_value_
       << " | " << ((getNativeType() == NativeType::Float) ? "FLOAT":"Float") << ": " << f_value_
       << " | " << ((getNativeType() == NativeType::Boolean) ? "BOOLEAN":"Boolean") << ": " << (b_value_ ? "true":"false")
       << " | " << ((getNativeType() == NativeType::Object) ? "OBJECT":"Object") << ": " << j_value_.dump();

    return (ss.str());
}

}
}
