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

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

#include <AdminClientProvision.hpp>
#include <Engine.hpp>
#include <EngineGrammarV2.hpp>
#include <EngineHost.hpp>
#include <MockClientData.hpp>
#include <MockServerData.hpp>
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

void AdminClientProvision::saveDynamics() const {
    std::lock_guard<std::mutex> guard(json_mutex_);

    std::int64_t seq = seq_.load();
    std::int64_t begin = seq_begin_.load();
    std::int64_t end = seq_end_.load();
    std::uint64_t cps = cps_.load();

    json_["dynamics"]["sequence"] = seq; // raw internal iterator (kept for backward compatibility)
    json_["dynamics"]["sequenceBegin"] = begin;
    json_["dynamics"]["sequenceEnd"] = end;
    json_["dynamics"]["cps"] = cps;
    json_["dynamics"]["repeat"] = repeat_.load();

    // Human-friendly observability (additive; does not alter the internal iterator).
    //
    // The internal iterator uses the classic "use-then-increment" idiom starting at
    // 'begin - 1', so 'seq_ == begin - 1' is the sentinel for "armed but no element of
    // the range emitted yet" (which shows up as a confusing negative value, e.g. -1 when
    // begin is 0). The following fields expose that state without the user having to
    // decode the raw sentinel:
    //
    //   state:        armed     -> prepared, waiting for the first tick (seq == begin - 1)
    //                 running   -> ticker active (timer scheduled with cps > 0)
    //                 paused    -> positioned within the range but no ticker (cps = 0)
    //                 exhausted -> range consumed without repeat (seq > end)
    //   nextSequence: the sequence value the NEXT request will carry (begin when armed,
    //                 seq + 1 while progressing); null when exhausted (nothing pending).
    const char *state;
    nlohmann::json nextSequence;
    if (seq < begin) { // sentinel 'begin - 1': armed, not yet started
        state = "armed";
        nextSequence = begin;
    }
    else if (seq > end) { // consumed the whole range (no repeat)
        state = "exhausted";
        nextSequence = nullptr;
    }
    else {
        state = (isTicking() && cps > 0) ? "running" : "paused";
        nextSequence = (seq + 1 > end) ? (repeat_.load() ? begin : end) : (seq + 1);
    }
    json_["dynamics"]["state"] = state;
    json_["dynamics"]["nextSequence"] = nextSequence;
}

