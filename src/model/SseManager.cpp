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

#include <SseManager.hpp>

#include <ert/tracing/Logger.hpp>

namespace h2agent
{
namespace model
{

uint64_t SseManager::addConnection(std::unordered_set<std::string> keys, write_cb_t writeCb) {
    uint64_t id = next_id_.fetch_add(1);
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[id] = {std::move(keys), std::move(writeCb)};
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("SSE connection added (id=%lu, keys=%zu, total=%zu)", id, connections_[id].keys.size(), connections_.size()), ERT_FILE_LOCATION));
    return id;
}

void SseManager::removeConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(id);
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("SSE connection removed (id=%lu, remaining=%zu)", id, connections_.size()), ERT_FILE_LOCATION));
}

void SseManager::notify(const std::string &key, const nlohmann::json &value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connections_.empty()) return;
    }

    // Format SSE event only if there are active connections
    nlohmann::json data;
    data["key"] = key;
    data["value"] = value;
    std::string event = "event: vault-set\ndata: " + data.dump() + "\n\n";

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[id, conn] : connections_) {
        if (conn.keys.empty() || conn.keys.count(key)) {
            try {
                conn.writeCb(event);
            } catch (...) {
                LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("SSE write failed for connection %lu", id), ERT_FILE_LOCATION));
            }
        }
    }
}

size_t SseManager::activeConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

}
}
