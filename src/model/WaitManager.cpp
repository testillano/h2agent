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
#include <Vault.hpp>
#include <MockEvent.hpp>

namespace h2agent
{
namespace model
{

namespace {

// Expected value matching: scalars are compared by their text, so a barrier does
// not depend on how the value was stored ('?value=60' matches 60, "60" and 60.0
// written as text "60"); objects/arrays are compared as JSON.
std::string scalarText(const nlohmann::json &v) {
    return v.is_string() ? v.get<std::string>() : v.dump();
}

bool matchesTarget(const nlohmann::json &value, const nlohmann::json &target) {
    if (value.is_structured() || target.is_structured()) return value == target;
    if (value.is_null() || target.is_null()) return value == target;
    return scalarText(value) == scalarText(target);
}

}

bool WaitManager::waitForVault(const std::string &key, const nlohmann::json &targetValue,
                               unsigned int timeoutMs,
                               nlohmann::json &resultValue, nlohmann::json &previousValue) {

    timeoutMs = std::min(timeoutMs, MAX_TIMEOUT_MS);

    std::unique_lock<std::mutex> lock(mutex_);

    if (active_waiters_ >= MAX_WAITERS) {
        return false; // caller should return 429
    }

    if (shutdown_) return false;

    // Capture initial value
    previousValue = nullptr;
    if (vault_) vault_->tryGet(key, previousValue);

    // Check if already satisfied
    if (!targetValue.is_null() && matchesTarget(previousValue, targetValue)) {
        resultValue = previousValue;
        return true;
    }

    active_waiters_++;
    bool result = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
        if (shutdown_) return true;
        resultValue = nullptr;
        bool exists = vault_ && vault_->tryGet(key, resultValue);
        if (targetValue.is_null()) {
            return exists && resultValue != previousValue;
        }
        return exists && matchesTarget(resultValue, targetValue);
    });
    active_waiters_--;

    if (shutdown_) return false;

    if (!result) {
        resultValue = nullptr;
        if (vault_) vault_->tryGet(key, resultValue);
    }

    return result;
}

void WaitManager::addMatch(EventWaiter &w, const DataKey &key, const std::shared_ptr<MockEvent> &event) {
    if (w.seen.insert(event.get()).second) w.matched.emplace_back(key, event);
}

void WaitManager::evaluate(EventWaiter &w) {
    const std::uint64_t needed = w.spec->eventNumber ? w.spec->eventNumber : 1;
    w.done = (w.matched.size() >= needed);
}

WaitOutcome WaitManager::waitForEvent(const EventWaitSpec &spec, const std::function<std::vector<KeyedEvent>()> &scan,
                                      unsigned int timeoutMs, std::optional<EventWaitMatch> &match) {

    timeoutMs = std::min(timeoutMs, MAX_TIMEOUT_MS);
    EventWaiter w;
    w.spec = &spec;

    // Register before scanning: an event loaded meanwhile is seen by onEvent()
    // (and deduplicated if the scan also finds it).
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return WaitOutcome::Shutdown;
        if (active_waiters_ >= MAX_WAITERS) return WaitOutcome::Full;
        active_waiters_++;
        event_waiters_.push_back(&w);
        event_waiters_count_++;
    }

    std::vector<KeyedEvent> stored;
    if (!spec.onlyNew && scan) stored = scan();  // without the lock: loads are not blocked

    std::unique_lock<std::mutex> lock(mutex_);
    for (const auto &e : stored) addMatch(w, e.first, e.second);
    evaluate(w);
    cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return shutdown_ || w.done; });

    event_waiters_.erase(std::remove(event_waiters_.begin(), event_waiters_.end(), &w), event_waiters_.end());
    event_waiters_count_--;
    active_waiters_--;

    if (shutdown_) return WaitOutcome::Shutdown;
    if (!w.done) return WaitOutcome::Timeout;

    // Chronological order (stored events and notified ones may interleave):
    std::stable_sort(w.matched.begin(), w.matched.end(), [](const KeyedEvent &a, const KeyedEvent &b) {
        return a.second->getReceptionTimestampUs() < b.second->getReceptionTimestampUs();
    });
    std::size_t index = 0;
    if (spec.eventNumber) index = spec.eventNumber - 1;  // the n-th matching event
    else if (!spec.onlyNew) index = w.matched.size() - 1;  // state condition: the latest
    // onlyNew: the first event loaded after the request
    match.emplace(EventWaitMatch{w.matched[index].first, w.matched[index].second, spec.onlyNew ? 0 : index + 1});
    return WaitOutcome::Met;
}

void WaitManager::onEvent(const DataKey &key, const std::shared_ptr<MockEvent> &event) {
    if (event_waiters_count_.load() == 0 || !event) return;  // load path: no lock without data waiters

    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto *w : event_waiters_) {
            if (w->done || !w->spec->matches(key, *event)) continue;
            addMatch(*w, key, event);
            evaluate(*w);
            wake |= w->done;
        }
    }
    if (wake) cv_.notify_all();
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
