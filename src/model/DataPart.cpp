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

#include <functions.hpp>

#include <DataPart.hpp>


namespace h2agent
{
namespace model
{

bool DataPart::assignFromHex(const std::string &strAsHex) {
    return h2agent::model::fromHexString(strAsHex, str_);
}

void DataPart::decode(const std::string& contentType) {

    //// Normalize content-type:
    //std::string ct = contentType;
    //std::transform(ct.begin(), ct.end(), ct.begin(), [](unsigned char c) {
    //    return std::tolower(c);
    //});

    if (contentType == "application/json") {
        h2agent::model::parseJsonContent(str_, json_, true /* write exception message */);
    }
    else if (contentType.rfind("text/", 0) == 0) {
        json_ = str_;
    }
    //else if (contentType.rfind("multipart/", 0) == 0) {
    //}
    else {
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Unsupported content-type '%s' decoding for json representation. Generic procedure will be applied (hex string for non printable data received)", contentType.c_str()), ERT_FILE_LOCATION));
        std::string output;
        if (h2agent::model::asHexString(str_, output)) {
            json_ = str_;
        }
        else {
            json_ = std::move(output);
        }
    }
}

}
}

