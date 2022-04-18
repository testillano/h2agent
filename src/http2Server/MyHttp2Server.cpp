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
#include <ert/http2comm/URLFunctions.hpp>

#include <MyHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockRequestData.hpp>
#include <functions.hpp>

namespace h2agent
{
namespace http2server
{


MyHttp2Server::MyHttp2Server(size_t workerThreads, boost::asio::io_service *timersIoService):
    ert::http2comm::Http2Server("MockHttp2Server", workerThreads, timersIoService),
    admin_data_(nullptr),
    general_unique_server_sequence_(1) {

    mock_request_data_ = new model::MockRequestData();

    server_data_ = true;
    server_data_requests_history_ = true;
    purge_execution_ = true;
}

void MyHttp2Server::enableMyMetrics(ert::metrics::Metrics *metrics) {

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

bool MyHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    // NO RESTRICTIONS FOR SIMULATED NODE
    allowedMethods = {"POST", "GET", "PUT", "DELETE", "HEAD"};
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "PUT" || req.method() == "DELETE" || req.method() == "HEAD");
}

bool MyHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    // NO RESTRICTIONS FOR SIMULATED NODE
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "PUT" || req.method() == "DELETE" || req.method() == "HEAD");
}


bool MyHttp2Server::checkHeaders(const nghttp2::asio_http2::server::request&
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

std::string MyHttp2Server::serverDataConfigurationAsJsonString() const {
    nlohmann::json result;

    result["storeEvents"] = server_data_ ? "true":"false";
    result["storeEventsRequestsHistory"] = server_data_requests_history_ ? "true":"false";
    result["purgeExecution"] = purge_execution_ ? "true":"false";

    return result.dump();
}

void MyHttp2Server::receive(const nghttp2::asio_http2::server::request& req,
                            const std::string& requestBody,
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

    // Query parameters transformation:
    h2agent::model::AdminMatchingData::UriPathQueryParametersFilterType uriPathQueryParametersFilterType = getAdminData()->getMatchingData(). getUriPathQueryParametersFilter();
    std::map<std::string, std::string> qmap; // query parameters map

    if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::Ignore) {
        uriQuery = "";
    }
    else if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::SortAmpersand) {

        qmap = h2agent::http2server::extractQueryParameters(uriQuery);
        uriQuery = h2agent::http2server::sortQueryParameters(qmap);
    }
    else if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::SortSemicolon) {
        qmap = h2agent::http2server::extractQueryParameters(uriQuery, ';');
        uriQuery = h2agent::http2server::sortQueryParameters(qmap, ';');
    }

    std::string uri = uriPath;
    if (uriQuery != "") {
        uri += "?";
        uri += uriQuery;
    }

    LOGDEBUG(
        std::stringstream ss;
        ss << "REQUEST RECEIVED (traffic interface)| Method: " << method
        << " | Headers: " << h2agent::http2server::headersAsString(req.header())
        << " | Uri (path + query parameters if not ignored): " << uri;
        if (!requestBody.empty()) ss << " | Body: " << requestBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

// Admin provision & matching configuration:
    const h2agent::model::AdminProvisionData & provisionData = getAdminData()->getProvisionData();
    const h2agent::model::AdminMatchingData & matchingData = getAdminData()->getMatchingData();

// Find mock context:
    std::string inState;
    /*bool requestFound = */getMockRequestData()->findLastRegisteredRequest(method, uri, inState); // if not found, inState will be 'initial'

// Matching algorithm:
    h2agent::model::AdminMatchingData::AlgorithmType algorithmType = matchingData.getAlgorithm();
    std::shared_ptr<h2agent::model::AdminProvision> provision;

    if (algorithmType == h2agent::model::AdminMatchingData::FullMatching) {
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'FullMatching' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, uri);
    }
    else if (algorithmType == h2agent::model::AdminMatchingData::FullMatchingRegexReplace) {

        std::string transformedUri = std::regex_replace (uri, matchingData.getRgx(), matchingData.getFmt());
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Uri after regex-replace transformation: '%s'", transformedUri.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            msg = ert::tracing::Logger::asString("Searching 'FullMatchingRegexReplace' provision for method '%s', uri '%s' and state '%s'", method.c_str(), transformedUri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, transformedUri);
    }
    else if (algorithmType == h2agent::model::AdminMatchingData::PriorityMatchingRegex) {

        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Searching 'PriorityMatchingRegex' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uri.c_str(), inState.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.findWithPriorityMatchingRegex(inState, method, uri);
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

        // OPTIONAL SCHEMAS VALIDATION
        const h2agent::model::AdminSchemaData & schemaData = getAdminData()->getSchemaData();
        std::shared_ptr<h2agent::model::AdminSchema> requestSchema;
        std::shared_ptr<h2agent::model::AdminSchema> responseSchema;
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
        provision->setMockRequestData(mock_request_data_); // could be used by event source
        provision->transform(uri, uriPath, qmap, requestBody, req.header(), getGeneralUniqueServerSequence(),
                             statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod, requestSchema, responseSchema);

        // Special out-states:
        if (purge_execution_ && outState == "purge") {
            bool somethingDeleted;
            bool success = getMockRequestData()->clear(somethingDeleted, method, uri);
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
            bool hasVirtualMethod = (!outStateMethod.empty() && outStateMethod != method);

            // Store request event context information
            if (server_data_) {
                getMockRequestData()->loadRequest(inState, (hasVirtualMethod ? provision->getOutState():outState), method, uri, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, server_data_requests_history_ /* history enabled */);

                // Virtual storage:
                if (hasVirtualMethod) {
                    getMockRequestData()->loadRequest(inState, outState, outStateMethod /* foreign method */, uri, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, server_data_requests_history_ /* history enabled */, method /* virtual origin coming from method */);
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
            getMockRequestData()->loadRequest(""/* empty inState, which will be omitted in server data register */, ""/*outState (same as before)*/, method, uri, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, true /* history enabled ALWAYS FOR UNKNOWN EVENTS */);
        }
        // metrics
        if(metrics_) {
            observed_requests_unprovisioned_counter_->Increment();
        }
    }


    LOGDEBUG(
        std::stringstream ss;
        ss << "RESPONSE TO SEND| StatusCode: " << statusCode << " | Headers: " << h2agent::http2server::headersAsString(headers);
        if (!responseBody.empty()) ss << " | Body: " << responseBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

// Move to next sequence value:
    general_unique_server_sequence_++;
}

}
}
