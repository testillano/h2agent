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
#include <Vault.hpp>
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
    seq_(0), seq_begin_(0), seq_end_(0), cps_(0), repeat_(false), timer_(nullptr), io_context_(nullptr) {;}


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
    json_["dynamics"]["cps"] = cps_;
    json_["dynamics"]["repeat"] = repeat_;
}

void AdminClientProvision::executeOnFilterFail(
        const std::vector<std::shared_ptr<Transformation>> &fallbacks,
        TypeConverter &sourceVault, std::map<std::string, std::string> &variables,
        const std::string &requestUri, const nghttp2::asio_http2::header_map &requestHeaders,
        std::uint64_t sendSeq, bool usesRequestBodyAsTransformationJsonTarget,
        const nlohmann::json &requestBodyJson,
        const ert::http2comm::Http2Client::response *receivedResponse,
        std::string &requestMethod, std::string &requestUri_out,
        nlohmann::json &requestBodyJson_out, std::string &requestBody,
        nghttp2::asio_http2::header_map &requestHeaders_out,
        unsigned int &requestDelayMs, unsigned int &requestTimeoutMs,
        std::string &outState, bool &breakCondition) const {

    for (const auto &fallback : fallbacks) {
        bool fbEraser = false;
        if (!processSources(fallback, sourceVault, variables, requestUri, requestHeaders, fbEraser, sendSeq, usesRequestBodyAsTransformationJsonTarget, requestBodyJson, receivedResponse)) continue;
        std::smatch fbMatches{};
        std::string fbSource{};
        if (fallback->hasFilter() && (fbEraser || !processFilters(fallback, sourceVault, variables, fbMatches, fbSource))) {
            executeOnFilterFail(fallback->getOnFilterFail(), sourceVault, variables, requestUri, requestHeaders, sendSeq, usesRequestBodyAsTransformationJsonTarget, requestBodyJson, receivedResponse, requestMethod, requestUri_out, requestBodyJson_out, requestBody, requestHeaders_out, requestDelayMs, requestTimeoutMs, outState, breakCondition);
            continue;
        }
        processTargets(fallback, sourceVault, variables, fbMatches, fbEraser, fallback->hasFilter(), requestMethod, requestUri_out, requestBodyJson_out, requestBody, requestHeaders_out, requestDelayMs, requestTimeoutMs, outState, breakCondition);
    }
}

