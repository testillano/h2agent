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

#include <MyAdminHttp2Server.hpp>
#include <MyTrafficHttp2Server.hpp>
//#include <MyTrafficHttp2Client.hpp>

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

bool statusCodeOK(int statusCode) {
    return (statusCode >= ert::http2comm::ResponseCode::OK && statusCode < ert::http2comm::ResponseCode::MULTIPLE_CHOICES); // [200,300)
}

MyAdminHttp2Server::MyAdminHttp2Server(const std::string &name, size_t workerThreads):
    ert::http2comm::Http2Server(name, workerThreads, workerThreads, nullptr) {

    admin_data_ = new model::AdminData();

    // Client data storage
    client_data_ = true;
    client_data_key_history_ = true;
    purge_execution_ = true;
}

MyAdminHttp2Server::~MyAdminHttp2Server()
{
    delete (admin_data_);
}

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
} // LCOV_EXCL_LINE

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
} // LCOV_EXCL_LINE

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
    statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
}

int MyAdminHttp2Server::serverMatching(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "server-matching operation; ";

    h2agent::model::AdminServerMatchingData::LoadResult loadResult = admin_data_->loadServerMatching(configurationObject);
    int result = ((loadResult == h2agent::model::AdminServerMatchingData::Success) ? ert::http2comm::ResponseCode::CREATED:ert::http2comm::ResponseCode::BAD_REQUEST); // 201 or 400

    if (loadResult == h2agent::model::AdminServerMatchingData::Success) {
        log += "valid schema and matching data received";

        // Warn in case previous server provisions exists:
        if (admin_data_->getServerProvisionData().size() != 0)
            LOGWARNING(
            if (admin_data_->getServerProvisionData().size() != 0) {
            ert::tracing::Logger::warning("There are current server provisions: remove/update them to avoid unexpected behavior (matching must be configured firstly !)", ERT_FILE_LOCATION);
            }
        );
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
    int result = ((loadResult == h2agent::model::AdminServerProvisionData::Success) ? ert::http2comm::ResponseCode::CREATED:ert::http2comm::ResponseCode::BAD_REQUEST); // 201 or 400

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminServerProvisionData::Success) {
        log += (isArray ? "valid schemas and server provisions data received":"valid schema and server provision data received");
    }
    else if (loadResult == h2agent::model::AdminServerProvisionData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminServerProvisionData::BadContent) {
        log += (isArray ? "detected one invalid server provision data received":"invalid server provision data received");
    }

    return result;
}

int MyAdminHttp2Server::clientEndpoint(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "client-endpoint operation; ";

    h2agent::model::AdminClientEndpointData::LoadResult loadResult = admin_data_->loadClientEndpoint(configurationObject, common_resources_);
    int result = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
    if (loadResult == h2agent::model::AdminClientEndpointData::Success) {
        result = ert::http2comm::ResponseCode::CREATED; // 201
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::Accepted) {
        result = ert::http2comm::ResponseCode::ACCEPTED; // 202
    }

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminClientEndpointData::Success || loadResult == h2agent::model::AdminClientEndpointData::Accepted) {
        log += (isArray ? "valid schemas and client endpoints data received":"valid schema and client endpoint data received");
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminClientEndpointData::BadContent) {
        log += (isArray ? "detected one invalid client endpoint data received":"invalid client endpoint data received");
    }

    return result;
}

int MyAdminHttp2Server::clientProvision(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "client-provision operation; ";

    h2agent::model::AdminClientProvisionData::LoadResult loadResult = admin_data_->loadClientProvision(configurationObject, common_resources_);
    int result = ((loadResult == h2agent::model::AdminClientProvisionData::Success) ? 201:400);

    bool isArray = configurationObject.is_array();
    if (loadResult == h2agent::model::AdminClientProvisionData::Success) {
        log += (isArray ? "valid schemas and client provisions data received":"valid schema and client provision data received");
    }
    else if (loadResult == h2agent::model::AdminClientProvisionData::BadSchema) {
        log += (isArray ? "detected one invalid schema":"invalid schema");
    }
    else if (loadResult == h2agent::model::AdminClientProvisionData::BadContent) {
        log += (isArray ? "detected one invalid client provision data received":"invalid client provision data received");
    }

    return result;
}

