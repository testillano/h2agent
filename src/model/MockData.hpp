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

#include <vector>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <MockEventsHistory.hpp>
#include <MockEvent.hpp>


namespace h2agent
{
namespace model
{


/**
 * This class stores the mock events.
 *
 * This is useful to post-verify the internal data (content and schema validation) on testing system.
 * Also, the last request for an specific key, is used to know the state which is used to get the
 * corresponding provision information.
 */
class MockData : public Map<mock_events_key_t, std::shared_ptr<MockEventsHistory>>
{
protected:
    // Get the events list for data key provided, and pass by reference a boolean to know if the list must be inaugurated
    std::shared_ptr<MockEventsHistory> getEvents(const DataKey &dataKey, bool &maiden);

public:
    MockData() {};
    ~MockData() = default;

    using KeyType = mock_events_key_t;
    using ValueType = std::shared_ptr<MockEventsHistory>;


    /** Clears internal data
     *
     * @param somethingDeleted boolean to know if something was deleted, by reference
     * @param ekey Event key given by data key and event history number (1..N) to filter selection
     * Empty event key will remove the whole history for data key provided.
     * Event number '-1' will select the latest event in events history.
     *
     * @return Boolean about success of operation (not related to the number of events removed)
     */
    bool clear(bool &somethingDeleted, const EventKey &ekey);

    /**
     * Json string representation for class information filtered (json array)
     *
     * @param elkey Event location key given by data key, event history number (1..N) and event path to filter selection
     * Event history number (1..N) is optional, but cannot be provided alone (needs data key).
     * If empty, whole history is selected for data key provided.
     * If provided '-1' (unsigned long long max), the latest event is selected.
     * Event Path within the event selected is optional, but cannot be provided alone (needs data key and event number).
     * @param validQuery Boolean result passed by reference.
     *
     * @return Json string representation ('[]' for empty array).
     * @warning This method may throw exception due to dump() when unexpected data is stored on json wrap: execute under try/catch block.

     */
    std::string asJsonString(const EventLocationKey &elkey, bool &validQuery) const;

    /**
     * Json string representation for class summary (total number of events and first/last/random keys).
     *
     * @param maxKeys Maximum number of keys to be displayed in the summary (protection for huge server data size). No limit by default.
     *
     * @return Json string representation.
     */
    std::string summary(const std::string &maxKeys = "") const;

    /**
     * Gets the mock server key event in specific position
     *
     * @param ekey Event key given by data key and event history number (1..N) to filter selection
     * Value '-1' (unsigned long long max) selects the latest event.
     * Value '0' is not accepted, and null will be returned in this case.
     *
     * All the parameters are mandatory to select the single request event.
     * If any is missing, nullptr is returned.
     *
     * @return mock request pointer
     * @see size()
     */
    std::shared_ptr<MockEvent> getEvent(const EventKey &ekey) const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;

    /**
     * Finds most recent mock context entry state.
     *
     * @param key Data key triggered.
     * @param state Request current state filled by reference.
     * If nothing found or no state (unprovisioned events), 'initial' will be set.
     *
     * @return Boolean about if the request is found or not
     */
    bool findLastRegisteredRequestState(const DataKey &key, std::string &state) const;
};

}
}

