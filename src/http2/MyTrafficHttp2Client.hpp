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
#include <ert/http2comm/Http2Client.hpp>
#include <MockClientData.hpp>

namespace h2agent
{
namespace model
{
class Configuration;
class GlobalVariable;
class FileManager;
class AdminData;
}

namespace http2
{

class MyTrafficHttp2Client : public ert::http2comm::Http2Client
{
    std::uint64_t general_unique_client_sequence_{};

    model::AdminData *admin_data_{};
    model::MockClientData *mock_client_events_data_{};

    // metrics:
    ert::metrics::Metrics *metrics_{};

    ert::metrics::counter_t *observed_requests_processed_counter_{};
    ert::metrics::counter_t *observed_requests_failed_counter_{};

    ert::metrics::counter_t *observed_responses_processed_counter_{};
    ert::metrics::counter_t *observed_responses_timeout_counter_{};

public:

    MyTrafficHttp2Client(const std::string &name, const std::string& host, const std::string& port, bool secure = false, boost::asio::io_service *timersIoService = nullptr /*temporary */):
        ert::http2comm::Http2Client(name, host, port, secure),
        admin_data_(nullptr) {

        mock_client_events_data_ = new model::MockClientData();

        //general_unique_client_sequence_ = 1;
    }

    ~MyTrafficHttp2Client() {
        delete (mock_client_events_data_);
    }

    /**
    * Enable metrics
    *
    *  @param metrics Optional metrics object to compute counters
    */
    void enableMyMetrics(ert::metrics::Metrics *metrics);

    //void responseTimeout();

    void setAdminData(model::AdminData *p) {
        admin_data_ = p;
    }
    model::AdminData *getAdminData() const {
        return admin_data_;
    }

    void setMockClientData(model::MockClientData *p) {
        mock_client_events_data_ = p;
    }
    model::MockClientData *getMockClientData() const {
        return mock_client_events_data_;
    }

    const std::uint64_t &getGeneralUniqueClientSequence() const {
        return general_unique_client_sequence_;
    }

    void incrementGeneralUniqueClientSequence() {
        general_unique_client_sequence_++;
    }
};

}
}