int MyAdminHttp2Server::globalVariable(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "global-variable operation; ";

    int result = getGlobalVariable()->loadJson(configurationObject) ? ert::http2comm::ResponseCode::CREATED:ert::http2comm::ResponseCode::BAD_REQUEST; // 201 or 400
    log += (statusCodeOK(result) ? "valid schema and global variables received":"invalid schema");

    return result;
}

int MyAdminHttp2Server::schema(const nlohmann::json &configurationObject, std::string& log) const
{
    log = "schema operation; ";

    h2agent::model::AdminSchemaData::LoadResult loadResult = admin_data_->loadSchema(configurationObject);
    int result = ((loadResult == h2agent::model::AdminSchemaData::Success) ? ert::http2comm::ResponseCode::CREATED:ert::http2comm::ResponseCode::BAD_REQUEST); // 201 or 400

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
        else if (pathSuffix == "client-provision") {
            statusCode = clientProvision(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "schema") {
            statusCode = schema(requestJson, jsonResponse_response);
        }
        else if (pathSuffix == "global-variable") {
            statusCode = globalVariable(requestJson, jsonResponse_response);
        }
        else {
            statusCode = ert::http2comm::ResponseCode::NOT_IMPLEMENTED; // 501
            jsonResponse_response = "unsupported operation";
        }
    }
    else
    {
        // Response data:
        statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
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

    // composed path suffixes
    std::smatch matches;
    static std::regex clientProvisionId("^client-provision/(.*)", std::regex::optimize);


    if (pathSuffix == "server-matching/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getServerMatchingData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server-provision/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getServerProvisionData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "client-endpoint/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getClientEndpointData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "client-provision/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getClientProvisionData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "schema/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = admin_data_->getSchemaData().getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server-data/summary") {
        std::string maxKeys = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("maxKeys");
            if (it != qmap.end()) maxKeys = it->second;
        }

        responseBody = getMockServerData()->summary(maxKeys);
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "client-data/summary") {
        std::string maxKeys = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("maxKeys");
            if (it != qmap.end()) maxKeys = it->second;
        }

        responseBody = getMockClientData()->summary(maxKeys);
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "global-variable") {
        std::string name = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("name");
            if (it != qmap.end()) name = it->second;
            if (name.empty()) {
                statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
                responseBody = "";
            }
        }
        if (statusCode != ert::http2comm::ResponseCode::BAD_REQUEST) { // 400
            if (name.empty()) {
                responseBody = getGlobalVariable()->asJsonString();
                statusCode = ((responseBody == "{}") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // response body will be emptied by nghttp2 when status code is 204 (No Content)
            }
            else {
                bool exists;
                responseBody = getGlobalVariable()->getValue(name, exists);
                statusCode = (exists ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT); // response body will be emptied by nghttp2 when status code is 204 (No Content)
            }
        }
    }
    else if (pathSuffix == "global-variable/schema") {
        // Add the $id field dynamically (full URI including scheme/host)
        nlohmann::json jsonSchema = getGlobalVariable()->getSchema().getJson();
        jsonSchema["$id"] = uri;
        responseBody = jsonSchema.dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server-matching") {
        responseBody = admin_data_->getServerMatchingData().getJson().dump();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server-provision") {
        bool ordered = (admin_data_->getServerMatchingData().getAlgorithm() == h2agent::model::AdminServerMatchingData::RegexMatching);
        responseBody = admin_data_->getServerProvisionData().asJsonString(ordered);
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "server-provision/unused") {
        bool ordered = (admin_data_->getServerMatchingData().getAlgorithm() == h2agent::model::AdminServerMatchingData::RegexMatching);
        responseBody = admin_data_->getServerProvisionData().asJsonString(ordered, true /*unused*/);
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "client-endpoint") {
        responseBody = admin_data_->getClientEndpointData().asJsonString();
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "client-provision") {
        responseBody = admin_data_->getClientProvisionData().asJsonString();
        statusCode = ((responseBody == "[]") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "client-provision/unused") {
        responseBody = admin_data_->getClientProvisionData().asJsonString(true /*unused*/);
        statusCode = ((responseBody == "[]") ? 204:200); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (std::regex_match(pathSuffix, matches, clientProvisionId)) { // client-provision/<client provision id>
        triggerClientOperation(matches.str(1), queryParams, statusCode);
        bool result = statusCodeOK(statusCode);
        responseBody = buildJsonResponse(result, (result ? "operation processed":"operation failed"));
    }
    else if (pathSuffix == "schema") {
        responseBody = admin_data_->getSchemaData().asJsonString();
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "server-data") {
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string eventNumber = "";
        std::string eventPath = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("eventNumber");
            if (it != qmap.end()) eventNumber = it->second;
            it = qmap.find("eventPath");
            if (it != qmap.end()) eventPath = it->second;
        }

        bool validQuery = false;
        try { // dump could throw exception if something weird is done (binary data with non-binary content-type)
            h2agent::model::EventLocationKey elkey(requestMethod, requestUri, eventNumber, eventPath);
            responseBody = getMockServerData()->asJsonString(elkey, validQuery);
        }
        catch (const std::exception& e)
        {
            //validQuery = false; // will be ert::http2comm::ResponseCode::OK (200) with empty result (corner case)
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
        statusCode = validQuery ? ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK):ert::http2comm::ResponseCode::BAD_REQUEST; // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "client-data") {
        std::string clientEndpointId = "";
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string eventNumber = "";
        std::string eventPath = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("clientEndpointId");
            if (it != qmap.end()) clientEndpointId = it->second;
            it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("eventNumber");
            if (it != qmap.end()) eventNumber = it->second;
            it = qmap.find("eventPath");
            if (it != qmap.end()) eventPath = it->second;
        }

        bool validQuery = false;
        try { // dump could throw exception if something weird is done (binary data with non-binary content-type)
            h2agent::model::EventLocationKey elkey(clientEndpointId, requestMethod, requestUri, eventNumber, eventPath);
            responseBody = getMockClientData()->asJsonString(elkey, validQuery);
        }
        catch (const std::exception& e)
        {
            //validQuery = false; // will be ert::http2comm::ResponseCode::OK (200) with empty result (corner case)
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
        statusCode = validQuery ? ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK):ert::http2comm::ResponseCode::BAD_REQUEST; // response body will be emptied by nghttp2 when status code is 204 (No Content)
    }
    else if (pathSuffix == "configuration") {
        responseBody = getConfiguration()->asJsonString();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server/configuration") {
        responseBody = getHttp2Server()->configurationAsJsonString();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "server-data/configuration") {
        responseBody = getHttp2Server()->dataConfigurationAsJsonString();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "client-data/configuration") {
        responseBody = clientDataConfigurationAsJsonString();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "files/configuration") {
        responseBody = getFileManager()->configurationAsJsonString();
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else if (pathSuffix == "files") {
        responseBody = getFileManager()->asJsonString();
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // 204 or 200
    }
    else if (pathSuffix == "udp-sockets") {
        responseBody = getSocketManager()->asJsonString();
        statusCode = ((responseBody == "[]") ? ert::http2comm::ResponseCode::NO_CONTENT:ert::http2comm::ResponseCode::OK); // 204 or 200
    }
    else if (pathSuffix == "logging") {
        responseBody = ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel());
        headers.emplace("content-type", nghttp2::asio_http2::header_value{"text/html"});
        jsonContent = false;
        statusCode = ert::http2comm::ResponseCode::OK; // 200
    }
    else {
        statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
        responseBody = buildJsonResponse(false, std::string("invalid operation '") + pathSuffix + std::string("'"));
    }

    if (jsonContent) headers.emplace("content-type", nghttp2::asio_http2::header_value{"application/json"});
}

