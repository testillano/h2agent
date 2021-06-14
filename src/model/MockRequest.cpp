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

#include <MockRequest.hpp>

#include <chrono>


namespace h2agent
{
namespace model
{


bool MockRequest::load(const std::string &pstate, const std::string &state, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                       unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs) {

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

    return true;
}

nlohmann::json MockRequest::getJson() const {
    nlohmann::json result;

    result["receptionTimestampMs"] = (std::uint64_t)reception_timestamp_ms_;

    result["state"] = state_;

    if (headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: headers_)
            hdrs[x.first] = x.second.value;
        result["headers"] = hdrs;
    }

    if (!body_.empty()) {
        try {
            result["body"] = nlohmann::json::parse(body_);
        }
        catch (nlohmann::json::parse_error& e)
        {
            /*
            std::stringstream ss;
            ss << "Json body parse error: " << e.what() << '\n'
            << "exception id: " << e.id << '\n'
            << "byte position of error: " << e.byte << std::endl;
            ert::tracing::Logger::error(ss.str(), ERT_FILE_LOCATION);
            */

            // Response data:
            result["body"] = e.what();
        }
    }

    // Additional information
    result["previousState"] = pstate_;
    if (!response_body_.empty()) {
        try {
            result["responseBody"] = nlohmann::json::parse(response_body_);
        }
        catch (nlohmann::json::parse_error& e)
        {
            /*
            std::stringstream ss;
            ss << "Json body parse error: " << e.what() << '\n'
            << "exception id: " << e.id << '\n'
            << "byte position of error: " << e.byte << std::endl;
            ert::tracing::Logger::error(ss.str(), ERT_FILE_LOCATION);
            */

            // Response data:
            result["responseBody"] = e.what();
        }
    }

    result["responseDelayMs"] = (unsigned int)response_delay_ms_;
    result["responseStatusCode"] = (unsigned int)response_status_code_;
    if (response_headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: response_headers_)
            hdrs[x.first] = x.second.value;
        result["responseHeaders"] = hdrs;
    }
    result["serverSequence"] = (unsigned int)server_sequence_;


    return result;
}

}
}

