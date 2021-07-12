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

bool MockRequestData::loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                                  unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                                  bool historyEnabled, const std::string &virtualOriginComingFromMethod) {


    // Find MockRequests
    std::shared_ptr<MockRequests> requests;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, method, uri);
    auto it = map_.find(key);
    if (it != end()) {
        requests = it->second;
    }
    else {
        requests = std::make_shared<MockRequests>();
    }

    if (!requests->loadRequest(pstate, state, method, uri, headers, body, responseStatusCode, responseHeaders, responseBody, serverSequence, responseDelayMs, historyEnabled, virtualOriginComingFromMethod)) {
        return false;
    }

    if (it == end()) add(key, requests); // push the key in the map:

    return true;
}

bool MockRequestData::clear(bool &somethingDeleted, const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber)
{
    bool result = true;
    somethingDeleted = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        somethingDeleted = (Map::size() > 0);
        Map::clear();
        return result;
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return false;

    mock_requests_key_t key;
    calculateMockRequestsKey(key, requestMethod, requestUri);

    auto it = map_.find(key);
    if (it == end())
        return true; // nothing found to be removed

    // Check request number:
    std::uint64_t u_requestNumber = 0;
    if (!requestNumber.empty()) {
        try {
            u_requestNumber = std::stoull(requestNumber);
        }
        catch(std::exception &e)
        {
            std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to unsigned long long integer: %s", requestNumber.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
            return false;
        }

        if (u_requestNumber == 0) {
            somethingDeleted = true;
            Map::remove(it); // remove whole history (requests list)
        }
        else {
            somethingDeleted = it->second->removeNumber(u_requestNumber);
        }
    }
    else {
        somethingDeleted = true;
        Map::remove(it); // remove whole history (requests list)
    }

    return result;
}

void MockRequestData::asJson(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &success, nlohmann::json &object) const {

    success = false;

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return;

    if (!requestMethod.empty()) {

        // Check request number:
        std::uint64_t u_requestNumber = 0;
        if (!requestNumber.empty()) {
            try {
                u_requestNumber = std::stoull(requestNumber);
            }
            catch(std::exception &e)
            {
                std::string msg = ert::tracing::Logger::asString("Error converting string '%s' to unsigned long long integer: %s", requestNumber.c_str(), e.what());
                ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
                return;
            }
        }

        mock_requests_key_t key;
        calculateMockRequestsKey(key, requestMethod, requestUri);

        auto it = map_.find(key);
        if (it != end()) {
            object = it->second->getRequestsJson(u_requestNumber); // request method and uri are provided (and perhaps request number), we retrieve the requests array (documented)
        }
    }
    else {
        for (auto it = map_.begin(); it != map_.end(); it++) {
            object.push_back(it->second->getJson(/* whole server internal data including method and uri, as no query parameters are provided */));
        };
    }

    success = true;
}

std::string MockRequestData::asJsonString(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &success) const {

    nlohmann::json object;
    asJson(requestMethod, requestUri, requestNumber, success, object);
    return ((map_.size() != 0) ? object.dump():"null"); // guarantee "null" if empty (nlohmann could change):
}

bool MockRequestData::findLastRegisteredRequest(const std::string &method, const std::string &uri, std::string &state) const {

    mock_requests_key_t key;
    calculateMockRequestsKey(key, method, uri);

    auto it = map_.find(key);
    state = DEFAULT_ADMIN_PROVISION_STATE;
    if (it != end()) {
        state = it->second->getLastRegisteredRequestState();
        return true;
    }

    return false;
}

bool MockRequestData::loadRequestsSchema(const nlohmann::json& schema) {

    requests_schema_.setSchema(schema);

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

