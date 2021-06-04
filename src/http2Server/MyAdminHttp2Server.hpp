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

#pragma once

#include <vector>
#include <string>
#include <memory>

#include <ert/http2comm/Http2Server.hpp>

#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>

namespace h2agent
{
namespace model
{
class AdminData;
class MockRequestData;
}

namespace http2server
{

class MyAdminHttp2Server: public ert::http2comm::Http2Server
{
    h2agent::jsonschema::JsonSchema server_matching_schema_;
    h2agent::jsonschema::JsonSchema server_provision_schema_;

    model::AdminData *admin_data_;
    model::MockRequestData *mock_request_data_;

    std::string getPathSuffix(const std::string &uriPath) const; // important: leading slash is omitted on extraction
    void buildJsonResponse(bool result, const std::string &response, std::string &jsonResponse) const;

    void receiveEMPTY(unsigned int& statusCode, std::string &responseBody) const;
    void receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, std::string &responseBody) const;
    void receiveGET(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode, std::string &responseBody) const;
    void receiveDELETE(const std::string &pathSuffix, unsigned int& statusCode) const;


public:
    MyAdminHttp2Server(size_t workerThreads);

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

    model::AdminData *getAdminData() const {
        return admin_data_;
    }
    void setMockRequestData(model::MockRequestData *d) {
        mock_request_data_ = d;
    }
    model::MockRequestData *getMockRequestData() const {
        return mock_request_data_;
    }
};

}
}

