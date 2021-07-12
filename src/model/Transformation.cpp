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

#include <Transformation.hpp>

#include <sstream>
#include <algorithm>

namespace h2agent
{
namespace model
{


bool Transformation::load(const nlohmann::json &j) {

    // Mandatory
    auto source_it = j.find("source");
    std::string sourceSpec = *source_it;

    auto target_it = j.find("target");
    std::string targetSpec = *target_it;

    // Optional
    auto it = j.find("filter");
    filter_ = "";
    has_filter_ = false;
    if (it != j.end()) {
        has_filter_ = true;

        // [filter_type_]
        //   RegexCapture        regular expression literal -> [filter_rgx_]
        //   RegexReplace        regular expression literal (rgx) -> [filter_rgx_]  /  replace format (fmt) -> [filter_]
        //   Append              suffix value -> [filter_]
        //   Prepend             prefix value -> [filter_]
        //   AppendVar           variable name with suffix value -> [filter_]
        //   PrependVar          variable name with prefix value -> [filter_]
        //   Sum                 amount -> [filter_i_/filter_u_/filter_f_/filter_number_type_]
        //   Multiply            amount -> [filter_i_/filter_u_/filter_f_/filter_number_type_]
        //   ConditionVar        variable name -> [filter_]

        auto f_it = it->find("RegexCapture");

        try {
            if (f_it != it->end()) {
                filter_ = *f_it;
                filter_rgx_.assign(*f_it);
                filter_type_ = FilterType::RegexCapture;
            }
            else if ((f_it = it->find("RegexReplace")) != it->end()) {
                filter_rgx_.assign(*(f_it->find("rgx")));
                filter_ = *(f_it->find("fmt"));
                filter_type_ = FilterType::RegexReplace;
            }
            else if ((f_it = it->find("Append")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::Append;
            }
            else if ((f_it = it->find("Prepend")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::Prepend;
            }
            else if ((f_it = it->find("AppendVar")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::AppendVar;
            }
            else if ((f_it = it->find("PrependVar")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::PrependVar;
            }
            else if ((f_it = it->find("Sum")) != it->end()) {
                if (f_it->is_number_integer()) {
                    filter_i_ = *f_it;
                    filter_number_type_ = 0 ;
                }
                else if (f_it->is_number_unsigned()) {
                    filter_u_ = *f_it;
                    filter_number_type_ = 1 ;
                }
                else if (f_it->is_number_float()) {
                    filter_f_ = *f_it;
                    filter_number_type_ = 2 ;
                }
                filter_type_ = FilterType::Sum;
            }
            else if ((f_it = it->find("Multiply")) != it->end()) {
                if (f_it->is_number_integer()) {
                    filter_i_ = *f_it;
                    filter_number_type_ = 0 ;
                }
                else if (f_it->is_number_unsigned()) {
                    filter_u_ = *f_it;
                    filter_number_type_ = 1 ;
                }
                else if (f_it->is_number_float()) {
                    filter_f_ = *f_it;
                    filter_number_type_ = 2 ;
                }
                filter_type_ = FilterType::Multiply;
            }
            else if ((f_it = it->find("ConditionVar")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::ConditionVar;
            }
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            return false;
        }
    }

    // Interpret source/target:

    // SOURCE (enum SourceType { RequestUri = 0, RequestUriPath, RequestUriParam, RequestBody, RequestHeader, GeneralRandom, GeneralTimestamp, GeneralStrftime, GeneralUnique, SVar, Value, Event, InState };)
    source_ = ""; // empty by default (-), as many cases are only work modes and no parameters(+) are included in their transformation configuration

    // Source specifications:
    // - request.uri: whole request *URI*  path, including the possible query parameters.
    // - request.uri.path: request *URI* path part.
    // + request.uri.param.<name>: request URI specific parameter `<name>`.
    // - request.body: request body document.
    // + request.body.<node1>..<nodeN>: request body node path.
    // + request.header.<hname>: request header component (i.e. *content-type*).
    // + general.random.<min>.<max>: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.
    // + general.timestamp.<unit>: UNIX epoch time in `s` (seconds), `ms` (milliseconds) or `ns` (nanoseconds).
    // + general.strftime.<format>: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/).
    // - general.recvseq: sequence id increased for every mock reception (starts on *0* when the *h2agent*  is started).
    // + var.<id>: general purpose variable.
    // - value.<value>: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string), will be converted to the target type.
    // - inState: current processing state.

    // Regex needed:
    static std::regex requestUriParam("^request.uri.param.(.*)"); // no need to escape dots as this is validated in schema
    static std::regex requestBodyNode("^request.body.(.*)");
    static std::regex requestHeader("^request.header.(.*)");
    static std::regex generalRandom("^general\\.random\\.([-+]{0,1}[0-9]+)\\.([-+]{0,1}[0-9]+)$"); // no need to validate min/max as it was done at schema
    static std::regex generalTimestamp("general.timestamp.(.*)"); // no need to validate s/ms/ns as it was done at schema
    static std::regex generalStrftime("general.strftime.(.*)"); // free format, errors captured
    static std::regex varId("var.(.*)");
    static std::regex value("value.(.*)");
    static std::regex event("event.(.*)");

    std::smatch matches; // to capture regex group(s)

    try {
        if (sourceSpec == "request.uri") {
            source_type_ = SourceType::RequestUri;
        }
        else if (sourceSpec == "request.uri.path") {
            source_type_ = SourceType::RequestUriPath;
        }
        else if (std::regex_match(sourceSpec, matches, requestUriParam)) { // parameter name
            source_ = matches[1];
            source_type_ = SourceType::RequestUriParam;
        }
        else if (sourceSpec == "request.body") { // whole document
            source_type_ = SourceType::RequestBody;
        }
        else if (std::regex_match(sourceSpec, matches, requestBodyNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "request.body.foo.bar" turns into "/foo/bar")
            source_ = matches[1];
            std::replace(source_.begin(), source_.end(), '.', '/');
            source_.insert(source_.begin(), '/');
            source_type_ = SourceType::RequestBody;
        }
        else if (std::regex_match(sourceSpec, matches, requestHeader)) { // header name
            source_ = matches[1];
            source_type_ = SourceType::RequestHeader;
        }
        else if (std::regex_match(sourceSpec, matches, generalRandom)) { // range "<min>.<max>", i.e.: "-3.8", "0.100", "-15.+2", etc. These go to -> [source_i1_] and [source_i2_]
            source_i1_ = stoi(matches[1]);
            source_i2_ = stoi(matches[2]);
            source_type_ = SourceType::GeneralRandom;
        }
        else if (std::regex_match(sourceSpec, matches, generalTimestamp)) { // unit (s: seconds, ms: milliseconds, ns: nanoseconds)
            source_ = matches[1];
            source_type_ = SourceType::GeneralTimestamp;
        }
        else if (std::regex_match(sourceSpec, matches, generalStrftime)) { // current date/time formatted by as described in https://www.cplusplus.com/reference/ctime/strftime/
            source_ = matches[1];
            source_type_ = SourceType::GeneralStrftime;
        }
        else if (sourceSpec == "general.recvseq") {
            source_type_ = SourceType::GeneralUnique;
        }
        else if (std::regex_match(sourceSpec, matches, varId)) { // variable id
            source_ = matches[1];
            source_type_ = SourceType::SVar;
        }
        else if (std::regex_match(sourceSpec, matches, value)) { // value content
            source_ = matches[1];
            source_type_ = SourceType::Value;
        }
        else if (std::regex_match(sourceSpec, matches, event)) { // value content
            source_ = matches[1];
            source_type_ = SourceType::Event;
        }
        else if (sourceSpec == "inState") {
            source_type_ = SourceType::InState;
        }
    }
    catch (std::regex_error &e) {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return false;
    }

    // TARGET (enum TargetType { ResponseBodyString = 0, ResponseBodyInteger, ResponseBodyUnsigned, ResponseBodyFloat, ResponseBodyBoolean, ResponseBodyObject, ResponseBodyJsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, OutState };)
    target_ = ""; // empty by default (-), as many cases are only work modes and no parameters(+) are included in their transformation configuration

    // Target specifications:
    // - response.body.string *[string]*: response body document storing expected string.
    // - response.body.integer *[number]*: response body document storing expected integer.
    // - response.body.unsigned *[unsigned number]*: response body document storing expected unsigned integer.
    // - response.body.float *[float number]*: response body document storing expected float number.
    // - response.body.boolean *[boolean]*: response body document storing expected booolean.
    // - response.body.object *[json object]*: response body document storing expected object.
    // - response.body.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as root node.
    // + response.body.string.<n1>..<nN> *[string]*: response body node path storing expected string.
    // + response.body.integer.<n1>..<nN> *[number]*: response body node path storing expected integer.
    // + response.body.unsigned.<n1>..<nN> *[unsigned number]*: response body node path storing expected unsigned integer.
    // + response.body.float.<n1>..<nN> *[float number]*: response body node path storing expected float number.
    // + response.body.boolean.<n1>..<nN> *[boolean]*: response body node path storing expected booblean.
    // + response.body.object.<n1>..<nN> *[json object]*: response body node path storing expected object.
    // + response.body.jsonstring.<node1>..<nodeN> *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path.
    // + response.header.<hname> *[string (or number as string)]*: response header component (i.e. *location*).
    // - response.statusCode *[unsigned integer]*: response status code.
    // - response.delayMs *[unsigned integer]*: simulated delay to respond.
    // + var.<id> *[string (or number as string)]*: general purpose variable.
    // - outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.
    // + outState.[POST|GET|PUT|DELETE|HEAD] *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This overrides the default provisioned one.

    // Regex needed:
    static std::regex responseBodyStringNode("^response.body.string.(.*)");
    static std::regex responseBodyIntegerNode("^response.body.integer.(.*)");
    static std::regex responseBodyUnsignedNode("^response.body.unsigned.(.*)");
    static std::regex responseBodyFloatNode("^response.body.float.(.*)");
    static std::regex responseBodyBooleanNode("^response.body.boolean.(.*)");
    static std::regex responseBodyObjectNode("^response.body.object.(.*)");
    static std::regex responseBodyJsonStringNode("^response.body.jsonstring.(.*)");
    static std::regex responseHeader("^response.header.(.*)");
    static std::regex outStateMethod("^outState.(POST|GET|PUT|DELETE|HEAD)");
    bool convertTargetToJsonPointerPath = false;

    try {
        if (targetSpec == "response.body.string") { // whole document
            target_type_ = TargetType::ResponseBodyString;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyStringNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyString;
        }
        else if (targetSpec == "response.body.integer") { // whole document
            target_type_ = TargetType::ResponseBodyInteger;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyIntegerNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyInteger;
        }
        else if (targetSpec == "response.body.unsigned") { // whole document
            target_type_ = TargetType::ResponseBodyUnsigned;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyUnsignedNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyUnsigned;
        }
        else if (targetSpec == "response.body.float") { // whole document
            target_type_ = TargetType::ResponseBodyFloat;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyFloatNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyFloat;
        }
        else if (targetSpec == "response.body.boolean") { // whole document
            target_type_ = TargetType::ResponseBodyBoolean;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyBooleanNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyBoolean;
        }
        else if (targetSpec == "response.body.object") { // whole document
            target_type_ = TargetType::ResponseBodyObject;
        }
        else if (targetSpec == "response.body.jsonstring") { // whole document
            target_type_ = TargetType::ResponseBodyJsonString;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyObjectNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyObject;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJsonStringNode)) { // nlohmann::json_pointer path (when path provided, i.e.: "response.body.foo.bar" turns into "/foo/bar")
            convertTargetToJsonPointerPath = true;
            target_ = matches[1];
            target_type_ = TargetType::ResponseBodyJsonString;
        }
        else if (std::regex_match(targetSpec, matches, responseHeader)) { // header name
            target_ = matches[1];
            target_type_ = TargetType::ResponseHeader;
        }
        else if (targetSpec == "response.statusCode") {
            target_type_ = TargetType::ResponseStatusCode;
        }
        else if (targetSpec == "response.delayMs") {
            target_type_ = TargetType::ResponseDelayMs;
        }
        else if (std::regex_match(targetSpec, matches, varId)) { // variable id
            target_ = matches[1];
            target_type_ = TargetType::TVar;
        }
        else if (targetSpec == "outState") {
            target_type_ = TargetType::OutState;
        }
        else if (std::regex_match(targetSpec, matches, outStateMethod)) { // method
            target_ = matches[1];
            target_type_ = TargetType::OutState;
        }
    }
    catch (std::regex_error &e) {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return false;
    }

    if (convertTargetToJsonPointerPath) {
        std::replace(target_.begin(), target_.end(), '.', '/');
        target_.insert(target_.begin(), '/');
    }


    LOGDEBUG(
        std::stringstream ss;

        ss << "TRANSFORMATION| source_type_ (RequestUri = 0, RequestUriPath, RequestUriParam, RequestBody, RequestHeader, GeneralRandom, GeneralTimestamp, GeneralStrftime, GeneralUnique, SVar, Value, Event, InState): " << source_type_
        << " | source_ (RequestUriParam, RequestBody(empty: whole, path: node), RequestHeader, GeneralTimestamp, GeneralStrftime, SVar, Value, Event): " << source_
        << " | source_i1_ (GeneralRandom min): " << source_i1_
        << " | source_i2_ (GeneralRandom max): " << source_i2_
        << " | target_type_ (ResponseBodyString = 0, ResponseBodyInteger, ResponseBodyUnsigned, ResponseBodyFloat, ResponseBodyBoolean, ResponseBodyObject, ResponseBodyJsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, OutState): " << target_type_
        << " | target_ (ResponseBodyString/Number/Unsigned/Float/Boolean/Object(empty: whole, path: node), ResponseHeader, TVar, OutState(empty: current method, method: another)): " << target_;

        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

    LOGDEBUG(
        std::stringstream ss;

    if (has_filter_) {

    ss << " |FILTER| filter_type_ (RegexCapture = 0, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar): " << filter_type_
       /*<< " | filter_rgx_: ?"*/
       << " | filter_ (RegexReplace(fmt), RegexCapture(literal, although not actually needed, but useful to access & print on traces), Append, Prepend, AppendVar, PrependVar, ConditionVar): " << filter_
       << " | filter_number_type_ for Sum/Multiply (0: integer, 1: unsigned, 2: float): " << filter_number_type_
       << " | filter_i_ (Sum, Multiply): " << filter_i_
       << " | filter_u_ (Sum, Multiply): " << filter_u_
       << " | filter_f_ (Sum, Multiply): " << filter_f_;

    ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    }

    );

    return true;
}


}
}

