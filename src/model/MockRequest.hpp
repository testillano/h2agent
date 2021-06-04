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


// Mock key:
typedef std::string mock_request_key_t;
// Future proof: instead of using a key = <method><uri>, we could agreggate them:
// typedef std::pair<std::string, std::string> mock_key_t;
// But in order to compile, we need to define a hash function for the unordered map:
// https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR)
// https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)

void calculateMockRequestKey(mock_request_key_t &key, const std::string &method, const std::string &uri);


class MockRequest
{
    std::string state_;

    std::string method_;
    std::string uri_;

    nghttp2::asio_http2::header_map headers_{};
    std::string body_;

public:

    MockRequest() {;}

    // setters:

    /**
     * Load request information
     *
     * @param state Request state
     * @param method Request method
     * @param uri Request URI path
     * @param headers Request headers
     * @param body Request body
     *
     * @return Operation success
     */
    bool load(const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body);

    // getters:

    /**
     * Gets the mock request key as '<request-method>|<request-uri>'
     *
     * @return Mock request key
     */
    mock_request_key_t getKey() const;

    /** Request state
     *
     * @return current state
     */
    const std::string &getState() const {
        return state_;
    }

    /** Request method
      *
      * @return Request method
      */
    const std::string &getMethod() const {
        return method_;
    }

    /** Request URI
     *
     * @return Request URI
     */
    const std::string &getUri() const {
        return uri_;
    }

    /** Request headers
     *
     * @return Request headers
     */
    const nghttp2::asio_http2::header_map &getHeaders() const {
        return headers_;
    }

    /** Request body
     *
     * @return Request body
     */
    const std::string &getBody() const {
        return body_;
    }

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;
};

}
}
