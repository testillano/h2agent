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

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

/**
 * Tokenizes query parameters string into key/values
 *
 * @param queryParams query parameters URI part
 * @param separator key/values separator, ampersand by default
 *
 * @return Map of key/values for query parameters
 */
std::map<std::string, std::string> extractQueryParameters(const std::string &queryParams, char separator = '&' /* maybe ';' */);

/**
 * Sorts query parameters string
 *
 * @param qmap of key/values for query parameters
 * @param separator key/values separator, ampersand by default
 *
 * @return sorted query parameters URI part.
 */
std::string sortQueryParameters(const std::map<std::string, std::string> &qmap, char separator = '&' /* maybe ';' */);

/**
 * Loads file into string content
 *
 * @param filePath file path to load
 * @param content content read by reference
 *
 * @return Boolean with operation success
 */
bool getFileContent(const std::string &filePath, std::string &content);

/**
 * Parses json string to json object
 *
 * @param content string content to parse
 * @param jsonObject object read by reference
 * @param writeException object will store exception literal (false by default)
 *
 * @return Boolean with operation success
 */
bool parseJsonContent(const std::string &content, nlohmann::json &jsonObject, bool writeException = false);

/**
 * Represents the input as ascii string (non printable characters are dots).
 *
 * @param input string to convert
 * @param output result passed by reference
 *
 * @return Boolean about if input is printable.
 */
bool asAsciiString(const std::string &input, std::string &output);

/**
 * Represents the input as hexadecimal string.
 *
 * @param input string to convert
 * @param output result passed by reference
 *
 * @return Boolean about if input is printable.
 * A printable character is a character that occupies a printing position on a display.
 * Check https://cplusplus.com/reference/cctype/isprint/
 */
bool asHexString(const std::string &input, std::string &output);

/**
 * Interpret the input as hexadecimal string.
 *
 * @param input hex string to convert (prefix '0x' can be provided, i.e.: 0xa4cd02").
 * So, this string is an hexadecimal octet sequence representation with even number of digits.
 * @param output result passed by reference
 *
 * @return Boolean about successful operation
 */
bool fromHexString(const std::string &input, std::string &output);

}
}
