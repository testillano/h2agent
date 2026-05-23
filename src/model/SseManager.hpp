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

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

/**
 * Manages Server-Sent Events (SSE) connections for vault event streaming.
 *
 * Each connection watches a set of vault keys. When a vault mutation occurs,
 * matching connections receive the event via their write callback.
 */
class SseManager
{
public:
    using write_cb_t = std::function<void(const std::string &)>;

    SseManager() = default;
    ~SseManager() = default;

    /**
     * Registers an SSE connection.
     *
     * @param keys Set of vault keys this connection watches (empty = all keys)
     * @param writeCb Callback to write SSE-formatted data to the stream
     * @return Connection ID for later removal
     */
    uint64_t addConnection(std::unordered_set<std::string> keys, write_cb_t writeCb);

    /**
     * Removes an SSE connection.
     */
    void removeConnection(uint64_t id);

    /**
     * Notifies all matching connections of a vault mutation.
     * Called by Vault on every write.
     */
    void notify(const std::string &key, const nlohmann::json &value);

    /** Returns number of active SSE connections. */
    size_t activeConnections() const;

private:
    struct Connection {
        std::unordered_set<std::string> keys; // empty = watch all
        write_cb_t writeCb;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Connection> connections_;
    std::atomic<uint64_t> next_id_{1};
};

}
}
