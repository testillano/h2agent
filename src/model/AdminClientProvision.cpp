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
#include <cinttypes> // PRIu64, etc.
//#include <fcntl.h> // non-blocking fgets call

#include <nlohmann/json.hpp>
#include <arashpartow/exprtk.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

#include <AdminClientProvision.hpp>
#include <MockClientData.hpp>
#include <MockServerData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <AdminData.hpp>

#include <functions.hpp>


typedef exprtk::expression<double>   expression_t;
typedef exprtk::parser<double>       parser_t;

namespace h2agent
{
namespace model
{

AdminClientProvision::AdminClientProvision() : in_state_(DEFAULT_ADMIN_PROVISION_STATE),
    out_state_(DEFAULT_ADMIN_PROVISION_CLIENT_OUT_STATE),
    request_delay_ms_(0), request_timeout_ms_(0), mock_client_events_data_(nullptr), mock_server_events_data_(nullptr),
    seq_(0), seq_begin_(0), seq_end_(0), rps_(0), repeat_(false), timer_(nullptr), io_context_(nullptr) {;}


std::shared_ptr<h2agent::model::AdminSchema> AdminClientProvision::getRequestSchema() {

    if(request_schema_id_.empty()) return nullptr;

    if (admin_data_->getSchemaData().size() != 0) { // the only way to destroy schema references, is to clean whole schema data
        if (request_schema_) return request_schema_; // provision cache
        request_schema_ = admin_data_->getSchemaData().find(request_schema_id_);
    }

    LOGWARNING(
        if (!request_schema_) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for incoming message: VALIDATION will be IGNORED", request_schema_id_.c_str()), ERT_FILE_LOCATION);
    );

    return request_schema_;
}

std::shared_ptr<h2agent::model::AdminSchema> AdminClientProvision::getResponseSchema() {

    if(response_schema_id_.empty()) return nullptr;

    if (admin_data_->getSchemaData().size() != 0) { // the only way to destroy schema references, is to clean whole schema data
        if (response_schema_) return response_schema_; // provision cache
        response_schema_ = admin_data_->getSchemaData().find(response_schema_id_);
    }

    LOGWARNING(
        if (!response_schema_) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for outgoing message: VALIDATION will be IGNORED", response_schema_id_.c_str()), ERT_FILE_LOCATION);
    );

    return response_schema_;
}

void AdminClientProvision::saveDynamics() {
    json_["dynamics"]["sequence"] = seq_;
    json_["dynamics"]["sequenceBegin"] = seq_begin_;
    json_["dynamics"]["sequenceEnd"] = seq_end_;
    json_["dynamics"]["rps"] = rps_;
    json_["dynamics"]["repeat"] = repeat_;
}

void AdminClientProvision::transform( std::string &requestMethod,
                                      std::string &requestUri,
                                      std::string &requestBody,
                                      nghttp2::asio_http2::header_map &requestHeaders,
                                      std::string &outState,
                                      unsigned int &requestDelayMs,
                                      unsigned int &requestTimeoutMs,
                                      std::string &error
                                    )
{
    // Default values without transformations:
    requestMethod = getRequestMethod();
    requestUri = getRequestUri();
    requestHeaders = getRequestHeaders();
    outState = getOutState();
    requestDelayMs = getRequestDelayMilliseconds();
    requestTimeoutMs = getRequestTimeoutMilliseconds();

    // Find out if request body will need to be cloned (this is true if any transformation uses it as target):
    bool usesRequestBodyAsTransformationJsonTarget = false;
    for (const auto &t : transformations_) {
        if (t->getTargetType() == Transformation::TargetType::RequestBodyJson_String ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_Integer ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_Unsigned ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_Float ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_Boolean ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_Object ||
                t->getTargetType() == Transformation::TargetType::RequestBodyJson_JsonString) {
            usesRequestBodyAsTransformationJsonTarget = true;
            break;
        }
    }

    nlohmann::json requestBodyJson;
    if (usesRequestBodyAsTransformationJsonTarget) {
        requestBodyJson = getRequestBody();   // clone provision response body to manipulate this copy and finally we will dump() it over 'responseBody':
        // if(usesRequestBodyAsTransformationJsonTarget) requestBody = requesteBodyJson.dump(); <--- place this after transformations (*)
    }
    else {
        requestBody = getRequestBodyAsString(); // this could be overwritten by targets RequestBodyString or RequestBodyHexString
    }

    // Dynamic variables map: inherited along the transformation chain
    std::map<std::string, std::string> variables; // source & target variables (key=variable name/value=variable value)
    variables["sequence"] = std::to_string(seq_); // reserved read-only variable

    // Type converter:
    TypeConverter sourceVault{};

    // Apply transformations sequentially
    bool breakCondition = false;
    for (const auto &transformation : transformations_) {

        if (breakCondition) break;

        bool eraser = false;

        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Processing transformation item: %s", transformation->asString().c_str()), ERT_FILE_LOCATION));

        // SOURCES
        if (!processSources(transformation, sourceVault, variables, requestUri, requestHeaders, eraser, 0 /*sendseq not available here*/, usesRequestBodyAsTransformationJsonTarget, requestBodyJson)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on source", ERT_FILE_LOCATION));
            continue;
        }

        std::smatch matches;
        std::string source;

        // FILTERS
        bool hasFilter = transformation->hasFilter();
        if (hasFilter) {
            if (eraser || !processFilters(transformation, sourceVault, variables, matches, source)) {
                LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on filter", ERT_FILE_LOCATION));
                if (eraser) LOGWARNING(ert::tracing::Logger::warning("Filter is not allowed when using 'eraser' source type. Transformation will be ignored.", ERT_FILE_LOCATION));
                continue;
            }
        }

        // TARGETS
        if (!processTargets(transformation, sourceVault, variables, matches, eraser, hasFilter, requestMethod, requestUri, requestBodyJson, requestBody, requestHeaders, requestDelayMs, requestTimeoutMs, outState, breakCondition)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on target", ERT_FILE_LOCATION));
            continue;
        }
    }

    // Request schema validation
    if (getRequestSchema()) {
        if (!getRequestSchema()->validate(usesRequestBodyAsTransformationJsonTarget ? requestBodyJson:getRequestBody(), error)) {
            //error = "Invalid request built against request schema provided: ";
            return;
        }
    }

    // (*) Regenerate final requestBody after transformations:
    if(usesRequestBodyAsTransformationJsonTarget && !requestBodyJson.empty()) {
        try {
            requestBody = requestBodyJson.dump();
        }
        catch (const std::exception& e)
        {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
    }
}

void AdminClientProvision::transformResponse( const std::string &requestUri,
        const nghttp2::asio_http2::header_map &requestHeaders,
        const ert::http2comm::Http2Client::response &receivedResponse,
        std::uint64_t generalUniqueClientSequence,
        std::string &outState
                                            )
{
    if (on_response_transformations_.empty()) return;

    // Dynamic variables map: inherited along the transformation chain
    std::map<std::string, std::string> variables;
    variables["sequence"] = std::to_string(seq_);

    // Type converter:
    TypeConverter sourceVault{};

    // Dummy request body json (not modified in response phase, but needed by processSources signature)
    nlohmann::json dummyRequestBodyJson;

    // Apply transformations sequentially
    bool breakCondition = false;
    for (const auto &transformation : on_response_transformations_) {

        if (breakCondition) break;

        bool eraser = false;

        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Processing on-response transformation item: %s", transformation->asString().c_str()), ERT_FILE_LOCATION));

        // SOURCES (with response context)
        if (!processSources(transformation, sourceVault, variables, requestUri, requestHeaders, eraser, generalUniqueClientSequence, false, dummyRequestBodyJson, &receivedResponse)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on source", ERT_FILE_LOCATION));
            continue;
        }

        std::smatch matches;
        std::string source;

        // FILTERS
        bool hasFilter = transformation->hasFilter();
        if (hasFilter) {
            if (eraser || !processFilters(transformation, sourceVault, variables, matches, source)) {
                LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on filter", ERT_FILE_LOCATION));
                if (eraser) LOGWARNING(ert::tracing::Logger::warning("Filter is not allowed when using 'eraser' source type. Transformation will be ignored.", ERT_FILE_LOCATION));
                continue;
            }
        }

        // TARGETS (response phase: only var/globalVar/outState/file/socket/break/clientEvent targets apply)
        // We reuse processTargets with dummy request body params
        nlohmann::json dummyJson;
        std::string dummyString;
        std::string dummyMethod;
        std::string dummyUri;
        nghttp2::asio_http2::header_map dummyHeaders;
        unsigned int dummyDelayMs = 0;
        unsigned int dummyTimeoutMs = 0;
        if (!processTargets(transformation, sourceVault, variables, matches, eraser, hasFilter, dummyMethod, dummyUri, dummyJson, dummyString, dummyHeaders, dummyDelayMs, dummyTimeoutMs, outState, breakCondition)) {
            LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on target", ERT_FILE_LOCATION));
            continue;
        }
    }

    // Response schema validation:
    if (getResponseSchema()) {
        nlohmann::json responseJson;
        if (h2agent::model::parseJsonContent(receivedResponse.body, responseJson)) {
            std::string error{};
            if (!getResponseSchema()->validate(responseJson, error)) {
                ert::tracing::Logger::error(ert::tracing::Logger::asString("Response schema validation failed: %s", error.c_str()), ERT_FILE_LOCATION);
            }
        }
    }
}

bool AdminClientProvision::processSources(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        std::map<std::string, std::string>& variables,
        const std::string &requestUri,
        const nghttp2::asio_http2::header_map &requestHeaders,
        bool &eraser,
        std::uint64_t generalUniqueClientSequence,
        bool usesRequestBodyAsTransformationJsonTarget, const nlohmann::json &requestBodyJson,
        const ert::http2comm::Http2Client::response *receivedResponse) const {

    switch (transformation->getSourceType()) {
    case Transformation::SourceType::RequestUri:
    {
        sourceVault.setString(requestUri);
        break;
    }
    case Transformation::SourceType::RequestBody:
    {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_);
        if (usesRequestBodyAsTransformationJsonTarget) {
            if (!sourceVault.setObject(requestBodyJson, path)) return false;
        }
        else {
            sourceVault.setString(getRequestBodyAsString());
        }
        break;
    }
    case Transformation::SourceType::ResponseBody:
    {
        if (!receivedResponse) return false;
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_);
        nlohmann::json responseJson;
        if (!h2agent::model::parseJsonContent(receivedResponse->body, responseJson)) {
            sourceVault.setString(receivedResponse->body);
        }
        else if (!sourceVault.setObject(responseJson, path)) {
            return false;
        }
        break;
    }
    case Transformation::SourceType::RequestHeader:
    {
        auto iter = requestHeaders.find(transformation->getSource());
        if (iter != requestHeaders.end()) sourceVault.setString(iter->second.value);
        else return false;
        break;
    }
    case Transformation::SourceType::ResponseHeader:
    {
        if (!receivedResponse) return false;
        auto iter = receivedResponse->headers.find(transformation->getSource());
        if (iter != receivedResponse->headers.end()) sourceVault.setString(iter->second.value);
        else return false;
        break;
    }
    case Transformation::SourceType::ResponseStatusCode:
    {
        if (!receivedResponse) return false;
        sourceVault.setInteger(receivedResponse->statusCode);
        break;
    }
    case Transformation::SourceType::Eraser:
    {
        sourceVault.setString("");
        eraser = true;
        break;
    }
    case Transformation::SourceType::Math:
    {
        std::string expressionString = transformation->getSource();
        replaceVariables(expressionString, transformation->getSourcePatterns(), variables, global_variable_);
        expression_t expression;
        parser_t parser;
        parser.compile(expressionString, expression);
        double result = expression.value();
        if (result == (int)result) sourceVault.setInteger(expression.value());
        else sourceVault.setFloat(expression.value());
        break;
    }
    case Transformation::SourceType::Random:
    {
        int range = transformation->getSourceI2() - transformation->getSourceI1() + 1;
        sourceVault.setInteger(transformation->getSourceI1() + (rand() % range));
        break;
    }
    case Transformation::SourceType::RandomSet:
    {
        sourceVault.setStringReplacingVariables(transformation->getSourceTokenized()[rand () % transformation->getSourceTokenized().size()], transformation->getSourcePatterns(), variables, global_variable_);
        break;
    }
    case Transformation::SourceType::Timestamp:
    {
        if (transformation->getSource() == "s") sourceVault.setInteger(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        else if (transformation->getSource() == "ms") sourceVault.setInteger(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        else if (transformation->getSource() == "us") sourceVault.setInteger(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        else if (transformation->getSource() == "ns") sourceVault.setInteger(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        break;
    }
    case Transformation::SourceType::Strftime:
    {
        std::time_t unixTime = 0;
        std::time (&unixTime);
        char buffer[100] = {0};
        strftime(buffer, sizeof(buffer), transformation->getSource().c_str(), localtime(&unixTime));
        sourceVault.setStringReplacingVariables(std::string(buffer), transformation->getSourcePatterns(), variables, global_variable_);
        break;
    }
    case Transformation::SourceType::Sendseq:
    {
        sourceVault.setUnsigned(generalUniqueClientSequence);
        break;
    }
    case Transformation::SourceType::Seq:
    {
        sourceVault.setUnsigned(seq_);
        break;
    }
    case Transformation::SourceType::Recvseq:
    {
        sourceVault.setUnsigned(generalUniqueClientSequence); // reuse for client context
        break;
    }
    case Transformation::SourceType::SVar:
    {
        std::string varname = transformation->getSource();
        replaceVariables(varname, transformation->getSourcePatterns(), variables, global_variable_);
        auto iter = variables.find(varname);
        if (iter != variables.end()) sourceVault.setString(iter->second);
        else return false;
        break;
    }
    case Transformation::SourceType::SGVar:
    {
        std::string varname = transformation->getSource();
        replaceVariables(varname, transformation->getSourcePatterns(), variables, global_variable_);
        std::string globalVariableValue{};
        if (global_variable_->tryGet(varname, globalVariableValue)) sourceVault.setString(globalVariableValue);
        else return false;
        break;
    }
    case Transformation::SourceType::Value:
    {
        sourceVault.setStringReplacingVariables(transformation->getSource(), transformation->getSourcePatterns(), variables, global_variable_);
        break;
    }
    case Transformation::SourceType::ServerEvent:
    {
        std::string event_method = transformation->getSourceTokenized()[0];
        replaceVariables(event_method, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_uri = transformation->getSourceTokenized()[1];
        replaceVariables(event_uri, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_number = transformation->getSourceTokenized()[2];
        replaceVariables(event_number, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_path = transformation->getSourceTokenized()[3];
        replaceVariables(event_path, transformation->getSourcePatterns(), variables, global_variable_);

        EventKey ekey(event_method, event_uri, event_number);
        auto mockServerRequest = mock_server_events_data_->getEvent(ekey);
        if (!mockServerRequest) return false;
        if (!sourceVault.setObject(mockServerRequest->getJson(), event_path)) return false;
        break;
    }
    case Transformation::SourceType::ClientEvent:
    {
        std::string event_endpoint = transformation->getSourceTokenized()[0];
        replaceVariables(event_endpoint, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_method = transformation->getSourceTokenized()[1];
        replaceVariables(event_method, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_uri = transformation->getSourceTokenized()[2];
        replaceVariables(event_uri, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_number = transformation->getSourceTokenized()[3];
        replaceVariables(event_number, transformation->getSourcePatterns(), variables, global_variable_);
        std::string event_path = transformation->getSourceTokenized()[4];
        replaceVariables(event_path, transformation->getSourcePatterns(), variables, global_variable_);

        DataKey dkey(event_endpoint, event_method, event_uri);
        EventKey ekey(dkey, event_number);
        auto mockClientRequest = mock_client_events_data_->getEvent(ekey);
        if (!mockClientRequest) return false;
        if (!sourceVault.setObject(mockClientRequest->getJson(), event_path)) return false;
        break;
    }
    case Transformation::SourceType::InState:
    {
        sourceVault.setString(getInState());
        break;
    }
    case Transformation::SourceType::STxtFile:
    {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_);
        std::string content;
        file_manager_->read(path, content, true);
        sourceVault.setString(std::move(content));
        break;
    }
    case Transformation::SourceType::SBinFile:
    {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, global_variable_);
        std::string content;
        file_manager_->read(path, content, false);
        sourceVault.setString(std::move(content));
        break;
    }
    case Transformation::SourceType::Command:
    {
        std::string command = transformation->getSource();
        replaceVariables(command, transformation->getSourcePatterns(), variables, global_variable_);
        static char buffer[256];
        std::string output{};
        FILE *fp = popen(command.c_str(), "r");
        variables["rc"] = "-1";
        if (fp) {
            while(fgets(buffer, sizeof(buffer), fp)) output += buffer;
            variables["rc"] = std::to_string(WEXITSTATUS(pclose(fp)));
        }
        sourceVault.setString(std::move(output));
        break;
    }
    // Not applicable in client context:
    case Transformation::SourceType::RequestUriPath:
    case Transformation::SourceType::RequestUriParam:
        return false;
    }

    return true;
}

bool AdminClientProvision::processFilters(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        const std::map<std::string, std::string>& variables,
        std::smatch &matches,
        std::string &source) const
{
    bool success = false;
    std::string targetS;
    std::int64_t targetI = 0;
    std::uint64_t targetU = 0;
    double targetF = 0;

    if (transformation->getFilterType() != Transformation::FilterType::Sum && transformation->getFilterType() != Transformation::FilterType::Multiply) {
        source = sourceVault.getString(success);
        if (!success) return false;
    }

    switch (transformation->getFilterType()) {
    case Transformation::FilterType::RegexCapture:
    {
        if (std::regex_match(source, matches, transformation->getFilterRegex()) && matches.size() >=1) {
            targetS = matches.str(0);
            sourceVault.setString(targetS);
        }
        else return false;
        break;
    }
    case Transformation::FilterType::RegexReplace:
    {
        targetS = std::regex_replace(source, transformation->getFilterRegex(), transformation->getFilter());
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::Append:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, global_variable_);
        targetS = source + filter;
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::Prepend:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, global_variable_);
        targetS = filter + source;
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::Sum:
    {
        switch (transformation->getFilterNumberType()) {
        case 0: targetI = sourceVault.getInteger(success); if (success) targetI += transformation->getFilterI(); sourceVault.setInteger(targetI); break;
        case 1: targetU = sourceVault.getUnsigned(success); if (success) targetU += transformation->getFilterU(); sourceVault.setUnsigned(targetU); break;
        case 2: targetF = sourceVault.getFloat(success); if (success) targetF += transformation->getFilterF(); sourceVault.setFloat(targetF); break;
        }
        break;
    }
    case Transformation::FilterType::Multiply:
    {
        switch (transformation->getFilterNumberType()) {
        case 0: targetI = sourceVault.getInteger(success); if (success) targetI *= transformation->getFilterI(); sourceVault.setInteger(targetI); break;
        case 1: targetU = sourceVault.getUnsigned(success); if (success) targetU *= transformation->getFilterU(); sourceVault.setUnsigned(targetU); break;
        case 2: targetF = sourceVault.getFloat(success); if (success) targetF *= transformation->getFilterF(); sourceVault.setFloat(targetF); break;
        }
        break;
    }
    case Transformation::FilterType::ConditionVar:
    {
        std::string conditionVar = transformation->getFilter();
        bool negate = (!conditionVar.empty() && conditionVar[0] == '!');
        if (negate) conditionVar = conditionVar.substr(1);
        auto iter = variables.find(conditionVar);
        bool exists = (iter != variables.end() && !iter->second.empty());
        if (negate) exists = !exists;
        if (!exists) return false;
        break;
    }
    case Transformation::FilterType::EqualTo:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, global_variable_);
        if (source != filter) return false;
        break;
    }
    case Transformation::FilterType::DifferentFrom:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, global_variable_);
        if (source == filter) return false;
        break;
    }
    case Transformation::FilterType::JsonConstraint:
    {
        nlohmann::json sourceJson;
        if (!h2agent::model::parseJsonContent(source, sourceJson)) return false;
        std::string result;
        h2agent::model::jsonConstraint(sourceJson, transformation->getFilterObject(), result);
        sourceVault.setString(result.empty() ? "1" : result);
        break;
    }
    case Transformation::FilterType::SchemaId:
    {
        auto schema = admin_data_->getSchemaData().find(transformation->getFilter());
        if (!schema) return false;
        std::string error{};
        nlohmann::json sourceJson;
        if (!h2agent::model::parseJsonContent(source, sourceJson)) return false;
        bool valid = schema->validate(sourceJson, error);
        sourceVault.setString(valid ? "1" : error);
        break;
    }
    }

    return true;
}

bool AdminClientProvision::processTargets(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        std::map<std::string, std::string>& variables,
        const std::smatch &matches,
        bool eraser,
        bool hasFilter,
        std::string &requestMethod,
        std::string &requestUri,
        nlohmann::json &requestBodyJson,
        std::string &requestBodyAsString,
        nghttp2::asio_http2::header_map &requestHeaders,
        unsigned int &requestDelayMs,
        unsigned int &requestTimeoutMs,
        std::string &outState,
        bool &breakCondition) const
{
    bool success = false;
    std::string targetS;
    std::int64_t targetI = 0;
    std::uint64_t targetU = 0;
    double targetF = 0;
    bool boolean = false;
    nlohmann::json obj;

    std::string target = transformation->getTarget();
    replaceVariables(target, transformation->getTargetPatterns(), variables, global_variable_);

    try {
        switch (transformation->getTargetType()) {
        case Transformation::TargetType::RequestBodyString:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            requestBodyAsString = targetS;
            break;
        }
        case Transformation::TargetType::RequestBodyHexString:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (!h2agent::model::fromHexString(targetS, requestBodyAsString)) return false;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_String:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            nlohmann::json::json_pointer j_ptr(target);
            requestBodyJson[j_ptr] = targetS;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_Integer:
        {
            targetI = sourceVault.getInteger(success);
            if (!success) return false;
            nlohmann::json::json_pointer j_ptr(target);
            requestBodyJson[j_ptr] = targetI;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_Unsigned:
        {
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            nlohmann::json::json_pointer j_ptr(target);
            requestBodyJson[j_ptr] = targetU;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_Float:
        {
            targetF = sourceVault.getFloat(success);
            if (!success) return false;
            nlohmann::json::json_pointer j_ptr(target);
            requestBodyJson[j_ptr] = targetF;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_Boolean:
        {
            boolean = sourceVault.getBoolean(success);
            if (!success) return false;
            nlohmann::json::json_pointer j_ptr(target);
            requestBodyJson[j_ptr] = boolean;
            break;
        }
        case Transformation::TargetType::RequestBodyJson_Object:
        {
            if (eraser) {
                if (target.empty()) { requestBodyJson.erase(requestBodyJson.begin(), requestBodyJson.end()); return false; }
                size_t lastSlashPos = target.find_last_of("/");
                std::string parentPath = target.substr(0, lastSlashPos);
                std::string childKey = (lastSlashPos + 1 < target.size()) ? target.substr(lastSlashPos + 1) : "";
                nlohmann::json::json_pointer j_ptr(parentPath);
                requestBodyJson[j_ptr].erase(childKey);
                return false;
            }
            nlohmann::json::json_pointer j_ptr(target);
            switch (sourceVault.getNativeType()) {
            case TypeConverter::NativeType::Object: obj = sourceVault.getObject(success); if (success) { if (target.empty()) requestBodyJson.merge_patch(obj); else requestBodyJson[j_ptr] = obj; } break;
            case TypeConverter::NativeType::String: targetS = sourceVault.getString(success); if (success) requestBodyJson[j_ptr] = targetS; break;
            case TypeConverter::NativeType::Integer: targetI = sourceVault.getInteger(success); if (success) requestBodyJson[j_ptr] = targetI; break;
            case TypeConverter::NativeType::Unsigned: targetU = sourceVault.getUnsigned(success); if (success) requestBodyJson[j_ptr] = targetU; break;
            case TypeConverter::NativeType::Float: targetF = sourceVault.getFloat(success); if (success) requestBodyJson[j_ptr] = targetF; break;
            case TypeConverter::NativeType::Boolean: boolean = sourceVault.getBoolean(success); if (success) requestBodyJson[j_ptr] = boolean; break;
            }
            break;
        }
        case Transformation::TargetType::RequestBodyJson_JsonString:
        {
            nlohmann::json::json_pointer j_ptr(target);
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (!h2agent::model::parseJsonContent(targetS, obj)) return false;
            if (target.empty()) requestBodyJson.merge_patch(obj); else requestBodyJson[j_ptr] = obj;
            break;
        }
        case Transformation::TargetType::RequestHeader_t:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            requestHeaders.emplace(target, nghttp2::asio_http2::header_value{targetS});
            break;
        }
        case Transformation::TargetType::RequestDelayMs:
        {
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            requestDelayMs = targetU;
            break;
        }
        case Transformation::TargetType::RequestTimeoutMs:
        {
            targetU = sourceVault.getUnsigned(success);
            if (!success) return false;
            requestTimeoutMs = targetU;
            break;
        }
        case Transformation::TargetType::RequestUri_t:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            requestUri = targetS;
            break;
        }
        case Transformation::TargetType::RequestMethod_t:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            requestMethod = targetS;
            break;
        }
        case Transformation::TargetType::TVar:
        {
            if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                std::string varname;
                if (matches.size() >=1) {
                    variables[target] = matches.str(0);
                    for(size_t i=1; i < matches.size(); i++) {
                        varname = target + "." + std::to_string(i);
                        variables[varname] = matches.str(i);
                    }
                }
            }
            else {
                targetS = sourceVault.getString(success);
                if (!success) return false;
                if (hasFilter) {
                    if(transformation->getFilterType() == Transformation::FilterType::JsonConstraint || transformation->getFilterType() == Transformation::FilterType::SchemaId) {
                        if (targetS != "1") { variables[target + ".fail"] = targetS; targetS = ""; }
                    }
                }
                variables[target] = targetS;
            }
            break;
        }
        case Transformation::TargetType::TGVar:
        {
            if (eraser) {
                bool exists;
                global_variable_->remove(target, exists);
            }
            else if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                std::string varname;
                if (matches.size() >=1) {
                    global_variable_->load(target, matches.str(0));
                    for(size_t i=1; i < matches.size(); i++) {
                        varname = target + "." + std::to_string(i);
                        global_variable_->load(varname, matches.str(i));
                    }
                }
            }
            else {
                targetS = sourceVault.getString(success);
                if (!success) return false;
                global_variable_->load(target, targetS);
            }
            break;
        }
        case Transformation::TargetType::OutState:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            outState = targetS;
            break;
        }
        case Transformation::TargetType::TTxtFile:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (eraser) { file_manager_->empty(target); }
            else {
                bool longTerm = (transformation->getTargetPatterns().empty());
                file_manager_->write(target, targetS, true, (longTerm ? configuration_->getLongTermFilesCloseDelayUsecs():configuration_->getShortTermFilesCloseDelayUsecs()));
            }
            break;
        }
        case Transformation::TargetType::TBinFile:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (eraser) { file_manager_->empty(target); }
            else {
                bool longTerm = (transformation->getTargetPatterns().empty());
                file_manager_->write(target, targetS, false, (longTerm ? configuration_->getLongTermFilesCloseDelayUsecs():configuration_->getShortTermFilesCloseDelayUsecs()));
            }
            break;
        }
        case Transformation::TargetType::UDPSocket:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            std::string path = target;
            size_t lastDotPos = target.find_last_of("|");
            unsigned int delayMs = atoi(target.substr(lastDotPos + 1).c_str());
            path = target.substr(0, lastDotPos);
            socket_manager_->write(path, targetS, delayMs * 1000);
            break;
        }
        case Transformation::TargetType::ClientEventToPurge:
        {
            if (!eraser) return false;
            std::string event_endpoint = transformation->getTargetTokenized()[0];
            replaceVariables(event_endpoint, transformation->getTargetPatterns(), variables, global_variable_);
            std::string event_method = transformation->getTargetTokenized()[1];
            replaceVariables(event_method, transformation->getTargetPatterns(), variables, global_variable_);
            std::string event_uri = transformation->getTargetTokenized()[2];
            replaceVariables(event_uri, transformation->getTargetPatterns(), variables, global_variable_);
            std::string event_number = transformation->getTargetTokenized()[3];
            replaceVariables(event_number, transformation->getTargetPatterns(), variables, global_variable_);

            bool clientDataDeleted = false;
            DataKey dkey(event_endpoint, event_method, event_uri);
            EventKey ekey(dkey, event_number);
            mock_client_events_data_->clear(clientDataDeleted, ekey);
            break;
        }
        case Transformation::TargetType::Break:
        {
            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (targetS.empty()) return false;
            breakCondition = true;
            return false;
        }
        // Not applicable in client pre-send context:
        case Transformation::TargetType::ResponseBodyString:
        case Transformation::TargetType::ResponseBodyHexString:
        case Transformation::TargetType::ResponseBodyJson_String:
        case Transformation::TargetType::ResponseBodyJson_Integer:
        case Transformation::TargetType::ResponseBodyJson_Unsigned:
        case Transformation::TargetType::ResponseBodyJson_Float:
        case Transformation::TargetType::ResponseBodyJson_Boolean:
        case Transformation::TargetType::ResponseBodyJson_Object:
        case Transformation::TargetType::ResponseBodyJson_JsonString:
        case Transformation::TargetType::ResponseHeader_t:
        case Transformation::TargetType::ResponseStatusCode_t:
        case Transformation::TargetType::ResponseDelayMs:
        case Transformation::TargetType::ServerEventToPurge:
            break;
        }
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
    }

    return true;
}

