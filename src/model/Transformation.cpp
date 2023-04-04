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
#include <functions.hpp>

#include <sstream>
#include <algorithm>

namespace h2agent
{
namespace model
{

void Transformation::collectVariablePatterns(const std::string &str, std::map<std::string, std::string> &patterns) {

    static std::regex re("@\\{[^\\{\\}]*\\}"); // @{[^{}]*} with curly braces escaped
    // or: R"(@\{[^\{\}]*\})"

    std::string::const_iterator it(str.cbegin());
    std::smatch matches;
    std::string pattern;
    patterns.clear();
    while (std::regex_search(it, str.cend(), matches, re)) {
        it = matches.suffix().first;
        pattern = matches[0];
        patterns[pattern] = pattern.substr(2, pattern.size()-3); // @{foo} -> foo
    }
}

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
        //   EqualTo             value -> [filter_]

        auto f_it = it->find("RegexCapture");

        try {
            if (f_it != it->end()) {
                filter_ = *f_it;
                filter_rgx_.assign(filter_, std::regex::optimize);
                filter_type_ = FilterType::RegexCapture;
            }
            else if ((f_it = it->find("RegexReplace")) != it->end()) {
                filter_rgx_.assign(std::string(*(f_it->find("rgx"))), std::regex::optimize);
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
            else if ((f_it = it->find("EqualTo")) != it->end()) {
                filter_ = *f_it;
                filter_type_ = FilterType::EqualTo;
            }
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            return false;
        }
    }

    // Interpret source/target:

    // SOURCE (enum SourceType { RequestUri = 0, RequestUriPath, RequestUriParam, RequestBody, ResponseBody, RequestHeader, Eraser,
    //                           Math, Random, RandomSet, Timestamp, Strftime, Recvseq, SVar, SGVar, Value, ServerEvent, InState };)
    source_ = ""; // empty by default (-), as many cases are only work modes and no parameters(+) are included in their transformation configuration

    // Source specifications:
    // - request.uri: whole `url-decoded` request *URI* (path together with possible query parameters). This is the unmodified original *URI*, not necessarily the same as the classification *URI*.
    // - request.uri.path: `url-decoded` request *URI* path part.
    // + request.uri.param.<name>: request URI specific parameter `<name>`.
    // - request.body: request body document.
    // + request.body./<node1>/../<nodeN>: request body node path.
    // + request.header.<hname>: request header component (i.e. *content-type*).
    // - eraser: this is used to indicate that the *target* specified (next section) must be removed or reset.
    // + math.`<expression>`: this source is based in Arash Partow's exprtk math library compilation.
    // + random.<min>.<max>: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.
    // + timestamp.<unit>: UNIX epoch time in `s` (seconds), `ms` (milliseconds), `us` (microseconds) or `ns` (nanoseconds).
    // + strftime.<format>: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/).
    // - recvseq: sequence id increased for every mock reception (starts on *1* when the *h2agent* is started).
    // + var.<id>: general purpose variable.
    // + globalVar.<id>: general purpose global variable.
    // - value.<value>: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string), will be converted to the target type.
    // - inState: current processing state.
    // + serverEvent.`<server event address in query parameters format>`: access server context indexed by request *method* (`requestMethod`), *URI* (`requestUri`), events *number* (`eventNumber`) and events number *path* (`eventPath`).
    // + txtFile.`<path>`: reads text content from file with the path provided.
    // + binFile.`<path>`: reads binary content from file with the path provided.
    // + command.`<command>`: executes command on process shell and captures the standard output.