void MyAdminHttp2Server::receiveDELETE(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const
{
    LOGDEBUG(ert::tracing::Logger::debug("receiveDELETE()",  ERT_FILE_LOCATION));

    if (pathSuffix == "server-provision") {
        statusCode = (admin_data_->clearServerProvisions() ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT);  // 200 or 204
    }
    else if (pathSuffix == "client-endpoint") {
        statusCode = (admin_data_->clearClientEndpoints() ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT);  // 200 or 204
    }
    else if (pathSuffix == "client-provision") {
        statusCode = (admin_data_->clearClientProvisions() ? 200:204);
    }
    else if (pathSuffix == "schema") {
        statusCode = (admin_data_->clearSchemas() ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT);  // 200 or 204
    }
    else if (pathSuffix == "server-data") {
        bool serverDataDeleted = false;
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string eventNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("eventNumber");
            if (it != qmap.end()) eventNumber = it->second;
        }

        h2agent::model::EventKey ekey(requestMethod, requestUri, eventNumber);
        bool success = getMockServerData()->clear(serverDataDeleted, ekey);

        statusCode = (success ? (serverDataDeleted ? ert::http2comm::ResponseCode::OK /*200*/:ert::http2comm::ResponseCode::NO_CONTENT /*204*/):ert::http2comm::ResponseCode::BAD_REQUEST /*400*/);
    }
    else if (pathSuffix == "client-data") {
        bool clientDataDeleted = false;
        std::string clientEndpointId = "";
        std::string requestMethod = "";
        std::string requestUri = "";
        std::string eventNumber = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("clientEndpointId");
            if (it != qmap.end()) clientEndpointId = it->second;
            it = qmap.find("requestMethod");
            if (it != qmap.end()) requestMethod = it->second;
            it = qmap.find("requestUri");
            if (it != qmap.end()) requestUri = it->second;
            it = qmap.find("eventNumber");
            if (it != qmap.end()) eventNumber = it->second;
        }

        h2agent::model::EventKey ekey(clientEndpointId, requestMethod, requestUri, eventNumber);
        bool success = getMockClientData()->clear(clientDataDeleted, ekey);

        statusCode = (success ? (clientDataDeleted ? ert::http2comm::ResponseCode::OK /*200*/:ert::http2comm::ResponseCode::NO_CONTENT /*204*/):ert::http2comm::ResponseCode::BAD_REQUEST /*400*/);
    }
    else if (pathSuffix == "global-variable") {
        bool globalVariableDeleted = false;
        std::string name = "";
        if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
            std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
            auto it = qmap.find("name");
            if (it != qmap.end()) name = it->second;
            if (name.empty()) {
                statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
            }
        }
        if (statusCode != ert::http2comm::ResponseCode::BAD_REQUEST) { // 400
            if (name.empty()) {
                statusCode = (getGlobalVariable()->clear() ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT); // 204
            }
            else {
                bool exists;
                getGlobalVariable()->removeVariable(name, exists);
                statusCode = (exists ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::NO_CONTENT); // 200 or 204
            }
        }
    }
    else {
        statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
    }
}

