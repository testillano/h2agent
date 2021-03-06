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

#include <MockServerEventsData.hpp>
#include <AdminServerProvision.hpp>


namespace h2agent
{
namespace model
{

bool MockServerEventsData::string2uint64andSign(const std::string &input, std::uint64_t &output, bool &negative) const {

    bool result = false;

    if (!input.empty()) {
        negative = (input[0] == '-');

        try {
            output = std::stoull(negative ? input.substr(1):input);
            result = true;
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to unsigned long long integer%s: %s", input.c_str(), (negative ? " with negative sign":""), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
        }
    }

    return result;
}

bool MockServerEventsData::checkSelection(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber) const {

    // Bad request checkings:
    if (requestMethod.empty() != requestUri.empty()) {
        ert::tracing::Logger::error("If query parameter 'requestMethod' is provided, 'requestUri' shall be, and the opposite", ERT_FILE_LOCATION);
        return false;
    }

    if (!requestNumber.empty() && requestMethod.empty()) {
        ert::tracing::Logger::error("Query parameter 'requestNumber' cannot be provided alone: requestMethod and requestUri are also needed", ERT_FILE_LOCATION);
        return false;
    }

    return true;
}

void MockServerEventsData::loadRequest(const std::string &previousState, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body, const std::chrono::microseconds &receptionTimestampUs, unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs, bool historyEnabled, const std::string &virtualOriginComingFromMethod, const std::string &virtualOriginComingFromUri) {


    // Find MockServerKeyEvents
    std::shared_ptr<MockServerKeyEvents> requests(nullptr);

    mock_server_events_key_t key{};
    calculateMockServerKeyEventsKey(key, method, uri);

    write_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it != end()) {
        requests = it->second;
    }
    else {
        requests = std::make_shared<MockServerKeyEvents>();
    }

    requests->loadRequest(previousState, state, method, uri, headers, body, receptionTimestampUs, responseStatusCode, responseHeaders, responseBody, serverSequence, responseDelayMs, historyEnabled, virtualOriginComingFromMethod, virtualOriginComingFromUri);

    if (it == end()) add(key, requests); // push the key in the map:
}

bool MockServerEventsData::clear(bool &somethingDeleted, const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber)
{
    bool result = true;
    somethingDeleted = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        write_guard_t guard(rw_mutex_);
        somethingDeleted = (map_.size() > 0);
        map_.clear();
        return result;
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return false;

    mock_server_events_key_t key{};
    calculateMockServerKeyEventsKey(key, requestMethod, requestUri);

    write_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return true; // nothing found to be removed

    // Check request number:
    if (!requestNumber.empty()) {
        bool reverse = false;
        std::uint64_t u_requestNumber = 0;
        if (!string2uint64andSign(requestNumber, u_requestNumber, reverse))
            return false;

        somethingDeleted = it->second->removeMockServerKeyEvent(u_requestNumber, reverse);
        if (it->second->size() == 0) remove(it); // remove key when history is dropped (https://github.com/testillano/h2agent/issues/53).
    }
    else {
        somethingDeleted = true;
        remove(it); // remove key
    }

    return result;
}

std::string MockServerEventsData::asJsonString(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &validQuery) const {

    validQuery = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        validQuery = true;
        return ((size() != 0) ? getJson().dump() : "[]"); // server data is shown as an array
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return "[]";

    validQuery = true;

    mock_server_events_key_t key{};
    calculateMockServerKeyEventsKey(key, requestMethod, requestUri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return "[]"; // nothing found to be built

    // Check request number:
    if (!requestNumber.empty()) {
        bool reverse = false;
        std::uint64_t u_requestNumber = 0;
        if (!string2uint64andSign(requestNumber, u_requestNumber, reverse))
            return "[]";

        auto ptr = it->second->getMockServerKeyEvent(u_requestNumber, reverse);
        if (ptr) {
            return ptr->getJson().dump();
        }
        else return "[]";
    }
    else {
        return it->second->getJson().dump();
    }

    return "[]";
}

std::string MockServerEventsData::summary(const std::string &maxKeys) const {
    nlohmann::json result;

    result["totalKeys"] = (std::uint64_t)size();

    bool negative = false;
    std::uint64_t u_maxKeys = 0;
    if (!string2uint64andSign(maxKeys, u_maxKeys, negative)) {
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

        key["method"] = it->second->getMethod();
        key["uri"] = it->second->getUri();
        key["amount"] = (std::uint64_t)historySize;
        result["displayedKeys"]["list"].push_back(key);
        displayedKeys += 1;
    };
    if (displayedKeys > 0) result["displayedKeys"]["amount"] = (std::uint64_t)displayedKeys;
    result["totalEvents"] = (std::uint64_t)totalEvents;

    return result.dump();
}

std::shared_ptr<MockServerKeyEvent> MockServerEventsData::getMockServerKeyEvent(const std::string &requestMethod, const std::string &requestUri,const std::string &requestNumber) const {

    if (requestMethod.empty()) {
        LOGDEBUG(ert::tracing::Logger::debug("Empty 'requestMethod' provided: cannot retrieve the server data event", ERT_FILE_LOCATION));
        return nullptr;
    }

    if (requestUri.empty()) {
        LOGDEBUG(ert::tracing::Logger::debug("Empty 'requestUri' provided: cannot retrieve the server data event", ERT_FILE_LOCATION));
        return nullptr;
    }

    if (requestNumber.empty()) {
        LOGDEBUG(ert::tracing::Logger::debug("Empty 'requestNumber' provided: cannot retrieve the server data event", ERT_FILE_LOCATION));
        return nullptr;
    }

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("requestMethod: %s | requestUri: %s | requestNumber: %s", requestMethod.c_str(), requestUri.c_str(), requestNumber.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    mock_server_events_key_t key{};
    calculateMockServerKeyEventsKey(key, requestMethod, requestUri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return nullptr; // nothing found

    bool reverse = false;
    std::uint64_t u_requestNumber = 0;
    if (!string2uint64andSign(requestNumber, u_requestNumber, reverse))
        return nullptr;

    return (it->second->getMockServerKeyEvent(u_requestNumber, reverse));
}

nlohmann::json MockServerEventsData::getJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->getJson());
    };

    return result;
}

bool MockServerEventsData::findLastRegisteredRequestState(const std::string &method, const std::string &uri, std::string &state) const {

    mock_server_events_key_t key{};
    calculateMockServerKeyEventsKey(key, method, uri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it != end()) {
        state = it->second->getLastRegisteredRequestState(); // by design, a key always contains at least one history event (https://github.com/testillano/h2agent/issues/53).
        return true;
    }

    state = DEFAULT_ADMIN_SERVER_PROVISION_STATE;
    return false;
}

bool MockServerEventsData::loadRequestsSchema(const nlohmann::json& schema) {

    requests_schema_.setJson(schema);

    if (!requests_schema_.isAvailable()) {
        LOGWARNING(
            ert::tracing::Logger::warning("Requests won't be validated (schema will be ignored)", ERT_FILE_LOCATION);
        );

        return false;
    }

    return true;
}

}
}

