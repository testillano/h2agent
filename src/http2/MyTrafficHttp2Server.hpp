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
#include <cstdint>
#include <atomic>
#include <chrono>

#include <boost/asio.hpp>

#include <JsonSchema.hpp>

#include <ert/metrics/Metrics.hpp>

#include <ert/http2comm/Http2Server.hpp>

namespace h2agent
{
namespace model
{
class MockServerEventsData;
class Configuration;
class GlobalVariable;
class FileManager;
class AdminData;
}

namespace http2
{

class MyTrafficHttp2Server: public ert::http2comm::Http2Server
{
    bool server_data_{};
    bool server_data_key_history_{};
    bool purge_execution_{};

    model::MockServerEventsData *mock_server_events_data_{};
    model::Configuration *configuration_{};
    model::GlobalVariable *global_variable_{};
    model::FileManager *file_manager_{};
    model::AdminData *admin_data_{};

    // metrics:
    ert::metrics::Metrics *metrics_{};

    ert::metrics::counter_t *observed_requests_processed_counter_{};
    ert::metrics::counter_t *observed_requests_unprovisioned_counter_{};
    ert::metrics::counter_t *purged_contexts_successful_counter_{};
    ert::metrics::counter_t *purged_contexts_failed_counter_{};

    std::atomic<int> max_busy_threads_{0};
    std::atomic<bool> receive_request_body_{true};
    std::atomic<bool> pre_reserve_request_body_{true};

public:
    MyTrafficHttp2Server(size_t workerThreads, boost::asio::io_service *timersIoService);
    ~MyTrafficHttp2Server();

    /**
    * Enable metrics
    *
    *  @param metrics Optional metrics object to compute counters
    */
    void enableMyMetrics(ert::metrics::Metrics *metrics);

    bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods);

    bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req);

    bool checkHeaders(const nghttp2::asio_http2::server::request& req);

    bool receiveDataLen(const nghttp2::asio_http2::server::request& req); //virtual
    bool preReserveRequestBody(); //virtual

    void receive(const std::uint64_t &receptionId,
                 const nghttp2::asio_http2::server::request& req,
                 const std::string &requestBody,
                 const std::chrono::microseconds &receptionTimestampUs,
                 unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                 std::string& responseBody, unsigned int &responseDelayMs);

    model::MockServerEventsData *getMockServerEventsData() const {
        return mock_server_events_data_;
    }

    void setConfiguration(model::Configuration *p) {
        configuration_ = p;
    }
    model::Configuration *getConfiguration() const {
        return configuration_;
    }

    void setGlobalVariable(model::GlobalVariable *p) {
        global_variable_ = p;
    }
    model::GlobalVariable *getGlobalVariable() const {
        return global_variable_;
    }

    void setFileManager(model::FileManager *p) {
        file_manager_ = p;
    }
    model::FileManager *getFileManager() const {
        return file_manager_;
    }

    void setAdminData(model::AdminData *p) {
        admin_data_ = p;
    }
    model::AdminData *getAdminData() const {
        return admin_data_;
    }

    std::string dataConfigurationAsJsonString() const;
    std::string configurationAsJsonString() const;

    void discardData(bool discard = true) {
        server_data_ = !discard;
    }

    void discardDataKeyHistory(bool discard = true) {
        server_data_key_history_ = !discard;
    }

    void disablePurge(bool disable = true) {
        purge_execution_ = !disable;
    }

    void setReceiveRequestBody(bool receive = true) {
        receive_request_body_.store(receive);
    }

    void setPreReserveRequestBody(bool preReserve = true) {
        pre_reserve_request_body_.store(preReserve);
    }
};

}
}

