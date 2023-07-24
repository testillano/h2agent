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

#include <memory>

#include <ert/tracing/Logger.hpp>

#include <SocketManager.hpp>

namespace h2agent
{
namespace model
{


void SocketManager::enableMetrics(ert::metrics::Metrics *metrics) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::counter_family_ref_t cf = metrics->addCounterFamily("UDPSocket_observed_operations_total", "H2agent udp socket operations");
        observed_open_operation_counter_ = &(cf.Add({{"operation", "open"}}));
        observed_write_operation_counter_ = &(cf.Add({{"operation", "write"}}));
        observed_delayed_write_operation_counter_ = &(cf.Add({{"operation", "delayedWrite"}}));
        observed_instant_write_operation_counter_ = &(cf.Add({{"operation", "instantWrite"}}));
        observed_error_open_operation_counter_ = &(cf.Add({{"success", "false"}, {"operation", "open"}}));
    }
}

void SocketManager::incrementObservedOpenOperationCounter() {
    if (metrics_) observed_open_operation_counter_->Increment();
}

void SocketManager::incrementObservedWriteOperationCounter() {
    if (metrics_) observed_write_operation_counter_->Increment();
}

void SocketManager::incrementObservedDelayedWriteOperationCounter() {
    if (metrics_) observed_delayed_write_operation_counter_->Increment();
}

void SocketManager::incrementObservedInstantWriteOperationCounter() {
    if (metrics_) observed_instant_write_operation_counter_->Increment();
}

void SocketManager::incrementObservedErrorOpenOperationCounter() {
    if (metrics_) observed_error_open_operation_counter_->Increment();
}

void SocketManager::write(const std::string &path, const std::string &data, unsigned int writeDelayUs) {

    std::shared_ptr<SafeSocket> safeSocket;

    auto it = get(path);
    if (it != end()) {
        safeSocket = it->second;
    }
    else {
        safeSocket = std::make_shared<SafeSocket>(this, path, io_service_);
        add(path, safeSocket);
    }

    safeSocket->write(data, writeDelayUs);
}

bool SocketManager::clear()
{
    bool result = (map_.size() > 0); // something deleted

    map_.clear(); // shared_ptr dereferenced too

    return result;
}

nlohmann::json SocketManager::getJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->getJson());
    };

    return result;
}

std::string SocketManager::asJsonString() const {

    return ((size() != 0) ? getJson().dump() : "[]");
}


}
}

