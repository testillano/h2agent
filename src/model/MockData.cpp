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

#include <string>

#include <ert/tracing/Logger.hpp>

#include <MockData.hpp>
#include <JsonDump.hpp>


namespace h2agent
{
namespace model
{

std::vector<KeyedEvent> MockData::collectEvents(const std::function<bool(const DataKey &, const MockEvent &)> &matches) const {

    std::vector<KeyedEvent> result;
    forEach([&](const mock_events_key_t&, const std::shared_ptr<MockEventsHistory> &history) {
        const DataKey &key = history->getKey();
        read_guard_t guard(history->getMutex());
        for (const auto &event : history->getEvents()) {
            if (matches(key, *event)) result.emplace_back(key, event);
        }
    });
    return result;
}

std::shared_ptr<MockEventsHistory> MockData::getEvents(const DataKey &dataKey, bool &maiden) {

    bool exists;
    auto result = get(dataKey.getKey(), exists);
    if (exists) {
        return result;
    }
    maiden = true;
    return std::make_shared<MockEventsHistory>(dataKey);
}

bool MockData::clear(bool &somethingDeleted, const EventKey &ekey)
{
    bool result = true;
    somethingDeleted = false;

    if (ekey.empty()) {
        somethingDeleted = (size() > 0);
        Map::clear();
        return result;
    }

    if (!ekey.checkSelection())
        return false;

    bool exists;
    auto key = ekey.getKey();
    auto value = get(key, exists);
    if (!exists)
        return true; // nothing found to be removed

    // Check event number:
    bool aux{};
    if (ekey.hasNumber()) {
        if (!ekey.validNumber())
            return false;
        somethingDeleted = value->removeEvent(ekey.getUNumber(), ekey.reverse());
        if (value->size() == 0) remove(key, aux); // remove key when history is dropped (https://github.com/testillano/h2agent/issues/53).
    }
    else {
        somethingDeleted = true;
        remove(key, aux); // remove key
    }

    return result;
}

std::string MockData::asJsonString(const EventLocationKey &elkey, bool &validQuery) const {

    //validQuery = false;

    if (elkey.empty()) {
        validQuery = true;
        return ((size() != 0) ? h2agent::model::dumpJson(getJson()) : "[]"); // server data is shown as an array
    }

    if (!elkey.checkSelection()) {
        validQuery = false;
        return "[]";
    }

    validQuery = true;

    bool exists;
    auto result = get(elkey.getKey(), exists);
    if (!exists)
        return "[]"; // nothing found to be built

    // Check event number:
    if (elkey.hasNumber()) {
        if (!elkey.validNumber()) {
            validQuery = false;
            return "[]";
        }

        auto ptr = result->getEvent(elkey.getUNumber(), elkey.reverse());
        if (ptr) {
            return h2agent::model::dumpJson(ptr->getJson(elkey.getPath()));
        }
        else return "[]";
    }
    else {
        return h2agent::model::dumpJson(result->getJson());
    }

    return "[]";
}

std::string MockData::summary(const std::string &maxKeys) const {
    nlohmann::json result;

    result["totalKeys"] = (std::uint64_t)size();

    bool negative = false;
    std::uint64_t u_maxKeys = 0;
    if (!h2agent::model::string2uint64andSign(maxKeys, u_maxKeys, negative)) {
        u_maxKeys = std::numeric_limits<uint64_t>::max();
    }

    size_t totalEvents = 0;
    size_t historySize = 0;
    size_t displayedKeys = 0;
    nlohmann::json key;

    this->forEach([&](const KeyType& k, const ValueType& value) {
        size_t historySize = value->size();
        totalEvents += historySize;
        if (displayedKeys < u_maxKeys) {
            value->getKey().keyToJson(key);
            key["amount"] = (std::uint64_t)historySize;
            result["displayedKeys"]["list"].push_back(key);
            displayedKeys += 1;
        }
    });

    if (displayedKeys > 0) result["displayedKeys"]["amount"] = (std::uint64_t)displayedKeys;
    result["totalEvents"] = (std::uint64_t)totalEvents;

    return h2agent::model::dumpJson(result);
}

std::shared_ptr<MockEvent> MockData::getEvent(const EventKey &ekey) const {

    if (!ekey.complete()) {
        LOGDEBUG(ert::tracing::Logger::debug("Must provide everything to address the event (data key and event number)", ERT_FILE_LOCATION));
        return nullptr;
    }

    LOGDEBUG(ert::tracing::Logger::debug(ekey.asString(), ERT_FILE_LOCATION));

    bool exists{};
    auto result = get(ekey.getKey(), exists);
    if (!exists)
        return nullptr; // nothing found

    if (!ekey.validNumber())
        return nullptr;

    return (result->getEvent(ekey.getUNumber(), ekey.reverse()));
}

nlohmann::json MockData::getJson() const {

    nlohmann::json result;

    this->forEach([&](const KeyType& k, const ValueType& value) {
        result.push_back(value->getJson());
    });

    return result;
}

bool MockData::findLastRegisteredRequestState(const DataKey &key, std::string &state) const {

    bool exists{};
    auto result = get(key.getKey(), exists);

    if (exists) {
        state = result->getLastRegisteredRequestState(); // by design, a key always contains at least one history event (https://github.com/testillano/h2agent/issues/53).
        if (state.empty()) { // unprovisioned event must be understood as missing (ignore register)
            state = DEFAULT_ADMIN_PROVISION_STATE;
        }

        return true;
    }

    state = DEFAULT_ADMIN_PROVISION_STATE;
    return false;
}

bool MockData::findLastRegisteredRequestState(const DataKey &key, std::string &state, Variables &chainVariables) const {

    bool exists{};
    auto result = get(key.getKey(), exists);

    if (exists) {
        state = result->getLastRegisteredRequestState();
        if (state.empty()) {
            state = DEFAULT_ADMIN_PROVISION_STATE;
        }
        // Scoped variables live along an outState chain: back in the initial state
        // the chain has ended (or never started), so a new one starts empty. Without
        // this, every request to a key inherited the variables of the previous one
        // (e.g. a stale '<var>.fail' validation report).
        if (state == DEFAULT_ADMIN_PROVISION_STATE) chainVariables.clear();
        else chainVariables = result->getChainVariables();

        return true;
    }

    state = DEFAULT_ADMIN_PROVISION_STATE;
    chainVariables.clear();
    return false;
}

void MockData::storeChainVariables(const DataKey &key, const Variables &chainVariables) {

    bool exists{};
    auto result = get(key.getKey(), exists);

    if (exists) {
        result->setChainVariables(chainVariables);
    }
}

}
}

