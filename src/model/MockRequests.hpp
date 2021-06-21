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

#include <nghttp2/asio_http2_server.h>

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <MockRequest.hpp>
#include <common.hpp>

namespace h2agent
{
namespace model
{


// Mock key:
typedef std::string mock_requests_key_t;
// Future proof: instead of using a key = <method><uri>, we could agreggate them:
// typedef std::pair<std::string, std::string> mock_key_t;
// But in order to compile, we need to define a hash function for the unordered map:
// https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR)
// https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)

void calculateMockRequestsKey(mock_requests_key_t &key, const std::string &method, const std::string &uri);



class MockRequests
{
    mutable mutex_t rw_mutex_;

    std::string method_;
    std::string uri_;

    std::string initial;

    std::vector<std::shared_ptr<MockRequest>> requests_;

public:

    MockRequests() {
        initial = "initial";
    }

    // setters:

    /**
     * Loads requests information
     *
     * @param pstate Previous request state
     * @param state Request state
     * @param method Request method
     * @param uri Request URI path
     * @param headers Request headers
     * @param body Request body
     *
     * @param responseStatusCode Response status code
     * @param responseHeaders Response headers
     * @param responseBody Response body
     * @param serverSequence Server sequence
     * @param responseDelayMs Response delay in milliseconds
     *
     * @param historyEnabled Requests complete history storage
     * @param virtualOriginComingFromMethod Marks event as virtual one, adding a field with the origin method which caused it. Non-virtual by default (empty parameter).
     *
     * @return Boolean about success operation
     */
    bool loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                     unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                     bool historyEnabled, const std::string virtualOriginComingFromMethod = "");


    // getters:


    /**
     * Builds json document for class information
     *
     * @param requestNumber Request history number (1..N) to filter selection.
     * Value '0':  whole history is provided for method/uri provided.
     * Value '-1' (unsigned long long max): the latest event is filtered.
     *
     * @return Json object
     */
    nlohmann::json getJson(std::uint64_t requestNumber) const;

    /**
     * Gets the mock requests key as '<request-method>|<request-uri>'
     *
     * @return Mock request key
     */
    mock_requests_key_t getKey() const;

    /** Last registered request state
    *
    * @return Last registered request state
    */
    const std::string &getLastRegisteredRequestState() const {
        read_guard_t guard(rw_mutex_);
        return requests_.back()->getState(); // when invoked, always exists at least 1 request in the history
    }
};

}
}

