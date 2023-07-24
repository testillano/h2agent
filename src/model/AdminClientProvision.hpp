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

#include <memory>
#include <string>
#include <vector>
#include <chrono>

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
typedef std::string admin_client_provision_key_t; // <inState>#<client provision id>

class MockClientData;
class MockServerData;
class Configuration;
class GlobalVariable;
class FileManager;
class SocketManager;


class AdminClientProvision
{
    nlohmann::json json_{}; // provision reference

    admin_client_provision_key_t key_{}; // calculated in every load()

    // Cached information:
    std::string in_state_{};
    std::string client_provision_id_{};

    std::string out_state_{};
    std::string client_endpoint_id_{};
    std::string request_method_{};
    std::string request_uri_{};
    nghttp2::asio_http2::header_map request_headers_{};
    nlohmann::json request_body_{};
    std::string request_body_string_{};

    unsigned int request_delay_ms_{};
    unsigned int request_timeout_ms_{};

    // Schemas:
    std::string request_schema_id_{};
    std::string response_schema_id_{};

    model::MockClientData *mock_client_events_data_{}; // just in case it is used
    model::MockServerData *mock_server_events_data_{}; // just in case it is used
    model::Configuration *configuration_{}; // just in case it is used
    model::GlobalVariable *global_variable_{}; // just in case it is used
    model::FileManager *file_manager_{}; // just in case it is used
    model::SocketManager *socket_manager_{}; // just in case it is used

    void loadTransformation(std::vector<std::shared_ptr<Transformation>> &transformationsVector, const nlohmann::json &j);

    std::vector<std::shared_ptr<Transformation>> transformations_{};
    std::vector<std::shared_ptr<Transformation>> on_response_transformations_{};

    // Dynamic load parameters:
    std::uint64_t seq_{};
    std::uint64_t seq_begin_{};
    std::uint64_t seq_end_{};
    unsigned int rps_{};
    bool repeat_{};

    void saveDynamics();

    /*
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
                            nlohmann::json &responseBodyJson, // to manipulate json
                            std::string &responseBodyAsString, // to set native data (readable or not)
                            nghttp2::asio_http2::header_map &responseHeaders,
                            unsigned int &responseDelayMs,
                            std::string &outState,
                            std::string &outStateMethod,
                            std::string &outStateUri) const;
    */


public:

    AdminClientProvision();

    // transform logic

    /**
     * Applies transformations vector over request processed
     * Also checks optional schema validation for outgoing traffic
     *
     * @param requestMethod Request method resulting from provision & transformation
     * @param requestUri Request uri (include query parameters) resulting from provision & transformation
     * @param requestBody Request body resulting from provision & transformation
     * @param requestHeaders Request headers resulting from provision & transformation
     * @param outState out-state for request processed resulting from provision & transformation
     * @param requestSchema Optional json schema to validate outgoing traffic. Nothing is done when nullptr is provided.
     * @param requestDelayMs Request delay milliseconds resulting from provision & transformation
     * @param requestTimeoutMs Request timeout milliseconds resulting from provision & transformation
     * @param error Error detail for unexpected cases
     */
    void transform( std::string &requestMethod,
                    std::string &requestUri,
                    std::string &requestBody,
                    nghttp2::asio_http2::header_map &requestHeaders,
                    std::string &outState,
                    std::shared_ptr<h2agent::model::AdminSchema> requestSchema,
                    unsigned int &requestDelayMs,
                    unsigned int &requestTimeoutMs,
                    std::string &error
                  ) const;
    // setters:

    /**
     * Update dynamics configuration for triggering
     *
     * @param sequenceBegin Range initial sequence
     * @param sequenceEnd Range final sequence
     * @param rps Rate for sequences triggering (in events per second)
     * @param repeat Repeat range processing when exhausted
     *
     * @return Operation success
     */
    bool updateTriggering(const std::string &sequenceBegin, const std::string &sequenceEnd, const std::string &rps, const std::string &repeat);

    /**
     * Load provision information
     *
     * @param j Json provision object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j);

    /**
     * Sets the internal mock client data,
     * just in case it is used in event source
     */
    void setMockClientData(model::MockClientData *p) {
        mock_client_events_data_ = p;
    }

    /**
     * Sets the internal mock server data,
     * just in case it is used in event source
     */
    void setMockServerData(model::MockServerData *p) {
        mock_server_events_data_ = p;
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

    // getters:

    /**
     * Gets the provision identifier
     *
     * @return Client provision id
     */
    const std::string &getClientProvisionId() const {
        return client_provision_id_;
    }

    /**
     * Gets the provision key (identifier)
     *
     * @return Provision key
     */
    const admin_client_provision_key_t &getKey() const {
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
     * Provisioned client endpoint id
     *
     * @return Client endpoint id
     */
    const std::string &getClientEndpointId() const {
        return client_endpoint_id_;
    }

    /**
     * Provisioned out state
     *
     * @return Out state
     */
    const std::string &getOutState() const {
        return out_state_;
    }

    /**
     * Provisioned request method
     *
     * @return Request method
     */
    const std::string &getRequestMethod() const {
        return request_method_;
    }

    /**
     * Provisioned request uri
     *
     * @return Request uri
     */
    const std::string &getRequestUri() const {
        return request_uri_;
    }

    /**
     * Provisioned request headers
     *
     * @return Request headers
     */
    const nghttp2::asio_http2::header_map &getRequestHeaders() const {
        return request_headers_;
    }

    /**
     * Provisioned request body as json representation
     *
     * @return Request body as json representation
     */
    const nlohmann::json &getRequestBody() const {
        return request_body_;
    }

    /**
     * Provisioned response body as string.
     *
     * This is useful as cached request data when the provision
     * request is not modified with transformation items.
     *
     * When the object is not a valid json, the data is
     * assumed as a readable string (TODO: refactor for multipart support)
     *
     * @return Request body string
     */
    const std::string &getRequestBodyAsString() const {
        return request_body_string_;
    }

    /**
     * Provisioned request delay milliseconds
     *
     * @return Request delay milliseconds
     */
    unsigned int getRequestDelayMilliseconds() const {
        return request_delay_ms_;
    }

    /**
     * Provisioned request timeout milliseconds
     *
     * @return Request timeout milliseconds
     */
    unsigned int getRequestTimeoutMilliseconds() const {
        return request_timeout_ms_;
    }

    /**
     * Provisioned request schema identifier
     *
     * @return Request schema string identifier
     */
    const std::string &getRequestSchemaId() const {
        return request_schema_id_;
    }

    /**
     * Provisioned response schema identifier
     *
     * @return Response schema string identifier
     */
    const std::string &getResponseSchemaId() const {
        return response_schema_id_;
    }

    /**
     * Current sequence
     *
     * @return Provision sequence
     */
    const std::uint64_t & getSeq() const {
        return seq_;
    }
};

}
}

