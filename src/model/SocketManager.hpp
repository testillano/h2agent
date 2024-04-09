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

#include <nlohmann/json.hpp>

#include <ert/metrics/Metrics.hpp>

#include <Map.hpp>
#include <SafeSocket.hpp>


namespace ert
{
namespace metrics
{
class Metrics;
}
}

namespace h2agent
{
namespace model
{

/**
 * This class stores a list of safe files.
 */
class SocketManager : public Map<std::string, std::shared_ptr<SafeSocket>>
{
    mutable mutex_t rw_mutex_{};
    boost::asio::io_context *io_context_{};

    // metrics (will be passed to SafeSocket):
    ert::metrics::Metrics *metrics_{};

    ert::metrics::counter_t *observed_open_operation_counter_{};
    ert::metrics::counter_t *observed_write_operation_counter_{};
    ert::metrics::counter_t *observed_delayed_write_operation_counter_{};
    ert::metrics::counter_t *observed_instant_write_operation_counter_{};
    ert::metrics::counter_t *observed_error_open_operation_counter_{};

public:
    /**
    * Socket manager class
    *
    * @param timersIoContext timers io context needed to schedule delayed write operations.
    * If you never schedule write operations (@see write) it may be 'nullptr'.
    * @param metrics underlaying reference for SafeSocket in order to compute prometheus metrics
    * about I/O operations. It may be 'nullptr' if no metrics are enabled.
    *
    * @see SafeSocket
    */
    SocketManager(boost::asio::io_context *timersIoContext = nullptr, ert::metrics::Metrics *metrics = nullptr) : io_context_(timersIoContext), metrics_(metrics) {;}
    ~SocketManager() = default;

    /**
    * Set metrics reference
    *
    * @param metrics Optional metrics object to compute counters
    * @param source Source label for prometheus metrics
    */
    void enableMetrics(ert::metrics::Metrics *metrics, const std::string &source);


    /** incrementObservedOpenOperationCounter */
    void incrementObservedOpenOperationCounter();

    /** incrementObservedWriteOperationCounter */
    void incrementObservedWriteOperationCounter();

    /** incrementObservedDelayedWriteOperationCounter */
    void incrementObservedDelayedWriteOperationCounter();

    /** incrementObservedInstantWriteOperationCounter */
    void incrementObservedInstantWriteOperationCounter();

    /** incrementObservedErrorOpenOperationCounter */
    void incrementObservedErrorOpenOperationCounter();

    /**
     * Write socket
     *
     * @param path path file to write. Can be relative (to execution directory) or absolute.
     * @param data data string to write.
     * @param writeDelayUs delay for write operation.
     * Zero value means that no planned write is scheduled, so the socket is written instantly.
     */
    void write(const std::string &path, const std::string &data, unsigned int writeDelayUs);

    /** Clears list
     *
     * @return Boolean about success of operation (something removed, nothing removed: already empty)
     */
    bool clear();

    /**
     * Builds json document for class configuration
     *
     * @return Json object
     */
    nlohmann::json getConfigurationJson() const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;

    /**
     * Json string representation for class information (json object)
     *
     * @return Json string representation ('[]' for empty object).
     */
    std::string asJsonString() const;
};

}
}

