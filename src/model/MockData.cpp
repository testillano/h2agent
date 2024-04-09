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


namespace h2agent
{
namespace model
{

std::shared_ptr<MockEventsHistory> MockData::getEvents(const DataKey &dataKey, bool &maiden) {

    auto it = get(dataKey.getKey());
    if (it != end()) {
        return it->second;
    }
    maiden = true;
    return std::make_shared<MockEventsHistory>(dataKey);
}

bool MockData::clear(bool &somethingDeleted, const EventKey &ekey)
{
    bool result = true;
    somethingDeleted = false;

    if (ekey.empty()) {
        write_guard_t guard(rw_mutex_);
        somethingDeleted = (map_.size() > 0);
        map_.clear();
        return result;
    }

    if (!ekey.checkSelection())
        return false;

    write_guard_t guard(rw_mutex_);

    auto it = get(ekey.getKey());
    if (it == end())
        return true; // nothing found to be removed

    // Check event number:
    if (ekey.hasNumber()) {
        if (!ekey.validNumber())
            return false;
        somethingDeleted = it->second->removeEvent(ekey.getUNumber(), ekey.reverse());
        if (it->second->size() == 0) remove(it); // remove key when history is dropped (https://github.com/testillano/h2agent/issues/53).
    }
    else {
        somethingDeleted = true;
        remove(it); // remove key
    }

    return result;
}

std::string MockData::asJsonString(const EventLocationKey &elkey, bool &validQuery) const {

    //validQuery = false;

    if (elkey.empty()) {
        validQuery = true;
        return ((size() != 0) ? getJson().dump() : "[]"); // server data is shown as an array
    }

    if (!elkey.checkSelection()) {
        validQuery = false;
        return "[]";
    }

    validQuery = true;

    read_guard_t guard(rw_mutex_);

    auto it = get(elkey.getKey());
    if (it == end())
        return "[]"; // nothing found to be built

    // Check event number:
    if (elkey.hasNumber()) {
        if (!elkey.validNumber()) {
            validQuery = false;
            return "[]";
        }

        auto ptr = it->second->getEvent(elkey.getUNumber(), elkey.reverse());
        if (ptr) {
            return ptr->getJson(elkey.getPath()).dump();
        }
        else return "[]";
    }
    else {
        return it->second->getJson().dump();
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

    read_guard_t guard(rw_mutex_);
    for (auto it = begin(); it != end(); it++) {
        size_t historySize = it->second->size();
        totalEvents += historySize;
        if (displayedKeys >= u_maxKeys) continue;

        it->second->getKey().keyToJson(key);
        key["amount"] = (std::uint64_t)historySize;
        result["displayedKeys"]["list"].push_back(key);
        displayedKeys += 1;
    };
    if (displayedKeys > 0) result["displayedKeys"]["amount"] = (std::uint64_t)displayedKeys;
    result["totalEvents"] = (std::uint64_t)totalEvents;

    return result.dump();
}

std::shared_ptr<MockEvent> MockData::getEvent(const EventKey &ekey) const {

    if (!ekey.complete()) {
        LOGDEBUG(ert::tracing::Logger::debug("Must provide everything to address the event (data key and event number)", ERT_FILE_LOCATION));
        return nullptr;
    }

    LOGDEBUG(ert::tracing::Logger::debug(ekey.asString(), ERT_FILE_LOCATION));

    read_guard_t guard(rw_mutex_);

    auto it = get(ekey.getKey());
    if (it == end())
        return nullptr; // nothing found

    if (!ekey.validNumber())
        return nullptr;

    return (it->second->getEvent(ekey.getUNumber(), ekey.reverse()));
}

nlohmann::json MockData::getJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->getJson());
    };

    return result;
}

bool MockData::findLastRegisteredRequestState(const DataKey &key, std::string &state) const {

    read_guard_t guard(rw_mutex_);

    auto it = get(key.getKey());
    if (it != end()) {
        state = it->second->getLastRegisteredRequestState(); // by design, a key always contains at least one history event (https://github.com/testillano/h2agent/issues/53).
        if (state.empty()) { // unprovisioned event must be understood as missing (ignore register)
            state = DEFAULT_ADMIN_PROVISION_STATE;
        }

        return true;
    }

    state = DEFAULT_ADMIN_PROVISION_STATE;
    return false;
}

}
}