void MyAdminHttp2Server::receivePUT(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode)
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
    else if (pathSuffix == "server-data/configuration" || pathSuffix == "client-data/configuration") {

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

        bool serverMode = (pathSuffix == "server-data/configuration"); // true: server mode, false: client mode
        const char *mode = (serverMode ? "server":"client");

        if (success) {
            if (!discard.empty()) {
                if (serverMode) getHttp2Server()->discardData(b_discard);
                else discardClientData(b_discard);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard %s-data: %s", mode, b_discard ? "true":"false"), ERT_FILE_LOCATION));
            }
            if (!discardKeyHistory.empty()) {
                if (serverMode) getHttp2Server()->discardDataKeyHistory(b_discardKeyHistory);
                else discardClientDataKeyHistory(b_discardKeyHistory);
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Discard %s-data key history: %s", mode, b_discardKeyHistory ? "true":"false"), ERT_FILE_LOCATION));
            }
            if (!disablePurge.empty()) {
                if (serverMode) getHttp2Server()->disablePurge(b_disablePurge);
                else disableClientPurge(b_disablePurge);
                LOGWARNING(
                    ert::tracing::Logger::warning(ert::tracing::Logger::asString("Disable %s purge execution: %s", mode, b_disablePurge ? "true":"false"), ERT_FILE_LOCATION);
                    if (!b_disablePurge && b_discardKeyHistory)
                    ert::tracing::Logger::warning(ert::tracing::Logger::asString("Purge execution will be limited as history is discarded for %s data", mode), ERT_FILE_LOCATION);
                    if (!b_disablePurge && b_discard)
                        ert::tracing::Logger::warning(ert::tracing::Logger::asString("Purge execution has no sense as no events will be stored at %s storage", mode), ERT_FILE_LOCATION);
                    );
            }
        }
        else {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Cannot keep requests history if %s data storage is discarded", mode), ERT_FILE_LOCATION);
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

    statusCode = success ? ert::http2comm::ResponseCode::OK:ert::http2comm::ResponseCode::BAD_REQUEST; // 200 or 400
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
    std::string uriQuery = req.uri().raw_query; // parameter values may be percent-encoded

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

void MyAdminHttp2Server::triggerClientOperation(const std::string &clientProvisionId, const std::string &queryParams, unsigned int& statusCode) const {

    std::string inState = DEFAULT_ADMIN_PROVISION_STATE; // administrative operation triggers "initial" provisions by default
    std::string sequenceBegin = "";
    std::string sequenceEnd = "";
    std::string rps = "";
    std::string repeat = "";

    if (!queryParams.empty()) { // https://stackoverflow.com/questions/978061/http-get-with-request-body#:~:text=Yes.,semantic%20meaning%20to%20the%20request.
        std::map<std::string, std::string> qmap = h2agent::model::extractQueryParameters(queryParams);
        auto it = qmap.find("inState");
        if (it != qmap.end()) inState = it->second;
        it = qmap.find("sequenceBegin");
        if (it != qmap.end()) sequenceBegin = it->second;
        it = qmap.find("sequenceEnd");
        if (it != qmap.end()) sequenceEnd = it->second;
        it = qmap.find("rps");
        if (it != qmap.end()) rps = it->second;
        it = qmap.find("repeat");
        if (it != qmap.end()) repeat = it->second;
    }

    // Admin provision:
    const h2agent::model::AdminClientProvisionData & provisionData = getAdminData()->getClientProvisionData();
    std::shared_ptr<h2agent::model::AdminClientProvision> provision = provisionData.find(inState, clientProvisionId);

    if (!provision) {
        statusCode = ert::http2comm::ResponseCode::NOT_FOUND; // 404
        return;
    }

    statusCode = ert::http2comm::ResponseCode::OK; // 200
    if (!sequenceBegin.empty() || !sequenceEnd.empty() || !rps.empty() || !repeat.empty()) {
        if (provision->updateTriggering(sequenceBegin, sequenceEnd, rps, repeat)) {
            statusCode = ert::http2comm::ResponseCode::ACCEPTED; // 202; "sender" operates asynchronously
        }
        else {
            statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
            return;
        }
    }

    // OPTIONAL SCHEMAS VALIDATION
    const h2agent::model::AdminSchemaData & schemaData = getAdminData()->getSchemaData();
    std::shared_ptr<h2agent::model::AdminSchema> requestSchema(nullptr);
    std::shared_ptr<h2agent::model::AdminSchema> responseSchema(nullptr);
    std::string requestSchemaId = provision->getRequestSchemaId();
    if (!requestSchemaId.empty()) {
        requestSchema = schemaData.find(requestSchemaId);
        LOGWARNING(
            if (!requestSchema) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for outgoing message: VALIDATION will be IGNORED", requestSchemaId.c_str()), ERT_FILE_LOCATION);
        );
    }
    std::string responseSchemaId = provision->getResponseSchemaId();
    if (!responseSchemaId.empty()) {
        responseSchema = schemaData.find(responseSchemaId);
        LOGWARNING(
            if (!responseSchema) ert::tracing::Logger::warning(ert::tracing::Logger::asString("Missing schema '%s' referenced in provision for incoming message: VALIDATION will be IGNORED", responseSchemaId.c_str()), ERT_FILE_LOCATION);
        );
    }

    // Process provision (before sending)
    provision->employ(); // set provision as employed:
    std::string requestMethod{};
    std::string requestUri{};
    std::string requestBody{};
    nghttp2::asio_http2::header_map requestHeaders;

    std::string outState{};
    unsigned int requestDelayMs{};
    unsigned int requestTimeoutMs{};

    std::string error{}; // error detail (empty when all is OK)

    provision->transform(requestMethod, requestUri, requestBody, requestHeaders, outState, requestSchema, requestDelayMs, requestTimeoutMs, error);
    LOGDEBUG(
        ert::tracing::Logger::debug(ert::tracing::Logger::asString("Request method: %s", requestMethod.c_str()), ERT_FILE_LOCATION);
        ert::tracing::Logger::debug(ert::tracing::Logger::asString("Request uri: %s", requestUri.c_str()), ERT_FILE_LOCATION);
        ert::tracing::Logger::debug(ert::tracing::Logger::asString("Request body: %s", requestBody.c_str()), ERT_FILE_LOCATION);
    );

    if (error.empty()) {
        const h2agent::model::AdminClientEndpointData & clientEndpointData = getAdminData()->getClientEndpointData();
        std::shared_ptr<h2agent::model::AdminClientEndpoint> clientEndpoint(nullptr);
        clientEndpoint = clientEndpointData.find(provision->getClientEndpointId());
        if (clientEndpoint) {
            if (clientEndpoint->getPermit()) {
                clientEndpoint->connect();
                ert::http2comm::Http2Client::response response = clientEndpoint->getClient()->send(requestMethod, requestUri, requestBody, requestHeaders, std::chrono::milliseconds(requestTimeoutMs));
                clientEndpoint->getClient()->incrementGeneralUniqueClientSequence();

                // Store event:
                if (client_data_) {
                    h2agent::model::DataKey dataKey(provision->getClientEndpointId(), requestMethod, requestUri);
                    h2agent::model::DataPart responseBodyDataPart(response.body);
                    getMockClientData()->loadEvent(dataKey, provision->getClientProvisionId(), inState, outState, response.sendingUs, response.receptionUs, response.statusCode, requestHeaders, response.headers, requestBody, responseBodyDataPart, clientEndpoint->getGeneralUniqueClientSequence(), provision->getSeq(), requestDelayMs, requestTimeoutMs, client_data_key_history_ /* history enabled */);
                }
            }
            else {
                error = "Referenced client endpoint is disabled (not permitted)";
            }
        }
        else {
            error = "Referenced client endpoint is not provisioned";
        }
    }
    else {
        statusCode = ert::http2comm::ResponseCode::BAD_REQUEST; // 400
    }
}

std::string MyAdminHttp2Server::clientDataConfigurationAsJsonString() const {
    nlohmann::json result;

    result["storeEvents"] = client_data_;
    result["storeEventsKeyHistory"] = client_data_key_history_;
    result["purgeExecution"] = purge_execution_;

    return result.dump();
}

}
}