void AdminClientProvision::transform( std::string &requestMethod,
                                      std::string &requestUri,
                                      std::string &requestBody,
                                      nghttp2::asio_http2::header_map &requestHeaders,
                                      std::string &outState,
                                      unsigned int &requestDelayMs,
                                      unsigned int &requestTimeoutMs,
                                      std::string &error,
                                      std::map<std::string, std::string> &variables
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

    // Scoped variables: update reserved read-only variable
    variables["sequence"] = std::to_string(seq_);

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

                // onFilterFail:
                executeOnFilterFail(transformation->getOnFilterFail(), sourceVault, variables, requestUri, requestHeaders, 0, usesRequestBodyAsTransformationJsonTarget, requestBodyJson, nullptr, requestMethod, requestUri, requestBodyJson, requestBody, requestHeaders, requestDelayMs, requestTimeoutMs, outState, breakCondition);

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

bool AdminClientProvision::transformResponse( const std::string &requestUri,
        const nghttp2::asio_http2::header_map &requestHeaders,
        const ert::http2comm::Http2Client::response &receivedResponse,
        std::uint64_t sendSeq,
        std::string &outState,
        std::map<std::string, std::string> &variables
                                            )
{
    bool validationOk = true;

    if (!on_response_transformations_.empty()) {

    // Scoped variables: update reserved read-only variable
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
        if (!processSources(transformation, sourceVault, variables, requestUri, requestHeaders, eraser, sendSeq, false, dummyRequestBodyJson, &receivedResponse)) {
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

                // onFilterFail:
                {
                    nlohmann::json dummyJson2;
                    std::string dummyString2, dummyMethod2, dummyUri2;
                    nghttp2::asio_http2::header_map dummyHeaders2;
                    unsigned int dummyDelayMs2 = 0, dummyTimeoutMs2 = 0;
                    executeOnFilterFail(transformation->getOnFilterFail(), sourceVault, variables, requestUri, requestHeaders, sendSeq, false, dummyRequestBodyJson, &receivedResponse, dummyMethod2, dummyUri2, dummyJson2, dummyString2, dummyHeaders2, dummyDelayMs2, dummyTimeoutMs2, outState, breakCondition);
                }

                continue;
            }
        }
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

    } // !on_response_transformations_.empty()

    // Expected response status code validation:
    if (expected_response_status_code_ != 0 && receivedResponse.statusCode != (int)expected_response_status_code_) {
        ert::tracing::Logger::error(ert::tracing::Logger::asString("Expected response status code %u but got %d", expected_response_status_code_, receivedResponse.statusCode), ERT_FILE_LOCATION);
        validationOk = false;
    }

    // Response schema validation:
    if (getResponseSchema()) {
        nlohmann::json responseJson;
        if (h2agent::model::parseJsonContent(receivedResponse.body, responseJson)) {
            std::string error{};
            if (!getResponseSchema()->validate(responseJson, error)) {
                ert::tracing::Logger::error(ert::tracing::Logger::asString("Response schema validation failed: %s", error.c_str()), ERT_FILE_LOCATION);
                validationOk = false;
            }
        }
    }

    return validationOk;
}

