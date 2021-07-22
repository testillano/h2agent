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
    if (!requestNumber.empty()) {

        std::uint64_t u_requestNumber{};
        if (!string2uint64(requestNumber, u_requestNumber))
            return false;

        somethingDeleted = it->second->removeMockRequest(u_requestNumber);
    }
    else {
        somethingDeleted = true;
        Map::remove(it); // remove key
    }

    return result;
}

std::string MockRequestData::asJsonString(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &success) const {

    success = false;

    if (requestMethod.empty() && requestUri.empty() && requestNumber.empty()) {
        success = true;
        return ((map_.size() != 0) ? asJson().dump() : "null");
    }

    if (!checkSelection(requestMethod, requestUri, requestNumber))
        return "null";

    mock_requests_key_t key;
    calculateMockRequestsKey(key, requestMethod, requestUri);

    auto it = map_.find(key);
    if (it == end())
        return "null"; // nothing found to be built

    // Check request number:
    if (!requestNumber.empty()) {

        std::uint64_t u_requestNumber{};
        if (!string2uint64(requestNumber, u_requestNumber))
            return "null";

        auto ptr = it->second->getMockRequest(u_requestNumber);
        if (ptr) {
            success = true;
            return ptr->getJson().dump();
        }
        else return "null";
    }
    else {
        success = true;
        return it->second->asJson().dump();
    }

    return "null";
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

    auto it = map_.find(key);
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

    for (auto it = map_.begin(); it != map_.end(); it++) {
        result.push_back(it->second->asJson());
    };

    return result;
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

