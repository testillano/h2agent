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
//#include <fcntl.h> // non-blocking fgets call

#include <nlohmann/json.hpp>
#include <arashpartow/exprtk.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

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
        std::map<std::string, std::string>& variables,
        const std::string &requestUri,
        const std::string &requestUriPath,
        const std::map<std::string, std::string> &requestQueryParametersMap,
        const DataPart &requestBodyDataPart,
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
        if (requestBodyDataPart.isJson()) {
            std::string path = transformation->getSource(); // document path (empty or not to be whole or node)
            replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_->get());
            if (!sourceVault.setObject(requestBodyDataPart.getJson(), path)) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract path '%s' from request body (it is null) in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                return false;
            }
        }
        else {
            sourceVault.setString(requestBodyDataPart.str());
        }
    }
    else if (transformation->getSourceType() == Transformation::SourceType::ResponseBody) {
        std::string path = transformation->getSource(); // document path (empty or not to be whole or node)
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_->get());
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
        replaceVariables(expressionString, transformation->getSourcePatterns(), variables, global_variable_->get());

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
        sourceVault.setStringReplacingVariables(transformation->getSourceTokenized()[rand () % transformation->getSourceTokenized().size()], transformation->getSourcePatterns(), variables, global_variable_->get()); // replace variables if they exist
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

        sourceVault.setStringReplacingVariables(std::string(buffer), transformation->getSourcePatterns(), variables, global_variable_->get()); // replace variables if they exist
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Recvseq) {
        sourceVault.setUnsigned(generalUniqueServerSequence);
    }
    else if (transformation->getSourceType() == Transformation::SourceType::SVar) {
        std::string varname = transformation->getSource();
        replaceVariables(varname, transformation->getSourcePatterns(), variables, global_variable_->get());
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
        replaceVariables(varname, transformation->getSourcePatterns(), variables, global_variable_->get());
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
        sourceVault.setStringReplacingVariables(transformation->getSource(), transformation->getSourcePatterns(), variables, global_variable_->get()); // replace variables if they exist
    }
    else if (transformation->getSourceType() == Transformation::SourceType::ServerEvent) {
        // transformation->getSourceTokenized() is a vector:
        //
        // requestMethod: index 0
        // requestUri:    index 1
        // eventNumber:   index 2
        // eventPath:     index 3
        std::string event_method = transformation->getSourceTokenized()[0];
        replaceVariables(event_method, transformation->getSourcePatterns(), variables, global_variable_->get());
        std::string event_uri = transformation->getSourceTokenized()[1];
        replaceVariables(event_uri, transformation->getSourcePatterns(), variables, global_variable_->get());
        std::string event_number = transformation->getSourceTokenized()[2];
        replaceVariables(event_number, transformation->getSourcePatterns(), variables, global_variable_->get());
        std::string event_path = transformation->getSourceTokenized()[3];
        replaceVariables(event_path, transformation->getSourcePatterns(), variables, global_variable_->get());

        // Now, access the server data for the former selection values:
        nlohmann::json object;
        auto mockServerRequest = mock_server_events_data_->getMockServerKeyEvent(event_method, event_uri, event_number);
        if (!mockServerRequest) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unable to extract server event for variable '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }

        if (!sourceVault.setObject(mockServerRequest->getJson(), event_path /* document path (empty or not to be whole 'requests number' or node) */)) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Unexpected error extracting server event for variable '%s' in transformation item", transformation->getSource().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            return false;
        }

        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Extracted object from server event: %s", sourceVault.asString().c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
    }
    else if (transformation->getSourceType() == Transformation::SourceType::InState) {
        sourceVault.setString(getInState());
    }
    else if (transformation->getSourceType() == Transformation::SourceType::STxtFile) {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_->get());

        std::string content;
        file_manager_->read(path, content, true/*text*/);
        sourceVault.setString(std::move(content));
    }
    else if (transformation->getSourceType() == Transformation::SourceType::SBinFile) {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_->get());

        std::string content;
        file_manager_->read(path, content, false/*binary*/);
        sourceVault.setString(std::move(content));
    }
    else if (transformation->getSourceType() == Transformation::SourceType::Command) {
        std::string command = transformation->getSource();
        replaceVariables(command, transformation->getSourcePatterns(), variables, global_variable_->get());

        static char buffer[256];
        std::string output;

        FILE *fp = popen(command.c_str(), "r");
        if (fp) {
            /* This makes asyncronous the command execution, but we will have broken pipe and cannot capture anything.
            // fgets is blocking (https://stackoverflow.com/questions/6055702/using-fgets-as-non-blocking-function-c/6055774#6055774)
            int fd = fileno(fp);
            int flags = fcntl(fd, F_GETFL, 0);
            flags |= O_NONBLOCK;
            fcntl(fd, F_SETFL, flags);
            */

            while(fgets(buffer, sizeof(buffer), fp))
            {
                output += buffer;
            }
            variables["rc"] = std::to_string(WEXITSTATUS(/* status = */pclose(fp))); // rc = status >>= 8; // divide by 256
        }
        else {
            variables["rc"] = "-1";
        }

        sourceVault.setString(std::move(output));
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
        else if (transformation->getFilterType() == Transformation::FilterType::ConditionVar) { // TODO: if condition is false, source storage could be omitted to improve performance
            // Get variable value for the variable name 'transformation->getFilter()':
            std::string varname = transformation->getFilter();
            bool reverse = (transformation->getFilter()[0] == '!');
            if (reverse) {
                varname.erase(0,1);
            }
            auto iter = variables.find(varname);
            bool conditionVar = ((iter != variables.end()) && !(iter->second.empty()));

            if ((reverse && !conditionVar)||(!reverse && conditionVar)) {
                sourceVault.setString(source);
            }
            else
                return false;
        }
        else if (transformation->getFilterType() == Transformation::FilterType::EqualTo) {
            // Get value for the comparison 'transformation->getFilter()':
            if (source == transformation->getFilter())
                sourceVault.setString("yes");
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
        std::string &responseBodyAsString,
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

        replaceVariables(target, transformation->getTargetPatterns(), variables, global_variable_->get());
        if (!target2.empty()) {
            replaceVariables(target2, transformation->getTarget2Patterns(), variables, global_variable_->get());
        }

        if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyString) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignment
            responseBodyAsString = targetS;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyHexString) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignment
            if (!h2agent::model::fromHexString(targetS, responseBodyAsString)) return false;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_String) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetS;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_Integer) {
            // extraction
            targetI = sourceVault.getInteger(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetI;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_Unsigned) {
            // extraction
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetU;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_Float) {
            // extraction
            targetF = sourceVault.getFloat(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = targetF;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_Boolean) {
            // extraction
            boolean = sourceVault.getBoolean(success);
            if (!success) return false;
            // assignment
            nlohmann::json::json_pointer j_ptr(target);
            responseBodyJson[j_ptr] = boolean;
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_Object) {

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
        else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJson_JsonString) {

            // assignment for valid extraction
            nlohmann::json::json_pointer j_ptr(target);

            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (!h2agent::model::parseJsonContent(targetS, obj))
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
        else if (transformation->getTargetType() == Transformation::TargetType::TTxtFile) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;

            if (eraser) {
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into text file '%s'", target.c_str()), ERT_FILE_LOCATION));
                file_manager_->empty(target/*path*/);
            }
            else {
                // assignments
                bool longTerm =(transformation->getTargetPatterns().empty()); // path is considered fixed (long term files), instead of arbitrary and dynamic (short term files)
                // even if @{varname} is missing (empty value) we consider the intention to allow force short term
                // files type.
                file_manager_->write(target/*path*/, targetS/*data*/, true/*text*/, (longTerm ? configuration_->getLongTermFilesCloseDelayUsecs():configuration_->getShortTermFilesCloseDelayUsecs()));
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::TBinFile) {
            // extraction
            targetS = sourceVault.getString(success);
            if (!success) return false;

            if (eraser) {
                LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Eraser source into binary file '%s'", target.c_str()), ERT_FILE_LOCATION));
                file_manager_->empty(target/*path*/);
            }
            else {
                // assignments
                bool longTerm =(transformation->getTargetPatterns().empty()); // path is considered fixed (long term files), instead of arbitrary and dynamic (short term files)
                // even if @{varname} is missing (empty value) we consider the intention to allow force short term
                // files type.
                file_manager_->write(target/*path*/, targetS/*data*/, false/*binary*/, (longTerm ? configuration_->getLongTermFilesCloseDelayUsecs():configuration_->getShortTermFilesCloseDelayUsecs()));
            }
        }
        else if (transformation->getTargetType() == Transformation::TargetType::ServerEventToPurge) {
            if (!eraser) {
                LOGDEBUG(ert::tracing::Logger::debug("'ServerEventToPurge' target type only works with 'eraser' source type. This transformation will be ignored.", ERT_FILE_LOCATION));
                return false;
            }
            // transformation->getTargetTokenized() is a vector:
            //
            // requestMethod: index 0
            // requestUri:    index 1
            // eventNumber:   index 2
            std::string event_method = transformation->getTargetTokenized()[0];
            replaceVariables(event_method, transformation->getTargetPatterns(), variables, global_variable_->get());
            std::string event_uri = transformation->getTargetTokenized()[1];
            replaceVariables(event_uri, transformation->getTargetPatterns(), variables, global_variable_->get());
            std::string event_number = transformation->getTargetTokenized()[2];
            replaceVariables(event_number, transformation->getTargetPatterns(), variables, global_variable_->get());

            bool serverDataDeleted = false;
            bool success = mock_server_events_data_->clear(serverDataDeleted, event_method, event_uri, event_number);

            if (!success) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unexpected error while removing server data event '%s' in transformation item", transformation->getTarget().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                return false;
            }

            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Server event '%s' removal result: %s", transformation->getTarget().c_str(), (serverDataDeleted ? "SUCCESS":"NOTHING REMOVED"));
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
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
                                      DataPart &requestBodyDataPart,
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

    // Check if the request body must be decoded:
    bool mustDecodeRequestBody = false;
    if (requestSchema) {
        mustDecodeRequestBody = true;
    }
    else {
        for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
            if ((*it)->getSourceType() == Transformation::SourceType::RequestBody) {
                if (!requestBodyDataPart.str().empty()) {
                    mustDecodeRequestBody = true;
                }
                else {
                    LOGINFORMATIONAL(ert::tracing::Logger::informational("Empty request body received: some transformations will be ignored", ERT_FILE_LOCATION));
                }
                break;
            }
        }
    }
    if (mustDecodeRequestBody) {
        requestBodyDataPart.decode(requestHeaders);
    }

    // Request schema validation (normally used to validate native json received, but can also be used to validate the agent json representation (multipart, text, etc.)):
    if (requestSchema) {
        if (!requestSchema->validate(requestBodyDataPart.getJson())) {
            responseStatusCode = 400; // bad request
            return; // INTERRUPT TRANSFORMATIONS
        }
    }

    // Find out if response body will need to be cloned (this is true if any transformation uses it as target):
    bool usesResponseBodyAsTransformationJsonTarget = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
        if ((*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_String ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_Integer ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_Unsigned ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_Float ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_Boolean ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_Object ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJson_JsonString) {
            usesResponseBodyAsTransformationJsonTarget = true;
            break;
        }
    }

    nlohmann::json responseBodyJson;
    if (usesResponseBodyAsTransformationJsonTarget) {
        responseBodyJson = getResponseBody();   // clone provision response body to manipulate this copy and finally we will dump() it over 'responseBody':
        // if(usesResponseBodyAsTransformationJsonTarget) responseBody = responseBodyJson.dump(); <--- place this after transformations (*)
    }
    else {
        responseBody = getResponseBodyAsString(); // this could be overwritten by targets ResponseBodyString or ResponseBodyHexString
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

        // SOURCES: RequestUri, RequestUriPath, RequestUriParam, RequestBody, ResponseBody, RequestHeader, Eraser, Math, Random, Timestamp, Strftime, Recvseq, SVar, SGvar, Value, ServerEvent, InState
        if (!processSources(transformation, sourceVault, variables, requestUri, requestUriPath, requestQueryParametersMap, requestBodyDataPart, requestHeaders, eraser, generalUniqueServerSequence)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on source", ERT_FILE_LOCATION));
            continue;
        }

        std::smatch matches; // BE CAREFUL!: https://stackoverflow.com/a/51709911/2576671
        // So, we can't use 'matches' as container because source may change: BUT, using that source exclusively, it will work (*)
        std::string source; // Now, this never will be out of scope, and 'matches' will be valid.

        // FILTERS: RegexCapture, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar, EqualTo
        bool hasFilter = transformation->hasFilter();
        if (hasFilter) {
            if (!processFilters(transformation, sourceVault, variables, matches, source, eraser)) {
                LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on filter", ERT_FILE_LOCATION));
                continue;
            }
        }

        // TARGETS: ResponseBodyString, ResponseBodyHexString, ResponseBodyJson_String, ResponseBodyJson_Integer, ResponseBodyJson_Unsigned, ResponseBodyJson_Float, ResponseBodyJson_Boolean, ResponseBodyJson_Object, ResponseBodyJson_JsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, TGVar, OutState, TTxtFile, TBinFile, ServerEventToPurge
        if (!processTargets(transformation, sourceVault, variables, matches, eraser, hasFilter, responseStatusCode, responseBodyJson, responseBody, responseHeaders, responseDelayMs, outState, outStateMethod, outStateUri)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on target", ERT_FILE_LOCATION));
            continue;
        }

    }

    // (*) Regenerate final responseBody after transformations:
    if(usesResponseBodyAsTransformationJsonTarget) {
        try {
            responseBody = responseBodyJson.dump(); // this may arise type error, for example in case of trying to set json field value with binary data:
            // When having a provision transformation from 'request.body' to 'response.body.json.string./whatever':
            // echo -en '\x80\x01' | curl --http2-prior-knowledge -i -H 'content-type:application/octet-stream' -X GET "$TRAFFIC_URL/uri" --data-binary @-
            //
            // This is not valid and must be protected. The user should use another kind of target to store binary.
        }
        catch (const std::exception& e)
        {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
    }

    // Response schema validation (not supported for response body created by non-json targets, to simplify the fact to parse need on ResponseBodyString/ResponseBodyHexString):
    if (responseSchema) {
        if (!responseSchema->validate(usesResponseBodyAsTransformationJsonTarget ? responseBodyJson:getResponseBody())) {
            responseStatusCode = ert::http2comm::ResponseCode::INTERNAL_SERVER_ERROR; // 500: built response will be anyway sent although status code is overwritten with internal server error.
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