bool AdminClientProvision::updateTriggering(const std::string &sequenceBegin, const std::string &sequenceEnd, const std::string &rps, const std::string &repeat) {

    // Range reads:
    bool negative = false;
    std::uint64_t u_sequenceBegin = seq_begin_;
    std::uint64_t u_sequenceEnd = seq_end_;

    if (!sequenceBegin.empty()) {
        if (!h2agent::model::string2uint64andSign(sequenceBegin, u_sequenceBegin, negative) || negative) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'sequenceBegin' value: %s (must be >= 0)", sequenceBegin.c_str()), ERT_FILE_LOCATION));
            return false;
        }
    }

    if (!sequenceEnd.empty()) {
        if (!h2agent::model::string2uint64andSign(sequenceEnd, u_sequenceEnd, negative) || negative) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'sequenceEnd' value: %s (must be >= 0)", sequenceEnd.c_str()), ERT_FILE_LOCATION));
            return false;
        }
    }

    // Range check:
    if (u_sequenceBegin > u_sequenceEnd) {
        LOGWARNING(std::string fmt = std::string("Incompatible range for 'sequenceBegin' and 'sequenceEnd': %") + PRIu64 + std::string(" > %") + PRIu64; ert::tracing::Logger::warning(ert::tracing::Logger::asString(fmt.c_str(), u_sequenceBegin, u_sequenceEnd), ERT_FILE_LOCATION));
        return false;
    }

    // Range assignment:
    seq_begin_ = u_sequenceBegin;
    seq_end_ = u_sequenceEnd;


    // Rate:
    if (!rps.empty()) {
        std::uint64_t aux;
        if (!h2agent::model::string2uint64andSign(rps, aux, negative) || negative) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'rps' value: %s (must be >= 0)", rps.c_str()), ERT_FILE_LOCATION));
            return false;
        }
        rps_ = aux;
    }

    // Repeat:
    if (!repeat.empty()) {
        if (repeat != "true" && repeat != "false") {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'repeat' value: %s (allowed: true|false)", repeat.c_str()), ERT_FILE_LOCATION));
            return false;
        }
        repeat_ = (repeat == "true");
    }

    // Save dynamics into provision json:
    saveDynamics();

    return true;
}

