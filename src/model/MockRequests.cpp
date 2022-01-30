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

#include <MockRequests.hpp>


namespace h2agent
{
namespace model
{

void calculateMockRequestsKey(mock_requests_key_t &key, const std::string &method, const std::string &uri) {
    // key <request-method>#<request-uri>
    key += method;
    key += "#";
    key += uri;
}

bool MockRequests::removeMockRequest(std::uint64_t requestNumber, bool reverse) {

    write_guard_t guard(rw_mutex_);

    if (requests_.size() == 0 || requestNumber == 0) return false;
    if (requestNumber > requests_.size()) return false;

    requests_.erase(requests_.begin() + (reverse ? (requests_.size() - requestNumber):(requestNumber - 1)));

    return true;
}

mock_requests_key_t MockRequests::getKey() const {

    mock_requests_key_t result;
    calculateMockRequestsKey(result, method_, uri_);
    return result;
}

void MockRequests::loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                               unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                               bool historyEnabled, const std::string virtualOriginComingFromMethod) {

    method_ = method;
    uri_ = uri;

    auto request = std::make_shared<MockRequest>();
    request->load(pstate, state, headers, body, responseStatusCode, responseHeaders, responseBody, serverSequence, responseDelayMs, virtualOriginComingFromMethod);

    write_guard_t guard(rw_mutex_);

    if (!historyEnabled && requests_.size() != 0) {
        requests_[0] = request; // overwrite with this latest reception
    }
    else {
        requests_.push_back(request);
    }
}

std::shared_ptr<MockRequest> MockRequests::getMockRequest(std::uint64_t requestNumber, bool reverse) const {

    read_guard_t guard(rw_mutex_);

    if (requests_.size() == 0) return nullptr;
    if (requestNumber == 0) return nullptr;

    if (requestNumber > requests_.size()) return nullptr;

    if (reverse) {
        return *(requests_.begin() + (requests_.size() - requestNumber));
        //return *(std::prev(requests_.end())); // this would be the last
    }

    return *(requests_.begin() + (requestNumber - 1));
}


nlohmann::json MockRequests::asJson() const {
    nlohmann::json result;

    result["method"] = method_;
    result["uri"] = uri_;

    read_guard_t guard(rw_mutex_);
    for (auto it = requests_.begin(); it != requests_.end(); it ++) {
        result["requests"].push_back((*it)->getJson());
    }

    return result;
}

const std::string &MockRequests::getLastRegisteredRequestState() const {
    read_guard_t guard(rw_mutex_);
    return requests_.back()->getState(); // when invoked, always exists at least 1 request in the history
}

}
}

