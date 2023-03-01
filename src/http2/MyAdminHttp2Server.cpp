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
#include <errno.h>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/URLFunctions.hpp>

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockServerEventsData.hpp>
#include <Configuration.hpp>
#include <GlobalVariable.hpp>
#include <FileManager.hpp>
#include <functions.hpp>


namespace h2agent
{
namespace http2
{

bool statusCodeOK(int statusCode) {
    return (statusCode >= 200 && statusCode < 300);
}

MyAdminHttp2Server::MyAdminHttp2Server(size_t workerThreads):
    ert::http2comm::Http2Server("AdminHttp2Server", workerThreads, workerThreads, nullptr) {

    admin_data_ = new model::AdminData();
}

MyAdminHttp2Server::~MyAdminHttp2Server()
{
    delete (admin_data_);
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
    std::string result;

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
    std::string result;
    result = R"({ "result":")";
    result += (responseResult ? "true":"false");
    result += R"(", "response": )";
    result += R"(")";
    result += responseBody;
    result += R"(" })";
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Json Response %s", result.c_str()), ERT_FILE_LOCATION));

    return result;
}

void MyAdminHttp2Server::receiveNOOP(unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receiveNOOP()",  ERT_FILE_LOCATION));
    // Response document:
    // {
    //   "result":"<true or false>",
    //   "response":"<additional information>"
    // }
    responseBody = buildJsonResponse(false, "no operation provided");
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
    statusCode = 400;
}

int MyAdminHttp2Server::serverMatching(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "server-matching operation; ";

    h2agent::model::AdminServerMatchingData::LoadResult loadResult = admin_data_->loadServerMatching(configurationObject);
    int result = ((loadResult == h2agent::model::AdminServerMatchingData::Success) ? 201:400);

    if (loadResult == h2agent::model::AdminServerMatchingData::Success) {
        log += "valid schema and matching data received";
    }
    else if (loadResult == h2agent::model::AdminServerMatchingData::BadSchema) {
        log += "invalid schema";
    }
    else if (loadResult == h2agent::model::AdminServerMatchingData::BadContent) {
        log += "invalid matching data received";
    }

    return result;
}

int MyAdminHttp2Server::serverProvision(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "server-provision operation; ";

    h2agent::model::AdminServerProvisionData::LoadResult loadResult = admin_data_->loadServerProvision(configurationObject, common_resources_);
    int result = ((loadResult == h2agent::model::AdminServerProvisionData::Success) ? 201:400);

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminServerProvisionData::Success) {
        log += (isArray ? "valid schemas and provisions data received":"valid schema and provision data received");
    }
    else if (loadResult == h2agent::model::AdminServerProvisionData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminServerProvisionData::BadContent) {
        log += (isArray ? "detected one invalid provision data received":"invalid provision data received");
    }

    return result;
}

int MyAdminHttp2Server::clientEndpoint(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "client-endpoint operation; ";

    h2agent::model::AdminClientEndpointData::LoadResult loadResult = admin_data_->loadClientEndpoint(configurationObject, common_resources_);
    int result = 400;
    if (loadResult == h2agent::model::AdminClientEndpointData::Success) {
        result = 201;
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::Accepted) {
        result = 202;
    }

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminClientEndpointData::Success || loadResult == h2agent::model::AdminClientEndpointData::Accepted) {
        log += (isArray ? "valid schemas and client endpoints data received":"valid schema and client endpoints data received");
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::BadContent) {
        log += (isArray ? "detected one invalid client endpoint data received":"invalid client endpoint data received");
    }

    return result;
}

int MyAdminHttp2Server::globalVariable(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "global-variable operation; ";

    int result = getGlobalVariable()->loadJson(configurationObject) ? 201:400;
    log += (statusCodeOK(result) ? "valid schema and global variables received":"invalid schema");

    return result;
}

