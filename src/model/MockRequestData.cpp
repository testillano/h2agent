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

#include <MockRequestData.hpp>
#include <AdminProvision.hpp>


namespace h2agent
{
namespace model
{

bool MockRequestData::string2uint64(const std::string &input, std::uint64_t &output) const {

    bool result = false;

    if (!input.empty()) {
        try {
            output = std::stoull(input);
            result = true;
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to unsigned long long integer: %s", input.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
        }
    }

    return result;
}

bool MockRequestData::checkSelection(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber) const {

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

void MockRequestData::loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                                  unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                                  bool historyEnabled, const std::string &virtualOriginComingFromMethod, const std::string &virtualOriginComingFromUri) {


    // Find MockRequests
    std::shared_ptr<MockRequests> requests;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, method, uri);

    write_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it != end()) {
        requests = it->second;
    }
    else {
        requests = std::make_shared<MockRequests>();
    }

    requests->loadRequest(pstate, state, method, uri, headers, body, responseStatusCode, responseHeaders, responseBody, serverSequence, responseDelayMs, historyEnabled, virtualOriginComingFromMethod, virtualOriginComingFromUri);

    if (it == end()) add(key, requests); // push the key in the map:
}

bool MockRequestData::clear(bool &somethingDeleted, const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber)
{
    bool result = true;
    somethingDeleted = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        somethingDeleted = (Map::size() > 0);
        write_guard_t guard(rw_mutex_);
        Map::clear();
        return result;
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return false;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, requestMethod, requestUri);

    write_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return true; // nothing found to be removed

    // Check request number:
    if (!requestNumber.empty()) {
        bool reverse = (requestNumber[0] == '-');
        std::uint64_t u_requestNumber{};
        if (!string2uint64(reverse ? requestNumber.substr(1):requestNumber, u_requestNumber))
            return false;

        somethingDeleted = it->second->removeMockRequest(u_requestNumber, reverse);
    }
    else {
        somethingDeleted = true;
        Map::remove(it); // remove key
    }

    return result;
}

std::string MockRequestData::asJsonString(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &validQuery) const {

    validQuery = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        validQuery = true;
        return ((size() != 0) ? asJson().dump() : "[]"); // server data is shown as an array
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return "[]";

    validQuery = true;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, requestMethod, requestUri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return "[]"; // nothing found to be built

    // Check request number:
    if (!requestNumber.empty()) {
        bool reverse = (requestNumber[0] == '-');
        std::uint64_t u_requestNumber{};
        if (!string2uint64(reverse ? requestNumber.substr(1):requestNumber, u_requestNumber))
            return "[]";

        auto ptr = it->second->getMockRequest(u_requestNumber, reverse);
        if (ptr) {
            return ptr->getJson().dump();
        }
        else return "[]";
    }
    else {
        return it->second->asJson().dump();
    }

    return "[]";
}

std::string MockRequestData::summary(const std::string &maxKeys) const {
    nlohmann::json result;

    result["totalKeys"] = (unsigned int)size();

    std::uint64_t u_maxKeys{};
    if (!string2uint64(maxKeys, u_maxKeys)) {
        u_maxKeys = std::numeric_limits<uint64_t>::max();
    }

    size_t totalEvents = 0;
    size_t historySize;
    size_t displayedKeys = 0;
    nlohmann::json key;
    for (auto it = begin(); it != end(); it++) {
        size_t historySize = it->second->size();
        totalEvents += historySize;
        if (displayedKeys >= u_maxKeys) continue;

        key["method"] = it->second->getMethod();
        key["uri"] = it->second->getUri();
        key["amount"] = (unsigned int)historySize;
        result["displayedKeys"]["list"].push_back(key);
        displayedKeys += 1;
    };
    if (displayedKeys > 0) result["displayedKeys"]["amount"] = (unsigned int)displayedKeys;
    result["totalEvents"] = (unsigned int)totalEvents;

    return result.dump();
}

std::shared_ptr<MockRequest> MockRequestData::getMockRequest(const std::string &requestMethod, const std::string &requestUri,const std::string &requestNumber) const {

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("requestMethod: %s | requestUri: %s | requestNumber: %s", requestMethod.c_str(), requestUri.c_str(), requestNumber.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    if (requestMethod.empty())
        return nullptr;

    if (requestMethod.empty() || requestUri.empty() || requestNumber.empty())
        return nullptr;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, requestMethod, requestUri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    if (it == end())
        return nullptr; // nothing found

    // Check request number:
    std::uint64_t u_requestNumber{};
    if (!string2uint64(requestNumber, u_requestNumber))
        return nullptr;

    return (it->second->getMockRequest(u_requestNumber));
}

nlohmann::json MockRequestData::asJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->asJson());
    };

    return result;
}

bool MockRequestData::findLastRegisteredRequest(const std::string &method, const std::string &uri, std::string &state) const {

    mock_requests_key_t key;
    calculateMockRequestsKey(key, method, uri);

    read_guard_t guard(rw_mutex_);

    auto it = get(key);
    state = DEFAULT_ADMIN_PROVISION_STATE;
    if (it != end()) {
        state = it->second->getLastRegisteredRequestState();
        return true;
    }

    return false;
}

bool MockRequestData::loadRequestsSchema(const nlohmann::json& schema) {

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

