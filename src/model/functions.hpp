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
 * Compose string key by aggregation with separator '#': '<k1>#<k2>[#<k3>]'
 *
 * @param Composed key by reference
 * @param k1 First key part (should not contain '#', and must be non-empty)
 * @param k2 First key part (should not contain '#', and must be non-empty)
 * @param k3 First key part (optional)
 */
void calculateStringKey(std::string &key, const std::string &k1, const std::string &k2, const std::string &k3 = "");

/**
 * Compose string key by aggregation with separator '#'
 * If two parts are provided, it builds '<k1>#<k2>'.
 * If one part is provided, it prepends the key with '<k1>#'.
 *
 * @param Composed key by reference
 * @param k1 First key part (should not contain '#', and must be non-empty)
 * @param k2 First key part (optional)
 */
void aggregateKeyPart(std::string &key, const std::string &k1, const std::string &k2 = "");

/**
 * Interprets integer with sign as string
 *
 * @param input Number as string
 * @param output Natural number by reference
 * @param negative Boolean indicating the number sign
 *
 * @return Boolean about successful operation
 */
bool string2uint64andSign(const std::string &input, std::uint64_t &output, bool &negative);

/**
 * Tokenizes query parameters string into key/values
 *
 * @param queryParams query parameters URI part (? not nicluded)
 * @param sorted query parameters URI part, filled by reference.
 * If nullptr provided, the sort procedure is ignored (better performance).
 * @param separator key/values separator, ampersand by default
 *
 * @return Map of key/values for query parameters
 */
std::map<std::string, std::string> extractQueryParameters(const std::string &queryParams, std::string *sortedQueryParameters = nullptr, char separator = '&' /* maybe ';' */);

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

/**
 * Json object validation
 *
 * Constraints 'expected' object within 'received' one, so the restriction is
 * ruled by first acting as a subset in this way:
 *
 * 1) it miss/ignore nodes actually received without problem (less restrictive)
 * 2) it MUST NOT contradict the ones regarding received information.
 *
 * The function is recursive, so restriction extends along the document content.
 *
 * @param received Object against which expected is validated.
 * @param expected Expected subset object.
 * @param failReport Validation report for the fail case (empty when succeed).
 *
 * @return Boolean about successful validation
 */
bool jsonConstraint(const nlohmann::json &received, const nlohmann::json &expected, std::string &failReport);

/**
 * Adapts name to a valid metrics name
 * Non valid characters (https://prometheus.io/docs/instrumenting/writing_exporters/#naming) will be replaced by underscore.
 *
 * @param in String to adapt
 *
 * @return Adapted string to a valid metrics name
 */
std::string fixMetricsName(const std::string &in);

}
}