bool AdminClientProvision::processSources(std::shared_ptr<Transformation> transformation,
        TypeConverter& sourceVault,
        std::map<std::string, std::string>& variables,
        const std::string &requestUri,
        const nghttp2::asio_http2::header_map &requestHeaders,
        bool &eraser,
        std::uint64_t sendSeq,
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
        replaceVariables(path, transformation->getSourcePatterns(), variables, vault_);
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
        replaceVariables(path, transformation->getSourcePatterns(), variables, vault_);
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
    case Transformation::SourceType::RequestHeaders:
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &h : requestHeaders) arr.push_back({{"name", h.first}, {"value", h.second.value}});
        sourceVault.setObject(arr, "");
        break;
    }
    case Transformation::SourceType::ResponseHeaders:
    {
        if (!receivedResponse) return false;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &h : receivedResponse->headers) arr.push_back({{"name", h.first}, {"value", h.second.value}});
        sourceVault.setObject(arr, "");
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
        replaceVariables(expressionString, transformation->getSourcePatterns(), variables, vault_);
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
        sourceVault.setStringReplacingVariables(transformation->getSourceTokenized()[rand () % transformation->getSourceTokenized().size()], transformation->getSourcePatterns(), variables, vault_);
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
        sourceVault.setStringReplacingVariables(std::string(buffer), transformation->getSourcePatterns(), variables, vault_);
        break;
    }
    case Transformation::SourceType::Sendseq:
    {
        sourceVault.setUnsigned(sendSeq);
        break;
    }
    case Transformation::SourceType::SVar:
    {
        std::string varname = transformation->getSource();
        replaceVariables(varname, transformation->getSourcePatterns(), variables, vault_);
        auto iter = variables.find(varname);
        if (iter != variables.end()) sourceVault.setString(iter->second);
        else return false;
        break;
    }
    case Transformation::SourceType::SGVar:
    {
        std::string varname = transformation->getSource();
        replaceVariables(varname, transformation->getSourcePatterns(), variables, vault_);
        nlohmann::json vaultValue{};
        if (vault_->tryGet(varname, vaultValue)) {
            std::string path = transformation->getSource2();
            if (!path.empty()) replaceVariables(path, transformation->getSourcePatterns(), variables, vault_);
            if (!sourceVault.setObject(vaultValue, path)) return false;
        }
        else return false;
        break;
    }
    case Transformation::SourceType::Value:
    {
        sourceVault.setStringReplacingVariables(transformation->getSource(), transformation->getSourcePatterns(), variables, vault_);
        break;
    }
    case Transformation::SourceType::ServerEvent:
    {
        std::string event_method = transformation->getSourceTokenized()[0];
        replaceVariables(event_method, transformation->getSourcePatterns(), variables, vault_);
        std::string event_uri = transformation->getSourceTokenized()[1];
        replaceVariables(event_uri, transformation->getSourcePatterns(), variables, vault_);
        std::string event_number = transformation->getSourceTokenized()[2];
        replaceVariables(event_number, transformation->getSourcePatterns(), variables, vault_);
        std::string event_path = transformation->getSourceTokenized()[3];
        replaceVariables(event_path, transformation->getSourcePatterns(), variables, vault_);
        std::string event_recvseq = transformation->getSourceTokenized()[4];
        replaceVariables(event_recvseq, transformation->getSourcePatterns(), variables, vault_);

        std::shared_ptr<MockEvent> mockServerRequest;
        if (!event_recvseq.empty()) {
            try {
                DataKey dkey(event_method, event_uri);
                mockServerRequest = mock_server_events_data_->getEventByRecvSeq(dkey, (std::uint64_t)std::stoull(event_recvseq));
            }
            catch (const std::exception&) { return false; }
        }
        else {
            EventKey ekey(event_method, event_uri, event_number);
            mockServerRequest = mock_server_events_data_->getEvent(ekey);
        }
        if (!mockServerRequest) return false;
        if (!sourceVault.setObject(mockServerRequest->getJson(), event_path)) {
            ert::tracing::Logger::warning(ert::tracing::Logger::asString("Cannot extract path '%s' from server event for source '%s'", event_path.c_str(), transformation->getSource().c_str()), ERT_FILE_LOCATION);
            return false;
        }
        break;
    }
    case Transformation::SourceType::ClientEvent:
    {
        std::string event_endpoint = transformation->getSourceTokenized()[0];
        replaceVariables(event_endpoint, transformation->getSourcePatterns(), variables, vault_);
        std::string event_method = transformation->getSourceTokenized()[1];
        replaceVariables(event_method, transformation->getSourcePatterns(), variables, vault_);
        std::string event_uri = transformation->getSourceTokenized()[2];
        replaceVariables(event_uri, transformation->getSourcePatterns(), variables, vault_);
        std::string event_number = transformation->getSourceTokenized()[3];
        replaceVariables(event_number, transformation->getSourcePatterns(), variables, vault_);
        std::string event_path = transformation->getSourceTokenized()[4];
        replaceVariables(event_path, transformation->getSourcePatterns(), variables, vault_);
        std::string event_sendseq = transformation->getSourceTokenized()[5];
        replaceVariables(event_sendseq, transformation->getSourcePatterns(), variables, vault_);

        DataKey dkey(event_endpoint, event_method, event_uri);
        std::shared_ptr<MockEvent> mockClientRequest;
        if (!event_sendseq.empty()) {
            try {
                mockClientRequest = mock_client_events_data_->getEventBySendSeq(dkey, (std::uint64_t)std::stoull(event_sendseq));
            }
            catch (const std::exception&) { return false; }
        }
        else {
            EventKey ekey(dkey, event_number);
            mockClientRequest = mock_client_events_data_->getEvent(ekey);
        }
        if (!mockClientRequest) return false;
        if (!sourceVault.setObject(mockClientRequest->getJson(), event_path)) {
            ert::tracing::Logger::warning(ert::tracing::Logger::asString("Cannot extract path '%s' from client event for source '%s'", event_path.c_str(), transformation->getSource().c_str()), ERT_FILE_LOCATION);
            return false;
        }
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
        replaceVariables(path, transformation->getSourcePatterns(), variables, vault_);
        std::string content;
        file_manager_->read(path, content, true);
        sourceVault.setString(std::move(content));
        break;
    }
    case Transformation::SourceType::SBinFile:
    {
        std::string path = transformation->getSource();
        replaceVariables(path, transformation->getSourcePatterns(), variables, vault_);
        std::string content;
        file_manager_->read(path, content, false);
        sourceVault.setString(std::move(content));
        break;
    }
    case Transformation::SourceType::Command:
    {
        std::string command = transformation->getSource();
        replaceVariables(command, transformation->getSourcePatterns(), variables, vault_);
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
    case Transformation::SourceType::Recvseq:
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

    if (transformation->getFilterType() != Transformation::FilterType::Sum && transformation->getFilterType() != Transformation::FilterType::Multiply && transformation->getFilterType() != Transformation::FilterType::FStrftime && transformation->getFilterType() != Transformation::FilterType::RegexKey && transformation->getFilterType() != Transformation::FilterType::Size) {
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
        replaceVariables(filter, transformation->getFilterPatterns(), variables, vault_);
        targetS = source + filter;
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::Prepend:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, vault_);
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
        replaceVariables(filter, transformation->getFilterPatterns(), variables, vault_);
        if (source != filter) return false;
        break;
    }
    case Transformation::FilterType::DifferentFrom:
    {
        std::string filter = transformation->getFilter();
        replaceVariables(filter, transformation->getFilterPatterns(), variables, vault_);
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
    case Transformation::FilterType::Split:
    {
        std::int64_t size = transformation->getFilterI();
        std::uint64_t count = transformation->getFilterU();
        const std::string &sep = transformation->getFilter();
        const std::string &filler = transformation->getFilterFiller();
        bool numeric = (transformation->getFilterNumberType() != 0);

        std::uint64_t totalLen = static_cast<std::uint64_t>(size) * count;
        std::string padded = source;

        if (padded.size() > totalLen) {
            padded = padded.substr(padded.size() - totalLen);
        }
        else if (padded.size() < totalLen && !filler.empty()) {
            std::string padding;
            while (padding.size() + padded.size() < totalLen) {
                padding += filler;
            }
            if (padding.size() + padded.size() > totalLen) {
                padding = padding.substr(padding.size() + padded.size() - totalLen);
            }
            padded = padding + padded;
        }

        targetS.clear();
        for (std::uint64_t i = 0; i < count; i++) {
            if (i > 0) targetS += sep;
            std::string group = padded.substr(i * size, size);
            if (numeric) {
                try {
                    targetS += std::to_string(std::stoull(group));
                }
                catch (...) {
                    targetS += group;
                }
            }
            else {
                targetS += group;
            }
        }
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::BaseConvert:
    {
        int baseIn = static_cast<int>(transformation->getFilterI());
        int baseOut = static_cast<int>(transformation->getFilterU());
        bool capital = (transformation->getFilterNumberType() != 0);

        try {
            unsigned long long val = std::stoull(source, nullptr, baseIn);
            if (baseOut == 10) {
                targetS = std::to_string(val);
            }
            else {
                targetS.clear();
                if (val == 0) { targetS = "0"; }
                else {
                    while (val > 0) {
                        int d = val % baseOut;
                        targetS += (d < 10) ? char('0' + d) : char((capital ? 'A' : 'a') + d - 10);
                        val /= baseOut;
                    }
                    std::reverse(targetS.begin(), targetS.end());
                }
            }
        }
        catch (...) {
            targetS = source;
        }
        sourceVault.setString(targetS);
        break;
    }
    case Transformation::FilterType::FStrptime:
    {
        struct tm tm{};
        if (strptime(source.c_str(), transformation->getFilter().c_str(), &tm) == nullptr) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Strptime filter failed to parse '%s' with format '%s'", source.c_str(), transformation->getFilter().c_str()), ERT_FILE_LOCATION);
            return false;
        }
        std::int64_t epoch = static_cast<std::int64_t>(timegm(&tm));
        switch (transformation->getFilterNumberType()) {
        case 1: epoch *= 1000; break;
        case 2: epoch *= 1000000; break;
        case 3: epoch *= 1000000000; break;
        }
        sourceVault.setInteger(epoch);
        break;
    }
    case Transformation::FilterType::FStrftime:
    {
        std::int64_t epoch = sourceVault.getInteger(success);
        if (!success) {
            ert::tracing::Logger::error("Strftime filter requires a numeric source (epoch)", ERT_FILE_LOCATION);
            return false;
        }
        switch (transformation->getFilterNumberType()) {
        case 1: epoch /= 1000; break;
        case 2: epoch /= 1000000; break;
        case 3: epoch /= 1000000000; break;
        }
        time_t t = static_cast<time_t>(epoch);
        struct tm tm{};
        gmtime_r(&t, &tm);
        char buf[256];
        if (std::strftime(buf, sizeof(buf), transformation->getFilter().c_str(), &tm) == 0) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Strftime filter failed to format epoch %ld with format '%s'", (long)t, transformation->getFilter().c_str()), ERT_FILE_LOCATION);
            return false;
        }
        sourceVault.setString(std::string(buf));
        break;
    }
    case Transformation::FilterType::RegexKey:
    {
        bool objSuccess = false;
        nlohmann::json obj = sourceVault.getObject(objSuccess); // copy: setObject below clears sourceVault
        if (!objSuccess || !obj.is_object()) {
            ert::tracing::Logger::error("RegexKey filter requires a JSON object source", ERT_FILE_LOCATION);
            return false;
        }
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            source = it.key(); // copy key to source (lives in transform() scope for matches lifetime)
            if (std::regex_match(source, matches, transformation->getFilterRegex())) {
                if (!sourceVault.setObject(it.value(), ""))
                    return false;
                return true;
            }
        }
        return false;
    }
    case Transformation::FilterType::Size:
    {
        bool objSuccess = false;
        nlohmann::json obj = sourceVault.getObject(objSuccess);
        if (objSuccess && (obj.is_object() || obj.is_array())) {
            targetS = std::to_string(obj.size());
        }
        else {
            source = sourceVault.getString(objSuccess);
            targetS = objSuccess ? std::to_string(source.size()) : "0";
        }
        sourceVault.setString(targetS);
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
    replaceVariables(target, transformation->getTargetPatterns(), variables, vault_);

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
            else if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexKey) {
                // Store the value in the target variable
                targetS = sourceVault.getString(success);
                if (!success) return false;
                variables[target] = targetS;
                // Store matched key (.0) and capture groups (.1, .2, ...) in variables
                for(size_t i=0; i < matches.size(); i++) {
                    variables[target + "." + std::to_string(i)] = matches.str(i);
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
            std::string gvarPath = transformation->getTarget2();
            if (!gvarPath.empty()) replaceVariables(gvarPath, transformation->getTarget2Patterns(), variables, vault_);

            if (eraser) {
                bool exists;
                vault_->remove(target, exists);
            }
            else if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                if (matches.size() >=1) {
                    nlohmann::json captureObj = nlohmann::json::object();
                    captureObj["0"] = matches.str(0);
                    for(size_t i=1; i < matches.size(); i++) {
                        captureObj[std::to_string(i)] = matches.str(i);
                    }
                    if (gvarPath.empty()) {
                        vault_->load(target, std::move(captureObj));
                    } else {
                        vault_->loadAtPath(target, gvarPath, captureObj);
                    }
                }
            }
            else if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexKey) {
                // Store value in vault (as today)
                bool objSuccess = false;
                const nlohmann::json &obj = sourceVault.getObject(objSuccess);
                if (objSuccess) {
                    if (gvarPath.empty()) {
                        vault_->load(target, obj);
                    } else {
                        vault_->loadAtPath(target, gvarPath, obj);
                    }
                } else {
                    targetS = sourceVault.getString(success);
                    if (!success) return false;
                    nlohmann::json val(targetS);
                    if (gvarPath.empty()) {
                        vault_->load(target, std::move(val));
                    } else {
                        vault_->loadAtPath(target, gvarPath, val);
                    }
                }
                // Store matched key (.0) and capture groups (.1, .2, ...) in variables
                for(size_t i=0; i < matches.size(); i++) {
                    variables[target + "." + std::to_string(i)] = matches.str(i);
                }
            }
            else {
                bool objSuccess = false;
                const nlohmann::json &obj = sourceVault.getObject(objSuccess);
                if (objSuccess) {
                    if (gvarPath.empty()) {
                        vault_->load(target, obj);
                    } else {
                        vault_->loadAtPath(target, gvarPath, obj);
                    }
                } else {
                    targetS = sourceVault.getString(success);
                    if (!success) return false;
                    nlohmann::json val(targetS);
                    if (gvarPath.empty()) {
                        vault_->load(target, std::move(val));
                    } else {
                        vault_->loadAtPath(target, gvarPath, val);
                    }
                }
            }
            break;
        }
        case Transformation::TargetType::TGVarJson_Object:
        {
            std::string gvarPath = transformation->getTarget2();
            if (!gvarPath.empty()) replaceVariables(gvarPath, transformation->getTarget2Patterns(), variables, vault_);

            bool objSuccess = false;
            const nlohmann::json &obj = sourceVault.getObject(objSuccess);
            if (!objSuccess) return false;

            if (gvarPath.empty()) {
                vault_->load(target, obj);
            } else {
                vault_->loadAtPath(target, gvarPath, obj);
            }
            break;
        }
        case Transformation::TargetType::TGVarJson_JsonString:
        {
            std::string gvarPath = transformation->getTarget2();
            if (!gvarPath.empty()) replaceVariables(gvarPath, transformation->getTarget2Patterns(), variables, vault_);

            targetS = sourceVault.getString(success);
            if (!success) return false;
            if (!h2agent::model::parseJsonContent(targetS, obj))
                return false;

            if (gvarPath.empty()) {
                vault_->load(target, obj);
            } else {
                vault_->loadAtPath(target, gvarPath, obj);
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
            // transformation->getTargetTokenized() is a vector:
            //
            // clientEndpointId: index 0
            // requestMethod:    index 1
            // requestUri:       index 2
            // eventNumber:      index 3
            // sendseq:          index 4
            std::string event_endpoint = transformation->getTargetTokenized()[0];
            replaceVariables(event_endpoint, transformation->getTargetPatterns(), variables, vault_);
            std::string event_method = transformation->getTargetTokenized()[1];
            replaceVariables(event_method, transformation->getTargetPatterns(), variables, vault_);
            std::string event_uri = transformation->getTargetTokenized()[2];
            replaceVariables(event_uri, transformation->getTargetPatterns(), variables, vault_);
            std::string event_number = transformation->getTargetTokenized()[3];
            replaceVariables(event_number, transformation->getTargetPatterns(), variables, vault_);
            std::string event_sendseq = transformation->getTargetTokenized()[4];
            replaceVariables(event_sendseq, transformation->getTargetPatterns(), variables, vault_);

            bool clientDataDeleted = false;
            DataKey dkey(event_endpoint, event_method, event_uri);

            if (!event_sendseq.empty()) {
                // Stable addressing by send sequence:
                clientDataDeleted = mock_client_events_data_->removeEventBySendSeq(dkey, (std::uint64_t)std::stoull(event_sendseq));
            }
            else {
                // Positional addressing by event number:
                EventKey ekey(dkey, event_number);
                mock_client_events_data_->clear(clientDataDeleted, ekey);
            }
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
        case Transformation::TargetType::ClientProvision_t:
            break;
        }
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
    }

    return true;
}

