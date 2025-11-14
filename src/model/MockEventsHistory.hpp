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
#include <memory>

#include <MockEvent.hpp>
#include <common.hpp>
#include <keys.hpp>


namespace h2agent
{
namespace model
{

class MockEventsHistory
{
    std::vector<std::shared_ptr<MockEvent>> events_{};
    mutable mutex_t rw_mutex_{}; // specific mutex to protect events_

protected:
    DataKey data_key_;

public:

    /**
    * Constructor
    *
    * @param dataKey Events key ([client enpoint id,] method & uri).
    */
    MockEventsHistory(const DataKey &dataKey) : data_key_(dataKey) {;}

    // setters:

    /**
    * Loads event into history
    *
    * @param event Event to load
    * @param historyEnabled Events complete history storage
    *
    * Memory must be reserved by the user
    */
    void loadEvent(std::shared_ptr<MockEvent> event, bool historyEnabled);

    /**
     * Removes vector item for a given position
     * We could also access from the tail (reverse chronological order)

     * @param eventNumber Event history number (1..N) to filter selection.
     * @param reverse Reverse the order to get the event from the tail instead the head.
     *
     * @return Boolean about if something was deleted
     */
    bool removeEvent(std::uint64_t eventNumber, bool reverse);

    // getters:

    /**
     * Gets the mock key event in specific position (last by default)
     *
     * @param eventNumber Event history number (1..N) to filter selection.
     * Value '0' is not accepted, and null will be returned in this case.
     * @param reverse Reverse the order to get the event from the tail instead the head.
     *
     * @return mock request pointer
     * @see size()
     */
    std::shared_ptr<MockEvent> getEvent(std::uint64_t eventNumber, bool reverse) const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;

    /**
     * Gets the mock events data key
     *
     * @return Mock event data key
     */
    const DataKey &getKey() const {
        return data_key_;
    }

    /** Number of events
    *
    * @return Events list size
    */
    size_t size() const {
        return events_.size();
    }

    /** Last registered request state
    *
    * @return Last registered request state
    */
    const std::string &getLastRegisteredRequestState() const;
};

}
}

