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
#include <mutex>
#include <shared_mutex>

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

    // An event is published (stored, readable by other threads: serverEvent sources,
    // admin inspection) right after load(), but it can still be written afterwards
    // (the sending timestamp is set when the response stream closes). Every access to
    // json_ after load() takes this lock; readers get copies.
    mutable std::shared_mutex json_mutex_{};

public:

    MockEvent() {;}

    /**
     * Sets the transformation pipeline execution trace (stored only when enabled
     * by the data configuration, see 'executionTrace')
     *
     * @param trace Json array with one entry per evaluated rule
     */
    void setExecutionTrace(const nlohmann::json &trace) {
        std::unique_lock<std::shared_mutex> lock(json_mutex_);
        json_["executionTrace"] = trace;
    }

    /**
     * Sets the failures of the event (present only when it failed), in evaluation
     * order: the first one gives the event state ('__core:failed:<reason>')
     *
     * @param failures Json array with one entry per failure ({"reason": ..., <detail fields>})
     */
    void setFailures(const nlohmann::json &failures) {
        std::unique_lock<std::shared_mutex> lock(json_mutex_);
        json_["failures"] = failures;
    }

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
    void load(const std::string &previousState, const std::string &state, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders);

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

    /** Reception timestamp
     *
     * @return Reception timestamp in microseconds
     */
    std::uint64_t getReceptionTimestampUs() const {
        return reception_timestamp_us_;
    }

    /**
     * Gets json document (a copy: the event may be written concurrently, see json_mutex_)
     *
     * @param path within the object to restrict selection (empty by default).
     *
     * @return Json object
     */
    nlohmann::json getJson(const std::string &path = "") const {
        std::shared_lock<std::shared_mutex> lock(json_mutex_);
        if (path.empty()) return json_;

        // const operator[] on a missing node is undefined behavior (JSON_ASSERT is
        // disabled on release builds): check existence first, null means not found.
        nlohmann::json::json_pointer p(path);
        if (!json_.contains(p)) return nlohmann::json();
        return json_[p];
    }
};

}
}

