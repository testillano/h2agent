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

#include <MockEvent.hpp>


namespace h2agent
{
namespace model
{

void MockEvent::load(const std::string &previousState, const std::string &state, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders) {

    previous_state_ = previousState;
    state_ = state;
    reception_timestamp_us_ = receptionTimestampUs.count();
    response_status_code_ = responseStatusCode;
    request_headers_ = requestHeaders;
    response_headers_ = responseHeaders;

    // Update json_:
    if (!previous_state_.empty() /* server mode: unprovisioned 501 comes with empty value, and states are meaningless there */) json_["previousState"] = previous_state_;
    if(!state_.empty() /* server mode: unprovisioned 501 comes with empty value, and states are meaningless there */) json_["state"] = state_;
    json_["receptionTimestampUs"] = (std::uint64_t)reception_timestamp_us_;
    json_["responseStatusCode"] = (int)response_status_code_;
    for (const auto& [field, member] : {
                std::pair{"requestHeaders", request_headers_}, std::pair{"responseHeaders", response_headers_} // LCOV_EXCL_LINE
            }) {
        if (member.size()) {
            nlohmann::json hdrs;
            for (const auto& [k, v] : member) {
                hdrs[k] = v.value;
            }
            json_[field] = hdrs;
        }
    }
}

}
}