int MyAdminHttp2Server::schema(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "schema operation; ";

    h2agent::model::AdminSchemaData::LoadResult loadResult = admin_data_->loadSchema(configurationObject);
    int result = ((loadResult == h2agent::model::AdminSchemaData::Success) ? 201:400);

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminSchemaData::Success) {
        log += (isArray ? "valid schemas and schemas data received":"valid schema and schema data received");
    }
    else if (loadResult == h2agent::model::AdminSchemaData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminSchemaData::BadContent) {
        log += (isArray ? "detected one invalid schema data received":"invalid schema data received");
    }

    return result;
}

void MyAdminHttp2Server::receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receivePOST()",  ERT_FILE_LOCATION));
    LOGDEBUG(ert::tracing::Logger::debug("Json body received (admin interface)", ERT_FILE_LOCATION));

    std::string jsonResponse_response;

    // All responses are json content:
    headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});

    // Admin schema validation:
    nlohmann::json requestJson;
    bool success = h2agent::model::parseJsonContent(requestBody, requestJson);

    if (success) {
        if (pathSuffix == "server-matching") {
            statusCode = serverMatching(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "server-provision") {
            statusCode = serverProvision(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "client-endpoint") {
            statusCode = clientEndpoint(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "schema") {
            statusCode = schema(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "global-variable") {
            statusCode = globalVariable(requestJson, jsonResponse_response);
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
    responseBody = buildJsonResponse(statusCodeOK(statusCode), jsonResponse_response);
}

void MyAdminHttp2Server::receiveGET(const std::string &uri, const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receiveGET()",  ERT_FILE_LOCATION));

    // All responses, except for 'logging', are json content:
    bool jsonContent = true;

    if (pathSuffix == "server-matching/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getServerMatchingData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-provision/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getServerProvisionData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = 200;
    }
    else if (pathSuffix == "client-endpoint/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getClientEndpointData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = 200;
    }
    else if (pathSuffix == "schema/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getSchemaData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-data/summary") {
        std::string maxKeys = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("maxKeys");
            if (it != qmap.end()) maxKeys = it->second;
        }

        responseBody = getHttp2Server()->getMockServerEventsData()->summary(maxKeys);
        statusCode = 200;
    }
    else if (pathSuffix == "global-variable") {
        std::string name = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("name");
            if (it != qmap.end()) name = it->second;
            if (name.empty()) {
                statusCode = 400;
                responseBody = "";
            }
        }
        if (statusCode != 400) {
            if (name.empty()) {
                responseBody = getGlobalVariable()->asJsonString();
                statusCode = ((responseBody == "{}") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
            }
            else {
                bool exists;
                responseBody = getGlobalVariable()->getValue(name, exists);
                statusCode = (exists ? 200:204); // response body will be emptied by nghttp2 when status code is 204 (No Content)
            }
        }
    }
    else if (pathSuffix == "global-variable/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = getGlobalVariable()->getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-matching") {
        responseBody = admin_data_->getServerMatchingData().getJson().dump();
        statusCode = 200;
    }
    else if (pathSuffix == "server-provision") {
        bool ordered = (admin_data_->getServerMatchingData().getAlgorithm() == h2agent::model::AdminServerMatchingData::RegexMatching);
        responseBody = admin_data_->getServerProvisionData().asJsonString(ordered);
        statusCode = ((responseBody == "[]") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "client-endpoint") {
        responseBody = admin_data_->getClientEndpointData().asJsonString();
        statusCode = ((responseBody == "[]") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "schema") {
        responseBody = admin_data_->getSchemaData().asJsonString();
        statusCode = ((responseBody == "[]") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "server-data") {
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string requestNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("requestNumber");
            if (it != qmap.end()) requestNumber = it->second;
        }

        bool validQuery = false;
        try { // dump could throw exception if something weird is done (binary data with non-binary content-type)
            responseBody = getHttp2Server()->getMockServerEventsData()->asJsonString(requestMethod, requestUri, requestNumber, validQuery);
        }
        catch (const std::exception& e)
        {
            //validQuery = false; // will be 200 with empty result (corner case)
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
        statusCode = validQuery ? ((responseBody == "[]") ? 204:200):400; // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "configuration") {
        responseBody = getConfiguration()->asJsonString();
        statusCode = 200;
    }
    else if (pathSuffix == "server/configuration") {
        responseBody = getHttp2Server()->configurationAsJsonString();
        statusCode = 200;
    }
    else if (pathSuffix == "server-data/configuration") {
        responseBody = getHttp2Server()->dataConfigurationAsJsonString();
        statusCode = 200;
    }
    else if (pathSuffix == "files/configuration") {
        responseBody = getFileManager()->configurationAsJsonString();
        statusCode = 200;
    }
    else if (pathSuffix == "files") {
        responseBody = getFileManager()->asJsonString();
        statusCode = ((responseBody == "[]") ? 204:200);
    }
    else if (pathSuffix == "logging") {
        responseBody = ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel());
        headers.emplace("content-type", nghttp2::asio_http2::header_value{"text/html"});
        jsonContent = false;
        statusCode = 200;
    }
    else {
        statusCode = 400;
        responseBody = buildJsonResponse(false, std::string("invalid operation '") + pathSuffix + std::string("'"));
    }

    if (jsonContent) headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
}

void MyAdminHttp2Server::receiveDELETE(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receiveDELETE()",  ERT_FILE_LOCATION));

    if (pathSuffix == "server-provision") {
        statusCode = (admin_data_->clearServerProvisions() ? 200:204);
    }
    else if (pathSuffix == "client-endpoint") {
        statusCode = (admin_data_->clearClientEndpoints() ? 200:204);
    }
    else if (pathSuffix == "schema") {
        statusCode = (admin_data_->clearSchemas() ? 200:204);
    }
    else if (pathSuffix == "server-data") {
        bool serverDataDeleted = false;
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string requestNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("requestNumber");
            if (it != qmap.end()) requestNumber = it->second;
        }

        bool success = getHttp2Server()->getMockServerEventsData()->clear(serverDataDeleted, requestMethod, requestUri, requestNumber);

        statusCode = (success ? (serverDataDeleted ? 200:204):400);
    }
    else if (pathSuffix == "global-variable") {
        bool globalVariableDeleted = false;
        std::string name = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("name");
            if (it != qmap.end()) name = it->second;
            if (name.empty()) {
                statusCode = 400;
            }
        }
        if (statusCode != 400) {
            if (name.empty()) {
                statusCode = (getGlobalVariable()->clear() ? 200:204);
            }
            else {
                bool exists;
                getGlobalVariable()->removeVariable(name, exists);
                statusCode = (exists ? 200:204);
            }
        }
    }
    else {
        statusCode = 400;
    }
}

void MyAdminHttp2Server::receivePUT(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receivePUT()",  ERT_FILE_LOCATION));

    bool success = false;

    if (pathSuffix == "logging") {
        std::string level = "?";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("level");
            if (it != qmap.end()) level = it->second;
        }

        std::string previousLevel = ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel());
        if (level != "?") {
            success = ert::tracing::Logger::setLevel(level);
        }

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
    else if (pathSuffix == "server/configuration") {
        std::string receiveRequestBody;
        std::string preReserveRequestBody;

        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("receiveRequestBody");
            if (it != qmap.end()) receiveRequestBody = it->second;
            it = qmap.find("preReserveRequestBody");
            if (it != qmap.end()) preReserveRequestBody = it->second;
        }

        bool b_receiveRequestBody = (receiveRequestBody == "true");
        bool b_preReserveRequestBody = (preReserveRequestBody == "true");

        success = (!receiveRequestBody.empty() || !preReserveRequestBody.empty());

        if (!receiveRequestBody.empty()) {
            success = (receiveRequestBody == "true" || receiveRequestBody == "false");
            if (success) {
                getHttp2Server()->setReceiveRequestBody(b_receiveRequestBody);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Traffic server request body reception: %s", b_receiveRequestBody ? "processed":"ignored"), ERT_FILE_LOCATION));
            }
        }

        if (success && !preReserveRequestBody.empty()) {
            success = (preReserveRequestBody == "true" || preReserveRequestBody == "false");
            if (success) {
                getHttp2Server()->setPreReserveRequestBody(b_preReserveRequestBody);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Traffic server dynamic request body allocation: %s", b_preReserveRequestBody ? "false":"true"), ERT_FILE_LOCATION));
            }
        }
    }
    else if (pathSuffix == "server-data/configuration") {

        std::string discard;
        std::string discardKeyHistory;
        std::string disablePurge;

        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("discard");
            if (it != qmap.end()) discard = it->second;
            it = qmap.find("discardKeyHistory");
            if (it != qmap.end()) discardKeyHistory = it->second;
            it = qmap.find("disablePurge");
            if (it != qmap.end()) disablePurge = it->second;
        }

        bool b_discard = (discard == "true");
        bool b_discardKeyHistory = (discardKeyHistory == "true");
        bool b_disablePurge = (disablePurge == "true");

        success = (!discard.empty() || !discardKeyHistory.empty() || !disablePurge.empty());

        if (success) {
            if (!discard.empty() && !discardKeyHistory.empty())
                success = !(b_discard && !b_discardKeyHistory); // it has no sense to try to keep history if whole data is discarded
        }

        if (success) {
            if (!discard.empty()) {
                getHttp2Server()->discardData(b_discard);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard data: %s", b_discard ? "true":"false"), ERT_FILE_LOCATION));
            }
            if (!discardKeyHistory.empty()) {
                getHttp2Server()->discardDataKeyHistory(b_discardKeyHistory);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard data key history: %s", b_discardKeyHistory ? "true":"false"), ERT_FILE_LOCATION));
            }
            if (!disablePurge.empty()) {
                getHttp2Server()->disablePurge(b_disablePurge);
                LOGWARNING(
                    ert::tracing::Logger::warning(ert::tracing::Logger::asString("Disable purge execution: %s", b_disablePurge ? "true":"false"), ERT_FILE_LOCATION);
                    if (!b_disablePurge && b_discardKeyHistory)
                    ert::tracing::Logger::warning(ert::tracing::Logger::asString("Purge execution will be limited as history is discarded"), ERT_FILE_LOCATION);
                    if (!b_disablePurge && b_discard)
                        ert::tracing::Logger::warning(ert::tracing::Logger::asString("Purge execution has no sense as no events will be stored"), ERT_FILE_LOCATION);
                    );
            }
        }
        else {
            ert::tracing::Logger::error("Cannot keep requests history if data storage is discarded", ERT_FILE_LOCATION);
        }
    }
    else if (pathSuffix == "files/configuration") {
        std::string readCache;

        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("readCache");
            if (it != qmap.end()) readCache = it->second;

            success = (readCache == "true" || readCache == "false");
        }

        if (success) {
            bool b_readCache = (readCache == "true");
            getFileManager()->enableReadCache(b_readCache);
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("File read cache: %s", b_readCache ? "true":"false"), ERT_FILE_LOCATION));
        }
    }

    statusCode = success ? 200:400;
}

void MyAdminHttp2Server::receive(const std::uint64_t &receptionId,
                                 const nghttp2::asio_http2::server::request&
                                 req,
                                 const std::string &requestBody,
                                 const std::chrono::microseconds &receptionTimestampUs,
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
        ss << "ADMIN REQUEST RECEIVED | Method: " << method
           << " | Headers: " << ert::http2comm::headersAsString(req.header())
           << " | Uri: " << req.uri().scheme << "://" << req.uri().host << uriPath;
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

    // No operation provided:
    if (noPathSuffix) {
        receiveNOOP(statusCode, headers, responseBody);
        return;
    }

    // Methods supported:
    if (method == "DELETE") {
        receiveDELETE(pathSuffix, uriQuery, statusCode);
        headers.clear();
        return;
    }
    else if (method == "GET") {
        receiveGET(req.uri().scheme + std::string("://") + req.uri().host + uriPath /* schema $id */, pathSuffix, uriQuery, statusCode, headers, responseBody);
        return;
    }
    else if (method == "POST") {
        receivePOST(pathSuffix, requestBody, statusCode, headers, responseBody);
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
