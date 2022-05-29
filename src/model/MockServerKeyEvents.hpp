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
//#include <cstdint>

#include <nlohmann/json.hpp>

#include <MockServerKeyEvent.hpp>
#include <common.hpp>

namespace h2agent
{
namespace model
{


// Mock key:
typedef std::string mock_server_events_key_t;
// Future proof: instead of using a key = <method><uri>, we could agreggate them:
// typedef std::pair<std::string, std::string> mock_key_t;
// But in order to compile, we need to define a hash function for the unordered map:
// https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR)
// https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)

void calculateMockServerKeyEventsKey(mock_server_events_key_t &key, const std::string &method, const std::string &uri);



class MockServerKeyEvents
{
    mutable mutex_t rw_mutex_{};

    std::string method_{};
    std::string uri_{};

    std::vector<std::shared_ptr<MockServerKeyEvent>> requests_{};

public:

    MockServerKeyEvents() {;}

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
     * @param virtualOriginComingFromUri Marks event as virtual one, adding a field with the origin uri which caused it. Non-virtual by default (empty parameter).
     */
    void loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                     unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                     bool historyEnabled, const std::string virtualOriginComingFromMethod = "", const std::string virtualOriginComingFromUri = "");


    /**
     * Removes vector item for a given position
     * We could also access from the tail (reverse chronological order)

     * @param requestNumber Request history number (1..N) to filter selection.
     * @param reverse Reverse the order to get the request from the tail instead the head.
     *
     * @return Boolean about if something was deleted
     */
    bool removeMockServerKeyEvent(std::uint64_t requestNumber, bool reverse);

    // getters:

    /**
     * Gets the mock server key event in specific position (last by default)
     *
     * @param requestNumber Request history number (1..N) to filter selection.
     * Value '0' is not accepted, and null will be returned in this case.
     * @param reverse Reverse the order to get the request from the tail instead the head.
     *
     * @return mock request pointer
     * @see size()
     */
    std::shared_ptr<MockServerKeyEvent> getMockServerKeyEvent(std::uint64_t requestNumber, bool reverse) const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;

    /**
     * Gets the mock requests key as '<request-method>|<request-uri>'
     *
     * @return Mock request key
     */
    mock_server_events_key_t getKey() const;

    /** Last registered request state
    *
    * @return Last registered request state
    */
    const std::string &getLastRegisteredRequestState() const;

    /** Number of requests
    *
    * @return Requests list size
    */
    size_t size() const {
        return requests_.size();
    }

    /** Get the method of the requests list
    *
    * @return Requests method key
    */
    const std::string &getMethod() const {
        return method_;
    }

    /** Get the uri of the requests list
    *
    * @return Requests uri key
    */
    const std::string &getUri() const {
        return uri_;
    }
};

}
}