    // Regex needed:
    static std::regex requestUriParam("^request.uri.param.(.*)", std::regex::optimize); // no need to escape dots as this is validated in schema
    static std::regex requestBodyNode("^request.body.(.*)", std::regex::optimize);
    static std::regex responseBodyNode("^response.body.(.*)", std::regex::optimize);
    static std::regex requestHeader("^request.header.(.*)", std::regex::optimize);
    static std::regex math("^math.(.*)", std::regex::optimize);
    static std::regex random("^random\\.([-+]{0,1}[0-9]+)\\.([-+]{0,1}[0-9]+)$", std::regex::optimize); // no need to validate min/max as it was done at schema
    static std::regex randomSet("^randomset.(.*)", std::regex::optimize);
    static std::regex timestamp("^timestamp.(.*)", std::regex::optimize); // no need to validate s/ms/us/ns as it was done at schema
    static std::regex strftime("^strftime.(.*)", std::regex::optimize); // free format, errors captured
    static std::regex varId("^var.(.*)", std::regex::optimize);
    static std::regex gvarId("^globalVar.(.*)", std::regex::optimize);
    static std::regex value("^value.([.\\s\\S]*)", std::regex::optimize); // added support for special characters: \n \t \r
    static std::regex serverEvent("^serverEvent.(.*)", std::regex::optimize);
    static std::regex txtFile("^txtFile.(.*)", std::regex::optimize);
    static std::regex binFile("^binFile.(.*)", std::regex::optimize);
    static std::regex command("^command.(.*)", std::regex::optimize);

    std::smatch matches; // to capture regex group(s)
    // BE CAREFUL!: https://stackoverflow.com/a/51709911/2576671
    // In this case, it is not a problem, as we store the match from sourceSpec or targetSpec before changing them.

    try {
        if (sourceSpec == "request.uri") {
            source_type_ = SourceType::RequestUri;
        }
        else if (sourceSpec == "request.uri.path") {
            source_type_ = SourceType::RequestUriPath;
        }
        else if (std::regex_match(sourceSpec, matches, requestUriParam)) { // parameter name
            source_ = matches.str(1);
            source_type_ = SourceType::RequestUriParam;
        }
        else if (sourceSpec == "request.body") { // whole document
            source_type_ = SourceType::RequestBody;
        }
        else if (std::regex_match(sourceSpec, matches, requestBodyNode)) { // nlohmann::json_pointer path
            source_ = matches.str(1);
            source_type_ = SourceType::RequestBody;
        }
        else if (sourceSpec == "response.body") { // whole document
            source_type_ = SourceType::ResponseBody;
        }
        else if (std::regex_match(sourceSpec, matches, responseBodyNode)) { // nlohmann::json_pointer path
            source_ = matches.str(1);
            source_type_ = SourceType::ResponseBody;
        }
        else if (std::regex_match(sourceSpec, matches, requestHeader)) { // header name
            source_ = matches.str(1);
            source_type_ = SourceType::RequestHeader;
        }
        else if (sourceSpec == "eraser") {
            source_type_ = SourceType::Eraser;
        }
        else if (std::regex_match(sourceSpec, matches, math)) { // math expression, i.e. "2*sqrt(2)"
            source_ = matches.str(1);
            source_type_ = SourceType::Math;
        }
        else if (std::regex_match(sourceSpec, matches, random)) { // range "<min>.<max>", i.e.: "-3.8", "0.100", "-15.+2", etc. These go to -> [source_i1_] and [source_i2_]
            source_i1_ = stoi(matches.str(1));
            source_i2_ = stoi(matches.str(2));
            source_type_ = SourceType::Random;
        }
        else if (std::regex_match(sourceSpec, matches, randomSet)) { // random set given by tokenized pipe-separated list of values
            source_ = matches.str(1);
            static std::regex pipedRgx(R"(\|)", std::regex::optimize);
            source_tokenized_ = std::vector<std::string>(
                                    std::sregex_token_iterator{begin(source_), end(source_), pipedRgx, -1},
                                    std::sregex_token_iterator{}
                                );
            source_type_ = SourceType::RandomSet;
        }
        else if (std::regex_match(sourceSpec, matches, timestamp)) { // unit (s: seconds, ms: milliseconds, us: microseconds, ns: nanoseconds)
            source_ = matches.str(1);
            source_type_ = SourceType::Timestamp;
        }
        else if (std::regex_match(sourceSpec, matches, strftime)) { // current date/time formatted by as described in https://www.cplusplus.com/reference/ctime/strftime/
            source_ = matches.str(1);
            source_type_ = SourceType::Strftime;
        }
        else if (sourceSpec == "recvseq") {
            source_type_ = SourceType::Recvseq;
        }
        else if (std::regex_match(sourceSpec, matches, varId)) { // variable id
            source_ = matches.str(1);
            source_type_ = SourceType::SVar;
        }
        else if (std::regex_match(sourceSpec, matches, gvarId)) { // global variable id
            source_ = matches.str(1);
            source_type_ = SourceType::SGVar;
        }
        else if (std::regex_match(sourceSpec, matches, value)) { // value content
            source_ = matches.str(1);
            source_type_ = SourceType::Value;
        }
        else if (std::regex_match(sourceSpec, matches, serverEvent)) { // value content
            source_ = matches.str(1); // i.e. requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/requestBody
            source_type_ = SourceType::ServerEvent;
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(source_);
            std::map<std::string, std::string>::const_iterator it;
            for (auto qp: {
                        "requestMethod", "requestUri", "eventNumber", "eventPath"
                    }) { // tokenized vector order
                it = qmap.find(qp);
                source_tokenized_.push_back((it != qmap.end()) ? it->second:"");
            }
        }
        else if (sourceSpec == "inState") {
            source_type_ = SourceType::InState;
        }
        else if (std::regex_match(sourceSpec, matches, txtFile)) { // path file
            source_ = matches.str(1);
            source_type_ = SourceType::STxtFile;
        }
        else if (std::regex_match(sourceSpec, matches, binFile)) { // path file
            source_ = matches.str(1);
            source_type_ = SourceType::SBinFile;
        }
        else if (std::regex_match(sourceSpec, matches, command)) { // command string
            source_ = matches.str(1);
            source_type_ = SourceType::Command;
        }
        else { // some things could reach this (strange characters within value.* for example):
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Cannot identify source type for: %s", sourceSpec.c_str()), ERT_FILE_LOCATION);
            return false;
        }
    }
    catch (std::regex_error &e) {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return false;
    }

