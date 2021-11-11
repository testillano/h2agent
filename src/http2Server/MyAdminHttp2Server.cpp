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

#include <chrono>
#include <sstream>
#include <errno.h>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/URLFunctions.hpp>

#include <MyAdminHttp2Server.hpp>
#include <MyHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockRequestData.hpp>
#include <functions.hpp>


namespace h2agent
{
namespace http2server
{

MyAdminHttp2Server::MyAdminHttp2Server(size_t workerThreads):
    ert::http2comm::Http2Server("AdminHttp2Server", workerThreads, nullptr) {

    admin_data_ = new model::AdminData();
}

//const std::pair<int, std::string> JSON_SCHEMA_VALIDATION(
//    ert::http2comm::ResponseCode::BAD_REQUEST,
//    "JSON_SCHEMA_VALIDATION");

bool MyAdminHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    allowedMethods = {"POST", "GET", "DELETE", "PUT"};
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "DELETE" || req.method() == "PUT");
}

bool MyAdminHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "DELETE" || req.method() == "PUT");
}


bool MyAdminHttp2Server::checkHeaders(const
                                      nghttp2::asio_http2::server::request& req)
{
    // Don't check headers for GET and DELETE:
    if (req.method() == "GET" || req.method() == "DELETE" || req.method() == "PUT") {
        return true;
    }

    auto ctype     = req.header().find("content-type");
    auto clength   = req.header().find("content-length");
    auto ctype_end = req.header().end();

    LOGDEBUG(
        ert::tracing::Logger::debug(
            ert::tracing::Logger::asString(
                "[ReceivedRequest] Headers: content-type = %s; content-length = %s",
                (ctype != ctype_end) ? ctype->second.value.c_str() : "(absent)",
                (clength != ctype_end) ? clength->second.value.c_str() : "(absent)"), ERT_FILE_LOCATION));

    if (ctype != ctype_end)
    {
        return (ctype->second.value == "application/json");
    }

    return (clength != ctype_end && clength->second.value != "0");
}


std::string MyAdminHttp2Server::getPathSuffix(const std::string &uriPath) const
{
    std::string result{};

    size_t apiPathSize = getApiPath().size(); // /admin/v1
    size_t uriPathSize = uriPath.size(); // /admin/v1<suffix>
    //LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("apiPathSize %d uriPathSize %d", apiPathSize, uriPathSize),  ERT_FILE_LOCATION));

    // Special case
    if (uriPathSize <= apiPathSize) return result; // indeed, it should not be lesser, as API & VERSION is already checked

    result = uriPath.substr(apiPathSize + 1);

    if (result.back() == '/') result.pop_back(); // normalize by mean removing last slash (if exists)

    return result;
}

/*
#include <iomanip>

void MyAdminHttp2Server::buildJsonResponse(bool result, const std::string &response, std::string &jsonResponse) const
{
    std::stringstream ss;
    ss << R"({ "result":")" << (result ? "true":"false") << R"(", "response": )" << std::quoted(response) << R"( })";
    jsonResponse = ss.str();
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("jsonResponse %s", jsonResponse.c_str()), ERT_FILE_LOCATION));
}

THIS WAS REPLACED TEMPORARILY BY A SLIGHTLY LESS EFFICIENT VERSION, TO AVOID VALGRIND COMPLAIN:
*/

std::string MyAdminHttp2Server::buildJsonResponse(bool responseResult, const std::string &responseBody) const
{
    std::string result{};
    result = R"({ "result":")";
    result += (responseResult ? "true":"false");
    result += R"(", "response": )";
    result += R"(")";
    result += responseBody;
    result += R"(" })";
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Json Response %s", result.c_str()), ERT_FILE_LOCATION));

    return result;
}

void MyAdminHttp2Server::receiveEMPTY(unsigned int& statusCode, std::string &responseBody) const
{
    // Response document:
    // {
    //   "result":"<true or false>",
    //   "response":"<additional information>"
    // }
    responseBody = buildJsonResponse(false, "no operation provided");
    statusCode = 400;
}

bool MyAdminHttp2Server::serverMatching(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "server-matching operation; ";

    h2agent::model::AdminMatchingData::LoadResult loadResult = admin_data_->loadMatching(configurationObject);
    bool result = (loadResult == h2agent::model::AdminMatchingData::Success);

    if (loadResult == h2agent::model::AdminMatchingData::Success) {
        log += "valid schema and matching data received";
    }
    else if (loadResult == h2agent::model::AdminMatchingData::BadSchema) {
        log += "invalid schema";
    }
    else if (loadResult == h2agent::model::AdminMatchingData::BadContent) {
        log += "invalid matching data received";
    }

    return result;
}

