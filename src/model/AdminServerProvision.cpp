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

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

#include <AdminServerProvision.hpp>
#include <Engine.hpp>
#include <EngineGrammarV1.hpp>
#include <EngineHost.hpp>
#include <MockServerData.hpp>
#include <MockClientData.hpp>
#include <Configuration.hpp>
#include <Vault.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <AdminData.hpp>

#include <functions.hpp>



namespace h2agent
{
namespace model
{

AdminServerProvision::AdminServerProvision() : in_state_(DEFAULT_ADMIN_PROVISION_STATE),
    out_state_(DEFAULT_ADMIN_PROVISION_STATE),
    response_delay_ms_(0), mock_server_events_data_(nullptr), mock_client_events_data_(nullptr) {;}


std::shared_ptr<h2agent::model::AdminSchema> AdminServerProvision::getRequestSchema() {

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

std::shared_ptr<h2agent::model::AdminSchema> AdminServerProvision::getResponseSchema() {

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
                                      std::vector<std::pair<std::string, std::string>> &clientProvisionTriggers,
                                      std::map<std::string, std::string> &variables
                                    )
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
    if (getRequestSchema()) {
        mustDecodeRequestBody = true;
    }
    else {
        for (const auto &t : transformations_) {
            if (t->getSourceType() == Transformation::SourceType::RequestBody) {
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
    if (getRequestSchema()) {
        std::string error{};
        if (!getRequestSchema()->validate(requestBodyDataPart.getJson(), error)) {
            responseStatusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
            return; // INTERRUPT TRANSFORMATIONS
        }
    }

    // Find out if response body will need to be cloned (this is true if any transformation uses it as target):
    bool usesResponseBodyAsTransformationJsonTarget = false;
    for (const auto &t : transformations_) {
        if (t->getTargetType() == Transformation::TargetType::ResponseBodyJson_String ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_Integer ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_Unsigned ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_Float ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_Boolean ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_Object ||
                t->getTargetType() == Transformation::TargetType::ResponseBodyJson_JsonString) {
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

    // Transformation pipeline: evaluated by the isolated engine (src/engine)
    // through ports over h2agent resources (EngineHost). The v1 grammar was
    // translated to the neutral engine grammar at load time (EngineGrammarV1).
    // The context references the request/response objects (nothing is copied):
    // the input is resolved lazily over the native request structures, response
    // body reads see the json being built (or the provision body when no json
    // target exists, as v1), and writes go straight to responseBodyJson/responseBody.
    ServerInputDocument input(requestUri, requestUriPath, requestQueryParametersMap, requestBodyDataPart,
                              requestHeaders);
    V1BodyDocument outputView(usesResponseBodyAsTransformationJsonTarget ? responseBodyJson : getResponseBody());
    h2agent::engine::EvaluationContext ctx;
    ctx.input = &input;
    ctx.outputView = &outputView;
    ctx.output = usesResponseBodyAsTransformationJsonTarget ? &responseBodyJson : nullptr;
    ctx.outBody = &responseBody;
    ctx.sequence = static_cast<std::int64_t>(generalUniqueServerSequence);
    ctx.inState = &getInState();
    ctx.outState = &outState;
    ctx.outStateMethod = &outStateMethod;
    ctx.outStateUri = &outStateUri;
    // Typed metadata written in place (no json, no mapping back):
    std::vector<std::pair<std::string, std::string>> headerWrites;
    ctx.meta.statusCode = &responseStatusCode;
    ctx.meta.delayMs = &responseDelayMs;
    ctx.meta.headers = &headerWrites;

    EngineStore store(variables, vault_);
    EngineEventSource events(mock_server_events_data_, mock_client_events_data_);
    EngineFileAccess files(file_manager_, configuration_);
    EngineSocketWriter sockets(socket_manager_);
    EngineClock clock;
    EngineShell shell;
    EngineSchemaValidator schemas(admin_data_);
    EngineTrigger trigger(clientProvisionTriggers);
    h2agent::engine::Ports ports;
    ports.store = &store;
    ports.events = &events;
    ports.files = &files;
    ports.sockets = &sockets;
    ports.clock = &clock;
    ports.shell = &shell;
    ports.schema = &schemas;
    ports.trigger = &trigger;

    if (engine_pipeline_) h2agent::engine::Engine{}.evaluate(engine_pipeline_, ctx, ports);

    // Everything was written in place; only header writes are applied (v1 emplace).
    for (auto &h : headerWrites)
        responseHeaders.emplace(std::move(h.first), nghttp2::asio_http2::header_value{std::move(h.second)});

    // (*) Regenerate final responseBody after transformations:
    if(usesResponseBodyAsTransformationJsonTarget && !responseBodyJson.empty()) {
        try {
            responseBody = responseBodyJson.dump(); // this may arise type error, for example in case of trying to set json field value with binary data:
            // When having a provision transformation from 'request.body' to 'response.body.json.string./whatever':
            // echo -en '\x80\x01' | curl --http2-prior-knowledge -i -H 'content-type:application/octet-stream' -X GET "<traffic url>/uri" --data-binary @-
            //
            // This is not valid and must be protected. The user should use another kind of target to store binary.
        }
        catch (const std::exception& e)
        {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
    }

    // Response schema validation (not supported for response body created by non-json targets, to simplify the fact to parse need on ResponseBodyString/ResponseBodyHexString):
    if (getResponseSchema()) {
        std::string error{};
        if (!getResponseSchema()->validate(usesResponseBodyAsTransformationJsonTarget ? responseBodyJson:getResponseBody(), error)) {
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
        if (in_state_.empty()) in_state_ = DEFAULT_ADMIN_PROVISION_STATE;
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

    it = j.find("responseHeaders");
    if (it != j.end() && it->is_object()) {
        for (auto& [key, val] : it->items())
            response_headers_.emplace(key, nghttp2::asio_http2::header_value{val});
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
        if (!transformations_.empty() && transformations_.back()->getTargetType() == Transformation::TargetType::Break) {
            LOGWARNING(ert::tracing::Logger::warning("Break as last 'transform' item is illogical (no further items to interrupt)", ERT_FILE_LOCATION));
        }
    }

    // Engine pipeline (v1 grammar -> neutral engine grammar, parsed once):
    {
        std::string error;
        engine_pipeline_ = h2agent::engine::Engine{}.parse(
                               EngineGrammarV1::translatePipeline(transform_it != j.end() ? *transform_it : nlohmann::json::array(),
                                       EngineGrammarV1::Mode::Server),
                               error);
        if (!engine_pipeline_) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid transformation pipeline: %s", error.c_str()),
                                        ERT_FILE_LOCATION);
            return false;
        }
    }

    // Store key:
    h2agent::model::calculateStringKey(key_, in_state_, request_method_, request_uri_);

    if (regexMatchingConfigured) {
        // Precompile regex with key, only for 'RegexMatching' algorithm:
        try {
            LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Assigning regex: %s", key_.c_str()), ERT_FILE_LOCATION));
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

void AdminServerProvision::loadTransformation(const nlohmann::json &j) {

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Loading transformation item: %s", j.dump().c_str()); // avoid newlines in traces (dump(n) pretty print)
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Transformation object to fill:
    auto transformation = std::make_shared<Transformation>();

    if (transformation->load(j)) {
        transformations_.push_back(transformation);
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Loaded transformation item: %s", transformation->asString().c_str()), ERT_FILE_LOCATION));
    }
    else {
        ert::tracing::Logger::error("Discarded transform item due to incoherent data", ERT_FILE_LOCATION);
    }
}


}
}

