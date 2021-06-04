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

#include <string>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <MockRequestData.hpp>
#include <AdminProvision.hpp>


namespace h2agent
{
namespace model
{

bool MockRequestData::load(const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body) {


    // Event object to fill:
    auto request = std::make_shared<MockRequest>();

    if (request->load(state, method, uri, headers, body)) {

        // Push the key in the map:
        mock_request_key_t key = request->getKey();
        add(key, request);

        return true;
    }

    return false;
}

bool MockRequestData::clear()
{
    bool result = (size() != 0);

    Map::clear();

    return result;
}

std::string MockRequestData::asJsonString(const std::string &requestMethod, const std::string &requestUri) const {

    nlohmann::json result;

    if (!requestMethod.empty() && !requestUri.empty()) {
        mock_request_key_t key;
        calculateMockRequestKey(key, requestMethod, requestUri);

        auto it = map_.find(key);
        if (it != end()) {
            result = it->second->getJson();
        }
    }
    else {
        for (auto it = map_.begin(); it != map_.end(); it++) {
            result.push_back(it->second->getJson());
        };
    }

    // guarantee "null" if empty (nlohmann could change):
    return (result.empty() ? "null":result.dump());
}

bool MockRequestData::find(const std::string &method, const std::string &uri, std::string &state) const {

    mock_request_key_t key;
    calculateMockRequestKey(key, method, uri);

    auto it = map_.find(key);
    state = DEFAULT_ADMIN_PROVISION_STATE;
    if (it != end()) {
        state = it->second->getState();
        return true;
    }

    return false;
}

}
}

