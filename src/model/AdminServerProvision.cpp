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

#include <sstream>
#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <time.h>       /* time_t, struct tm, time, localtime, strftime */
#include <string>
#include <algorithm>

#include <nlohmann/json.hpp>
#include <arashpartow/exprtk.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminServerProvision.hpp>
#include <MockServerEventsData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>

#include <functions.hpp>


typedef exprtk::expression<double>   expression_t;
typedef exprtk::parser<double>       parser_t;

namespace h2agent
{
namespace model
{

/*
void printVariables(const  std::map<std::string, std::string> &variables) {

  for (auto it = variables.begin(); it != variables.end(); it++) {
      LOGDEBUG(
         std::string msg = ert::tracing::Logger::asString("Var '%s' = '%s'", it->first.c_str(), it->second.c_str());
         ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
      );
  }
}
*/

void calculateAdminServerProvisionKey(admin_server_provision_key_t &key, const std::string &inState, const std::string &method, const std::string &uri) {
    // key: <in-state>#<request-method>#<request-uri>
    // hash '#' separator eases regexp usage for stored key
    key = inState;
    key += "#";
    key += method;
    key += "#";
    key += uri;
}


AdminServerProvision::AdminServerProvision() : in_state_(DEFAULT_ADMIN_SERVER_PROVISION_STATE),
    out_state_(DEFAULT_ADMIN_SERVER_PROVISION_STATE),
    response_delay_ms_(0), mock_server_events_data_(nullptr) {;}


bool AdminServerProvision::processSources(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        const std::map<std::string, std::string>& variables,
        const std::string &requestUri,
        const std::string &requestUriPath,
        const std::map<std::string, std::string> &requestQueryParametersMap,
        bool requestBodyJsonOrString,
        const nlohmann::json &requestBodyJson, // if json
        const std::string &requestBody, // if string
        const nghttp2::asio_http2::header_map &requestHeaders,
        bool &eraser,
        std::uint64_t generalUniqueServerSequence) const {

    if (transformation->getSourceType() == Transformation::SourceType::RequestUri) {
        sourceVault.setString(requestUri);
    }
    else if (transformation->getSourceType() == Transformation::SourceType::RequestUriPath) {
        sourceVault.setString(requestUriPath);
    }
    else if (transformation->getSourceType() == Transformation::SourceType::RequestUriParam) {
        auto iter = requestQueryParametersMap.find(transformation->getSource());
        if (iter != requestQueryParametersMap.end()) sourceVault.setString(iter->second);
        else {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract query parameter '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::RequestBody) {
        if(requestBodyJsonOrString) {
            std::string path = transformation->getSource(); // document path (empty or not to be whole or node)
            searchReplaceValueVariables(variables, path);
            if (!sourceVault.setObject(requestBodyJson, path)) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract path '%s' from request body (it is null) in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                return false;
            }
        }
        else {
            sourceVault.setString(requestBody);
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::ResponseBody) {
        std::string path = transformation->getSource(); // document path (empty or not to be whole or node)
        searchReplaceValueVariables(variables, path);
        if (!sourceVault.setObject(getResponseBody(), path)) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract path '%s' from response body (it is null) in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::RequestHeader) {
        auto iter = requestHeaders.find(transformation->getSource());
        if (iter != requestHeaders.end()) sourceVault.setString(iter->second.value);
        else {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract request header '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Eraser) {
        sourceVault.setString(""); // with other than response body nodes, it acts like setting empty string
        eraser = true;
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Math) {
        std::string expressionString = transformation->getSource();
        searchReplaceValueVariables(variables, expressionString);

        /*
           We don't use builtin variables as we can parse h2agent ones which is easier to implement:

           typedef exprtk::symbol_table<double> symbol_table_t;
           symbol_table_t symbol_table;
           double x = 2.0;
           symbol_table.add_variable("x",x);
           expression.register_symbol_table(symbol_table);
           parser.compile("3*x",expression);
           std::cout << expression.value() << std::endl; // 3*2
        */

        expression_t   expression;
        parser_t       parser;
        parser.compile(expressionString, expression);
        sourceVault.setFloat(expression.value());
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Random) {
        int range = transformation->getSourceI2() - transformation->getSourceI1() + 1;
        sourceVault.setInteger(transformation->getSourceI1() + (rand() % range));
    }
    else if (transformation->getSourceType() == Transformation::SourceType::RandomSet) {
        sourceVault.setStringReplacingVariables(transformation->getSourceTokenized()[rand () % transformation->getSourceTokenized().size()], variables); // replace variables if they exist
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Timestamp) {
        if (transformation->getSource() == "s") {
            sourceVault.setInteger(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
        else if (transformation->getSource() == "ms") {
            sourceVault.setInteger(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
        else if (transformation->getSource() == "us") {
            sourceVault.setInteger(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
        else if (transformation->getSource() == "ns") {
            sourceVault.setInteger(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Strftime) {
        std::time_t unixTime = 0;
        std::time (&unixTime);
        char buffer[100] = {0};
        /*size_t size = */strftime(buffer, sizeof(buffer), transformation->getSource().c_str(), localtime(&unixTime));
        //if (size > 1) { // convert TZ offset to RFC3339 format
        //    char minute[] = { buffer[size-2], buffer[size-1], '\0' };
        //    sprintf(buffer + size - 2, ":%s", minute);
        //}

        sourceVault.setStringReplacingVariables(std::string(buffer), variables); // replace variables if they exist
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Recvseq) {
        sourceVault.setUnsigned(generalUniqueServerSequence);
    }
    else if (transformation->getSourceType() == Transformation::SourceType::SVar) {
        std::string varname = transformation->getSource();
        searchReplaceValueVariables(variables, varname);
        auto iter = variables.find(varname);
        if (iter != variables.end()) sourceVault.setString(iter->second);
        else {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract source variable '%s' in transformation item", varname.c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::SGVar) {
        std::string varname = transformation->getSource();
        searchReplaceValueVariables(variables, varname);
        bool exists = false;
        std::string globalVariableValue = global_variable_->getValue(varname, exists);
        if (exists) sourceVault.setString(globalVariableValue);
        else {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract source global variable '%s' in transformation item", varname.c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Value) {
        sourceVault.setStringReplacingVariables(transformation->getSource(), variables); // replace variables if they exist
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Event) {
        std::string var_id_prefix = transformation->getSource();

        auto iter = variables.find(var_id_prefix + ".method");
        std::string event_method = (iter != variables.end()) ? (iter->second):"";

        iter = variables.find(var_id_prefix + ".uri");
        std::string event_uri = (iter != variables.end()) ? (iter->second):"";

        iter = variables.find(var_id_prefix + ".number");
        std::string event_number = (iter != variables.end()) ? (iter->second):"";

        iter = variables.find(var_id_prefix + ".path");
        std::string event_path = (iter != variables.end()) ? (iter->second):"";

        // Now, access the server data for the former selection values:
        nlohmann::json object;
        auto mockServerRequest = mock_server_events_data_->getMockServerKeyEvent(event_method, event_uri, event_number);
        if (!mockServerRequest) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract event for variable '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }

        if (!sourceVault.setObject(mockServerRequest->getJson(), event_path /* document path (empty or not to be whole 'requests number' or node) */)) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unexpected error extracting event for variable '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }

        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Extracted object from event: %s", sourceVault.asString().c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
    }
    else if (transformation->getSourceType() == Transformation::SourceType::InState) {
        sourceVault.setString(getInState());
    }


    return true;
}

bool AdminServerProvision::processFilters(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        const std::map<std::string, std::string>& variables,
        std::smatch &matches,
        std::string &source,
        bool eraser) const
{
    bool success = false;
    std::string targetS;
    std::int64_t targetI = 0;
    std::uint64_t targetU = 0;
    double targetF = 0;


    //std::string source; // (*)
    if (eraser) {
        LOGDEBUG(ert::tracing::Logger::debug("Filter is not allowed when using 'eraser' source type. This transformation will be ignored.", ERT_FILE_LOCATION));
        return false;
    }

    // all the filters except Sum/Multiply, require a string target
    if (transformation->getFilterType() != Transformation::FilterType::Sum && transformation->getFilterType() != Transformation::FilterType::Multiply) {
        source = sourceVault.getString(success);
        if (!success) return false;
    }

    try { // std::regex exceptions
        if (transformation->getFilterType() == Transformation::FilterType::RegexCapture) {

            if (std::regex_match(source, matches, transformation->getFilterRegex()) && matches.size() >=1) {
                targetS = matches.str(0);
                sourceVault.setString(targetS);
                LOGDEBUG(
                    std::stringstream ss;
                    ss << "Regex matches: Size = " << matches.size();
                for(size_t i=0; i < matches.size(); i++) {
                ss << " | [" << i << "] = " << matches.str(i);
                }
                ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
                );
            }
            else {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to match '%s' againt regex capture '%s' in transformation item", source.c_str(), transformation->getFilter().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                return false;
            }
        }
        else if (transformation->getFilterType() == Transformation::FilterType::RegexReplace) {
            targetS = std::regex_replace (source, transformation->getFilterRegex(), transformation->getFilter() /* fmt */);
            sourceVault.setString(targetS);
        }
        else if (transformation->getFilterType() == Transformation::FilterType::Append) {
            targetS = source + transformation->getFilter();
            sourceVault.setString(targetS);
        }
        else if (transformation->getFilterType() == Transformation::FilterType::Prepend) {
            targetS = transformation->getFilter() + source;
            sourceVault.setString(targetS);
        }
        else if (transformation->getFilterType() == Transformation::FilterType::AppendVar) {
            auto iter = variables.find(transformation->getFilter());
            if (iter != variables.end()) targetS = source + (iter->second);
            else {
                targetS = source;
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract variable '%s' in transformation item (empty value assumed)", transformation->getFilter().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
            }
            sourceVault.setString(targetS);
        }
        else if (transformation->getFilterType() == Transformation::FilterType::PrependVar) {
            auto iter = variables.find(transformation->getFilter());
            if (iter != variables.end()) targetS = (iter->second) + source;
            else {
                targetS = source;
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract variable '%s' in transformation item (empty value assumed)", transformation->getFilter().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
            }
            sourceVault.setString(targetS);
        }
        else if (transformation->getFilterType() == Transformation::FilterType::Sum) {
            if (transformation->getFilterNumberType() == 0 /* integer */) {
                targetI = sourceVault.getInteger(success);
                if (success) targetI += transformation->getFilterI();
                else return false;
                sourceVault.setInteger(targetI);
            }
            else if (transformation->getFilterNumberType() == 1 /* unsigned */) {
                targetU = sourceVault.getUnsigned(success);
                if (success) targetU += transformation->getFilterU();
                else return false;
                sourceVault.setUnsigned(targetU);
            }
            else if (transformation->getFilterNumberType() == 2 /* double */) {
                targetF = sourceVault.getFloat(success);
                if (success) targetF += transformation->getFilterF();
                else return false;
                sourceVault.setFloat(targetF);
            }
        }
        else if (transformation->getFilterType() == Transformation::FilterType::Multiply) {
            if (transformation->getFilterNumberType() == 0 /* integer */) {
                targetI = sourceVault.getInteger(success);
                if (success) targetI *= transformation->getFilterI();
                else return false;
                sourceVault.setInteger(targetI);
            }
            else if (transformation->getFilterNumberType() == 1 /* unsigned */) {
                targetU = sourceVault.getUnsigned(success);
                if (success) targetU *= transformation->getFilterU();
                else return false;
                sourceVault.setUnsigned(targetU);
            }
            else if (transformation->getFilterNumberType() == 2 /* double */) {
                targetF = sourceVault.getFloat(success);
                if (success) targetF *= transformation->getFilterF();
                else return false;
                sourceVault.setFloat(targetF);
            }
        }
        else if (transformation->getFilterType() == Transformation::FilterType::ConditionVar) {
            // Get variable value for the variable name 'transformation->getFilter()':
            auto iter = variables.find(transformation->getFilter());
            if ((iter != variables.end()) && !(iter->second.empty()))
                sourceVault.setString(source);
            else
                return false;
        }
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
    }


    return true;
}

bool AdminServerProvision::processTargets(std::shared_ptr<Transformation> transformation,
        TypeConverter &sourceVault,
        std::map<std::string, std::string>& variables,
        const std::smatch &matches,
        bool eraser,
        bool hasFilter,
        unsigned int &responseStatusCode,
        nlohmann::json &responseBodyJson,
        nghttp2::asio_http2::header_map &responseHeaders,
        unsigned int &responseDelayMs,
        std::string &outState,
        std::string &outStateMethod,
        std::string &outStateUri) const
{
    bool success = false;
    std::string targetS;
    std::int64_t targetI = 0;
    std::uint64_t targetU = 0;
    double targetF = 0;
    bool boolean = false;
    nlohmann::json obj;


    try { // nlohmann::json exceptions

        std::string target = transformation->getTarget();
        std::string target2 = transformation->getTarget2(); // foreign outState URI

        searchReplaceValueVariables(variables, target);
        if (!target2.empty()) {
            searchReplaceValueVariables(variables, target2);
        }

        if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyString) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetS;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyInteger) {
            // extraction
            targetI = sourceVault.getInteger(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetI;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyUnsigned) {
            // extraction
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetU;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyFloat) {
            // extraction
            targetF = sourceVault.getFloat(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetF;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyBoolean) {
            // extraction
            boolean = sourceVault.getBoolean(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = boolean;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyObject) {

            if (eraser) {
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into json path '%s'", target.c_str()), ERT_FILE_LOCATION));
                if (target.empty()) {
                    responseBodyJson.erase(responseBodyJson.begin(), responseBodyJson.end());
                    return false;
                }

                //erase() DOES NOT SUPPORT JSON POINTERS:
                //nlohmann::json::json_pointer j_ptr(target);
                //responseBodyJson.erase(j_ptr);
                //
                // For a path '/a/b/c' we must access to /a/b and then erase "c":
                size_t lastSlashPos = target.find_last_of("/");
                // lastSlashPos will never be std::string::npos here
                std::string parentPath = target.substr(0, lastSlashPos);
                std::string childKey = "";
                if (lastSlashPos + 1 < target.size()) childKey = target.substr(lastSlashPos + 1, target.size());
                nlohmann::json::json_pointer j_ptr(parentPath);
                responseBodyJson[j_ptr].erase(childKey);
                return false;
            }

            // extraction will be object if possible, falling back to the rest of formats with this priority: string, integer, unsigned, float, boolean
            // assignment for valid extraction
            nlohmann::json::json_pointer j_ptr(target);

            obj = sourceVault.getObject(success);
            if (!success) {
                targetS = sourceVault.getString(success);
                if (!success) {
                    targetI = sourceVault.getInteger(success);;
                    if (!success) {
                        targetU = sourceVault.getUnsigned(success);
                        if (!success) {
                            targetF = sourceVault.getFloat(success);
                            if (!success) {
                                boolean = sourceVault.getBoolean(success);
                                if (!success) return false;
                                else responseBodyJson[j_ptr] = boolean;
                            }
                            else responseBodyJson[j_ptr] = targetF;
                        }
                        else responseBodyJson[j_ptr] = targetU;
                    }
                    else responseBodyJson[j_ptr] = targetI;
                }
                else responseBodyJson[j_ptr] = targetS;
            }
            else {
                if (target.empty()) {
                    responseBodyJson.merge_patch(obj); // merge origin by default for target response.body.json.object
                }
                else {
                    responseBodyJson[j_ptr] = obj;
                }
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJsonString) {

            // assignment for valid extraction
            nlohmann::json::json_pointer j_ptr(target);

            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (!h2agent::http2::parseJsonContent(targetS, obj))
                return false;

            // assignment
            if (target.empty()) {
                responseBodyJson.merge_patch(obj); // merge origin by default for target response.body.json.object
            }
            else {
                responseBodyJson[j_ptr] = obj;
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseHeader) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignment
            responseHeaders.emplace(target, nghttp2::asio_http2::header_value{targetS});
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseStatusCode) {
            // extraction
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            // assignment
            responseStatusCode = targetU;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseDelayMs) {
            // extraction
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            // assignment
            responseDelayMs = targetU;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::TVar) {
            if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                std::string varname;
                if (matches.size() >=1) { // this protection shouldn't be needed as it would be continued above on RegexCapture matching...
                    variables[target] = matches.str(0); // variable "as is" stores the entire match
                    for(size_t i=1; i < matches.size(); i++) {
                        varname = target;
                        varname += ".";
                        varname += std::to_string(i);
                        variables[varname] = matches.str(i);
                        LOGDEBUG(
                            std::stringstream ss;
                            ss << "Variable '" << varname << "' takes value '" << matches.str(i) << "'";
                            ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
                        );
                    }
                }
            }
            else {
                // extraction
                targetS = sourceVault.getString(success);
                if (!success) return false;
                // assignment
                variables[target] = targetS;
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::TGVar) {
            if (eraser) {
                bool exists;
                global_variable_->removeVariable(target, exists);
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into global variable '%s' (%s)", target.c_str(), exists ? "removed":"missing"), ERT_FILE_LOCATION));
            }
            else if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                std::string varname;
                if (matches.size() >=1) { // this protection shouldn't be needed as it would be continued above on RegexCapture matching...
                    global_variable_->load(target, matches.str(0)); // variable "as is" stores the entire match
                    for(size_t i=1; i < matches.size(); i++) {
                        varname = target;
                        varname += ".";
                        varname += std::to_string(i);
                        global_variable_->load(varname, matches.str(i));
                        LOGDEBUG(
                            std::stringstream ss;
                            ss << "Variable '" << varname << "' takes value '" << matches.str(i) << "'";
                            ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
                        );
                    }
                }
            }
            else {
                // extraction
                targetS = sourceVault.getString(success);
                if (!success) return false;
                // assignment
                global_variable_->load(target, targetS);
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::OutState) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignments
            outState = targetS;
            outStateMethod = target; // empty on regular usage
            outStateUri = target2; // empty on regular usage
        }
        else if (transformation->getTargetType() == Transformation::TargetType::TxtFile) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;

            if (eraser) {
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into text file '%s'", target.c_str()), ERT_FILE_LOCATION));
                file_manager_->empty(target/*path*/);
            }
            else {
                // assignments
                bool shortTerm = (target != transformation->getTarget()); // something was replaced in target: path is considered arbitrary and dynamic: short term files
                file_manager_->write(target/*path*/, targetS/*data*/, true/*text*/, (shortTerm ? configuration_->getShortTermFilesCloseDelayUsecs():configuration_->getLongTermFilesCloseDelayUsecs()));
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::BinFile) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;

            if (eraser) {
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into binary file '%s'", target.c_str()), ERT_FILE_LOCATION));
                file_manager_->empty(target/*path*/);
            }
            else {
                // assignments
                bool shortTerm = (target != transformation->getTarget()); // something was replaced in target: path is considered arbitrary and dynamic: short term files
                file_manager_->write(target/*path*/, targetS/*data*/, false/*binary*/, (shortTerm ? configuration_->getShortTermFilesCloseDelayUsecs():configuration_->getLongTermFilesCloseDelayUsecs()));
            }
        }
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
    }


    return true;
}

void AdminServerProvision::transform( const std::string &requestUri,
                                      const std::string &requestUriPath,
                                      const std::map<std::string, std::string> &requestQueryParametersMap,
                                      const std::string &requestBody,
                                      const nghttp2::asio_http2::header_map &requestHeaders,
                                      std::uint64_t generalUniqueServerSequence,

                                      /* OUTPUT PARAMETERS WHICH ALREADY HAVE DEFAULT VALUES BEFORE TRANSFORMATIONS: */
                                      unsigned int &responseStatusCode,
                                      nghttp2::asio_http2::header_map &responseHeaders,
                                      std::string &responseBody,
                                      unsigned int &responseDelayMs,
                                      std::string &outState,
                                      std::string &outStateMethod,
                                      std::string &outStateUri,
                                      std::shared_ptr<h2agent::model::AdminSchema> requestSchema,
                                      std::shared_ptr<h2agent::model::AdminSchema> responseSchema
                                    ) const
{
    // Default values without transformations:
    responseStatusCode = getResponseCode();
    responseHeaders = getResponseHeaders();
    responseDelayMs = getResponseDelayMilliseconds();
    outState = getOutState(); // prepare next request state, with URI path before transformed with matching algorithms
    outStateMethod = "";
    outStateUri = "";

    // Find out if request body is a valid json or just a string
    nlohmann::json requestBodyJson;
    auto ct_it = requestHeaders.find("content-type");
    bool requestBodyJsonOrString = false;
    if (ct_it != requestHeaders.end()) {
        std::string ct = ct_it->second.value;
        std::transform(ct.begin(), ct.end(), ct.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        requestBodyJsonOrString = (ct == "application/json"); // still need to check if it is a valid json
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("CONTENT TYPE: %s", ct.c_str()), ERT_FILE_LOCATION));
        //LOGINFORMATIONAL(if (!requestBodyJsonOrString) ert::tracing::Logger::informational("Request body won't be interpreted as json", ERT_FILE_LOCATION));
    }

    // Find out if request body will need to be parsed:
    if (requestBodyJsonOrString) {
        bool requestBodyJsonRequired = false;

        if (requestSchema != nullptr) {
            requestBodyJsonRequired = true;
        }
        else {
            for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
                if ((*it)->getSourceType() == Transformation::SourceType::RequestBody) {
                    if (!requestBody.empty()) {
                        requestBodyJsonRequired = true;
                    }
                    else {
                        LOGINFORMATIONAL(ert::tracing::Logger::informational("Empty request body received: some transformations will be ignored", ERT_FILE_LOCATION));
                    }
                    break;
                }
            }
        }

        if (requestBodyJsonRequired) {
            // if fails to parse, we will consider it as an string ignoring the content-type:
            requestBodyJsonOrString = h2agent::http2::parseJsonContent(requestBody, requestBodyJson);
        }
    }

    // Request schema validation:
    if (requestSchema && requestBodyJsonOrString) {
        if (!requestSchema->validate(requestBodyJson)) {
            responseStatusCode = 400; // bad request
            return; // INTERRUPT TRANSFORMATIONS
        }
    }

    // Find out if response body will need to be cloned (this is true if any transformation uses it as target):
    bool usesResponseBodyAsTransformationTarget = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
        if ((*it)->getTargetType() == Transformation::TargetType::ResponseBodyString ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyInteger ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyUnsigned ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyFloat ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyBoolean ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyObject ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJsonString) {
            usesResponseBodyAsTransformationTarget = true;
            break;
        }
    }

    nlohmann::json responseBodyJson;
    if (usesResponseBodyAsTransformationTarget) {
        responseBodyJson = getResponseBody();   // clone provision response body to manipulate this copy and finally we will dump() it over 'responseBody':
        // if(usesResponseBodyAsTransformationTarget) responseBody = responseBodyJson.dump(); <--- place this after transformations (*)
    }
    else {
        responseBody = getResponseBodyString();
    }

    // Dynamic variables map: inherited along the transformation chain
    std::map<std::string, std::string> variables; // source & target variables (key=variable name/value=variable value)

    // Type converter:
    TypeConverter sourceVault{};

    // Apply transformations sequentially
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {

        auto transformation = (*it);
        bool eraser = false;

        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Processing transformation item: %s", transformation->asString().c_str()), ERT_FILE_LOCATION));

        // SOURCES: RequestUri, RequestUriPath, RequestUriParam, RequestBody, ResponseBody, RequestHeader, Eraser, Math, Random, Timestamp, Strftime, Recvseq, SVar, SGvar, Value, Event, InState
        if (!processSources(transformation, sourceVault, variables, requestUri, requestUriPath, requestQueryParametersMap, requestBodyJsonOrString, requestBodyJson, requestBody, requestHeaders, eraser, generalUniqueServerSequence))
            continue;

        std::smatch matches; // BE CAREFUL!: https://stackoverflow.com/a/51709911/2576671
        // So, we can't use 'matches' as container because source may change: BUT, using that source exclusively, it will work (*)
        std::string source; // Now, this never will be out of scope, and 'matches' will be valid.

        // FILTERS: RegexCapture, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar
        bool hasFilter = transformation->hasFilter();
        if (hasFilter) {
            if (!processFilters(transformation, sourceVault, variables, matches, source, eraser))
                continue;
        }

        // TARGETS: ResponseBodyString, ResponseBodyInteger, ResponseBodyUnsigned, ResponseBodyFloat, ResponseBodyBoolean, ResponseBodyObject, ResponseBodyJsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, TGVar, OutState
        if (!processTargets(transformation, sourceVault, variables, matches, eraser, hasFilter, responseStatusCode, responseBodyJson, responseHeaders, responseDelayMs, outState, outStateMethod, outStateUri))
            continue;

    }

    // (*) Regenerate final responseBody after transformations:
    if(usesResponseBodyAsTransformationTarget) responseBody = responseBodyJson.dump();

    // Response schema validation:
    if (responseSchema) {
        if (!responseSchema->validate(usesResponseBodyAsTransformationTarget ? responseBodyJson:getResponseBody())) {
            responseStatusCode = 500; // built response will be anyway sent although status code is overwritten with internal server error.
        }
    }
}

bool AdminServerProvision::load(const nlohmann::json &j, bool regexMatchingConfigured) {

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto requestMethod_it = j.find("requestMethod");
    request_method_ = *requestMethod_it;

    auto it = j.find("responseCode");
    response_code_ = *it;

    // Optional
    it = j.find("requestUri");
    if (it != j.end() && it->is_string()) {
        request_uri_ = *it;
    }

    it = j.find("inState");
    if (it != j.end() && it->is_string()) {
        in_state_ = *it;
        if (in_state_.empty()) in_state_ = DEFAULT_ADMIN_SERVER_PROVISION_STATE;
    }

    it = j.find("outState");
    if (it != j.end() && it->is_string()) {
        out_state_ = *it;
        if (out_state_.empty()) out_state_ = DEFAULT_ADMIN_SERVER_PROVISION_STATE;
    }

    it = j.find("requestSchemaId");
    if (it != j.end() && it->is_string()) {
        request_schema_id_ = *it;
        if (request_schema_id_.empty()) {
            ert::tracing::Logger::error("Invalid empty request schema identifier", ERT_FILE_LOCATION);
            return false;
        }
    }

    it = j.find("responseSchemaId");
    if (it != j.end() && it->is_string()) {
        response_schema_id_ = *it;
        if (response_schema_id_.empty()) {
            ert::tracing::Logger::error("Invalid empty response schema identifier", ERT_FILE_LOCATION);
            return false;
        }
    }

    it = j.find("responseHeaders");
    if (it != j.end() && it->is_object()) {
        loadResponseHeaders(*it);
    }

    it = j.find("responseBody");
    if (it != j.end()) {
        if (it->is_object() || it->is_array()) {
            response_body_ = *it;
            response_body_string_ = response_body_.dump(); // valid as cache for static responses (not updated with transformations)
        }
        else if (it->is_string()) {
            response_body_string_ = *it;
        }
        else if (it->is_number_integer() || it->is_number_unsigned()) {
            //response_body_integer_ = *it;
            int number = *it;
            response_body_string_ = std::to_string(number);
        }
        else if (it->is_number_float()) {
            //response_body_number_ = *it;
            response_body_string_ = std::to_string(double(*it));
        }
        else if (it->is_boolean()) {
            //response_body_boolean_ = *it;
            response_body_string_ = ((bool)(*it) ? "true":"false");
        }
        else if (it->is_null()) {
            //response_body_null_ = true;
            response_body_string_ = "null";
        }
    }

    it = j.find("responseDelayMs");
    if (it != j.end() && it->is_number()) {
        response_delay_ms_ = *it;
    }

    auto transform_it = j.find("transform");
    if (transform_it != j.end()) {
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(it);
        }
    }

    // Store key:
    calculateAdminServerProvisionKey(key_, in_state_, request_method_, request_uri_);

    if (regexMatchingConfigured) {
        // Precompile regex with key, only for 'RegexMatching' algorithm:
        try {
            regex_.assign(key_, std::regex::optimize);
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            ert::tracing::Logger::error("Invalid regular expression (detected when joining 'inState' and/or 'requestUri' from provision) for current 'RegexMatching' server matching algorithm", ERT_FILE_LOCATION);
            return false;
        }
    }

    return true;
}

void AdminServerProvision::loadResponseHeaders(const nlohmann::json &j) {
    for (auto& [key, val] : j.items())
        response_headers_.emplace(key, nghttp2::asio_http2::header_value{val});
}

void AdminServerProvision::loadTransformation(const nlohmann::json &j) {

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Loading transformation item: %s", j.dump().c_str()); // avoid newlines in traces (dump(n) pretty print)
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Transformation object to fill:
    auto transformation = std::make_shared<Transformation>();

    if (transformation->load(j)) {
        transformations_.push_back(transformation);
    }
    else {
        ert::tracing::Logger::error("Discarded transform filter due to incoherent data", ERT_FILE_LOCATION);
    }
}


}
}