    // TARGET (enum TargetType { ResponseBodyString = 0, ResponseBodyHexString, ResponseBodyJson_String, ResponseBodyJson_Integer, ResponseBodyJson_Unsigned, ResponseBodyJson_Float, ResponseBodyJson_Boolean, ResponseBodyJson_Object, ResponseBodyJson_JsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, TGVar, OutState, TTxtFile, TBinFile, ServerEventToPurge };)
    target_ = ""; // empty by default (-), as many cases are only work modes and no parameters(+) are included in their transformation configuration
    target2_ = ""; // same

    // Target specifications:
    // - response.body.string *[string]*: response body storing expected string processed.
    // - response.body.hexstring *[string]*: response body storing expected string processed from hexadecimal representation, for example `0x8001` (prefix `0x` is optional).
    // - response.body.json.string *[string]*: response body document storing expected string.
    // - response.body.json.integer *[number]*: response body document storing expected integer.
    // - response.body.json.unsigned *[unsigned number]*: response body document storing expected unsigned integer.
    // - response.body.json.float *[float number]*: response body document storing expected float number.
    // - response.body.json.boolean *[boolean]*: response body document storing expected booolean.
    // - response.body.json.object *[json object]*: response body document storing expected object.
    // - response.body.json.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as root node.
    // + response.body.json.string./<n1>/../<nN> *[string]*: response body node path storing expected string.
    // + response.body.json.integer./<n1>/../<nN> *[number]*: response body node path storing expected integer.
    // + response.body.json.unsigned./<n1>/../<nN> *[unsigned number]*: response body node path storing expected unsigned integer.
    // + response.body.json.float./<n1>/../<nN> *[float number]*: response body node path storing expected float number.
    // + response.body.json.boolean./<n1>/../<nN> *[boolean]*: response body node path storing expected booblean.
    // + response.body.json.object./<n1>/../<nN> *[json object]*: response body node path storing expected object.
    // + response.body.json.jsonstring./<n1>/../<nN> *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path.
    // + response.header.<hname> *[string (or number as string)]*: response header component (i.e. *location*).
    // - response.statusCode *[unsigned integer]*: response status code.
    // - response.delayMs *[unsigned integer]*: simulated delay to respond.
    // + var.<id> *[string (or number as string)]*: general purpose variable.
    // + globalVar.<id> *[string (or number as string)]*: general purpose global variable.
    // - outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.
    // + outState.`[POST|GET|PUT|DELETE|HEAD][.<uri>]` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.
    // + txtFile.`<path>` *[string]*: dumps source (as string) over text file with the path provided.
    // + binFile.`<path>` *[string]*: dumps source (as string) over binary file with the path provided.
    // + serverEvent.`<server event address in query parameters format>`: this target is always used in conjunction with `eraser`.

