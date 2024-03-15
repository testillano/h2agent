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
#include <DataPart.hpp>


namespace h2agent
{
namespace model
{

// Provision key:
typedef std::string admin_server_provision_key_t; // <inState>#<method>#<uri>

class MockServerData;
class MockClientData;
class Configuration;
class GlobalVariable;
class FileManager;
class SocketManager;


class AdminServerProvision
{
    bool employed_{};

    nlohmann::json json_{}; // provision reference

    admin_server_provision_key_t key_{}; // calculated in every load()
    std::regex regex_{}; // precompile key as possible regex for RegexMatching algorithm

    // Cached information:
    std::string in_state_{};
    std::string request_method_{};
    std::string request_uri_{};

    std::string out_state_{};
    unsigned int response_code_{};
    nghttp2::asio_http2::header_map response_headers_{};

    nlohmann::json response_body_{};
    std::string response_body_string_{};
    /* Tatsuhiro sends strings in response:
    int response_body_integer_{};
    double response_body_number_{};
    bool response_body_boolean_{};
    bool response_body_null_{};
    */

    unsigned int response_delay_ms_{};

    // Schemas:
    std::string request_schema_id_{};
    std::string response_schema_id_{};

    model::MockServerData *mock_server_events_data_{}; // just in case it is used
    model::MockClientData *mock_client_events_data_{}; // just in case it is used
    model::Configuration *configuration_{}; // just in case it is used
    model::GlobalVariable *global_variable_{}; // just in case it is used
    model::FileManager *file_manager_{}; // just in case it is used
    model::SocketManager *socket_manager_{}; // just in case it is used

    void loadTransformation(const nlohmann::json &j);

    std::vector<std::shared_ptr<Transformation>> transformations_{};

    // Three processing stages: get sources, apply filters and store targets:
    bool processSources(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        std::map<std::string, std::string>& variables,
                        const std::string &requestUri,
                        const std::string &requestUriPath,
                        const std::map<std::string, std::string> &requestQueryParametersMap,
                        const DataPart &requestBodyDataPart,
                        const nghttp2::asio_http2::header_map &requestHeaders,
                        bool &eraser,
                        std::uint64_t generalUniqueServerSequence,
                        bool usesResponseBodyAsTransformationJsonTarget, const nlohmann::json &responseBodyJson) const; // these two last parameters are used to
    // know if original response body provision
    // or the one dynamically modified, must be
    // used as source

    bool processFilters(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        const std::map<std::string, std::string>& variables,
                        std::smatch &matches,
                        std::string &source) const;

    bool processTargets(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        std::map<std::string, std::string>& variables,
                        const std::smatch &matches,
                        bool eraser,
                        bool hasFilter,
                        unsigned int &responseStatusCode,
                        nlohmann::json &responseBodyJson, // to manipulate json
                        std::string &responseBodyAsString, // to set native data (readable or not)
                        nghttp2::asio_http2::header_map &responseHeaders,
                        unsigned int &responseDelayMs,
                        std::string &outState,
                        std::string &outStateMethod,
                        std::string &outStateUri,
                        bool &breakCondition) const;


public:

    AdminServerProvision();

    // transform logic

    /**
     * Applies transformations vector over request received and ongoing response built
     * Also checks optional schema validation for incoming and/or outgoing traffic
     *
     * @param requestUri Request URI
     * @param requestUriPath Request URI path part
     * @param requestQueryParametersMap Query Parameters Map (if exists)
     * @param requestBodyDataPart Request Body data received (could be decoded if needed as source)
     * @param requestHeaders Request Headers Received
     * @param generalUniqueServerSequence HTTP/2 server monotonically increased sequence for every reception (unique)
     *
     * @param responseStatusCode Response status code filled by reference (if any transformation applies)
     * @param responseHeaders Response headers filled by reference (if any transformation applies)
     * @param responseBody Response body filled by reference (if any transformation applies)
     * @param responseDelayMs Response delay milliseconds filled by reference (if any transformation applies)
     * @param outState out-state for request context created, filled by reference (if any transformation applies)
     * @param outStateMethod method inferred towards a virtual server data entry created through a foreign out-state, filled by reference (if any transformation applies)
     * @param outStateUri uri inferred towards a virtual server data entry created through a foreign out-state, filled by reference (if any transformation applies)
     * @param requestSchema Optional json schema to validate incoming traffic. Nothing is done when nullptr is provided.
     * @param responseSchema Optional json schema to validate outgoing traffic. Nothing is done when nullptr is provided.
     */
    void transform( const std::string &requestUri,
                    const std::string &requestUriPath,
                    const std::map<std::string, std::string> &requestQueryParametersMap,
                    DataPart &requestBodyDataPart,
                    const nghttp2::asio_http2::header_map &requestHeaders,
                    std::uint64_t generalUniqueServerSequence,

                    unsigned int &responseStatusCode,
                    nghttp2::asio_http2::header_map &responseHeaders,
                    std::string &responseBody,
                    unsigned int &responseDelayMs,
                    std::string &outState,
                    std::string &outStateMethod,
                    std::string &outStateUri,
                    std::shared_ptr<h2agent::model::AdminSchema> requestSchema,
                    std::shared_ptr<h2agent::model::AdminSchema> responseSchema
                  ) const;

    // setters:

    /**
     * Load provision information
     *
     * @param j Json provision object
     * @param regexMatchingConfigured provision load depends on matching configuration (priority regexp)
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j, bool regexMatchingConfigured);

    /**
     * Sets the internal mock server data,
     * just in case it is used in event source
     */
    void setMockServerData(model::MockServerData *p) {
        mock_server_events_data_ = p;
    }

    /**
     * Sets the internal mock client data,
     * just in case it is used in event source
     */
    void setMockClientData(model::MockClientData *p) {
        mock_client_events_data_ = p;
    }

    /**
     * Sets the configuration reference,
     * just in case it is used in event target
     */
    void setConfiguration(model::Configuration *p) {
        configuration_ = p;
    }

    /**
     * Sets the global variables data reference,
     * just in case it is used in event source
     */
    void setGlobalVariable(model::GlobalVariable *p) {
        global_variable_ = p;
    }

    /**
     * Sets the file manager reference,
     * just in case it is used in event target
     */
    void setFileManager(model::FileManager *p) {
        file_manager_ = p;
    }

    /**
     * Sets the socket manager reference,
     * just in case it is used in event target
     */
    void setSocketManager(model::SocketManager *p) {
        socket_manager_ = p;
    }

    /**
     * Provision is being employed
     */
    void employ() {
        employed_ = true;
    }

    // getters:

    /**
     * Gets the provision key as '<in-state>|<request-method>|<request-uri>'
     *
     * @return Provision key
     */
    const admin_server_provision_key_t &getKey() const {
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

    /** Provisioned response body as json representation
     *
     * @return Response body as json representation
     */
    const nlohmann::json &getResponseBody() const {
        return response_body_;
    }

    /** Provisioned response body as string.
     *
     * This is useful as cached response data when the provision
     * response is not modified with transformation items.
     *
     * When the object is not a valid json, the data is
     * assumed as a readable string (TODO: refactor for multipart support)
     *
     * @return Response body string
     */
    const std::string &getResponseBodyAsString() const {
        return response_body_string_;
    }

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

    /** Provision was employed
     *
     * @return Boolean about if this provision has been used
     */
    bool employed() const {
        return employed_;
    }
};

}
}

