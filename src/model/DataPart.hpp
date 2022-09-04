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

#include <ert/multipart/Consumer.hpp>

namespace h2agent
{
namespace model
{

class DataPart;

/**
 * Multipart consumer specialization for h2agent
 */
class MyMultipartConsumer : public ert::multipart::Consumer {
    DataPart *data_part_;
    std::string content_type_;
    int data_count_;

public:
    MyMultipartConsumer(const std::string &boundary, DataPart *dp) : ert::multipart::Consumer(boundary), data_part_(dp), data_count_(1) {;}
    ~MyMultipartConsumer() {;}

    void receiveHeader(const std::string &name, const std::string &value);
    void receiveData(const std::string &data);
};

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
 * Other complex types like multipart could have an h2agent proprietary json representation
 * like this:
 *
 * <pre>
 * "<request|response>Body": {
 *   "multipart.1": {
 *     "headers": [ { "content-type": "text/html" } ]
 *     "content": "<h2 class=\"fg-white\">"
 *   },
 *   "multipart.2": {
 *     "headers": [ { "content-type": "application/octet-stream" } ]
 *     "content": "0xc0a80100"
 *   },
 *   "multipart.3": {
 *     "headers": [ { "content-type": "application/json" } ]
 *     "content": { "foo": "bar" }
 *   }
 * }
 * </pre>
 */
class DataPart {
    std::string str_; // raw data content: always filled with the original data received
    bool decoded_; // lazy decode indicator to skip multiple decode operations
    bool is_json_; // if not, we will use str_ as native source instead of json representation

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

public:
    /** Default constructor */
    DataPart() : decoded_(false), is_json_(false) {;}

    /** String constructor */
    DataPart(const std::string &str) : str_(str), decoded_(false), is_json_(false) {;}

    /** Move string constructor */
    DataPart(std::string &&str) : str_(std::move(str)), decoded_(false), is_json_(false) {;}

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
            is_json_ = other.is_json_;
            json_ = other.json_;
        }
        return *this;
    }

    /** Move assignment */
    DataPart& operator=(DataPart&& other) noexcept {
        if (this != &other) {
            str_ = std::move(other.str_);
            json_ = std::move(other.json_);
            decoded_ = other.decoded_; // it has no sense to move
            is_json_ = other.is_json_; // it has no sense to move
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

    /** getter to know if data was decoded as json (multipart is always representable as json) */
    bool isJson() const {
        return is_json_;
    }

    /** str as ascii string */
    std::string asAsciiString() const;

    /** getter for class data representation in json proprietary format */
    const nlohmann::json &getJson() const {
        return json_;
    }

    /** setters for class data */
    void assign(std::string &&str) {
        str_ = std::move(str);
        decoded_ = false;
        is_json_ = false;
    }
    void assign(const std::string &str) {
        str_ = str;
        decoded_ = false;
        is_json_ = false;
    }
    bool assignFromHex(const std::string &strAsHex);

    /** save json data decoded */
    void decodeContent(const std::string &content, const std::string &contentType, nlohmann::json &j);

    /** decode string data depending on content type */
    void decode(const nghttp2::asio_http2::header_map &headers /* to get the content-type */);

    friend class MyMultipartConsumer;
};

}
}

