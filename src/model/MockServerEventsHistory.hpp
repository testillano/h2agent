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

#include <MockEventsHistory.hpp>


namespace h2agent
{
namespace model
{


class MockServerEventsHistory : public MockEventsHistory
{
public:

    /**
    * Constructor
    *
    * @param dataKey Events key (method & uri).
    */
    MockServerEventsHistory(const DataKey &dataKey) : MockEventsHistory(dataKey) {;}

    // setters:

    /**
     * Loads events information
     *
     * @param previousState Previous request state
     * @param state Request state
     * @param receptionTimestampUs Microseconds reception timestamp
     * @param responseStatusCode Response status code
     * @param requestHeaders Request headers
     * @param responseHeaders Response headers
     *
     * @param requestBodyDataPart Request body
     * @param responseBody Response body
     * @param serverSequence Server sequence (1..N)
     * @param responseDelayMs Response delay in milliseconds
     *
     * @param historyEnabled Events complete history storage
     * @param virtualOriginComingFromMethod Marks event as virtual one, adding a field with the origin method which caused it. Non-virtual by default (empty parameter).
     * @param virtualOriginComingFromUri Marks event as virtual one, adding a field with the origin uri which caused it. Non-virtual by default (empty parameter).
     */
    void loadEvent(const std::string &previousState, const std::string &state, const std::chrono::microseconds &receptionTimestampUs, unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, DataPart &requestBodyDataPart, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs, bool historyEnabled, const std::string &virtualOriginComingFromMethod = "", const std::string &virtualOriginComingFromUri = "");
};

}
}

