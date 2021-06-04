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

#include <Map.hpp>
#include <MockRequest.hpp>

namespace h2agent
{
namespace model
{

class MockRequestData : public Map<mock_request_key_t, MockRequest>
{
public:
    MockRequestData() {};
    ~MockRequestData() = default;

    /**
     * Loads request data
     *
     * @param state Request state
     * @param method Request method
     * @param uri Request URI path
     * @param headers Request headers
     * @param body Request body
     *
     * @return Boolean about success operation
     */
    bool load(const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body);

    /** Clears internal data
     *
     * @return True if something was removed, false if already empty
     */
    bool clear();

    /**
     * Json string representation for class information
     *
     * @param requestMethod Request method to filter selection
     * @param requestUri Request URI path to filter selection
     *
     * @return Json string representation
     */
    std::string asJsonString(const std::string &requestMethod, const std::string &requestUri) const;

    /**
     * Finds mock context entry.
     *
     * @param method Request method which was received
     * @param uri Request URI path which was rreceived
     * @param state Request current state filled by reference. If nothing found, 'initial' will be set
     *
     * @return Boolean about if the request is found or not
     */
    bool find(const std::string &method, const std::string &uri, std::string &state) const;
};

}
}

