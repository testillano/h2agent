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

#include <ert/tracing/Logger.hpp>

#include <MockEventsHistory.hpp>


namespace h2agent
{
namespace model
{

void MockEventsHistory::loadEvent(std::shared_ptr<MockEvent> event, bool historyEnabled) {

    write_guard_t guard(rw_mutex_);

    if (!historyEnabled && events_.size() != 0) {
        events_[0] = event; // overwrite with this latest reception
    }
    else {
        events_.push_back(event);
    }
}

bool MockEventsHistory::removeEvent(std::uint64_t eventNumber, bool reverse) {

    write_guard_t guard(rw_mutex_);

    if (events_.size() == 0 || eventNumber == 0) return false;
    if (eventNumber > events_.size()) return false;

    events_.erase(events_.begin() + (reverse ? (events_.size() - eventNumber):(eventNumber - 1)));

    return true;
}

std::shared_ptr<MockEvent> MockEventsHistory::getEvent(std::uint64_t eventNumber, bool reverse) const {

    read_guard_t guard(rw_mutex_);

    if (events_.size() == 0) return nullptr;
    if (eventNumber == 0) return nullptr;

    if (eventNumber > events_.size()) return nullptr;

    if (reverse) {
        return *(events_.begin() + (events_.size() - eventNumber));
        //return *(std::prev(events_.end())); // this would be the last
    }

    return *(events_.begin() + (eventNumber - 1));
}

nlohmann::json MockEventsHistory::getJson() const {
    nlohmann::json result;

    data_key_.keyToJson(result);

    read_guard_t guard(rw_mutex_);
    for (auto it = events_.begin(); it != events_.end(); it ++) {
        result["events"].push_back((*it)->getJson());
    }

    return result;
}

const std::string &MockEventsHistory::getLastRegisteredRequestState() const {
    read_guard_t guard(rw_mutex_);
    // By design, there are no keys without history (at least 1 exists and the key is removed if it is finally deleted).
    // Then, back() is a valid iterator (https://github.com/testillano/h2agent/issues/53).
    return events_.back()->getState();
}

}
}

