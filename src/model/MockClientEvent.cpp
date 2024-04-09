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

#include <MockClientEvent.hpp>


namespace h2agent
{
namespace model
{

void MockClientEvent::load(const std::string &clientProvisionId, const std::string &previousState, const std::string &state, const std::chrono::microseconds &sendingTimestampUs, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &requestBody, DataPart &responseBodyDataPart, std::uint64_t clientSequence, std::uint64_t sequence, unsigned int requestDelayMs, unsigned int timeoutMs) {

    // Base class:
    MockEvent::load(previousState, state, receptionTimestampUs, responseStatusCode, requestHeaders, responseHeaders);

    client_provision_id_ = clientProvisionId;
    sending_timestamp_us_ = sendingTimestampUs.count();
    request_body_data_part_.assign(std::move(requestBody));
    request_body_data_part_.decode(requestHeaders);
    responseBodyDataPart.decode(responseHeaders);
    response_body_ = responseBodyDataPart.getJson();
    client_sequence_ = clientSequence;
    sequence_ = sequence;
    request_delay_ms_ = requestDelayMs;
    timeout_ms_ = timeoutMs;

    // Update json_:
    json_["clientProvisionId"] = client_provision_id_;
    json_["sendingTimestampUs"] = (std::uint64_t)sending_timestamp_us_;
    if (!request_body_data_part_.str().empty()) {
        json_["requestBody"] = request_body_data_part_.getJson();
    }
    if (!response_body_.empty()) {
        json_["responseBody"] = response_body_;
    }
    json_["clientSequence"] = (std::uint64_t)client_sequence_;
    json_["sequence"] = (std::uint64_t)sequence_;
    json_["requestDelayMs"] = (unsigned int)request_delay_ms_;
    json_["timeoutMs"] = (unsigned int)timeout_ms_;
}

}
}

