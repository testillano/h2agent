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

#ifndef H2AGENT_HTTP2SERVER_MYADMINHTTP2SERVER_H_
#define H2AGENT_HTTP2SERVER_MYADMINHTTP2SERVER_H_

#include <vector>
#include <string>
#include <memory>

#include <ert/http2comm/Http2Server.hpp>

#include <JsonSchema.hpp>

namespace h2agent
{
namespace http2server
{

class MyAdminHttp2Server: public ert::http2comm::Http2Server
{
    h2agent::jsonschema::JsonSchema schema_;

public:
    //schema_(schema),
    MyAdminHttp2Server(const nlohmann::json & jsonSchema, size_t maxWorkerThreads = -1) :
        ert::http2comm::Http2Server("AdminHttp2Server", maxWorkerThreads),
        schema_(jsonSchema) {};

    bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods);

    bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req);

    bool checkHeaders(const nghttp2::asio_http2::server::request& req);

    void receive(const nghttp2::asio_http2::server::request& req,
                 const std::string& requestBody,
                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                 std::string& responseBody);
};

}
}

#endif
