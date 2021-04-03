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


void MyAdminHttp2Server::receive(const nghttp2::asio_http2::server::request&
                                 req,
                                 const std::string& requestBody,
                                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                                 std::string& responseBody)
{
    LOGDEBUG(ert::tracing::Logger::debug("receive()",  ERT_FILE_LOCATION));

    // Check Path Prefix:
    /*
    struct uri_ref {
      std::string scheme;
      std::string host;
      // form after percent-encoding decoded
      std::string path;
      // original path, percent-encoded
      std::string raw_path;
      // original query, percent-encoded
      std::string raw_query;
      std::string fragment;
    };
    */

    // Admin schema validation:
    // JSON_SCHEMA_VALIDATION
    //

    //std::string method = req.method();
    //
    // DELAY EXAMPLE ///////////////////////////////////////////////////////////
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    statusCode = 200;
    responseBody = requestBody;

}

}
}
