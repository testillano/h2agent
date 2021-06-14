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

#include <map>
#include <string>

#include <nghttp2/asio_http2_server.h>

namespace h2agent
{
namespace http2server
{

/**
 * Tokenizes query parameters string into key/values
 *
 * @param queryParams query parameters URI part
 * @param separator key/values separator, ampersand by default.
 *
 * @return Map of key/values for query parameters
 */
std::map<std::string, std::string> extractQueryParameters(const std::string &queryParams, char separator = '&' /* maybe ';' */);

/**
 * Sorts query parameters string
 *
 * @param qmap of key/values for query parameters
 * @param separator key/values separator, ampersand by default.
 *
 * @return sorted query parameters URI part.
 */
std::string sortQueryParameters(const std::map<std::string, std::string> &qmap, char separator = '&' /* maybe ';' */);


/**
 * Prints headers list for traces
 *
 * @param headers nghttp2 headers map
 *
 * @return sorted query parameters URI part.
 */
std::string headersAsString(const nghttp2::asio_http2::header_map &headers);


}
}
