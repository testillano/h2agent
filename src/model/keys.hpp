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


#include <ert/tracing/Logger.hpp>

#include <functions.hpp>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{
// Mock events key:
typedef std::string mock_events_key_t;

/**
 * Events history key.
 */
class DataKey {

    std::string client_endpoint_id_{};
    std::string method_{};
    std::string uri_{};

    mock_events_key_t key_{};
    int nkeys_;

public:

    /**
     * Constructor for method & uri
     *
     * @param method Http method
     * @param uri Http Uri
     */
    DataKey(const std::string &method, const std::string &uri): method_(method), uri_(uri), nkeys_(2) {
        h2agent::model::calculateStringKey(key_, method_, uri_);
    }

    /**
     * Constructor for client endpoint id & method & uri
     *
     * @param clientEndpointId Client endpoint identifier
     * @param method Http method
     * @param uri Http Uri
     */
    DataKey(const std::string &clientEndpointId, const std::string &method, const std::string &uri): client_endpoint_id_(clientEndpointId), method_(method), uri_(uri), nkeys_(3) {
        h2agent::model::calculateStringKey(key_, client_endpoint_id_, method_, uri_);
    }

    /**
     * Gets number of used keys
     */
    int getNKeys() const {
        return nkeys_;
    }

    /**
     * Gets client endpoint id
     */
    const std::string &getClientEndpointId() const {
        return client_endpoint_id_;
    }

    /**
     * Gets method
     */
    const std::string &getMethod() const {
        return method_;
    }

    /**
     * Gets uri
     */
    const std::string &getUri() const {
        return uri_;
    }

    /**
     * Gets aggregated key
     */
    const mock_events_key_t &getKey() const {
        return key_;
    }

    /**
    * Boolean about if event key is empty (all components are empty)
    */
    bool empty() const {
        bool none = (method_.empty() && uri_.empty());
        if (getNKeys() == 3) {
            none &= (client_endpoint_id_.empty());
        }

        return none;
    }

    /**
    * Boolean about if event key is complete (all components are provided)
    */
    bool complete() const {
        bool all = (!method_.empty() && !uri_.empty());
        if (getNKeys() == 3) {
            all &= (!client_endpoint_id_.empty());
        }

        return all;
    }

    /**
    * Build data key into json document
    *
    * param doc Json document to update
    */
    void keyToJson(nlohmann::json &doc) const {
        doc["method"] =  method_;
        doc["uri"] = uri_;
        if (getNKeys() == 3) {
            doc["clientEndpointId"] = client_endpoint_id_;
        }
    }

    /**
     * Check key selection: all or nothing must be provided
     */
    bool checkSelection() const {
        LOGDEBUG(
        if(!complete() && !empty()) {
        if (getNKeys() == 2) {
                ert::tracing::Logger::debug("Key parts 'requestMethod' and 'requestUri' must be all provided or all missing", ERT_FILE_LOCATION);
            }
            else {
                ert::tracing::Logger::debug("Key parts 'clientEndpointId', 'requestMethod' and 'requestUri' must be all provided or all missing", ERT_FILE_LOCATION);
            }
        }
        );

        return (complete() || empty());
    }
};

/**
 * Event key
 *
 * Extends events history key with the event location within the history (1..N).
 */
class EventKey : public DataKey {

    std::string number_;
    bool valid_;
    std::uint64_t u_number_;
    bool reverse_;

public:

    /**
     * Constructor
     *
     * @param key Data key
     * @param number Position in events history (1..N)
     */
    EventKey(const DataKey &key, const std::string &number): DataKey(key), number_(number), valid_(false), u_number_(0), reverse_(false) {
        if (hasNumber()) valid_ = h2agent::model::string2uint64andSign(number_, u_number_, reverse_);
    }

    /**
     * Constructor
     * Used for method & uri
     *
     * @param method Http method
     * @param uri Http Uri
     * @param number Position in events history (1..N)
     */
    EventKey(const std::string &method, const std::string &uri, const std::string &number):
        DataKey(method, uri), number_(number), valid_(false), u_number_(0), reverse_(false) {
        if (hasNumber()) valid_ = h2agent::model::string2uint64andSign(number_, u_number_, reverse_);
    }

    /**
     * Constructor
     * Used for client endpoint & method & uri
     *
     * @param clientEndpointId Client endpoint identifier
     * @param method Http method
     * @param uri Http Uri
     * @param number Position in events history (1..N)
     */
    EventKey(const std::string &clientEndpointId, const std::string &method, const std::string &uri, const std::string &number):
        DataKey(clientEndpointId, method, uri), number_(number), valid_(false), u_number_(0), reverse_(false) {
        if (hasNumber()) valid_ = h2agent::model::string2uint64andSign(number_, u_number_, reverse_);
    }