bool MyAdminHttp2Server::serverProvision(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "server-provision operation; ";

    h2agent::model::AdminProvisionData::LoadResult loadResult = admin_data_->loadProvision(configurationObject);
    bool result = (loadResult == h2agent::model::AdminProvisionData::Success);

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminProvisionData::Success) {
        log += (isArray ? "valid schemas and provisions data received":"valid schema and provision data received");
    }
    else if (loadResult == h2agent::model::AdminProvisionData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminProvisionData::BadContent) {
        log += (isArray ? "detected one invalid provision data received":"invalid provision data received");
    }

    return result;
}

void MyAdminHttp2Server::receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, std::string &responseBody) const
{
    LOGDEBUG(ert::tracing::Logger::debug("Json body received (admin interface)", ERT_FILE_LOCATION));

    bool jsonResponse_result{};
    std::string jsonResponse_response{};

    // Admin schema validation:
    nlohmann::json requestJson;
    bool success = h2agent::http2server::parseJsonContent(requestBody, requestJson);

    if (success) {
        if (pathSuffix == "server-matching") {
            jsonResponse_result = serverMatching(requestJson, jsonResponse_response);
            statusCode = jsonResponse_result ? 201:400;
        }
        else if (pathSuffix == "server-provision") {
            jsonResponse_result = serverProvision(requestJson, jsonResponse_response);
            statusCode = jsonResponse_result ? 201:400;
        }
        else if (pathSuffix == "server-data/schema") {
            jsonResponse_response = "server-data/schema operation; ";
            if (!getHttp2Server()->getMockRequestData()->loadRequestsSchema(requestJson)) {
                statusCode = 400;
                jsonResponse_response += "load failed";
            }
            else {
                statusCode = 201;
                jsonResponse_result = true;
                jsonResponse_response += "valid schema loaded to validate traffic requests";
            }
        }
        else {
            statusCode = 501;
            jsonResponse_response = "unsupported operation";
        }
    }
    else
    {
        // Response data:
        statusCode = 400;
        jsonResponse_response = "failed to parse json from body request";
    }

    // Build json response body:
    responseBody = buildJsonResponse(jsonResponse_result, jsonResponse_response);
}

void MyAdminHttp2Server::receiveGET(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode, std::string &responseBody) const
{
    if (pathSuffix == "server-provision/schema") {
        responseBody = admin_data_->getProvisionData().getSchema().getJson().dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-matching/schema") {
        responseBody = admin_data_->getMatchingData().getSchema().getJson().dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-data/schema") {
        responseBody = (getHttp2Server()->getMockRequestData()->getRequestsSchema().isAvailable() ? getHttp2Server()->getMockRequestData()->getRequestsSchema().getJson().dump():"null");
        statusCode = (responseBody == "null" ? 204:200);
    }
    else if (pathSuffix == "server-provision") {
        bool ordered = (admin_data_->getMatchingData().getAlgorithm() == h2agent::model::AdminMatchingData::PriorityMatchingRegex);
        responseBody = admin_data_->getProvisionData().asJsonString(ordered);
        statusCode = (responseBody == "null" ? 204:200);
    }
    else if (pathSuffix == "server-matching") {
        responseBody = admin_data_->getMatchingData().getJson().dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-data") {
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string requestNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::http2server::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("requestNumber");
            if (it != qmap.end()) requestNumber = it->second;
        }

        bool validQuery;
        responseBody = getHttp2Server()->getMockRequestData()->asJsonString(requestMethod, requestUri, requestNumber, validQuery);
        statusCode = validQuery ? (responseBody == "null" ? 204:200):400;
    }
    else if (pathSuffix == "server-data/configuration") {
        responseBody = getHttp2Server()->serverDataConfigurationAsJsonString();
        statusCode = 200;
    }
    else {
        statusCode = 400;
        responseBody = buildJsonResponse(false, "invalid operation (allowed: server-provision|server-matching|server-data)");
    }
}

void MyAdminHttp2Server::receiveDELETE(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const
{
    bool somethingDeleted;

    if (pathSuffix == "server-provision") {
        statusCode = (admin_data_->clearProvisions() ? 200:204);
    }
    else if (pathSuffix == "server-data") {
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string requestNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::http2server::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("requestNumber");
            if (it != qmap.end()) requestNumber = it->second;
        }

        bool success = getHttp2Server()->getMockRequestData()->clear(somethingDeleted, requestMethod, requestUri, requestNumber);
        statusCode = success ? (somethingDeleted ? 200:204):400;
    }
    else {
        statusCode = 400;
    }
}

