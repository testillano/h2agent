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

#include <nghttp2/asio_http2_server.h>

#include <string>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

class AdminProvision
{
    std::string request_method_;
    std::string request_uri_;
    unsigned int response_code_;
    std::string in_state_;
    std::string out_state_;
    nghttp2::asio_http2::header_map response_headers_{};
    std::string response_body_;
    unsigned int response_delay_ms_;

public:

    // setters:

    void setRequestMethod(const std::string& rm) {
        request_method_ = rm;
    }
    void setRequestUri(const std::string& ru) {
        request_uri_ = ru;
    }
    void setResponseCode(unsigned int rc) {
        response_code_ = rc;
    }
    void setInState(const std::string& is) {
        in_state_ = is;
    }
    void setOutState(const std::string& os) {
        out_state_ = os;
    }
    void setResponseBody(const nlohmann::json &j) {
        response_body_ = j.dump();
    }
    void setResponseDelayMs(unsigned int delay) {
        response_delay_ms_ = delay;
    }

    /**
     * Loads admin provision response headers data
     *
     * @param j response headers json node
     */
    void loadResponseHeaders(const nlohmann::json &j);

    /**
     * Loads admin provision transformation data
     *
     * @param j transformation json node
     */
    void loadTransform(const nlohmann::json &j);
};

}
}