bool AdminClientProvision::updateTriggering(const std::string &sequenceBegin, const std::string &sequenceEnd, const std::string &cps, const std::string &repeat) {

    // Range reads:
    std::int64_t i_sequenceBegin = seq_begin_;
    std::int64_t i_sequenceEnd = seq_end_;

    if (!sequenceBegin.empty()) {
        try {
            i_sequenceBegin = std::stoll(sequenceBegin);
        } catch(std::exception &e) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'sequenceBegin' value: %s", sequenceBegin.c_str()), ERT_FILE_LOCATION));
            return false;
        }
    }

    if (!sequenceEnd.empty()) {
        try {
            i_sequenceEnd = std::stoll(sequenceEnd);
        } catch(std::exception &e) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'sequenceEnd' value: %s", sequenceEnd.c_str()), ERT_FILE_LOCATION));
            return false;
        }
    }

    // Range check:
    if (i_sequenceBegin > i_sequenceEnd) {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Incompatible range for 'sequenceBegin' and 'sequenceEnd': %" PRId64 " > %" PRId64, i_sequenceBegin, i_sequenceEnd), ERT_FILE_LOCATION));
        return false;
    }

    // Range assignment:
    bool rangeProvided = !sequenceBegin.empty() || !sequenceEnd.empty();
    if (i_sequenceBegin != seq_begin_ || i_sequenceEnd != seq_end_) {
        seq_begin_ = i_sequenceBegin;
        seq_end_ = i_sequenceEnd;
    }
    if (rangeProvided) {
        seq_ = seq_begin_ - 1; // reset iterator: new range means fresh start
    }


    // Rate:
    if (!cps.empty()) {
        bool negative = false;
        std::uint64_t aux;
        if (!h2agent::model::string2uint64andSign(cps, aux, negative) || negative) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid 'cps' value: %s (must be >= 0)", cps.c_str()), ERT_FILE_LOCATION));
            return false;
        }
        cps_ = aux;
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
    if (seq_ < seq_begin_ - 1 || seq_ > seq_end_) {
        seq_ = seq_begin_ - 1; // outside range: reset (exhausted or never started)
    }
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

