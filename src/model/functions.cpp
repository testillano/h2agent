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

#include <fstream>
#include <ctype.h>

#include <functions.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/URLFunctions.hpp>


namespace h2agent
{
namespace model
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
            ert::tracing::Logger::error("Cannot normalize URI query parameters: repeated key found", ERT_FILE_LOCATION);
            result.clear();
            return result;
        }

        std::string valueDecoded = ert::http2comm::URLFunctions::decode(value);
        bool decoded = (valueDecoded != value);
        if (decoded) {
            value = valueDecoded;
        }
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Extracted query parameter %s = %s%s", key.c_str(), value.c_str(), (decoded ? " (decoded)":"")), ERT_FILE_LOCATION));
        result.emplace(key, value);
        pos = ((qpair_end != std::string::npos) ? (qpair_end + 1) : std::string::npos);
        qpair_end = queryParams.find_first_of(separator, pos);
    }
    while ((qpair_end != std::string::npos) || (pos != std::string::npos));

    return result;
}

std::string sortQueryParameters(const std::map<std::string, std::string> &qmap, char separator) {

    std::string result = "";

    for(auto it = qmap.begin(); it != qmap.end(); it ++) {
        if (it != qmap.begin()) result += separator;
        result += it->first; // key
        if (!it->second.empty()) {
            result += "=";
            result += ert::http2comm::URLFunctions::encode(it->second); // value encoded just in case qmap decoded it before
        }
    }

    return result;
}

bool getFileContent(const std::string &filePath, std::string &content)
{
    std::ifstream ifs(filePath);

    if (!ifs.is_open()) {
        ert::tracing::Logger::error(ert::tracing::Logger::asString("Cannot open file '%s' !", filePath.c_str()), ERT_FILE_LOCATION);
        return false;
    }

    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Reading content from file '%s'", filePath.c_str()), ERT_FILE_LOCATION));
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    ifs.close();
    content = buffer.str();

    return true;
}

bool parseJsonContent(const std::string &content, nlohmann::json &jsonObject, bool writeException) {

    try {
        jsonObject = nlohmann::json::parse(content);
        LOGDEBUG(
            std::string msg("Json body parsed: ");
            msg += jsonObject.dump();
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
    }
    catch (nlohmann::json::parse_error& e)
    {
        std::stringstream ss;
        ss << "Json content parse error: " << e.what()
           << " | exception id: " << e.id
           << " | byte position of error: " << e.byte;

        if (writeException)
            jsonObject =  ss.str();

        // This will be debug, not error, because plain strings would always fail and some application
        //  could work with non-json bodies:
        LOGDEBUG(ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION));

        return false;
    }

    return true;
}

bool asAsciiString(const std::string &input, std::string &output) {

    bool result = true; // supposed printable by default

    if(input.empty()) {
        output = "<null>";
        return false;
    }

    std::for_each(input.begin(), input.end(), [&] (char const &c) {

        int printable = isprint(c);
        output += (printable ? c:'.');

        if(!printable) result = false;
    });

    return result;
}

bool asHexString(const std::string &input, std::string &output) {

    bool result = true; // supposed printable by default

    int byte;
    output = "0x";

    std::for_each(input.begin(), input.end(), [&] (char const &c) {

        byte = (c & 0xf0) >> 4;
        output += (byte >= 0 && byte <= 9) ? (byte + '0') : ((byte - 0xa) + 'a');
        byte = (c & 0x0f);
        output += (byte >= 0 && byte <= 9) ? (byte + '0') : ((byte - 0xa) + 'a');

        if (!isprint(c)) result = false;
    });

    return result;
}

bool fromHexString(const std::string &input, std::string &output) {

    bool result = true; // supposed successful by default

    bool has0x = (input.rfind("0x", 0) == 0);

    if((input.length() % 2) != 0) {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid hexadecimal string due to odd length (%d): %s", input.length(), input.c_str()), ERT_FILE_LOCATION));
        return false;
    }

    output = "";
    const char* src = input.data(); // fastest that accessing input[ii]
    unsigned char hex;
    int aux;

    for(int ii = 1 + (has0x ? 2:0), maxii = input.length(); ii < maxii; ii += 2) {
        if(isxdigit(aux = src[ii-1]) == 0) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid hexadecimal string: %s", input.c_str()), ERT_FILE_LOCATION));
            return false;
        }

        hex = ((aux >= '0' && aux <= '9') ? (aux - '0') : ((aux - 'a') + 0x0a)) << 4;

        if(isxdigit(aux = src[ii]) == 0) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Invalid hexadecimal string: %s", input.c_str()), ERT_FILE_LOCATION));
            return false;
        }

        hex |= (aux >= '0' && aux <= '9') ? (aux - '0') : ((aux - 'a') + 0x0a);
        output += hex;
    }

    return result;
}


}
}

