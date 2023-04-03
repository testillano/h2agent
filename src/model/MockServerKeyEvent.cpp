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

#include <MockServerKeyEvent.hpp>


namespace h2agent
{
namespace model
{


void MockServerKeyEvent::load(const std::string &previousState, const std::string &state, const nghttp2::asio_http2::header_map &requestHeaders, DataPart &requestBodyDataPart, const std::chrono::microseconds &receptionTimestampUs, unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs, const std::string &virtualOriginComingFromMethod, const std::string &virtualOriginComingFromUri) {

    reception_timestamp_us_ = receptionTimestampUs.count();

    previous_state_ = previousState;

    state_ = state;
    request_headers_ = requestHeaders;


    requestBodyDataPart.decode(requestHeaders);
    request_body_ = requestBodyDataPart.getJson();

    response_status_code_ = responseStatusCode;
    response_headers_ = responseHeaders;

    response_body_data_part_.assign(std::move(responseBody));
    response_body_data_part_.decode(responseHeaders);

    server_sequence_ = serverSequence;
    response_delay_ms_ = responseDelayMs;

    virtual_origin_coming_from_method_ = virtualOriginComingFromMethod;
    virtual_origin_coming_from_uri_ = virtualOriginComingFromUri;

    saveJson();
}

void MockServerKeyEvent::saveJson() {

    if (!virtual_origin_coming_from_method_.empty()) {
        json_["virtualOrigin"]["method"] = virtual_origin_coming_from_method_;
        json_["virtualOrigin"]["uri"] = virtual_origin_coming_from_uri_;
    }

    json_["receptionTimestampUs"] = (std::uint64_t)reception_timestamp_us_;

    if(!state_.empty() /* unprovisioned 501 comes with empty value, and states are meaningless there */) json_["state"] = state_;

    if (request_headers_.size()) {
        nlohmann::json hdrs; // headers as an object
        for(const auto &x: request_headers_)
            hdrs[x.first] = x.second.value;
        json_["requestHeaders"] = hdrs;
    }
    if (!request_body_.empty()) {
        json_["requestBody"] = request_body_;
    }

    // Additional information
    if (!previous_state_.empty() /* unprovisioned 501 comes with empty value, and states are meaningless there */) json_["previousState"] = previous_state_;
    if (!response_body_data_part_.str().empty()) {
        json_["responseBody"] = response_body_data_part_.getJson();
    }
    json_["responseDelayMs"] = (unsigned int)response_delay_ms_;
    json_["responseStatusCode"] = (unsigned int)response_status_code_;
    if (response_headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: response_headers_)
            hdrs[x.first] = x.second.value;
        json_["responseHeaders"] = hdrs;
    }
    json_["serverSequence"] = (std::uint64_t)server_sequence_;
}

}
}

