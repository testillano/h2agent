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
    enum SourceType { RequestUri = 0, RequestUriPath, RequestUriParam, RequestBody, ResponseBody, RequestHeader, Eraser, Math, Random, RandomSet, Timestamp, Strftime, Recvseq, SVar, SGVar, Value, Event, InState, STxtFile, SBinFile, Command };
    const char* SourceTypeAsText(const SourceType & type) const
    {
        static const char* text [] = { "RequestUri", "RequestUriPath", "RequestUriParam", "RequestBody", "ResponseBody", "RequestHeader", "Eraser", "Math", "Random", "RandomSet", "Timestamp", "Strftime", "Recvseq", "SVar", "SGVar", "Value", "Event", "InState", "STxtFile", "SBinFile", "Command" };
        return text [type];
    }

    // Target type
    enum TargetType { ResponseBodyString = 0, ResponseBodyHexString, ResponseBodyJson_String, ResponseBodyJson_Integer, ResponseBodyJson_Unsigned, ResponseBodyJson_Float, ResponseBodyJson_Boolean, ResponseBodyJson_Object, ResponseBodyJson_JsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, TGVar, OutState, TTxtFile, TBinFile };
    const char* TargetTypeAsText(const TargetType & type) const
    {
        static const char* text [] = { "ResponseBodyString", "ResponseBodyHexString", "ResponseBodyJson_String", "ResponseBodyJson_Integer", "ResponseBodyJson_Unsigned", "ResponseBodyJson_Float", "ResponseBodyJson_Boolean", "ResponseBodyJson_Object", "ResponseBodyJson_JsonString", "ResponseHeader", "ResponseStatusCode", "ResponseDelayMs", "TVar", "TGVar", "OutState", "TTxtFile", "TBinFile" };
        return text [type];
    }

    // Filter type
    enum FilterType { RegexCapture = 0, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar, EqualTo };
    const char* FilterTypeAsText(const FilterType & type) const
    {
        static const char* text [] = { "RegexCapture", "RegexReplace", "Append", "Prepend", "AppendVar", "PrependVar", "Sum", "Multiply", "ConditionVar", "EqualTo" };
        return text [type];
    }

    // setters:

    /**
     * Load transformation information
     *
     * @param j Json transformation object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j);


    /**
     * Class string representation
     *
     * @return string representation
     */
    std::string asString() const;


private:

    SourceType source_type_{};
    std::string source_{}; // RequestUriParam, RequestBody(empty: whole, path: node), ResponseBody(empty: whole, path: node),
    // RequestHeader, Math, Timestamp, Strftime, SVar, SGVar, Value, Event, STxtFile(path), SBinFile (path), Command(expression)
    std::vector<std::string> source_tokenized_{}; // RandomSet
    int source_i1_{}, source_i2_{}; // Random

    TargetType target_type_{};
    std::string target_{}; // ResponseBodyJson_String/Integer/Unsigned/Float/Boolean/Object/JsonString(empty: whole, path: node),
    // ResponseHeader, TVar, TGVar, OutState (foreign method part), TTxtFile(path), TBinFile (path)
    std::string target2_{}; // OutState (foreign uri part)

    bool has_filter_{};
    FilterType filter_type_{};
    std::string filter_{}; // RegexReplace(fmt), RegexCapture(literal, although not actually needed, but useful to access & print on traces), Append, Prepend, AppendVar, PrependVar, ConditionVar, EqualTo
    std::regex filter_rgx_{}; // RegexCapture, RegexReplace
    int filter_number_type_{}; // Sum, Multiply (0: integer, 1: unsigned, 2: float)
    std::int64_t filter_i_{}; // Sum, Multiply
    std::uint64_t filter_u_{}; // Sum, Multiply
    double filter_f_{}; // Sum, Multiply

    /**
     * Builds a map of patterns (pattern, varname) where pattern is @{varname}
     *
     * @param str string to analyze
     * @param patterns map generated by reference
     */
    void collectVariablePatterns(const std::string &str, std::map<std::string, std::string> &patterns);

    std::map<std::string, std::string> source_patterns_;
    std::map<std::string, std::string> target_patterns_;
    std::map<std::string, std::string> target2_patterns_;

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
    /** Gets target2 */
    const std::string &getTarget2() const {
        return target2_;
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
    /** Integer container for sum/multiply */
    std::int64_t getFilterI() const {
        return filter_i_;
    }
    /** Unsigned integer container for sum/multiply */
    std::uint64_t getFilterU() const {
        return filter_u_;
    }
    /** Float container for sum/multiply */
    double getFilterF() const {
        return filter_f_;
    }

    /** Source patterns */
    const std::map<std::string, std::string> &getSourcePatterns() const {
        return source_patterns_;
    }
    /** Target patterns */
    const std::map<std::string, std::string> &getTargetPatterns() const {
        return target_patterns_;
    }
    /** Target2 patterns */
    const std::map<std::string, std::string> &getTarget2Patterns() const {
        return target2_patterns_;
    }
};

}
}
