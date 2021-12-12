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


#include <string>
#include <vector>
#include <regex>
#include <cstdint>


#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

class Transformation
{
public:

    Transformation(): source_(""), source_i1_(0), source_i2_(0),
        target_(""),
        has_filter_(false), filter_(""), filter_number_type_(0), filter_i_(0), filter_u_(0), filter_f_(0) {;}

    // Source type
    enum SourceType { RequestUri = 0, RequestUriPath, RequestUriParam, RequestBody, RequestHeader, GeneralRandom, GeneralRandomSet, GeneralTimestamp, GeneralStrftime, GeneralUnique, SVar, Value, Event, InState };
    // Target type
    enum TargetType { ResponseBodyString = 0, ResponseBodyInteger, ResponseBodyUnsigned, ResponseBodyFloat, ResponseBodyBoolean, ResponseBodyObject, ResponseBodyJsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, OutState };
    // Filter type
    enum FilterType { RegexCapture = 0, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar };

    // setters:

    /**
     * Load transformation information
     *
     * @param j Json transformation object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j);


private:

    SourceType source_type_;
    std::string source_; // RequestUriParam, RequestBody(empty: whole, path: node), RequestHeader, GeneralTimestamp, GeneralStrftime, SVar, Value, Event
    std::vector<std::string> source_tokenized_; // GeneralRandomSet
    int source_i1_, source_i2_; // GeneralRandom

    TargetType target_type_;
    std::string target_; // ResponseBodyString/Integer/Unsigned/Float/Boolean/Object/JsonString(empty: whole, path: node), ResponseHeader, TVar

    bool has_filter_;
    FilterType filter_type_;
    std::string filter_; // RegexReplace(fmt), RegexCapture(literal, although not actually needed, but useful to access & print on traces), Append, Prepend, AppendVar, PrependVar, ConditionVar
    std::regex filter_rgx_; // RegexCapture, RegexReplace
    int filter_number_type_; // Sum, Multiply (0: integer, 1: unsigned, 2: float)
    std::int64_t filter_i_; // Sum, Multiply
    std::uint64_t filter_u_; // Sum, Multiply
    double filter_f_; // Sum, Multiply

public:

    // getters:

    /** Gets source type */
    SourceType getSourceType() const {
        return source_type_;
    }
    /** Gets source */
    const std::string &getSource() const {
        return source_;
    }
    /** Gets source tokenized */
    const std::vector<std::string> &getSourceTokenized() const {
        return source_tokenized_;
    }
    /** Gets source for random min */
    int getSourceI1() const {
        return source_i1_;
    }
    /** Gets source for random max */
    int getSourceI2() const {
        return source_i2_;
    }

    /** Gets target type */
    TargetType getTargetType() const {
        return target_type_;
    }
    /** Gets target */
    const std::string &getTarget() const {
        return target_;
    }

    /** Gets filter existence */
    bool hasFilter() const {
        return has_filter_;
    }
    /** Gets filter type */
    FilterType getFilterType() const {
        return filter_type_;
    }
    /** Gets filter */
    const std::string &getFilter() const {
        return filter_;
    }
    /** Gets filter regex */
    const std::regex &getFilterRegex() const {
        return filter_rgx_;
    }
    /** Gets filter type for sum/multiply */
    int getFilterNumberType() const {
        return filter_number_type_;
    }
    std::int64_t getFilterI() const {
        return filter_i_;
    }
    std::uint64_t getFilterU() const {
        return filter_u_;
    }
    double getFilterF() const {
        return filter_f_;
    }
};

}
}
