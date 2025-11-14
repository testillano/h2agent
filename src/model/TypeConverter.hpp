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

#pragma once


#include <nlohmann/json.hpp>
#include <string>
#include <sstream>
#include <cstdint>
#include <GlobalVariable.hpp>

namespace h2agent
{
namespace model
{
/**
 * Search/replace all the items in provided string
 *
 * @param str string passed by reference
 * @param from pattern to search
 * @param to value to replace in pattern ocurrences
 */
void searchReplaceAll(std::string& str, const std::string& from, const std::string& to);

/**
 * Replace variable patterns with their variable values using 2 sources:
 * local variables and global variables. Local variables are replaced with
 * priority over existing global variables with the same name.
 *
 * @param str string to update with replaced values
 * @param patterns map of patterns (pattern, varname) where pattern is @{varname}
 * @param vars local variables source map
 * @param gvars global variables
 */
void replaceVariables(std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars, GlobalVariable *gvars);


class TypeConverter {

public:

    enum NativeType { String = 0, Integer, Unsigned, Float, Boolean, Object };

private:

    std::string s_value_{}; // string
    std::int64_t i_value_{}; // integer
    std::uint64_t u_value_{}; // unsigned integer
    double f_value_{}; // float number
    bool b_value_{}; // boolean
    nlohmann::json j_value_{}; // json object

    NativeType native_type_{};

public:

    /**
    * Clears class content
    */
    void clear();

    /**
    * Default constructor
    */
    TypeConverter() {
        clear() ;
    }

    // setters

    /**
    * Sets string to vault
    *
    * @param str String assigned
    */
    void setString(const std::string &str);

    /**
    * Sets string to vault replacing variables in form @{varname}
    * This is done here to avoid a string copy
    *
    * @param str string assigned
    * @param patterns map of patterns (pattern, varname) where pattern is @{varname}
    * @param vars local variables source map
    * @param gvars global variables
    */
    void setStringReplacingVariables(const std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars, GlobalVariable *gvars);

    /**
    * Sets integer to vault
    *
    * @param i Number assigned
    */
    void setInteger(const std::int64_t i);

    /**
    * Sets unsigned integer to vault
    *
    * @param u Unsigned integer assigned
    */
    void setUnsigned(const std::uint64_t u);

    /**
    * Sets float number to vault
    *
    * @param f float number assigned
    */
    void setFloat(const double f);

    /**
    * Sets boolean to vault
    *
    * @param boolean Boolean assigned
    */
    void setBoolean(bool boolean);

    /**
    * Sets object to vault
    *
    * If the object is a string, this automatically will assing a string (@see setString()).
    * If the object is a integer, this automatically will assing an integer (@see setInteger()).
    * If the object is an unsigned integer, this automatically will assing an unsigned integer (@see setUnsigned()).
    * If the object is a float number, this automatically will assing a float number (@see setFloat()).
    * If the object is a boolean, this automatically will assing a boolean (@see setBoolean()).
    *
    * @param jsonSource Json Document from which extract the information
    * @param path Json path to extract from provided json document
    *
    * @return Return boolean for successful extraction (path is found), false otherwise (null extracted)
    */
    bool setObject(const nlohmann::json &jsonSource, const std::string &path);

    // getters

    /**
    * Gets native type selected from source
    * Thisi is useful to use the corresponding getter if no conversion is desired
    */
    NativeType getNativeType() const {
        return native_type_;
    }

    /**
    * Gets string type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result String data reference (you may check success result)
    * @warning multiple accesses could imply multiple conversions
    */
    const std::string &getString(bool &success);

    /**
    * Gets integer type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result Integer data reference (you may check success result)
    * @warning multiple accesses could imply multiple conversions
    */
    std::int64_t getInteger(bool &success);

    /**
    * Gets unsigned integer type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result Unsigned integer data reference (you may check success result)
    * @warning multiple accesses could imply multiple conversions
    */
    std::uint64_t getUnsigned(bool &success);

    /**
    * Gets float number type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result Float number data reference (you may check success result)
    * @warning multiple accesses could imply multiple conversions
    */
    double getFloat(bool &success);

    /**
    * Gets boolean type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result Boolean data reference (you may check success result)
    * @warning multiple accesses could imply multiple conversions
    */
    bool getBoolean(bool &success);

    /**
    * Gets json object type from vault
    *
    * @param success operation boolean result passed by reference
    *
    * @result Json object data reference (you may check success result)
    * @warning no conversions from other types are done here
    */
    const nlohmann::json &getObject(bool &success);

    /**
    * Class string representation
    *
    * Member attributes may not be updated until their corresponding
    * getters are invoked, then, this representation is ONLY RELIABLE
    * for current native type.
    */
    std::string asString();
};

}
}
