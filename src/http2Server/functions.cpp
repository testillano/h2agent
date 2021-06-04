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

#include <functions.hpp>

#include <ert/tracing/Logger.hpp>

namespace h2agent
{
namespace http2server
{

std::map<std::string, std::string> extractQueryParameters(const std::string &queryParams, char separator) {

    std::map<std::string, std::string> result;

    if (queryParams.empty()) return result;

    // Inspired in https://github.com/ben-zen/uri-library
    // Loop over the query string looking for '&'s (maybe ';'s), then check each one for
    // an '=' to find keys and values; if there's not an '=' then the key
    // will have an empty value in the map.
    size_t pos = 0;
    size_t qpair_end = queryParams.find_first_of(separator);
    do
    {
        std::string qpair = queryParams.substr(pos, ((qpair_end != std::string::npos) ? (qpair_end - pos) : std::string::npos));
        size_t key_value_divider = qpair.find_first_of('=');
        std::string key = qpair.substr(0, key_value_divider);
        std::string value;
        if (key_value_divider != std::string::npos)
        {
            value = qpair.substr((key_value_divider + 1));
        }

        if (result.count(key) != 0)
        {
            ert::tracing::Logger::error("Cannot normalize URI query parameters: bad key found", ERT_FILE_LOCATION);
            result.clear();
            return result;
        }

        result.emplace(key, value);
        pos = ((qpair_end != std::string::npos) ? (qpair_end + 1) : std::string::npos);
        qpair_end = queryParams.find_first_of(separator, pos);
    }
    while ((qpair_end != std::string::npos) || (pos != std::string::npos));

    return result;
}

void sortQueryParameters(std::string &queryParams, char separator) {

    std::map<std::string, std::string> qmap = extractQueryParameters(queryParams, separator);

    // Build sort string:
    queryParams = "";
    for(auto it = qmap.begin(); it != qmap.end(); it ++) {
        if (it != qmap.begin()) queryParams += separator;
        queryParams += it->first; // key
        if (!it->second.empty()) {
            queryParams += "=";
            queryParams += it->second; // value
        }
    }
}

}
}