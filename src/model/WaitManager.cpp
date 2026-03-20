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

#include <algorithm>
#include <chrono>

#include <WaitManager.hpp>
#include <GlobalVariable.hpp>

namespace h2agent
{
namespace model
{

bool WaitManager::waitForGlobalVariable(const std::string &key, const std::string &targetValue,
                                        unsigned int timeoutMs,
                                        std::string &resultValue, std::string &previousValue) {

    timeoutMs = std::min(timeoutMs, MAX_TIMEOUT_MS);

    std::unique_lock<std::mutex> lock(mutex_);

    if (active_waiters_ >= MAX_WAITERS) {
        return false; // caller should return 429
    }

    if (shutdown_) return false;

    // Capture initial value
    previousValue.clear();
    if (global_variable_) global_variable_->tryGet(key, previousValue);

    // Check if already satisfied
    if (!targetValue.empty() && previousValue == targetValue) {
        resultValue = previousValue;
        return true;
    }

    active_waiters_++;
    bool result = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
        if (shutdown_) return true;
        resultValue.clear();
        bool exists = global_variable_ && global_variable_->tryGet(key, resultValue);
        if (targetValue.empty()) {
            return exists && resultValue != previousValue;
        }
        return exists && resultValue == targetValue;
    });
    active_waiters_--;

    if (shutdown_) return false;

    if (!result) {
        resultValue.clear();
        if (global_variable_) global_variable_->tryGet(key, resultValue);
    }

    return result;
}

void WaitManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
    cv_.notify_all();
}

void WaitManager::notify() {
    cv_.notify_all();
}

size_t WaitManager::activeWaiters() const {
    return active_waiters_;
}

bool WaitManager::isFull() const {
    return active_waiters_ >= MAX_WAITERS;
}

}
}