void MyAdminHttp2Server::receivePUT(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const
{
    bool success = false;

    if (pathSuffix == "logging") {
        std::string level = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::http2server::extractQueryParameters(queryParams);
            auto it = qmap.find("level");
            if (it != qmap.end()) level = it->second;
        }

        std::string previousLevel = ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel());
        success = ert::tracing::Logger::setLevel(level);
        //LOGWARNING(
        if (success) {
            if (level != previousLevel)
                ert::tracing::Logger::warning(ert::tracing::Logger::asString("Log level changed: %s -> %s", previousLevel.c_str(), level.c_str()), ERT_FILE_LOCATION);
            else
                ert::tracing::Logger::warning(ert::tracing::Logger::asString("Log level unchanged (already %s)", previousLevel.c_str()), ERT_FILE_LOCATION);
        }
        else {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid log level provided (%s). Keeping current (%s)", level.c_str(), previousLevel.c_str()), ERT_FILE_LOCATION);
        }
        //);
    }
    else if (pathSuffix == "server-data/configuration") {

        std::string discard{};
        std::string discardRequestsHistory{};

        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::http2server::extractQueryParameters(queryParams);
            auto it = qmap.find("discard");
            if (it != qmap.end()) discard = it->second;
            it = qmap.find("discardRequestsHistory");
            if (it != qmap.end()) discardRequestsHistory = it->second;
        }

        bool b_discard = (discard == "true");
        bool b_discardRequestsHistory = (discardRequestsHistory == "true");

        success = true;
        if (!discard.empty() && !discardRequestsHistory.empty())
            success = !(b_discard && !b_discardRequestsHistory); // it has no sense to try to keep history if whole server data is discarded

        if (success) {
            if (!discard.empty()) {
                getHttp2Server()->discardServerData(b_discard);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard server data: %s", b_discard ? "true":"false"), ERT_FILE_LOCATION));
            }
            if (!discardRequestsHistory.empty()) {
                getHttp2Server()->discardServerDataRequestsHistory(b_discardRequestsHistory);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard server data requests history: %s", b_discardRequestsHistory ? "true":"false"), ERT_FILE_LOCATION));
            }
        }
        else {
            ert::tracing::Logger::error("Cannot keep requests history if whole server data is discarded", ERT_FILE_LOCATION);
        }
    }

    statusCode = success ? 200:400;
}

void MyAdminHttp2Server::receive(const nghttp2::asio_http2::server::request&
                                 req,
                                 const std::string& requestBody,
                                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                                 std::string& responseBody, unsigned int &responseDelayMs)
{
    LOGDEBUG(ert::tracing::Logger::debug("receive()",  ERT_FILE_LOCATION));

    // see uri_ref struct (https://nghttp2.org/documentation/asio_http2.h.html#asio-http2-h)
    std::string method = req.method();
    //std::string uriPath = req.uri().raw_path; // percent-encoded
    std::string uriPath = req.uri().path; // decoded
    std::string uriRawQuery = req.uri().raw_query; // percent-encoded
    std::string uriQuery = ((uriRawQuery.empty()) ? "":ert::http2comm::URLFunctions::decode(uriRawQuery)); // now decoded

    // Get path suffix normalized:
    std::string pathSuffix = getPathSuffix(uriPath);
    bool noPathSuffix = pathSuffix.empty();
    LOGDEBUG(
    if (noPathSuffix) {
    ert::tracing::Logger::debug("URI Path Suffix: <null>", ERT_FILE_LOCATION);
    }
    else {
        std::stringstream ss;
        ss << "ADMIN REQUEST RECEIVED [" << pathSuffix << "]| Method: " << method << " | Headers: " << h2agent::http2server::headersAsString(req.header()) << " | Uri Path: " << uriPath;
        if (!uriQuery.empty()) {
            ss << " | Query Params: " << uriQuery;
        }
        if (!requestBody.empty()) {
            std::string requestBodyWithoutNewlines = requestBody; // administrative interface receives json bodies in POST requests, so we normalize for logging
            requestBodyWithoutNewlines.erase(std::remove(requestBodyWithoutNewlines.begin(), requestBodyWithoutNewlines.end(), '\n'), requestBodyWithoutNewlines.end());
            ss << " | Body: " << requestBodyWithoutNewlines;
        }
        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
    }
    );

    // Defaults
    responseBody.clear();
    headers.emplace("Content-Type", nghttp2::asio_http2::header_value{"application/json"}); // except DELETE

    // No operation provided:
    if (noPathSuffix) {
        receiveEMPTY(statusCode, responseBody);
        return;
    }

    // Methods supported:
    if (method == "DELETE") {
        receiveDELETE(pathSuffix, uriQuery, statusCode);
        headers.clear();
        return;
    }
    else if (method == "GET") {
        receiveGET(pathSuffix, uriQuery, statusCode, responseBody);
        return;
    }
    else if (method == "POST") {
        receivePOST(pathSuffix, requestBody, statusCode, responseBody);
        return;
    }
    else if (method == "PUT") {
        receivePUT(pathSuffix, uriQuery, statusCode);
        headers.clear();
        return;
    }
}

}
}
