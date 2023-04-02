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


#include <MockEvent.hpp>


namespace h2agent
{
namespace model
{


class MockClientEvent : public MockEvent
{
    std::string client_provision_id_{};
    std::uint64_t sending_timestamp_us_{};
    DataPart request_body_data_part_{};
    nlohmann::json response_body_{};
    std::uint64_t client_sequence_{};
    std::uint64_t sequence_{};
    unsigned int request_delay_ms_{};
    unsigned int timeout_ms_{};

public:

    MockClientEvent() {;}

    // setters:

    /**
     * Loads request information
     *
     * @param clientProvisionId Client provision id responsible for the event
     * @param previousState Previous request state
     * @param state Request state
     * @param receptionTimestampUs Microseconds reception timestamp
     * @param responseStatusCode Response status code
     * @param requestHeaders Request headers
     * @param responseHeaders Response headers
     *
     * @param sendingTimestampUs Microseconds sending timestamp
     * @param requestBody Request body
     * @param responseBodyDataPart Response body
     * @param clientSequence Server sequence (1..N)
     * @param sequence test sequence (1..N)
     * @param requestDelayMs Request delay in milliseconds
     * @param timeoutMs Timeout in milliseconds
     */
    void load(const std::string &clientProvisionId, const std::string &previousState, const std::string &state, const std::chrono::microseconds &sendingTimestampUs, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &requestBody, DataPart &responseBodyDataPart, std::uint64_t clientSequence, std::uint64_t sequence, unsigned int requestDelayMs, unsigned int timeoutMs);

    // getters:

    /** Response body
     *
     * @return Response body
     */
    const nlohmann::json &getResponseBody() const {
        return response_body_;
    }
};

}
}