    // Regex needed:
    static std::regex responseBodyJson_StringNode("^response.body.json.string.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_IntegerNode("^response.body.json.integer.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_UnsignedNode("^response.body.json.unsigned.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_FloatNode("^response.body.json.float.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_BooleanNode("^response.body.json.boolean.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_ObjectNode("^response.body.json.object.(.*)", std::regex::optimize);
    static std::regex responseBodyJson_JsonStringNode("^response.body.json.jsonstring.(.*)", std::regex::optimize);
    static std::regex responseHeader("^response.header.(.*)", std::regex::optimize);
    static std::regex outStateMethodUri("^outState.(POST|GET|PUT|DELETE|HEAD)(\\..+)?", std::regex::optimize);

    try {
        if (targetSpec == "response.body.string") {
            target_type_ = TargetType::ResponseBodyString;
        }
        else if (targetSpec == "response.body.hexstring") {
            target_type_ = TargetType::ResponseBodyHexString;
        }
        else if (targetSpec == "response.body.json.string") { // whole document
            target_type_ = TargetType::ResponseBodyJson_String;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_StringNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_String;
        }
        else if (targetSpec == "response.body.json.integer") { // whole document
            target_type_ = TargetType::ResponseBodyJson_Integer;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_IntegerNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_Integer;
        }
        else if (targetSpec == "response.body.json.unsigned") { // whole document
            target_type_ = TargetType::ResponseBodyJson_Unsigned;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_UnsignedNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_Unsigned;
        }
        else if (targetSpec == "response.body.json.float") { // whole document
            target_type_ = TargetType::ResponseBodyJson_Float;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_FloatNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_Float;
        }
        else if (targetSpec == "response.body.json.boolean") { // whole document
            target_type_ = TargetType::ResponseBodyJson_Boolean;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_BooleanNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_Boolean;
        }
        else if (targetSpec == "response.body.json.object") { // whole document
            target_type_ = TargetType::ResponseBodyJson_Object;
        }
        else if (targetSpec == "response.body.json.jsonstring") { // whole document
            target_type_ = TargetType::ResponseBodyJson_JsonString;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_ObjectNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_Object;
        }
        else if (std::regex_match(targetSpec, matches, responseBodyJson_JsonStringNode)) { // nlohmann::json_pointer path
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseBodyJson_JsonString;
        }
        else if (std::regex_match(targetSpec, matches, responseHeader)) { // header name
            target_ = matches.str(1);
            target_type_ = TargetType::ResponseHeader;
        }
        else if (targetSpec == "response.statusCode") {
            target_type_ = TargetType::ResponseStatusCode;
        }
        else if (targetSpec == "response.delayMs") {
            target_type_ = TargetType::ResponseDelayMs;
        }
        else if (std::regex_match(targetSpec, matches, varId)) { // variable id
            target_ = matches.str(1);
            target_type_ = TargetType::TVar;
        }
        else if (std::regex_match(targetSpec, matches, gvarId)) { // global variable id
            target_ = matches.str(1);
            target_type_ = TargetType::TGVar;
        }
        else if (targetSpec == "outState") {
            target_type_ = TargetType::OutState;
        }
        else if (std::regex_match(targetSpec, matches, outStateMethodUri)) { // method
            target_ = matches.str(1); // <method>
            target2_ = matches.str(2); // .<uri>
            if (!target2_.empty()) {
                target2_ = target2_.substr(1); // remove the initial dot to store the uri
            }
            target_type_ = TargetType::OutState;
        }
        else if (std::regex_match(targetSpec, matches, txtFile)) { // path file
            target_ = matches.str(1);
            target_type_ = TargetType::TTxtFile;
        }
        else if (std::regex_match(targetSpec, matches, binFile)) { // path file
            target_ = matches.str(1);
            target_type_ = TargetType::TBinFile;
        }
        else if (std::regex_match(targetSpec, matches, serverEvent)) { // value content
            target_ = matches.str(1); // i.e. requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3
            target_type_ = TargetType::ServerEventToPurge;
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(target_);
            std::map<std::string, std::string>::const_iterator it;
            for (auto qp: {
                        "requestMethod", "requestUri", "eventNumber"
                    }) { // tokenized vector order
                it = qmap.find(qp);
                target_tokenized_.push_back((it != qmap.end()) ? it->second:"");
            }
        }
        else { // very strange to reach this:
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Cannot identify target type for: %s", targetSpec.c_str()), ERT_FILE_LOCATION);
            return false;
        }
    }
    catch (std::regex_error &e) {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return false;
    }

    //LOGDEBUG(ert::tracing::Logger::debug(asString(), ERT_FILE_LOCATION));

    // Variable patterns:
    collectVariablePatterns(source_, source_patterns_);
    collectVariablePatterns(target_, target_patterns_);
    collectVariablePatterns(target2_, target2_patterns_);

    return true;
}

std::string Transformation::asString() const {

    std::stringstream ss;


    // SOURCE
    ss << "SourceType: " << SourceTypeAsText(source_type_);
    if (source_type_ != SourceType::RequestUri && source_type_ != SourceType::RequestUriPath && source_type_ != SourceType::Eraser && source_type_ != SourceType::Recvseq && source_type_ != SourceType::InState) {
        ss << " | source_: " << source_;

        if (source_type_ == SourceType::RequestBody || source_type_ == SourceType::ResponseBody) {
            ss << " (empty: whole, path: node)";
        }
        else if (source_type_ == SourceType::Random) {
            ss << " | source_i1_: " << source_i1_ << " (Random min)" << " | source_i2_: " << source_i2_ << " (Random max)";
        }
        else if (source_type_ == SourceType::STxtFile || source_type_ == SourceType::SBinFile) {
            ss << " (path file)";
        }
        else if (source_type_ == SourceType::STxtFile || source_type_ == SourceType::Command) {
            ss << " (shell command expression)";
        }

        if (!source_patterns_.empty()) {
            ss << " | source variables:";
            for (auto it = source_patterns_.begin(); it != source_patterns_.end(); it ++) {
                ss << " " << it->second;
            }
        }
    }

    // TARGET
    ss << " | TargetType: " << TargetTypeAsText(target_type_);
    if (target_type_ != TargetType::ResponseStatusCode &&
            target_type_ != TargetType::ResponseDelayMs &&
            target_type_ != TargetType::ResponseBodyString &&
            target_type_ != TargetType::ResponseBodyHexString ) {

        ss << " | target_: " << target_;

        if (target_type_ == TargetType::ResponseBodyJson_String || target_type_ == TargetType::ResponseBodyJson_Integer || target_type_ == TargetType::ResponseBodyJson_Unsigned || target_type_ == TargetType::ResponseBodyJson_Float || target_type_ == TargetType::ResponseBodyJson_Boolean || target_type_ == TargetType::ResponseBodyJson_Object) {
            ss << " (empty: whole, path: node)";
        }
        else if (target_type_ == TargetType::OutState) {
            ss << " (empty: current method, method: another)" << " | target2_: " << target2_ << "(empty: current uri, uri: another)";
            if (!target2_patterns_.empty()) {
                ss << " | target2 variables:";
                for (auto it = target2_patterns_.begin(); it != target2_patterns_.end(); it ++) {
                    ss << " " << it->second;
                }
            }
        }
        else if (target_type_ == TargetType::TTxtFile || target_type_ == TargetType::TBinFile) {
            ss << " (path file)";
        }

        if (!target_patterns_.empty()) {
            ss << " | target variables:";
            for (auto it = target_patterns_.begin(); it != target_patterns_.end(); it ++) {
                ss << " " << it->second;
            }
        }
    }

    // FILTER
    if (has_filter_) {
        ss << " | FilterType: " << FilterTypeAsText(filter_type_);
        if (filter_type_ != FilterType::Sum && filter_type_ != FilterType::Multiply) {
            /*<< " | filter_rgx_: ?"*/
            ss << " | filter_ " << filter_;

            if (filter_type_ == FilterType::RegexReplace) {
                ss << " (fmt)";
            }
            else if (filter_type_ == FilterType::RegexCapture) {
                ss << " (literal, although not actually needed, but useful to access & print on traces)";
            }
        }
        else {
            ss << " | filter_number_type_: " << filter_number_type_ << " (0: integer, 1: unsigned, 2: float)"
               << " | filter_i_: " << filter_i_ << " | filter_u_: " << filter_u_ << " | filter_f_: " << filter_f_;
        }
    }

    return ss.str();
}


}
}