void AdminClientProvision::transform( std::string &requestMethod,
                                      std::string &requestUri,
                                      std::string &requestBody,
                                      nghttp2::asio_http2::header_map &requestHeaders,
                                      std::string &outState,
                                      unsigned int &requestDelayMs,
                                      unsigned int &requestTimeoutMs,
                                      std::string &error,
                                      Variables &variables,
                                      std::int64_t seq,
                                      nlohmann::json *executionTrace
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
    const bool usesRequestBodyAsTransformationJsonTarget = uses_json_body_target_;

    nlohmann::json requestBodyJson;
    if (usesRequestBodyAsTransformationJsonTarget) {
        requestBodyJson = getRequestBody();   // clone provision response body to manipulate this copy and finally we will dump() it over 'responseBody':
        // if(usesRequestBodyAsTransformationJsonTarget) requestBody = requesteBodyJson.dump(); <--- place this after transformations (*)
    }
    else {
        requestBody = getRequestBodyAsString(); // this could be overwritten by targets RequestBodyString or RequestBodyHexString
    }

    // Scoped variables: update reserved read-only variable
    variables["sequence"] = std::to_string(seq >= 0 ? seq : seq_.load());

    // Transformation pipeline evaluated by the isolated engine (see AdminServerProvision).
    // By reference (see AdminServerProvision::transform): nothing is copied.
    // request.uri reads go through meta (live value); requestHeaders only change
    // after evaluate() (header writes are applied at the end).
    ClientInputDocument input(requestUri, getRequestBodyAsString(), requestHeaders);
    V1BodyDocument outputView(usesRequestBodyAsTransformationJsonTarget ? requestBodyJson : getRequestBody());
    h2agent::engine::EvaluationContext ctx;
    ctx.input = &input;
    ctx.outputView = &outputView;
    ctx.output = usesRequestBodyAsTransformationJsonTarget ? &requestBodyJson : nullptr;
    ctx.outBody = &requestBody;
    // Live request metadata (request.uri/method/delayMs/timeoutMs sources and
    // targets) written in place on the output parameters:
    std::vector<std::pair<std::string, std::string>> headerWrites;
    input.setPendingHeaderWrites(&headerWrites);  // v1: header writes are visible to later header reads
    ctx.meta.uri = &requestUri;
    ctx.meta.method = &requestMethod;
    ctx.meta.delayMs = &requestDelayMs;
    ctx.meta.timeoutMs = &requestTimeoutMs;
    ctx.meta.headers = &headerWrites;
    ctx.sequence = 0;  // v1: sendseq is not available before sending
    ctx.inState = &getInState();
    ctx.outState = &outState;

    EngineStore store(variables, vault_);
    EngineEventSource events(mock_server_events_data_, mock_client_events_data_);
    EngineFileAccess files(file_manager_, configuration_);
    EngineSocketWriter sockets(socket_manager_);
    EngineClock clock;
    EngineShell shell;
    EngineSchemaValidator schemas(admin_data_);
    h2agent::engine::Ports ports;
    ports.store = &store;
    ports.events = &events;
    ports.files = &files;
    ports.sockets = &sockets;
    ports.clock = &clock;
    ports.shell = &shell;
    ports.schema = &schemas;  // no trigger port: clientProvision is not applicable in client provisions

    if (engine_pipeline_) {
        h2agent::engine::TransformResult result = h2agent::engine::Engine{}.evaluate(engine_pipeline_, ctx, ports, executionTrace != nullptr);
        if (executionTrace) appendExecutionTrace(*executionTrace, result, "transform");
    }

    // Everything was written in place; only header writes are applied (v1 emplace).
    for (auto &h : headerWrites)
        requestHeaders.emplace(std::move(h.first), nghttp2::asio_http2::header_value{std::move(h.second)});

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
        Variables &variables,
        std::int64_t seq,
        nlohmann::json *executionTrace
                                            )
{
    bool validationOk = true;

    if (has_response_transform_) {

        // Scoped variables: update reserved read-only variable
        variables["sequence"] = std::to_string(seq >= 0 ? seq : seq_.load());

        // Transformation pipeline evaluated by the isolated engine (response context).
        ClientInputDocument input(requestUri, getRequestBodyAsString(), requestHeaders, &receivedResponse);
        h2agent::engine::EvaluationContext ctx;
        ctx.input = &input;  // no output document: the response phase only writes store/outState/effects
        ctx.sequence = static_cast<std::int64_t>(sendSeq);
        ctx.inState = &getInState();
        ctx.outState = &outState;

        EngineStore store(variables, vault_);
        EngineEventSource events(mock_server_events_data_, mock_client_events_data_);
        EngineFileAccess files(file_manager_, configuration_);
        EngineSocketWriter sockets(socket_manager_);
        EngineClock clock;
        EngineShell shell;
        EngineSchemaValidator schemas(admin_data_);
        h2agent::engine::Ports ports;
        ports.store = &store;
        ports.events = &events;
        ports.files = &files;
        ports.sockets = &sockets;
        ports.clock = &clock;
        ports.shell = &shell;
        ports.schema = &schemas;  // no trigger port: clientProvision is not applicable in client provisions

        if (engine_response_pipeline_) {
            h2agent::engine::TransformResult result = h2agent::engine::Engine{}.evaluate(engine_response_pipeline_, ctx, ports, executionTrace != nullptr);
            if (executionTrace) appendExecutionTrace(*executionTrace, result, "onResponseTransform");
        }

    } // has_response_transform_

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
    if (i_sequenceBegin != seq_begin_.load() || i_sequenceEnd != seq_end_.load()) {
        seq_begin_ = i_sequenceBegin;
        seq_end_ = i_sequenceEnd;
    }
    if (rangeProvided) {
        seq_ = seq_begin_.load() - 1; // reset iterator: new range means fresh start
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

void AdminClientProvision::startTicking(boost::asio::io_context *ioContext, std::function<bool()> tickCallback) {
    stopTicking();
    io_context_ = ioContext;
    tick_callback_ = std::move(tickCallback);
    if (seq_.load() < seq_begin_.load() - 1 || seq_.load() >= seq_end_.load()) {
        seq_ = seq_begin_.load() - 1; // outside range: reset (exhausted or never started)
    }
    saveDynamics();
    last_dynamics_save_ = std::chrono::steady_clock::now();
    timer_ = new boost::asio::steady_timer(*io_context_);
    scheduleTick();
}

void AdminClientProvision::stopTicking() {
    if (timer_ && io_context_) {
        auto *t = timer_;
        timer_ = nullptr;
        tick_callback_ = nullptr;
        boost::asio::post(*io_context_, [t]() {
            t->cancel();
            delete t;
        });
    }
    else if (timer_) {
        timer_->cancel();
        delete timer_;
        timer_ = nullptr;
        tick_callback_ = nullptr;
    }
}

void AdminClientProvision::scheduleTick(bool first) {
    auto currentCps = cps_.load();
    if (!timer_ || currentCps == 0) {
        stopTicking();
        return;
    }

    auto period = std::chrono::microseconds(1000000 / currentCps);
    if (first)
        timer_->expires_after(period);               // first tick: relative to now
    else
        timer_->expires_at(timer_->expiry() + period); // subsequent: anchored, no drift

    timer_->async_wait([this](const boost::system::error_code &ec) {
        if (ec) return; // cancelled

        scheduleTick(false); // schedule next BEFORE callback to avoid drift

        // seq_ is the last dispatched sequence (sequenceBegin - 1 before the first tick):
        // each tick dispatches seq_ + 1, so the range sent is exactly [sequenceBegin, sequenceEnd].
        // If congested, don't advance seq (retry next tick).
        if (tick_callback_ && !tick_callback_()) {
            return; // congested: seq not advanced, will retry
        }

        seq_++;

        if (seq_.load() >= seq_end_.load()) { // range exhausted
            if (repeat_) {
                seq_ = seq_begin_.load() - 1; // next tick dispatches sequenceBegin again
            }
            else {
                seq_ = seq_end_.load() + 1; // dynamics: 'sequence > sequenceEnd' means done
                saveDynamics(); // final update before stopping
                stopTicking();
                return;
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_dynamics_save_ >= std::chrono::milliseconds(500)) {
            saveDynamics();
            last_dynamics_save_ = now;
        }
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
        if (out_state_ == DEFAULT_ADMIN_PROVISION_STATE) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid outState reserved value: %s", DEFAULT_ADMIN_PROVISION_STATE), ERT_FILE_LOCATION);
            return false;
        }
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

    // Transformation pipelines (grammar v2 lowered to the engine representation, parsed once).
    // requestBody source semantics depend on whether a json body target exists (v1 parity):
    {
        using Mode = EngineGrammarV2::Mode;
        const nlohmann::json rules = j.contains("transform") ? j["transform"] : nlohmann::json::array();
        const nlohmann::json responseRules = j.contains("onResponseTransform") ? j["onResponseTransform"] : nlohmann::json::array();
        const EngineGrammarV2::Summary summary = EngineGrammarV2::summarize(rules, Mode::ClientRequest);
        const EngineGrammarV2::Summary responseSummary = EngineGrammarV2::summarize(responseRules, Mode::ClientResponse);
        uses_json_body_target_ = summary.usesJsonBodyTarget;
        needs_storage_ = summary.needsStorage || responseSummary.needsStorage;
        has_response_transform_ = !responseRules.empty();
        if (summary.endsWithStop) {
            LOGWARNING(ert::tracing::Logger::warning("Stop as last 'transform' item is illogical (no further items to interrupt)", ERT_FILE_LOCATION));
        }
        if (responseSummary.endsWithStop) {
            LOGWARNING(ert::tracing::Logger::warning("Stop as last 'onResponseTransform' item is illogical (no further items to interrupt)", ERT_FILE_LOCATION));
        }

        std::string error;
        const nlohmann::json lowered = EngineGrammarV2::lowerPipeline(rules, Mode::ClientRequest, uses_json_body_target_, error);
        const nlohmann::json loweredResponse = lowered.is_null() ? lowered :
                                               EngineGrammarV2::lowerPipeline(responseRules, Mode::ClientResponse, false, error);
        if (lowered.is_null() || loweredResponse.is_null()) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Transformation not supported: %s", error.c_str()), ERT_FILE_LOCATION);
            return false;
        }
        engine_pipeline_ = h2agent::engine::Engine{}.parse(lowered, error);
        if (engine_pipeline_) engine_response_pipeline_ = h2agent::engine::Engine{}.parse(loweredResponse, error);
        if (!engine_pipeline_ || !engine_response_pipeline_) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid transformation pipeline: %s", error.c_str()),
                                        ERT_FILE_LOCATION);
            return false;
        }
    }

    // Store key:
    h2agent::model::calculateStringKey(key_, in_state_, client_provision_id_);

    // Add dynamic load information to json__:
    saveDynamics();

    return true;
}



}
}
