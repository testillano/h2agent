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

#include <ert/metrics/Metrics.hpp>

#include <ert/http2comm/Http2Server.hpp>

namespace h2agent
{
namespace model
{
class AdminData;
class MockRequestData;
}

namespace http2server
{

class MyHttp2Server;

class MyAdminHttp2Server: public ert::http2comm::Http2Server
{
    model::AdminData *admin_data_;
    h2agent::http2server::MyHttp2Server *http2_server_; // used to set server-data configuration (discard contexts and/or history)

    std::string getPathSuffix(const std::string &uriPath) const; // important: leading slash is omitted on extraction
    std::string buildJsonResponse(bool responseResult, const std::string &responseBody) const;

    void receiveEMPTY(unsigned int& statusCode, std::string &responseBody) const;
    void receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, std::string &responseBody) const;
    void receiveGET(const std::string &uri, const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode, std::string &responseBody) const;
    void receiveDELETE(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const;
    void receivePUT(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const;


public:
    MyAdminHttp2Server(size_t workerThreads);
    ~MyAdminHttp2Server();

    bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods);

    bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req);

    bool checkHeaders(const nghttp2::asio_http2::server::request& req);

    void receive(const nghttp2::asio_http2::server::request& req,
                 const std::string& requestBody,
                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                 std::string& responseBody, unsigned int &responseDelayMs);

    model::AdminData *getAdminData() const {
        return admin_data_;
    }

    void setHttp2Server(h2agent::http2server::MyHttp2Server* ptr) {
        http2_server_ = ptr;
    }
    h2agent::http2server::MyHttp2Server *getHttp2Server(void) const {
        return http2_server_;
    }

    bool schema(const nlohmann::json &configurationObject, std::string& log) const;
    bool serverMatching(const nlohmann::json &configurationObject, std::string& log) const;
    bool serverProvision(const nlohmann::json &configurationObject, std::string& log) const;
    bool serverDataGlobal(const nlohmann::json &configurationObject, std::string& log) const;
};

}
}