void AdminClientProvision::startTicking(boost::asio::io_context *ioContext, std::function<void()> tickCallback) {
    stopTicking();
    io_context_ = ioContext;
    tick_callback_ = std::move(tickCallback);
    seq_ = seq_begin_;
    saveDynamics();
    timer_ = new boost::asio::steady_timer(*io_context_);
    scheduleTick();
}

void AdminClientProvision::stopTicking() {
    if (timer_) {
        timer_->cancel();
        delete timer_;
        timer_ = nullptr;
    }
    tick_callback_ = nullptr;
}

void AdminClientProvision::scheduleTick() {
    if (!timer_ || rps_ == 0) {
        stopTicking();
        return;
    }

    auto period = std::chrono::microseconds(1000000 / rps_);
    timer_->expires_after(period);
    timer_->async_wait([this](const boost::system::error_code &ec) {
        if (ec) return; // cancelled

        if (tick_callback_) tick_callback_();

        seq_++;
        saveDynamics();

        if (seq_ > seq_end_) {
            if (repeat_) {
                seq_ = seq_begin_;
                saveDynamics();
            }
            else {
                stopTicking();
                return;
            }
        }

        scheduleTick();
    });
}

bool AdminClientProvision::load(const nlohmann::json &j) {

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto it = j.find("id");
    client_provision_id_ = *it;

    // Optional
    it = j.find("endpoint");
    if (it != j.end() && it->is_string()) {
        client_endpoint_id_ = *it;
    }

    it = j.find("requestMethod");
    if (it != j.end() && it->is_string()) {
        request_method_ = *it;
    }

    it = j.find("requestUri");
    if (it != j.end() && it->is_string()) {
        request_uri_ = *it;
    }

    it = j.find("inState");
    if (it != j.end() && it->is_string()) {
        in_state_ = *it;
        if (in_state_.empty()) in_state_ = DEFAULT_ADMIN_PROVISION_STATE;
        if (in_state_ == DEFAULT_ADMIN_PROVISION_CLIENT_OUT_STATE) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid inState reserved value: %s", DEFAULT_ADMIN_PROVISION_CLIENT_OUT_STATE), ERT_FILE_LOCATION);
            return false;
        }
    }

    it = j.find("outState");
    if (it != j.end() && it->is_string()) {
        out_state_ = *it;
        if (out_state_.empty()) out_state_ = DEFAULT_ADMIN_PROVISION_STATE;
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

    it = j.find("requestHeaders");
    if (it != j.end() && it->is_object()) {
        for (auto& [key, val] : it->items())
            request_headers_.emplace(key, nghttp2::asio_http2::header_value{val});
    }

    it = j.find("requestBody");
    if (it != j.end()) {
        if (it->is_object() || it->is_array()) {
            request_body_ = *it;
            request_body_string_ = request_body_.dump(); // valid as cache for static requests (not updated with transformations)
        }
        else if (it->is_string()) {
            request_body_string_ = *it;
        }
        else if (it->is_number_integer() || it->is_number_unsigned()) {
            //request_body_integer_ = *it;
            int number = *it;
            request_body_string_ = std::to_string(number);
        }
        else if (it->is_number_float()) {
            //request_body_number_ = *it;
            request_body_string_ = std::to_string(double(*it));
        }
        else if (it->is_boolean()) {
            //request_body_boolean_ = *it;
            request_body_string_ = ((bool)(*it) ? "true":"false");
        }
        else if (it->is_null()) {
            //request_body_null_ = true;
            request_body_string_ = "null";
        }
    }

    it = j.find("requestDelayMs");
    if (it != j.end() && it->is_number()) {
        request_delay_ms_ = *it;
    }

    it = j.find("timeoutMs");
    if (it != j.end() && it->is_number()) {
        request_timeout_ms_ = *it;
    }

    auto transform_it = j.find("transform");
    if (transform_it != j.end()) {
        LOGDEBUG(ert::tracing::Logger::debug("Load transformations ('transform' node)", ERT_FILE_LOCATION));
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(transformations_, it);
        }
    }

    transform_it = j.find("onResponseTransform");
    if (transform_it != j.end()) {
        LOGDEBUG(ert::tracing::Logger::debug("Load transformations ('onResponseTransform' node)", ERT_FILE_LOCATION));
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(on_response_transformations_, it);
        }
    }

    // Store key:
    h2agent::model::calculateStringKey(key_, in_state_, client_provision_id_);

    // Add dynamic load information to json__:
    saveDynamics();

    return true;
}

void AdminClientProvision::loadTransformation(std::vector<std::shared_ptr<Transformation>> &transformationsVector, const nlohmann::json &j) {

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Loading transformation item: %s", j.dump().c_str()); // avoid newlines in traces (dump(n) pretty print)
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Transformation object to fill:
    auto transformation = std::make_shared<Transformation>();

    if (transformation->load(j)) {
        transformationsVector.push_back(transformation);
    }
    else {
        ert::tracing::Logger::error("Discarded transform item due to incoherent data", ERT_FILE_LOCATION);
    }
}


}
}
