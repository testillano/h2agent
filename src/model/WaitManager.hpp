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
#include <mutex>
#include <condition_variable>
#include <cstddef>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

class Vault;

/**
 * Manages blocking wait (long-poll) for vault entry changes.
 *
 * Provides condition-variable-based synchronization so that external
 * orchestrators can block until a vault entry changes, eliminating
 * polling loops.
 */
class WaitManager
{
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t active_waiters_{};
    bool shutdown_{};

    Vault *vault_{};

public:
    static constexpr size_t MAX_WAITERS = 32;
    static constexpr unsigned int MAX_TIMEOUT_MS = 300000; // 5 minutes

    WaitManager() = default;
    ~WaitManager() = default;

    void setVault(Vault *p) { vault_ = p; }

    /** Signals all waiters to abort and prevents new waits. */
    void shutdown();

    /**
     * Blocks until a vault entry changes (any change or specific value).
     *
     * @param key Variable key to watch.
     * @param targetValue If non-empty, wait until variable equals this value.
     *                    If empty, wait for any change from current value.
     * @param timeoutMs Maximum wait time in milliseconds (capped at MAX_TIMEOUT_MS).
     * @param resultValue Filled with the variable value on return.
     * @param previousValue Filled with the value captured before waiting.
     *
     * @return true if condition met, false on timeout.
     *         Returns false if too many waiters (caller should use 429).
     */
    bool waitForVault(const std::string &key, const nlohmann::json &targetValue,
                               unsigned int timeoutMs,
                               nlohmann::json &resultValue, nlohmann::json &previousValue);

    /** Wakes all waiters (called after vault entry mutation). */
    void notify();

    /** Returns current number of active waiters. */
    size_t activeWaiters() const;

    /** Returns true if waiter limit reached. */
    bool isFull() const;
};

}
}
