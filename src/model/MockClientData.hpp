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
#include <cstdint>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <MockData.hpp>
#include <MockClientEventsHistory.hpp>
#include <MockClientEvent.hpp>


namespace h2agent
{
namespace model
{


/**
 * This class stores the mock client events.
 *
 * This is useful to post-verify the internal data (content and schema validation) on testing system.
 * Also, the last request for an specific key, is used to know the state which is used to get the
 * corresponding provision information.
 */
class MockClientData : public MockData
{
public:
    MockClientData() {};
    ~MockClientData() = default;

    /**
     * Loads event data
     *
     * @param dataKey Events key (client endpoint id, method & uri).
     *
     * @param clientProvisionId Client provision id responsible for the event
     * @param previousState Previous request state
     * @param state Request state
     * @param sendingTimestampUs Microseconds sending timestamp
     * @param receptionTimestampUs Microseconds reception timestamp
     * @param responseStatusCode Response status code
     * @param requestHeaders Request headers
     * @param responseHeaders Response headers
     *
     * @param requestBody Request body
     * @param responseBodyDataPart Response body
     * @param clientSequence Server sequence (1..N)
     * @param sequence test sequence (1..N)
     * @param requestDelayMs Request delay in milliseconds
     * @param timeoutMs Timeout in milliseconds
     *
     * @param historyEnabled Events complete history storage
     */
    void loadEvent(const DataKey &dataKey, const std::string &clientProvisionId, const std::string &previousState, const std::string &state, const std::chrono::microseconds &sendingTimestampUs, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &requestBody, DataPart &responseBodyDataPart, std::uint64_t clientSequence, std::uint64_t sequence, unsigned int requestDelayMs, unsigned int timeoutMs, bool historyEnabled);
};

}
}

