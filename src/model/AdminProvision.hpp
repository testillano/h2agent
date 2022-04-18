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

#include <memory>
#include <string>
#include <vector>
#include <regex>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <AdminSchema.hpp>
#include <Transformation.hpp>
#include <TypeConverter.hpp>


#define DEFAULT_ADMIN_PROVISION_STATE "initial"


namespace h2agent
{
namespace model
{

// Provision key:
typedef std::string admin_provision_key_t;
// Future proof: instead of using a key = <method><uri>, we could agreggate them:
// typedef std::pair<std::string, std::string> admin_provision_key_t;
// But in order to compile, we need to define a hash function for the unordered map:
// https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR)
// https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)

void calculateAdminProvisionKey(admin_provision_key_t &key, const std::string &inState, const std::string &method, const std::string &uri);

class MockRequestData;


class AdminProvision
{
    nlohmann::json json_; // provision reference

    admin_provision_key_t key_; // calculated in every load()
    std::regex regex_; // precompile key as possible regex for PriorityMatchingRegex algorithm

    // Cached information:
    std::string request_method_{};
    std::string request_uri_{};
    std::string in_state_;

    std::string out_state_;
    unsigned int response_code_{};
    nghttp2::asio_http2::header_map response_headers_{};

    nlohmann::json response_body_;
    std::string response_body_string_{};
    /* Tatsuhiro sends strings in response:
    int response_body_integer_{};
    double response_body_number_{};
    bool response_body_boolean_{};
    bool response_body_null_{};
    */

    unsigned int response_delay_ms_;

    // Schemas:
    std::string request_schema_id_{};
    std::string response_schema_id_{};

    model::MockRequestData *mock_request_data_; // just in case it is used

    void loadResponseHeaders(const nlohmann::json &j);
    void loadTransformation(const nlohmann::json &j);

    std::vector<std::shared_ptr<Transformation>> transformations_;

    // Three processing stages: get sources, apply filters and store targets:
    bool processSources(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        const std::map<std::string, std::string>& variables,
                        const std::string &requestUri,
                        const std::string &requestUriPath,
                        const std::map<std::string, std::string> &requestQueryParametersMap,
                        bool requestBodyJsonParseable,
                        const nlohmann::json &requestBodyJson,
                        const nghttp2::asio_http2::header_map &requestHeaders,
                        bool &eraser,
                        std::uint64_t generalUniqueServerSequence) const;

    bool processFilters(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        const std::map<std::string, std::string>& variables,
                        std::smatch &matches,
                        std::string &source,
                        bool eraser) const;

    bool processTargets(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        std::map<std::string, std::string>& variables,
                        const std::smatch &matches,
                        bool eraser,
                        bool hasFilter,
                        unsigned int &responseStatusCode,
                        nlohmann::json &responseBodyJson,
                        nghttp2::asio_http2::header_map &responseHeaders,
                        unsigned int &responseDelayMs,
                        std::string &outState,
                        std::string &outStateMethod) const;


public:

    AdminProvision();

    // transform logic

