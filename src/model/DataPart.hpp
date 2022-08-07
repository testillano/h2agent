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

#include <nghttp2/asio_http2_server.h>

#include <string>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

/**
 * DataPart to store request/response body data
 * Also provides a way to represent data in json format, even for basic string values.
 *
 * So, when the body data is human-readable, the field "<request|response>Body"
 * will be the string itself as readable string:
 *
 * <pre>
 * "<request|response>Body": "this is human-readable text content"
 * </pre>
 *
 * When the body data is not human-readable, the field "<request|response>Body"
 * will be the data itself as hexadecimal string with prefix '0x':
 *
 * <pre>
 * "<request|response>Body": "0xc0a80100"
 * </pre>
 *
 * It is important to know the content-type, to understand that '0x' is not
 * part of an arbitrary string but an indicator of binary internal data which
 * could not be represented directly.
 *
 * When the body data is json content, the field "<request|response>Body"
 * will be the json object itself:
 *
 * <pre>
 * "<request|response>Body": { "foo": "bar" }
 * </pre>
 *
 * Other complex types like multipart could have a proprietary json representation
 * like this:
 *
 * <pre>
 * "<request|response>Body": {
 *   "multipart.1": {
 *     "content-type": "text/html",
 *     "content-data": "<h2 class=\"fg-white\">"
 *   },
 *   "multipart.2": {
 *     "content-type": "application/octet-stream",
 *     "content-data": "0xc0a80100"
 *   },
 *   "multipart.3": {
 *     "content-type": "application/json",
 *     "content-data": { "foo": "bar" }
 *   }
 * }
 * </pre>
 */
class DataPart {
    std::string str_; // raw data content: always filled with the original data received
    bool decoded_; // lazy decode indicator to skip multiple decode operations

    nlohmann::json json_; // data json representation valid for:
    // 1) parse json strings received (application/json)
    // 2) represent proprietary multipart json structure (multipart/?)
    // 3) represent human-readable strings (https://stackoverflow.com/questions/7487869/is-this-simple-string-considered-valid-json)
    // 4) represent non human-readable strings, as hex string prefixed with '0x'.

    // Note that 3) and 4) are using nlohmann::json to store a string, which is a shortcut for class json representation.
    // You shall cast it to string or use special library getters:
    //   std::string content = doc["example"]
    //   - or -
    //   doc["example"].get<std::string>()


    //std::vector<DataPart> v_; // multipart support

public:
    /** Default constructor */
    DataPart() : decoded_(false) {;}

    /** String constructor */
    DataPart(const std::string &str) : str_(str), decoded_(false) {;}

    /** Move string constructor */
    DataPart(std::string &&str) : str_(std::move(str)), decoded_(false) {;}

    /** Constructor */
    DataPart(const DataPart &bd) {
        *this = bd;
    }

    /** Move constructor */
    DataPart(DataPart &&bd) {
        *this = std::move(bd);
    }

    /** Destructor */
    ~DataPart() {;}

    /** Copy assignment */
    DataPart& operator=(const DataPart& other) noexcept {
        if (this != &other) {
            str_ = other.str_;
            decoded_ = other.decoded_;
            json_ = other.json_;
        }
        return *this;
    }

    /** Move assignment */
    DataPart& operator=(DataPart&& other) noexcept {
        if (this != &other) {
            str_ = std::move(other.str_);
            json_ = std::move(other.json_);
            decoded_ = std::move(other.decoded_);
        }
        return *this;
    }

    /** comparison operator */
    bool operator==(const DataPart &other) const {
        return str() == other.str();
    }

    /** getter for class data */
    const std::string &str() const {
        return str_;
    }

    /** str as ascii string */
    std::string asAsciiString() const;

    /** getter for class data representation in json propietary format */
    const nlohmann::json &getJson() const {
        return json_;
    }

    /** setters for class data */
    void assign(std::string &&str) {
        str_ = std::move(str);
        decoded_ = false;
    }
    void assign(const std::string &str) {
        str_ = str;
        decoded_ = false;
    }
    bool assignFromHex(const std::string &strAsHex);

    /** decode string data depending on content type */
    void decode(const std::string& contentType);
};

}
}

