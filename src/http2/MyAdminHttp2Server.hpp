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
#include <chrono>

#include <ert/metrics/Metrics.hpp>

#include <ert/http2comm/Http2Server.hpp>
#include <common.hpp>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{
class Configuration;
class GlobalVariable;
class FileManager;
class SocketManager;
class AdminData;
class MockServerData;
class MockClientData;
}

namespace http2
{

bool statusCodeOK(int statusCode);

class MyTrafficHttp2Server;

class MyAdminHttp2Server: public ert::http2comm::Http2Server
{
    model::AdminData *admin_data_{};
    model::common_resources_t common_resources_{}; // general configuration, global variables, file manager and mock server events data

    h2agent::http2::MyTrafficHttp2Server *http2_server_{}; // used to set server-data configuration (discard contexts and/or history)

    // Client data storage:
    bool client_data_{};
    bool client_data_key_history_{};
    bool purge_execution_{};

    std::string getPathSuffix(const std::string &uriPath) const; // important: leading slash is omitted on extraction
    std::string buildJsonResponse(bool responseResult, const std::string &responseBody) const;

    void receiveNOOP(unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const;
    void receivePOST(const std::string &pathSuffix, const std::string& requestBody, unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const;
    void receiveGET(const std::string &uri, const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode, nghttp2::asio_http2::header_map& headers, std::string &responseBody) const;
    void receiveDELETE(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode) const;
    void receivePUT(const std::string &pathSuffix, const std::string &queryParams, unsigned int& statusCode);

    void triggerClientOperation(const std::string &clientProvisionId, const std::string &queryParams, unsigned int& statusCode) const;

public:
    MyAdminHttp2Server(const std::string &name, size_t workerThreads);
    ~MyAdminHttp2Server();

    bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods);

    bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req);

    bool checkHeaders(const nghttp2::asio_http2::server::request& req);

    void receive(const std::uint64_t &receptionId,
                 const nghttp2::asio_http2::server::request& req,
                 const std::string &requestBody,
                 const std::chrono::microseconds &receptionTimestampUs,
                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                 std::string& responseBody, unsigned int &responseDelayMs);

    //bool receiveDataLen(const nghttp2::asio_http2::server::request& req); // virtual: default implementation (true) is required (request bodies are present in many operations).
    //bool preReserveRequestBody(); //virtual: default implementation (true) is acceptable for us (really, it does not matter).

    void setConfiguration(model::Configuration *p) {
        common_resources_.ConfigurationPtr = p;
    }
    model::Configuration *getConfiguration() const {
        return common_resources_.ConfigurationPtr;
    }

    void setGlobalVariable(model::GlobalVariable *p) {
        common_resources_.GlobalVariablePtr = p;
    }
    model::GlobalVariable *getGlobalVariable() const {
        return common_resources_.GlobalVariablePtr;
    }

    void setFileManager(model::FileManager *p) {
        common_resources_.FileManagerPtr = p;
    }
    model::FileManager *getFileManager() const {
        return common_resources_.FileManagerPtr;
    }

    void setSocketManager(model::SocketManager *p) {
        common_resources_.SocketManagerPtr = p;
    }
    model::SocketManager *getSocketManager() const {
        return common_resources_.SocketManagerPtr;
    }

    void setMetricsData(ert::metrics::Metrics *metrics, const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries, const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries, const std::string &applicationName) {
        common_resources_.MetricsPtr = metrics;
        common_resources_.ResponseDelaySecondsHistogramBucketBoundaries = responseDelaySecondsHistogramBucketBoundaries;
        common_resources_.MessageSizeBytesHistogramBucketBoundaries = messageSizeBytesHistogramBucketBoundaries;
        common_resources_.ApplicationName = applicationName;
    }

    model::AdminData *getAdminData() const {
        return admin_data_;
    }

    void setMockServerData(model::MockServerData *p) {
        common_resources_.MockServerDataPtr = p;
    }
    model::MockServerData *getMockServerData() const {
        return common_resources_.MockServerDataPtr;
    }

    void setMockClientData(model::MockClientData *p) {
        common_resources_.MockClientDataPtr = p;
    }
    model::MockClientData *getMockClientData() const {
        return common_resources_.MockClientDataPtr;
    }

    void setHttp2Server(h2agent::http2::MyTrafficHttp2Server* ptr) {
        http2_server_ = ptr;
    }

    h2agent::http2::MyTrafficHttp2Server *getHttp2Server(void) const {
        return http2_server_;
    }

    int serverMatching(const nlohmann::json &configurationObject, std::string& log) const;
    int serverProvision(const nlohmann::json &configurationObject, std::string& log) const;
    int clientEndpoint(const nlohmann::json &configurationObject, std::string& log) const;
    int clientProvision(const nlohmann::json &configurationObject, std::string& log) const;
    int globalVariable(const nlohmann::json &configurationObject, std::string& log) const;
    int schema(const nlohmann::json &configurationObject, std::string& log) const;

    // Client data storage:
    void discardClientData(bool discard = true) {
        client_data_ = !discard;
    }

    void discardClientDataKeyHistory(bool discard = true) {
        client_data_key_history_ = !discard;
    }

    void disableClientPurge(bool disable = true) {
        purge_execution_ = !disable;
    }

    std::string clientDataConfigurationAsJsonString() const;
};

}
}

