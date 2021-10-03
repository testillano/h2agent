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
    general_unique_server_sequence_(0) {

    mock_request_data_ = new model::MockRequestData();

    server_data_ = true;
    server_data_requests_history_ = true;
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

    return result.dump();
}

bool MyHttp2Server::setRequestsSchema(const std::string &schemaContent) {

    LOGDEBUG(ert::tracing::Logger::debug("Json string provided for requests schema", ERT_FILE_LOCATION));

    nlohmann::json schema;
    if(!h2agent::http2server::parseJsonContent(schemaContent, schema))
        return false;

    if (!getMockRequestData()->loadRequestsSchema(schema)) {
        LOGWARNING(
            ert::tracing::Logger::warning("Requests won't be validated (schema will be ignored)", ERT_FILE_LOCATION);
        );

        return false;
    }

    return true;
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

    if (uriQuery != "") {
        uriPath += "?";
        uriPath += uriQuery;
    }

    LOGDEBUG(
        std::stringstream ss;
        ss << "REQUEST RECEIVED (traffic interface)| Method: " << method
        << " | Headers: " << h2agent::http2server::headersAsString(req.header())
        << " | Uri Path (and query parameters if not ignored): " << uriPath;
        if (!requestBody.empty()) ss << " | Body: " << requestBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

    // Possible schema validation:
    if(getMockRequestData()->getRequestsSchema().isAvailable()) {
        // TODO: take advantage of this parsing for transformation and request storage below

        nlohmann::json requestJson;
        bool success = h2agent::http2server::parseJsonContent(requestBody, requestJson);

        if (success) {
            if (!getMockRequestData()->getRequestsSchema().validate(requestJson)) {
                statusCode = 400;
                LOGINFORMATIONAL(
                    ert::tracing::Logger::informational("Invalid schema for traffic request received", ERT_FILE_LOCATION);
                );
                return;
            }
        }
        else
        {
            // Response data:
            statusCode = 400;
            LOGINFORMATIONAL(
                ert::tracing::Logger::informational("Failed validation schema (parsing request body) for traffic request received", ERT_FILE_LOCATION);
            );
            return;
        }
    }

// Admin provision & matching configuration:
    const h2agent::model::AdminProvisionData & provisionData = getAdminData()->getProvisionData();
    const h2agent::model::AdminMatchingData & matchingData = getAdminData()->getMatchingData();

// Find mock context:
    std::string inState;
    bool requestFound = getMockRequestData()->findLastRegisteredRequest(method, uriPath, inState); // if not found, inState will be 'initial'

// Matching algorithm:
    h2agent::model::AdminMatchingData::AlgorithmType algorithmType = matchingData.getAlgorithm();
    std::shared_ptr<h2agent::model::AdminProvision> provision;

    if (algorithmType == h2agent::model::AdminMatchingData::FullMatching) {
        LOGDEBUG(
          std::string msg = ert::tracing::Logger::asString("Searching 'FullMatching' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uriPath.c_str(), method.c_str());
          ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, uriPath);
    }

    else if (algorithmType == h2agent::model::AdminMatchingData::FullMatchingRegexReplace) {

        std::string transformedUriPath = std::regex_replace (uriPath, matchingData.getRgx(), matchingData.getFmt());
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Uri Path after regex-replace transformation: '%s'", transformedUriPath.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            msg = ert::tracing::Logger::asString("Searching 'FullMatchingRegexReplace' provision for method '%s', uri '%s' and state '%s'", method.c_str(), transformedUriPath.c_str(), method.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.find(inState, method, transformedUriPath);
    }
    else if (algorithmType == h2agent::model::AdminMatchingData::PriorityMatchingRegex) {

        LOGDEBUG(
          std::string msg = ert::tracing::Logger::asString("Searching 'PriorityMatchingRegex' provision for method '%s', uri '%s' and state '%s'", method.c_str(), uriPath.c_str(), method.c_str());
          ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
        provision = provisionData.findWithPriorityMatchingRegex(inState, method, uriPath);
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

        // PREPARE & TRANSFORM
        provision->setMockRequestData(mock_request_data_); // could be used by event source
        provision->transform( uriPath, req.uri().raw_path, qmap, requestBody, req.header(), getGeneralUniqueServerSequence(),
                              statusCode, headers, responseBody, responseDelayMs, outState, outStateMethod);

        // Special out-states:
        // purge //
        if (outState == "purge") {
            bool somethingDeleted;
            bool success = getMockRequestData()->clear(somethingDeleted, method, uriPath);
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Requested purge in out-state. Removal %s", success ? "successful":"failed");
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
        }
        else {
            bool hasVirtualMethod = (!outStateMethod.empty() && outStateMethod != method);

            // Store request event context information
            if (server_data_) {
                getMockRequestData()->loadRequest(inState, (hasVirtualMethod ? provision->getOutState():outState), method, uriPath, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, server_data_requests_history_ /* history enabled */);

                // Virtual storage:
                if (hasVirtualMethod) {
                    getMockRequestData()->loadRequest(inState, outState, outStateMethod /* foreign method */, uriPath, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, server_data_requests_history_ /* history enabled */, method /* virtual origin coming from method */);
                }
            }
        }
    }
    else {
        statusCode = 501; // not implemented
        // Store even if not provision was identified (helps to troubleshoot design problems in test configuration):
        if (server_data_) {
            getMockRequestData()->loadRequest(""/*inState*/, ""/*outState*/, method, uriPath, req.header(), requestBody, statusCode, headers, responseBody, general_unique_server_sequence_, responseDelayMs, true /* history enabled ALWAYS FOR UNKNOWN EVENTS */);
        }
    }


    LOGDEBUG(
        std::stringstream ss;
        ss << "RESPONSE TO SENT| StatusCode: " << statusCode << " | Headers: " << h2agent::http2server::headersAsString(headers);
        if (!responseBody.empty()) ss << " | Body: " << responseBody;
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    );

// Move to next sequence value:
    general_unique_server_sequence_++;
}

}
}