    /**
     * Gets position in events history (1..N)
     */
    const std::string &getNumber() const {
        return number_;
    }

    /**
     * Returns boolean about if number is set or not
     */
    bool hasNumber() const {
        return !number_.empty();
    }

    /**
     * Returns boolean about if number is validated
     */
    bool validNumber() const {
        return valid_;
    }

    /**
     * Gets absolute part of the number
     */
    const std::uint64_t &getUNumber() const {
        return u_number_;
    }

    /**
     * Gets sign of the number which indicated reserve order of access
     */
    bool reverse() const {
        return reverse_;
    }

    /**
    * Boolean about if event key is empty (all components are empty) and also number is missing
    */
    bool empty() const {
        bool none = DataKey::empty();
        none &= (!hasNumber());

        return none;
    }

    /**
    * Boolean about if event key is complete (all components are provided) and also number exists
    */
    bool complete() const {
        bool all = DataKey::complete();
        all &= (hasNumber());

        return all;
    }

    /**
     * Check key selection: all or nothing must be provided.
     * Also, event number needs for key.
     */
    bool checkSelection() const {

        if (!DataKey::checkSelection()) return false;

        if (hasNumber() && getMethod().empty()) {
            LOGDEBUG(
            if (getNKeys() == 2) {
            ert::tracing::Logger::debug("Query parameter 'eventNumber' cannot be provided alone: requestMethod and requestUri are also needed", ERT_FILE_LOCATION);
            }
            else {
                ert::tracing::Logger::debug("Query parameter 'eventNumber' cannot be provided alone: clientEndpointId, requestMethod and requestUri are also needed", ERT_FILE_LOCATION);
            }
            );
            return false;
        }

        return true;
    }

    /**
     * Class string representation
     */
    std::string asString() const {
        std::string result{};
        if (getNKeys() == 3) {
            result = ert::tracing::Logger::asString("clientEndpointId: %s | ", getClientEndpointId().c_str());
        }
        result += ert::tracing::Logger::asString("requestMethod: %s | requestUri: %s | eventNumber: %s", getMethod().c_str(), getUri().c_str(), getNumber().c_str());
        return result;
    } // LCOV_EXCL_LINE
};

/**
 * Event location key
 *
 * Extends events key with the event path to access event subset.
 */
class EventLocationKey : public EventKey {

    std::string path_;

public:

    /**
     * Constructor
     * Used for method & uri
     *
     * @param method Http method
     * @param uri Http Uri
     * @param number Position in events history (1..N)
     * @param path Json path within event
     */
    EventLocationKey(const std::string &method, const std::string &uri, const std::string &number, const std::string &path):
        EventKey(method, uri, number), path_(path) {;}

    /**
     * Constructor
     * Used for client endpoint & method & uri
     *
     * @param clientEndpointId Client endpoint identifier
     * @param method Http method
     * @param uri Http Uri
     * @param number Position in events history (1..N)
     * @param path Json path within event
     */
    EventLocationKey(const std::string &clientEndpointId, const std::string &method, const std::string &uri, const std::string &number, const std::string &path):
        EventKey(clientEndpointId, method, uri, number), path_(path) {;}

    /**
     * Gets the path within the event
     */
    const std::string &getPath() const {
        return path_;
    }

//    /**
//     * Gets the content of the event path
//     */
//    const nlohmann::json &getLocation(const nlohmann::json &document) const {
//        nlohmann::json::json_pointer jptr(path_);
//        return document[jptr];
//    }
//
//    /**
//     * Boolean about if event key is empty (all components are empty) and also event number and path are missing
//     */
//    bool empty() const {
//       bool none = EventKey::empty();
//       none &= (path_.empty());
//
//       return none;
//    }
//
//    /**
//    * Boolean about if event key is complete (all components are provided) and also event number and path exist
//    */
//    bool complete() const {
//        bool all = EventKey::complete();
//        all &= (!path_.empty());
//
//        return all;
//    }

    /**
     * Check key selection: all or nothing must be provided.
     * Also, event number needs for key, and path needs for event number.
     */
    bool checkSelection() const {

        if (!EventKey::checkSelection()) return false;

        if (!getPath().empty() && !hasNumber()) {
            LOGDEBUG(
            if (getNKeys() == 2) {
            ert::tracing::Logger::error("Query parameter 'eventPath' cannot be provided alone: (requestMethod, requestUri and) eventNumber are also needed", ERT_FILE_LOCATION);
            }
            else {
                ert::tracing::Logger::error("Query parameter 'eventPath' cannot be provided alone: (clientEndpointId, requestMethod, requestUri and) eventNumber are also needed", ERT_FILE_LOCATION);
            }
            );
            return false;
        }

        return true;
    }
};

}
}

