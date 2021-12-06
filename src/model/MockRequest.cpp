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

#include <functions.hpp>

#include <MockRequest.hpp>

#include <chrono>


namespace h2agent
{
namespace model
{


bool MockRequest::load(const std::string &pstate, const std::string &state, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                       unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                       const std::string &virtualOriginComingFromMethod) {

    reception_timestamp_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    pstate_ = pstate;

    state_ = state;
    headers_ = headers;
    body_ = body;

    response_status_code_ = responseStatusCode;
    response_headers_ = responseHeaders;
    response_body_ = responseBody;
    server_sequence_ = serverSequence;
    response_delay_ms_ = responseDelayMs;

    virtual_origin_coming_from_method_ = virtualOriginComingFromMethod;

    saveJson();

    return true;
}

void MockRequest::saveJson() {

    if (!virtual_origin_coming_from_method_.empty())
        json_["virtualOriginComingFromMethod"] = virtual_origin_coming_from_method_;

    json_["receptionTimestampMs"] = (std::uint64_t)reception_timestamp_ms_;

    if(!state_.empty() /* unprovisioned 501 comes with empty value, and states are meaningless there */) json_["state"] = state_;

    if (headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: headers_)
            hdrs[x.first] = x.second.value;
        json_["headers"] = hdrs;
    }

    if (!body_.empty()) {
        h2agent::http2server::parseJsonContent(body_, json_["body"], true /* write exception */);
    }

    // Additional information
    if (!pstate_.empty() /* unprovisioned 501 comes with empty value, and states are meaningless there */) json_["previousState"] = pstate_;
    if (!response_body_.empty()) {
        h2agent::http2server::parseJsonContent(response_body_, json_["responseBody"], true /* write exception */);
    }

    json_["responseDelayMs"] = (unsigned int)response_delay_ms_;
    json_["responseStatusCode"] = (unsigned int)response_status_code_;
    if (response_headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: response_headers_)
            hdrs[x.first] = x.second.value;
        json_["responseHeaders"] = hdrs;
    }
    json_["serverSequence"] = (unsigned int)server_sequence_;
}

}
}

