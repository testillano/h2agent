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
#include <ert/http2comm/ResponseHeader.hpp>
#include <ert/http2comm/Http.hpp>

#include <MyAdminHttp2Server.hpp>

namespace h2agent
{
namespace http2server
{

const std::pair<int, std::string> JSON_SCHEMA_VALIDATION(
    ert::http2comm::ResponseCode::BAD_REQUEST,
    "JSON_SCHEMA_VALIDATION");


bool MyAdminHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    allowedMethods = {"POST", "GET"};
    return (req.method() == "POST" || req.method() == "GET");
}

bool MyAdminHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    return (req.method() == "POST");
}


bool MyAdminHttp2Server::checkHeaders(const
                                      nghttp2::asio_http2::server::request& req)
{
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


std::string MyAdminHttp2Server::getOperation(const std::string &uriPath) const
{
    std::string result = uriPath.substr(getApiPath().size() + 2 /* surrounding slashes */);
    if (result.back() == '/') result.pop_back(); // normalize by mean removing last slash (if exists)

    return result;
}

void MyAdminHttp2Server::buildJsonResponse(bool result, const std::string &response, std::string &jsonResponse) const
{
    std::stringstream ss;
    ss << R"({ "result":")" << (result ? "true":"false") << R"(", "response": )" << std::quoted(response) << R"( })";
    jsonResponse = ss.str();
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
    std::string queryParams = req.uri().raw_query;

    LOGDEBUG(
      std::string msg;
      msg = ert::tracing::Logger::asString("Method: %s; Uri Path: %s; Query parameters: %s; Request Body: %s",
                                           method.c_str(),
                                           uriPath.c_str(),
                                           (queryParams != "") ? queryParams.c_str():"<null>",
                                           requestBody.c_str());
      ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Response document:
    // {
    //   "result":"<true or false>",
    //   "response":"<additional information>"
    // }
    bool jsonResponse_result = false;
    std::string jsonResponse_response;

    // Admin schema validation:
    bool schemaIsValid = false;
    nlohmann::json requestJson;
    try {
        requestJson = nlohmann::json::parse(requestBody);
        LOGDEBUG(
            std::string msg("Json body received:\n\n");
            msg += requestJson.dump(4); // pretty print json body
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        // Extract operation from received URI path:
        std::string operation = getOperation(uriPath);
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Operation: %s", operation.c_str()), ERT_FILE_LOCATION));

        if( operation == "server_matching") {
            bool schemaIsValid = server_matching_schema_.validate(requestJson);

            // Store information
            //

            statusCode = schemaIsValid ? 201:400;
            jsonResponse_result = schemaIsValid;
            jsonResponse_response = "server_matching operation;";
            jsonResponse_response += schemaIsValid ? " valid":" invalid";
            jsonResponse_response += " schema";
        }
        else if (operation == "server_provision") {
            bool schemaIsValid = server_provision_schema_.validate(requestJson);

            // Store information
            //

            statusCode = schemaIsValid ? 201:400;
            jsonResponse_result = schemaIsValid;
            jsonResponse_response = "server_provision operation;";
            jsonResponse_response += schemaIsValid ? " valid":" invalid";
            jsonResponse_response += " schema";
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

    //
    // DELAY EXAMPLE ///////////////////////////////////////////////////////////
    //std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

}
}
