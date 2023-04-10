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

#include <string>
#include <chrono>

#include <nghttp2/asio_http2_server.h>
#include <nlohmann/json.hpp>

#include <DataPart.hpp>


namespace h2agent
{
namespace model
{


class MockEvent
{
    std::string previous_state_{};
    std::string state_{};
    std::uint64_t reception_timestamp_us_{};
    unsigned int response_status_code_{};
    nghttp2::asio_http2::header_map request_headers_{};
    nghttp2::asio_http2::header_map response_headers_{};

protected:

    nlohmann::json json_{}; // kept synchronized on load()

public:

    MockEvent() {;}

    // setters:

    /**
     * Loads event information
     *
     * @param previousState Previous request state
     * @param state Request state
     * @param receptionTimestampUs Microseconds reception timestamp
     * @param responseStatusCode Response status code
     * @param requestHeaders Request headers
     * @param responseHeaders Response headers
     */
    void load(const std::string &previousState, const std::string &state, const std::chrono::microseconds &receptionTimestampUs, unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders);

    // getters:

    /** Request state
     *
     * @return current state
     */
    const std::string &getState() const {
        return state_;
    }

    /** Request headers
     *
     * @return Request headers
     */
    const nghttp2::asio_http2::header_map &getRequestHeaders() const {
        return request_headers_;
    }

    /** Response headers
     *
     * @return Response headers
     */
    const nghttp2::asio_http2::header_map &getResponseHeaders() const {
        return response_headers_;
    }

    /**
     * Gets json document
     *
     * @param path within the object to restrict selection (empty by default).
     *
     * @return Json object
     */
    const nlohmann::json &getJson(const std::string &path = "") const {
        if (path.empty()) return json_;

        nlohmann::json::json_pointer p(path);
        return json_[p];
    }
};

}
}

