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

#include <MyTrafficHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockServerData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <functions.hpp>

namespace h2agent
{
namespace http2
{


MyTrafficHttp2Server::MyTrafficHttp2Server(size_t workerThreads, size_t maxWorkerThreads, boost::asio::io_context *timersIoContext, int maxQueueDispatcherSize):
    ert::http2comm::Http2Server("MockHttp2Server", workerThreads, maxWorkerThreads, timersIoContext, maxQueueDispatcherSize),
    admin_data_(nullptr) {

    server_data_ = true;
    server_data_key_history_ = true;
    purge_execution_ = true;
}

void MyTrafficHttp2Server::enableMyMetrics(ert::metrics::Metrics *metrics) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::counter_family_ref_t cf = metrics->addCounterFamily(std::string("ServerData_observed_requests_total"), "Http2 total requests observed in h2agent server");

        observed_requests_processed_counter_ = &(cf.Add({{"result", "processed"}}));
        observed_requests_unprovisioned_counter_ = &(cf.Add({{"result", "unprovisioned"}}));

        ert::metrics::counter_family_ref_t cf2 = metrics->addCounterFamily(std::string("ServerData_purged_contexts_total"), "Total contexts purged in h2agent server");

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

std::string MyTrafficHttp2Server::dataConfigurationAsJsonString() const {
    nlohmann::json result;

    result["storeEvents"] = server_data_;
    result["storeEventsKeyHistory"] = server_data_key_history_;
    result["purgeExecution"] = purge_execution_;

    return result.dump();
}

std::string MyTrafficHttp2Server::configurationAsJsonString() const {
    nlohmann::json result;

    result["receiveRequestBody"] = receive_request_body_.load();
    result["preReserveRequestBody"] = pre_reserve_request_body_.load();

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
    std::string uriQuery = req.uri().raw_query; // parameter values may be percent-encoded
    //std::string reqUriFragment = req.uri().fragment; // https://stackoverflow.com/a/65198345/2576671

    // Move request body to internal encoded body data:
    h2agent::model::DataPart requestBodyDataPart(std::move(requestBody));

    // Busy threads:
    int currentBusyThreads = getQueueDispatcherBusyThreads();
    if (currentBusyThreads > 0) { // 0 when queue dispatcher is not used
        int maxBusyThreads = max_busy_threads_.load();
        if (currentBusyThreads > maxBusyThreads) {
            maxBusyThreads = currentBusyThreads;
            max_busy_threads_.store(maxBusyThreads);
        }

        LOGINFORMATIONAL(
        if (receptionId % 5000 == 0) {
        std::string msg = ert::tracing::Logger::asString("'Current/maximum reached' busy worker threads: %d/%d | QueueDispatcher workers/size/max-size: %d/%d/%d", currentBusyThreads, maxBusyThreads, getQueueDispatcherThreads(), getQueueDispatcherSize(), getQueueDispatcherMaxSize());
            ert::tracing::Logger::informational(msg,  ERT_FILE_LOCATION);
        }
        );
    }

    LOGDEBUG(
        std::stringstream ss;
        // Original URI:
        std::string originalUri = uriPath;
    if (!uriQuery.empty()) {
    originalUri += "?";
    originalUri += uriQuery;
}
ss << "TRAFFIC REQUEST RECEIVED"
   << " | Reception id (general unique server sequence): " << receptionId
   << " | Method: " << method
   << " | Headers: " << ert::http2comm::headersAsString(req.header())
   << " | Uri: " << req.uri().scheme << "://" << req.uri().host << originalUri
   << " | Query parameters: " << ((getAdminData()->getServerMatchingData().getUriPathQueryParametersFilter() == h2agent::model::AdminServerMatchingData::Ignore) ? "ignored":"not ignored")
   << " | Body (as ascii string, dots for non-printable): " << requestBodyDataPart.asAsciiString();
   ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
);

    // Normalized URI: original URI with query parameters normalized (ordered) / Classification URI: may ignore, sort or pass by query parameters
    std::string normalizedUri = uriPath;
    std::string classificationUri = uriPath;

    // Query parameters transformation:
    std::map<std::string, std::string> qmap; // query parameters map
    if (!uriQuery.empty()) {
        char separator = ((getAdminData()->getServerMatchingData().getUriPathQueryParametersSeparator() == h2agent::model::AdminServerMatchingData::Ampersand) ? '&':';');
        qmap = h2agent::model::extractQueryParameters(uriQuery, separator); // needed even for 'Ignore' QParam filter type
        std::string uriQueryNormalized = h2agent::model::sortQueryParameters(qmap, separator);

        normalizedUri += "?";
        normalizedUri += uriQueryNormalized;

        h2agent::model::AdminServerMatchingData::UriPathQueryParametersFilterType uriPathQueryParametersFilterType = getAdminData()->getServerMatchingData().getUriPathQueryParametersFilter();
        if (uriPathQueryParametersFilterType != h2agent::model::AdminServerMatchingData::Ignore) {
            classificationUri += "?";
            if (uriPathQueryParametersFilterType == h2agent::model::AdminServerMatchingData::PassBy) {
                classificationUri += uriQuery;
            }
            else if (uriPathQueryParametersFilterType == h2agent::model::AdminServerMatchingData::Sort) {
                classificationUri += uriQueryNormalized;
            }
        }

    }

    LOGDEBUG(
        std::stringstream ss;
        ss << "Normalized Uri (server data event keys): " << req.uri().scheme << "://" << req.uri().host << normalizedUri;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

// Admin provision & matching configuration:
    const h2agent::model::AdminServerProvisionData & provisionData = getAdminData()->getServerProvisionData();
    const h2agent::model::AdminServerMatchingData & matchingData = getAdminData()->getServerMatchingData();

// Find mock context:
    std::string inState{};
    h2agent::model::DataKey normalizedKey(method, normalizedUri);

    /*bool requestFound = */getMockServerData()->findLastRegisteredRequestState(normalizedKey, inState); // if not found, inState will be 'initial'

// Matching algorithm:
    h2agent::model::AdminServerMatchingData::AlgorithmType algorithmType = matchingData.getAlgorithm();
    std::shared_ptr<h2agent::model::AdminServerProvision> provision(nullptr);

    LOGDEBUG(
        std::stringstream ss;
    if (algorithmType != h2agent::model::AdminServerMatchingData::FullMatchingRegexReplace) {
    ss << "Classification Uri: " << req.uri().scheme << "://" << req.uri().host << classificationUri;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    }
    );

    if (algorithmType == h2agent::model::AdminServerMatchingData::FullMatching) {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'FullMatching' provision for method '%s', classification uri '%s' and state '%s'", method.c_str(), classificationUri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, classificationUri);
    }
    else if (algorithmType == h2agent::model::AdminServerMatchingData::FullMatchingRegexReplace) {

        // In this case, our classification URI is pending to be transformed:
        classificationUri = std::regex_replace (classificationUri, matchingData.getRgx(), matchingData.getFmt());
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Classification Uri (after regex-replace transformation): %s", classificationUri.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            msg = ert::tracing::Logger::asString("Searching 'FullMatchingRegexReplace' provision for method '%s', classification uri '%s' and state '%s'", method.c_str(), classificationUri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, classificationUri);
    }
    else if (algorithmType == h2agent::model::AdminServerMatchingData::RegexMatching) {

        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'RegexMatching' provision for method '%s', classification uri '%s' and state '%s'", method.c_str(), classificationUri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        // as provision key is built combining inState, method and uri fields, a regular expression could also be provided for inState
        //  (method is strictly checked). TODO could we avoid this rare and unpredictable usage ?
        provision = provisionData.findRegexMatching(inState, method, classificationUri);
    }

    // Fall back to possible default provision (empty URI):
    if (!provision) {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("No provision found for classification URI. Trying with default fallback provision for '%s'", method.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        provision = provisionData.find(inState, method, "");
    }

    if (provision) {

        LOGDEBUG(ert::tracing::Logger::debug("Provision successfully indentified !", ERT_FILE_LOCATION));

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

        // Process provision
        provision->transform(normalizedUri, uriPath, qmap, requestBodyDataPart, req.header(), receptionId,
                             statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod, outStateUri, requestSchema, responseSchema);

        // Special out-states:
        if (purge_execution_ && outState == "purge") {
            bool somethingDeleted = false;
            bool success = getMockServerData()->clear(somethingDeleted, h2agent::model::EventKey(normalizedKey, ""));
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

            // Store event context information
            if (server_data_) {
                getMockServerData()->loadEvent(normalizedKey, inState, (hasVirtualMethod ? provision->getOutState():outState), receptionTimestampUs, statusCode, req.header(), headers, requestBodyDataPart, responseBody, receptionId, responseDelayMs, server_data_key_history_ /* history enabled */);

                // Virtual storage:
                if (hasVirtualMethod) {
                    LOGWARNING(
                        if (outStateMethod == method && outStateUri.empty()) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Redundant 'outState' foreign method with current provision one: '%s'", method.c_str()), ERT_FILE_LOCATION);
                    );
                    if (outStateUri.empty()) {
                        outStateUri = normalizedUri; // by default
                    }

                    h2agent::model::DataKey foreignKey(outStateMethod /* foreign method */, outStateUri /* foreign uri */);
                    getMockServerData()->loadEvent(foreignKey, inState, outState, receptionTimestampUs, statusCode, req.header(), headers, requestBodyDataPart, responseBody, receptionId, responseDelayMs, server_data_key_history_ /* history enabled */, method /* virtual method origin*/, normalizedUri /* virtual uri origin */);
                }
            }
        }

        // metrics
        if(metrics_) {
            observed_requests_processed_counter_->Increment();
        }
    }
    else {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Default fallback provision not found: returning status code 501 (Not Implemented).", method.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        statusCode = ert::http2comm::ResponseCode::NOT_IMPLEMENTED; // 501
        // Store even if not provision was identified (helps to troubleshoot design problems in test configuration):
        if (server_data_) {
            getMockServerData()->loadEvent(normalizedKey, ""/* empty inState, which will be omitted in server data register */, ""/*outState (same as before)*/, receptionTimestampUs, statusCode, req.header(), headers, requestBodyDataPart, responseBody, receptionId, responseDelayMs, true /* history enabled ALWAYS FOR UNKNOWN EVENTS */);
        }
        // metrics
        if(metrics_) {
            observed_requests_unprovisioned_counter_->Increment();
        }
    }


    LOGDEBUG(
        std::stringstream ss;
        ss << "RESPONSE TO SEND| StatusCode: " << statusCode << " | Headers: " << ert::http2comm::headersAsString(headers);
    if (!responseBody.empty()) {
    std::string output;
    h2agent::model::asAsciiString(responseBody, output);
        ss << " | Body (as ascii string, dots for non-printable): " << output;
    }
    ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );
}

}
}
