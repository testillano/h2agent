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

#include <boost/optional.hpp>
#include <sstream>
#include <map>
#include <errno.h>


#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/URLFunctions.hpp>

#include <MyTrafficHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockServerEventsData.hpp>
#include <GlobalVariable.hpp>
#include <functions.hpp>

namespace h2agent
{
namespace http2
{


MyTrafficHttp2Server::MyTrafficHttp2Server(size_t workerThreads, boost::asio::io_service *timersIoService):
    ert::http2comm::Http2Server("MockHttp2Server", workerThreads, timersIoService),
    admin_data_(nullptr) {

    mock_server_events_data_ = new model::MockServerEventsData();
    global_variable_ = new model::GlobalVariable();

    server_data_ = true;
    server_data_key_history_ = true;
    purge_execution_ = true;
}

MyTrafficHttp2Server::~MyTrafficHttp2Server() {
    delete (mock_server_events_data_);
    delete (global_variable_);
}

void MyTrafficHttp2Server::enableMyMetrics(ert::metrics::Metrics *metrics) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::counter_family_ref_t cf = metrics->addCounterFamily(std::string("h2agent_observed_requests_total"), "Http2 total requests observed in h2agent");

        observed_requests_processed_counter_ = &(cf.Add({{"result", "processed"}}));
        observed_requests_unprovisioned_counter_ = &(cf.Add({{"result", "unprovisioned"}}));

        ert::metrics::counter_family_ref_t cf2 = metrics->addCounterFamily(std::string("h2agent_purged_contexts_total"), "Total contexts purged in h2agent");

        purged_contexts_successful_counter_ = &(cf2.Add({{"result", "successful"}}));
        purged_contexts_failed_counter_ = &(cf2.Add({{"result", "failed"}}));
    }
}

bool MyTrafficHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    // NO RESTRICTIONS FOR SIMULATED NODE
    allowedMethods = {"POST", "GET", "PUT", "DELETE", "HEAD"};
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "PUT" || req.method() == "DELETE" || req.method() == "HEAD");
}

bool MyTrafficHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    // NO RESTRICTIONS FOR SIMULATED NODE
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "PUT" || req.method() == "DELETE" || req.method() == "HEAD");
}


bool MyTrafficHttp2Server::checkHeaders(const nghttp2::asio_http2::server::request&
                                        req)
{
    return true;
    /*
        auto ctype = req.header().find("content-type");
        auto ctype_end = std::end(req.header());

        return ((ctype != ctype_end) ? (ctype->second.value == "application/json") :
                false);
    */
}

std::string MyTrafficHttp2Server::serverDataConfigurationAsJsonString() const {
    nlohmann::json result;

    result["storeEvents"] = server_data_ ? "true":"false";
    result["storeEventsKeyHistory"] = server_data_key_history_ ? "true":"false";
    result["purgeExecution"] = purge_execution_ ? "true":"false";

    return result.dump();
}

std::string MyTrafficHttp2Server::serverConfigurationAsJsonString() const {
    nlohmann::json result;

    result["receiveRequestBody"] = receive_request_body_ ? "true":"false";
    result["preReserveRequestBody"] = pre_reserve_request_body_ ? "true":"false";

    return result.dump();
}

bool MyTrafficHttp2Server::receiveDataLen(const nghttp2::asio_http2::server::request& req) {
    LOGDEBUG(ert::tracing::Logger::debug("receiveRequestBody()",  ERT_FILE_LOCATION));

    // TODO: we could analyze req to get the provision and find out if request body is actually needed.
    // To cache the analysis, we should use complete URI as map key (data/len could be received in
    // chunks and that's why data/len reception sequence id is not valid and it is not provided by
    // http2comm library through this virtual method).

    return receive_request_body_.load();
}

bool MyTrafficHttp2Server::preReserveRequestBody() {
    return pre_reserve_request_body_.load();
}

