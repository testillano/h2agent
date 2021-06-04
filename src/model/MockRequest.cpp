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



namespace h2agent
{
namespace model
{

void calculateMockRequestKey(mock_request_key_t &key, const std::string &method, const std::string &uri) {
    // key <request-method>#<request-uri>
    key += method;
    key += "#";
    key += uri;
}

mock_request_key_t MockRequest::getKey() const {

    mock_request_key_t result;
    calculateMockRequestKey(result, method_, uri_);
    return result;
}

bool MockRequest::load(const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body) {
    state_ = state;
    method_ = method;
    uri_ = uri;
    headers_ = headers;
    body_ = body;

    return true;
}

nlohmann::json MockRequest::getJson() const {
    nlohmann::json result;

    result["state"] = state_;
    result["method"] = method_;
    result["uri"] = uri_;

    if (headers_.size()) {
        nlohmann::json hdrs;
        for(const auto &x: headers_)
            hdrs[x.first] = x.second.value;
        result["headers"] = hdrs;
    }

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

    return result;
}

}
}

