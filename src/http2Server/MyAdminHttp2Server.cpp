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
#include <iomanip>
#include <errno.h>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

#include <MyAdminHttp2Server.hpp>

#include <AdminData.hpp>

namespace h2agent
{
namespace http2server
{

MyAdminHttp2Server::MyAdminHttp2Server(size_t workerThreads):
    ert::http2comm::Http2Server("AdminHttp2Server", workerThreads),
    server_matching_schema_(h2agent::adminSchemas::server_matching),
    server_provision_schema_(h2agent::adminSchemas::server_provision),
    mock_data_(nullptr) {

    data_ = new model::AdminData();
}

//const std::pair<int, std::string> JSON_SCHEMA_VALIDATION(
//    ert::http2comm::ResponseCode::BAD_REQUEST,
//    "JSON_SCHEMA_VALIDATION");


bool MyAdminHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    allowedMethods = {"POST", "GET"};
    return (req.method() == "POST" || req.method() == "GET" || req.method() == "DELETE");
}

bool MyAdminHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    return (req.method() == "POST" || req.method() == "DELETE");
}


bool MyAdminHttp2Server::checkHeaders(const
                                      nghttp2::asio_http2::server::request& req)
{
    // Don't check headers for GET and DELETE:
    if (req.method() == "GET" || req.method() == "DELETE") {
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

    size_t apiPathSize = getApiPath().size(); // /provision/v1
    size_t uriPathSize = uriPath.size(); // /provision/v1<suffix>

    // Special case
    if (uriPathSize <= apiPathSize) return result; // indeed, it should not be lesser, as API & VERSION is already checked

    result = uriPath.substr(apiPathSize + 1);
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("result1 %s", result.c_str()),  ERT_FILE_LOCATION));

    if (result.back() == '/') result.pop_back(); // normalize by mean removing last slash (if exists)
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("result2 %s", result.c_str()),  ERT_FILE_LOCATION));

    return result;
}

void MyAdminHttp2Server::buildJsonResponse(bool result, const std::string &response, std::string &jsonResponse) const
{
    std::stringstream ss;
    ss << R"({ "result":")" << (result ? "true":"false") << R"(", "response": )" << std::quoted(response) << R"( })";
    jsonResponse = ss.str();
}

void MyAdminHttp2Server::receiveEMPTY(unsigned int& statusCode, std::string &responseBody) const
{
    // Response document:
    // {
    //   "result":"<true or false>",
    //   "response":"<additional information>"
    // }
    buildJsonResponse(false, "no operation provided", responseBody);
    statusCode = 400;
}

void MyAdminHttp2Server::receiveDELETE(const std::string &pathSuffix, unsigned int& statusCode) const
{
    if (pathSuffix == "server_provision") {
        data_->clearProvisions();
        statusCode = 204;
    }
    else {
        statusCode = 400;
    }
}

void MyAdminHttp2Server::receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, std::string &responseBody) const
{
    bool jsonResponse_result = false;
    std::string jsonResponse_response;

    // Admin schema validation:
    nlohmann::json requestJson;
    try {
        requestJson = nlohmann::json::parse(requestBody);
        LOGDEBUG(
            std::string msg("Json body received:\n\n");
            msg += requestJson.dump(4); // pretty print json body
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );


        if (pathSuffix == "server_matching") {

            jsonResponse_response = "server_matching operation; ";

            if (!server_matching_schema_.validate(requestJson)) {
                statusCode = 400;
                jsonResponse_response += "invalid schema";
            }
            else if (!data_->loadMatching(requestJson)) {
                statusCode = 400;
                jsonResponse_response += "invalid matching data received";
            }
            else {
                statusCode = 201;
                jsonResponse_result = true;
                jsonResponse_response += "valid schema and matching data received";
            }
        }
        else if (pathSuffix == "server_provision") {

            jsonResponse_response = "server_provision operation; ";

            if (!server_provision_schema_.validate(requestJson)) {
                statusCode = 400;
                jsonResponse_response += "invalid schema";
            }
            else if (!data_->loadProvision(requestJson)) {
                statusCode = 400;
                jsonResponse_response += "invalid provision data received";
            }
            else {
                statusCode = 201;
                jsonResponse_result = true;
                jsonResponse_response += "valid schema and provision data received";
            }
        }
        else {
            statusCode = 501;
            jsonResponse_response = "unsupported operation";
        }
    }
    catch (nlohmann::json::parse_error& e)
    {
        /*
        std::stringstream ss;
        ss << "Json body parse error: " << e.what() << '\n'
           << "exception id: " << e.id << '\n'
           << "byte position of error: " << e.byte << std::endl;
        ert::tracing::Logger::error(ss.str(), ERT_FILE_LOCATION);
        */

        // Response data:
        statusCode = 400;
        jsonResponse_response = "failed to parse json from body request";
    }

    // Build json response body:
    buildJsonResponse(jsonResponse_result, jsonResponse_response, responseBody);
}

void MyAdminHttp2Server::receive(const nghttp2::asio_http2::server::request&
                                 req,
                                 const std::string& requestBody,
                                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                                 std::string& responseBody)
{
    LOGDEBUG(ert::tracing::Logger::debug("receive()",  ERT_FILE_LOCATION));

    // see uri_ref struct (https://nghttp2.org/documentation/asio_http2.h.html#asio-http2-h)
    std::string method = req.method();
    std::string uriPath = req.uri().path;
    //std::string queryParams = req.uri().raw_query;

    LOGDEBUG(
        std::string msg;
        msg = ert::tracing::Logger::asString("Method: %s; Uri Path: %s; Request Body: %s",
                method.c_str(),
                uriPath.c_str(),
                requestBody.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Get path suffix normalized:
    std::string pathSuffix = getPathSuffix(uriPath);
    bool noPathSuffix = pathSuffix.empty();
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("URI Path Suffix: %s", (noPathSuffix ? "<null>" : pathSuffix.c_str())), ERT_FILE_LOCATION));

    // Defaults
    responseBody.clear();

    // No operation provided:
    if (noPathSuffix) {
        receiveEMPTY(statusCode, responseBody);
        return;
    }

    // Methods supported:
    if (method == "DELETE") {
        receiveDELETE(pathSuffix, statusCode);
        return;
    }
    else if (method == "POST") {
        receivePOST(pathSuffix, requestBody, statusCode, responseBody);
        return;
    }
}

}
}