    /**
     * Applies transformations vector over request received and ongoing reponse built
     * Also checks optional schema validation for incoming and/or outgoing traffic
     *
     * @param requestUri Request URI
     * @param requestUriPath Request URI path part
     * @param requestQueryParametersMap Query Parameters Map (if exists)
     * @param requestBody Request Body received
     * @param requestHeaders Request Headers Received
     * @param generalUniqueServerSequence HTTP/2 server monotonically increased sequence for every reception (unique)
     *
     * @param responseStatusCode Response status code filled by reference (if any transformation applies)
     * @param responseHeaders Response headers filled by reference (if any transformation applies)
     * @param responseBody Response body filled by reference (if any transformation applies)
     * @param responseDelayMs Response delay milliseconds filled by reference (if any transformation applies)
     * @param outState out-state for request context created, filled by reference (if any transformation applies)
     * @param outStateMethod out-state for request context created in foreign method (virtual server data entry created), filled by reference (if any transformation applies)
     * @param requestSchema Optional json schema to validate incoming traffic. Nothing is done when nullptr is provided.
     * @param responseSchema Optional json schema to validate outgoing traffic. Nothing is done when nullptr is provided.
     */
    void transform( const std::string &requestUri,
                    const std::string &requestUriPath,
                    const std::map<std::string, std::string> &requestQueryParametersMap,
                    const std::string &requestBody,
                    const nghttp2::asio_http2::header_map &requestHeaders,
                    std::uint64_t generalUniqueServerSequence,

                    unsigned int &responseStatusCode,
                    nghttp2::asio_http2::header_map &responseHeaders,
                    std::string &responseBody,
                    unsigned int &responseDelayMs,
                    std::string &outState,
                    std::string &outStateMethod,
                    std::shared_ptr<h2agent::model::AdminSchema> requestSchema,
                    std::shared_ptr<h2agent::model::AdminSchema> responseSchema
                  ) const;

    // setters:

    /**
     * Load provision information
     *
     * @param j Json provision object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j, bool priorityMatchingRegexConfigured);

    /**
     * Sets the internal mock request data,
     * just in case it is used in event source
     */
    void setMockRequestData(model::MockRequestData *p) {
        mock_request_data_ = p;
    }

    // getters:

    /**
     * Gets the provision key as '<in-state>|<request-method>|<request-uri>'
     *
     * @return Provision key
     */
    const admin_provision_key_t &getKey() const {
        return key_;
    }

    /**
     * Json for class information
     *
     * @return Json object
     */
    const nlohmann::json &getJson() const {
        return json_;
    }

    /**
     * Precompiled regex for provision key
     *
     * @return regex
     */
    const std::regex &getRegex() const {
        return regex_;
    }

    /** Provisioned out state
     *
     * @return Out state
     */
    const std::string &getOutState() const {
        return out_state_;
    }

    /** Provisioned in state
     *
     * @return In state
     */
    const std::string &getInState() const {
        return in_state_;
    }

    /** Provisioned response code
     *
     * @return Response code
     */
    unsigned int getResponseCode() const {
        return response_code_;
    }

    /** Provisioned response headers
     *
     * @return Response headers
     */
    const nghttp2::asio_http2::header_map &getResponseHeaders() const {
        return response_headers_;
    }

    /** Provisioned response body
     *
     * @return Response body
     */
    const nlohmann::json &getResponseBody() const {
        return response_body_;
    }

    /** Provisioned response body string
     *
     * @return Response body string
     */
    const std::string &getResponseBodyAsString() const {
        return response_body_string_;
    }

//    /** Provisioned response body integer
//     *
//     * @return Response body integer
//     */
//     const int &getResponseBodyAsInteger() const {
//        return response_body_integer_;
//    }
//
//    /** Provisioned response body number
//     *
//     * @return Response body number
//     */
//    const double &getResponseBodyAsNumber() const {
//        return response_body_number_;
//    }
//
//    /** Provisioned response body boolean
//     *
//     * @return Response body boolean
//     */
//    bool getResponseBodyAsBoolean() const {
//        return response_body_boolean_;
//    }
//
//    /** Provisioned response body null
//     *
//     * @return Response body null
//     */
//    bool getResponseBodyAsNull() const {
//        return response_body_null_;
//    }

    /** Provisioned response delay milliseconds
     *
     * @return Response delay milliseconds
     */
    unsigned int getResponseDelayMilliseconds() const {
        return response_delay_ms_;
    }

    /** Provisioned request schema identifier
     *
     * @return Request schema string identifier
     */
    const std::string &getRequestSchemaId() const {
        return request_schema_id_;
    }

    /** Provisioned response schema identifier
     *
     * @return Response schema string identifier
     */
    const std::string &getResponseSchemaId() const {
        return response_schema_id_;
    }
};

}
}