void MyTrafficHttp2Server::receive(const std::uint64_t &receptionId,
                                   const nghttp2::asio_http2::server::request& req,
                                   const std::string &requestBody,
                                   const std::chrono::microseconds &receptionTimestampUs,
                                   unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                                   std::string& responseBody, unsigned int &responseDelayMs)
{
    LOGDEBUG(ert::tracing::Logger::debug("receive()",  ERT_FILE_LOCATION));

    // see uri_ref struct (https://nghttp2.org/documentation/asio_http2.h.html#asio-http2-h)
    std::string method = req.method();
    //std::string uriRawPath = req.uri().raw_path; // percent-encoded
    std::string uriPath = req.uri().path; // decoded
    std::string uriRawQuery = req.uri().raw_query; // percent-encoded
    std::string uriQuery = ((uriRawQuery.empty()) ? "":ert::http2comm::URLFunctions::decode(uriRawQuery)); // now decoded
    //std::string reqUriFragment = req.uri().fragment; // https://stackoverflow.com/a/65198345/2576671

    // Busy threads:
    int currentBusyThreads = busyThreads();
    if (currentBusyThreads > 0) { // 0 when queue dispatcher is not used
        int maxBusyThreads = max_busy_threads_.load();
        if (currentBusyThreads > maxBusyThreads) {
            maxBusyThreads = currentBusyThreads;
            max_busy_threads_.store(maxBusyThreads);
        }

        LOGINFORMATIONAL(
        if (receptionId % 5000 == 0) {
        std::string msg = ert::tracing::Logger::asString("Current/maximum busy worker threads: %d/%d", currentBusyThreads, maxBusyThreads);
            ert::tracing::Logger::informational(msg,  ERT_FILE_LOCATION);
        }
        );
    }

    // Query parameters transformation:
    std::map<std::string, std::string> qmap; // query parameters map
    if (!uriQuery.empty()) {
        char separator = ((getAdminData()->getMatchingData().getUriPathQueryParametersSeparator() == h2agent::model::AdminServerMatchingData::Ampersand) ? '&':';');
        qmap = h2agent::http2::extractQueryParameters(uriQuery, separator);

        h2agent::model::AdminServerMatchingData::UriPathQueryParametersFilterType uriPathQueryParametersFilterType = getAdminData()->getMatchingData().getUriPathQueryParametersFilter();
        if (uriPathQueryParametersFilterType == h2agent::model::AdminServerMatchingData::Ignore) {
            uriQuery = "";
        }
        else if (uriPathQueryParametersFilterType == h2agent::model::AdminServerMatchingData::Sort) {
            uriQuery = h2agent::http2::sortQueryParameters(qmap, separator);
        }
    }

    std::string uri = uriPath;
    if (!uriQuery.empty()) {
        uri += "?";
        uri += uriQuery;
    }

    LOGDEBUG(
        std::stringstream ss;
        ss << "TRAFFIC REQUEST RECEIVED"
        << " | Reception id (general unique server sequence): " << receptionId
        << " | Method: " << method
        << " | Headers: " << ert::http2comm::headersAsString(req.header())
        << " | Uri: " << req.uri().scheme << "://" << req.uri().host << uri
        << " | Query parameters: " << ((getAdminData()->getMatchingData().getUriPathQueryParametersFilter() == h2agent::model::AdminServerMatchingData::Ignore) ? "ignored":"not ignored");
        if (!requestBody.empty()) ss << " | Body: " << requestBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

// Admin provision & matching configuration:
    const h2agent::model::AdminServerProvisionData & provisionData = getAdminData()->getProvisionData();
    const h2agent::model::AdminServerMatchingData & matchingData = getAdminData()->getMatchingData();

// Find mock context:
    std::string inState;
    /*bool requestFound = */getMockServerEventsData()->findLastRegisteredRequestState(method, uri, inState); // if not found, inState will be 'initial'

// Matching algorithm:
    h2agent::model::AdminServerMatchingData::AlgorithmType algorithmType = matchingData.getAlgorithm();
    std::shared_ptr<h2agent::model::AdminServerProvision> provision(nullptr);

    if (algorithmType == h2agent::model::AdminServerMatchingData::FullMatching) {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'FullMatching' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, uri);
    }
    else if (algorithmType == h2agent::model::AdminServerMatchingData::FullMatchingRegexReplace) {

        std::string transformedUri = std::regex_replace (uri, matchingData.getRgx(), matchingData.getFmt());
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Uri after regex-replace transformation: '%s'", transformedUri.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            msg = ert::tracing::Logger::asString("Searching 'FullMatchingRegexReplace' provision for method '%s', uri '%s' and state '%s'", method.c_str(), transformedUri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, transformedUri);
    }
    else if (algorithmType == h2agent::model::AdminServerMatchingData::RegexMatching) {

        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'RegexMatching' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        // as provision key is built combining inState, method and uri fields, a regular expression could also be provided for inState
        //  (method is strictly checked). TODO could we avoid this rare and unpredictable usage ?
        provision = provisionData.findRegexMatching(inState, method, uri);
    }

    // Fall back to possible default provision (empty URI):
    if (!provision) {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Trying with default provision for '%s'", method.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        provision = provisionData.find(inState, method, "");
    }

    if (provision) {

        std::string outState;
        std::string outStateMethod;
        std::string outStateUri;

        // OPTIONAL SCHEMAS VALIDATION
        const h2agent::model::AdminSchemaData & schemaData = getAdminData()->getSchemaData();
        std::shared_ptr<h2agent::model::AdminSchema> requestSchema(nullptr);
        std::shared_ptr<h2agent::model::AdminSchema> responseSchema(nullptr);
        std::string requestSchemaId = provision->getRequestSchemaId();
        if (!requestSchemaId.empty()) {
            requestSchema = schemaData.find(requestSchemaId);
            LOGWARNING(
                if (!requestSchema) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for incoming message: VALIDATION will be IGNORED", requestSchemaId.c_str()), ERT_FILE_LOCATION);
            );
        }
        std::string responseSchemaId = provision->getResponseSchemaId();
        if (!responseSchemaId.empty()) {
            responseSchema = schemaData.find(responseSchemaId);
            LOGWARNING(
                if (!responseSchema) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for outgoing message: VALIDATION will be IGNORED", responseSchemaId.c_str()), ERT_FILE_LOCATION);
            );
        }

        // PREPARE & TRANSFORM
        provision->setMockServerEventsData(mock_server_events_data_); // could be used by event source
        provision->setGlobalVariable(global_variable_);
        provision->transform(uri, uriPath, qmap, requestBody, req.header(), receptionId,
                             statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod, outStateUri, requestSchema, responseSchema);

        // Special out-states:
        if (purge_execution_ && outState == "purge") {
            bool somethingDeleted = false;
            bool success = getMockServerEventsData()->clear(somethingDeleted, method, uri);
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Requested purge in out-state. Removal %s", success ? "successful":"failed");
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            // metrics
            if(metrics_) {
                if (success) purged_contexts_successful_counter_->Increment();
                else purged_contexts_failed_counter_->Increment();
            }
        }
        else {
            bool hasVirtualMethod = !outStateMethod.empty();

            // Store request event context information
            if (server_data_) {
                getMockServerEventsData()->loadRequest(inState, (hasVirtualMethod ? provision->getOutState():outState), method, uri, req.header(), requestBody, receptionTimestampUs, statusCode, headers, responseBody, receptionId, responseDelayMs, server_data_key_history_ /* history enabled */);

                // Virtual storage:
                if (hasVirtualMethod) {
                    LOGWARNING(
                        if (outStateMethod == method && outStateUri.empty()) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Redundant 'outState' foreign method with current provision one: '%s'", method.c_str()), ERT_FILE_LOCATION);
                    );
                    if (outStateUri.empty()) {
                        outStateUri = uri; // by default
                    }

                    getMockServerEventsData()->loadRequest(inState, outState, outStateMethod /* foreign method */, outStateUri /* foreign uri */, req.header(), requestBody, receptionTimestampUs, statusCode, headers, responseBody, receptionId, responseDelayMs, server_data_key_history_ /* history enabled */, method /* virtual method origin*/, uri /* virtual uri origin */);
                }
            }
        }

        // metrics
        if(metrics_) {
            observed_requests_processed_counter_->Increment();
        }
    }
    else {
        statusCode = 501; // not implemented
        // Store even if not provision was identified (helps to troubleshoot design problems in test configuration):
        if (server_data_) {
            getMockServerEventsData()->loadRequest(""/* empty inState, which will be omitted in server data register */, ""/*outState (same as before)*/, method, uri, req.header(), requestBody, receptionTimestampUs, statusCode, headers, responseBody, receptionId, responseDelayMs, true /* history enabled ALWAYS FOR UNKNOWN EVENTS */);
        }
        // metrics
        if(metrics_) {
            observed_requests_unprovisioned_counter_->Increment();
        }
    }


    LOGDEBUG(
        std::stringstream ss;
        ss << "RESPONSE TO SEND| StatusCode: " << statusCode << " | Headers: " << ert::http2comm::headersAsString(headers);
        if (!responseBody.empty()) ss << " | Body: " << responseBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );
}

}
}
