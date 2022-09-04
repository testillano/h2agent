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
#include <ert/http2comm/Http2Headers.hpp>

#include <functions.hpp>

#include <DataPart.hpp>


namespace h2agent
{
namespace model
{

// MyMultipartConsumer

// {
//   "multipart.1": {
//     "content": "<h2 class=\"fg-white\">",
//     "headers": { "content-type": "text/html" }
//   },
//   "multipart.2": {
//     "content": "0xc0a80100",
//     "headers": { "content-type": "application/octet-stream" }
//   },
//   "multipart.3": {
//     "content": { "foo": "bar" },
//     "headers": { "content-type": "application/json" }
//   }
// }

void MyMultipartConsumer::receiveHeader(const std::string &name, const std::string &value) {

    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    if (n == "content-type") content_type_ = value;
    data_part_->json_["multipart." + std::to_string(data_count_)]["headers"][name] = value;
}

void MyMultipartConsumer::receiveData(const std::string &data) {
    nlohmann::json content;
    data_part_->decodeContent(data, content_type_, content);
    data_part_->json_["multipart." + std::to_string(data_count_)]["content"] = std::move(content);
    data_count_++;
}

// DataPart

std::string DataPart::asAsciiString() const {
    std::string result;
    h2agent::model::asAsciiString(str_, result);
    return result;
}

bool DataPart::assignFromHex(const std::string &strAsHex) {
    decoded_ = false;
    return h2agent::model::fromHexString(strAsHex, str_);
}

void DataPart::decodeContent(const std::string &content, const std::string &contentType, nlohmann::json &jsonRepresentation) {

    //// Normalize content-type:
    //std::string ct = contentType;
    //std::transform(ct.begin(), ct.end(), ct.begin(), [](unsigned char c) {
    //    return std::tolower(c);
    //});

    if (contentType == "application/json") {
        /*is_json_ = */h2agent::model::parseJsonContent(content, jsonRepresentation, true /* write exception message */);
        is_json_ = true; // even if json is invalid, we prefer to show the error description, but obey the content-type
    }
    else if (contentType.rfind("text/", 0) == 0) {
        jsonRepresentation = content;
    }
    else if (contentType.rfind("multipart/", 0) == 0) { // i.e.: multipart/related; boundary=abcdef12345
        size_t pos = contentType.find("boundary=");
        std::string boundary = (pos != std::string::npos) ? contentType.substr(pos + 9) : "";

        if (boundary.empty()) {
            ert::tracing::Logger::error(ert::tracing::Logger::asString("Invalid multipart boundary received: '%s'", boundary.c_str()), ERT_FILE_LOCATION);
        }
        else {
            is_json_ = true; // multipart decoded will always be represented by h2agent proprietary json:
            MyMultipartConsumer mpConsumer(boundary, this);
            mpConsumer.decode(content);
        }
    }
    else {
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Unsupported content-type '%s' decoding for json representation. Generic procedure will be applied (hex string for non printable data received)", contentType.c_str()), ERT_FILE_LOCATION));
        std::string output;
        if (h2agent::model::asHexString(content, output)) {
            jsonRepresentation = content;
        }
        else {
            jsonRepresentation = std::move(output);
        }
    }
}

void DataPart::decode(const nghttp2::asio_http2::header_map &headers) {

    if (decoded_) {
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Data part already decoded (%s). Skipping operation !", ert::http2comm::headersAsString(headers).c_str()), ERT_FILE_LOCATION));
        return;
    }

    if (str_.empty()) {
        decoded_ = true;
        is_json_ = false;
        return;
    }

    std::string contentType;
    auto ct_it = headers.find("content-type");
    if (ct_it != headers.end()) {
        contentType = ct_it->second.value;
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("content-type: %s", contentType.c_str()), ERT_FILE_LOCATION));
    }

    decodeContent(str_, contentType, json_);

    decoded_ = true;

    LOGDEBUG(
        std::string msg;
    try {
        msg = json_.dump();
    }
    catch (const std::exception& e)
    {
        msg = e.what();
    }
    ert::tracing::Logger::debug(ert::tracing::Logger::asString("DataPart json representation: %s", msg.c_str()), ERT_FILE_LOCATION);
    );
}

}
}

