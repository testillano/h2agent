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
    seq_(0), seq_begin_(0), seq_end_(0), rps_(0), repeat_(false) {;}


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
                                      std::shared_ptr<h2agent::model::AdminSchema> requestSchema,
                                      unsigned int &requestDelayMs,
                                      unsigned int &requestTimeoutMs,
                                      std::string &error
                                    ) const {

    // Default values without transformations:
    requestMethod = getRequestMethod();
    requestUri = getRequestUri();
    requestHeaders = getRequestHeaders();
    outState = getOutState();
    requestDelayMs = getRequestDelayMilliseconds();
    requestTimeoutMs = getRequestTimeoutMilliseconds();

    // Find out if request body will need to be cloned (this is true if any transformation uses it as target):
    bool usesRequestBodyAsTransformationJsonTarget = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
        if ((*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_String ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_Integer ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_Unsigned ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_Float ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_Boolean ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_Object ||
                (*it)->getTargetType() == Transformation::TargetType::RequestBodyJson_JsonString) {
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

    // Type converter:
    TypeConverter sourceVault{};

    // Apply transformations sequentially
    bool breakCondition = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {

        if (breakCondition) break;

        auto transformation = (*it);
        bool eraser = false;

        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Processing transformation item: %s", transformation->asString().c_str()), ERT_FILE_LOCATION));
        /*
                // SOURCES: RequestUri, RequestUriPath, RequestUriParam, RequestBody, ResponseBody, RequestHeader, Eraser, Math, Random, Timestamp, Strftime, Recvseq, SVar, SGvar, Value, ServerEvent, InState
                if (!processSources(transformation, sourceVault, variables, requestUri, requestUriPath, requestQueryParametersMap, requestBodyDataPart, requestHeaders, eraser, generalUniqueServerSequence)) {
                    LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on source", ERT_FILE_LOCATION));
                    continue;
                }

                std::smatch matches; // BE CAREFUL!: https://stackoverflow.com/a/51709911/2576671
                // So, we can't use 'matches' as container because source may change: BUT, using that source exclusively, it will work (*)
                std::string source; // Now, this never will be out of scope, and 'matches' will be valid.

                // FILTERS: RegexCapture, RegexReplace, Append, Prepend, Sum, Multiply, ConditionVar, EqualTo, DifferentFrom, JsonConstraint
                bool hasFilter = transformation->hasFilter();
                if (hasFilter) {
                    if (eraser || !processFilters(transformation, sourceVault, variables, matches, source)) {
                        LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on filter", ERT_FILE_LOCATION));
                        LOGWARNING(ert::tracing::Logger::warning("Filter is not allowed when using 'eraser' source type. Transformation will be ignored.", ERT_FILE_LOCATION));
                        continue;
                    }
                }

                // TARGETS: ResponseBodyString, ResponseBodyHexString, ResponseBodyJson_String, ResponseBodyJson_Integer, ResponseBodyJson_Unsigned, ResponseBodyJson_Float, ResponseBodyJson_Boolean, ResponseBodyJson_Object, ResponseBodyJson_JsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, TGVar, OutState, TTxtFile, TBinFile, ServerEventToPurge, Break
                if (!processTargets(transformation, sourceVault, variables, matches, eraser, hasFilter, responseStatusCode, responseBodyJson, responseBody, responseHeaders, responseDelayMs, outState, outStateMethod, outStateUri, breakCondition)) {
                    LOGDEBUG(ert::tracing::Logger::debug("Transformation item skipped on target", ERT_FILE_LOCATION));
                    continue;
                }
        */
    }

    // Request schema validation
    if (requestSchema) {
        if (!requestSchema->validate(usesRequestBodyAsTransformationJsonTarget ? requestBodyJson:getRequestBody())) {
            error = "Invalid request built against request schema provided";
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