void AdminClientProvision::scheduleTick(bool first) {
    if (!timer_ || cps_ == 0) {
        stopTicking();
        return;
    }

    auto period = std::chrono::microseconds(1000000 / cps_);
    if (first)
        timer_->expires_after(period);               // first tick: relative to now
    else
        timer_->expires_at(timer_->expiry() + period); // subsequent: anchored, no drift

    timer_->async_wait([this](const boost::system::error_code &ec) {
        if (ec) return; // cancelled

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

        scheduleTick(false); // schedule next BEFORE callback to avoid drift
        if (tick_callback_) tick_callback_();
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

    it = j.find("expectedResponseStatusCode");
    if (it != j.end() && it->is_number()) {
        expected_response_status_code_ = *it;
    }

    auto transform_it = j.find("transform");
    if (transform_it != j.end()) {
        LOGDEBUG(ert::tracing::Logger::debug("Load transformations ('transform' node)", ERT_FILE_LOCATION));
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(transformations_, it);
        }
        if (!transformations_.empty() && transformations_.back()->getTargetType() == Transformation::TargetType::Break) {
            LOGWARNING(ert::tracing::Logger::warning("Break as last 'transform' item is illogical (no further items to interrupt)", ERT_FILE_LOCATION));
        }
    }

    transform_it = j.find("onResponseTransform");
    if (transform_it != j.end()) {
        LOGDEBUG(ert::tracing::Logger::debug("Load transformations ('onResponseTransform' node)", ERT_FILE_LOCATION));
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(on_response_transformations_, it);
        }
        if (!on_response_transformations_.empty() && on_response_transformations_.back()->getTargetType() == Transformation::TargetType::Break) {
            LOGWARNING(ert::tracing::Logger::warning("Break as last 'onResponseTransform' item is illogical (no further items to interrupt)", ERT_FILE_LOCATION));
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
