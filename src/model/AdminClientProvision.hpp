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

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_context.hpp>

#include <ert/http2comm/Http2Client.hpp>

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
class AdminData;
class Configuration;
class Vault;
class FileManager;
class SocketManager;


class AdminClientProvision
{
    bool employed_{};

    mutable nlohmann::json json_{}; // provision reference (mutable for lazy dynamics update)

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
    unsigned int expected_response_status_code_{}; // 0 = not configured

    // Schemas:
    std::string request_schema_id_{};
    std::string response_schema_id_{};
    std::shared_ptr<h2agent::model::AdminSchema> request_schema_{};
    std::shared_ptr<h2agent::model::AdminSchema> response_schema_{};

    model::MockClientData *mock_client_events_data_{}; // just in case it is used
    model::MockServerData *mock_server_events_data_{}; // just in case it is used
    model::AdminData *admin_data_{}; // just in case it is used
    model::Configuration *configuration_{}; // just in case it is used
    model::Vault *vault_{}; // just in case it is used
    model::FileManager *file_manager_{}; // just in case it is used
    model::SocketManager *socket_manager_{}; // just in case it is used

    void loadTransformation(std::vector<std::shared_ptr<Transformation>> &transformationsVector, const nlohmann::json &j);

    std::vector<std::shared_ptr<Transformation>> transformations_{};
    std::vector<std::shared_ptr<Transformation>> on_response_transformations_{};

    // Dynamic load parameters:
    std::int64_t seq_{};
    std::int64_t seq_begin_{};
    std::int64_t seq_end_{};
    unsigned int cps_{};
    bool repeat_{};

    // Timer-based triggering:
    boost::asio::steady_timer *timer_{};
    boost::asio::io_context *io_context_{};
    std::function<void()> tick_callback_{};
    std::chrono::steady_clock::time_point last_dynamics_save_{};
    void scheduleTick(bool first = true);

    void saveDynamics() const;

    // Three processing stages: get sources, apply filters and store targets:
    // When receivedResponse is not nullptr, response.* sources are available (post-response phase)
    bool processSources(std::shared_ptr<Transformation> transformation,
                        TypeConverter& sourceVault,
                        std::map<std::string, std::string>& variables,
                        const std::string &requestUri,
                        const nghttp2::asio_http2::header_map &requestHeaders,
                        bool &eraser,
                        std::uint64_t sendSeq,
                        bool usesRequestBodyAsTransformationJsonTarget, const nlohmann::json &requestBodyJson,
                        const ert::http2comm::Http2Client::response *receivedResponse = nullptr) const;

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
                        std::string &requestMethod,
                        std::string &requestUri,
                        nlohmann::json &requestBodyJson,
                        std::string &requestBodyAsString,
                        nghttp2::asio_http2::header_map &requestHeaders,
                        unsigned int &requestDelayMs,
                        unsigned int &requestTimeoutMs,
                        std::string &outState,
                        bool &breakCondition) const;

    void executeOnFilterFail(
            const std::vector<std::shared_ptr<Transformation>> &fallbacks,
            TypeConverter &sourceVault, std::map<std::string, std::string> &variables,
            const std::string &requestUri, const nghttp2::asio_http2::header_map &requestHeaders,
            std::uint64_t sendSeq, bool usesRequestBodyAsTransformationJsonTarget,
            const nlohmann::json &requestBodyJson,
            const ert::http2comm::Http2Client::response *receivedResponse,
            std::string &requestMethod, std::string &requestUri_out,
            nlohmann::json &requestBodyJson_out, std::string &requestBody,
            nghttp2::asio_http2::header_map &requestHeaders_out,
            unsigned int &requestDelayMs, unsigned int &requestTimeoutMs,
            std::string &outState, bool &breakCondition) const;


public:

    AdminClientProvision();
    ~AdminClientProvision() { stopTicking(); }

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
     * @param requestDelayMs Request delay milliseconds resulting from provision & transformation
     * @param requestTimeoutMs Request timeout milliseconds resulting from provision & transformation
     * @param error Error detail for unexpected cases
     */
    void transform( std::string &requestMethod,
                    std::string &requestUri,
                    std::string &requestBody,
                    nghttp2::asio_http2::header_map &requestHeaders,
                    std::string &outState,
                    unsigned int &requestDelayMs,
                    unsigned int &requestTimeoutMs,
                    std::string &error,
                    std::map<std::string, std::string> &variables
                  );

    /**
     * Applies on-response transformations over the received response
     *
     * @param requestUri Request URI that was sent
     * @param requestHeaders Request headers that were sent
     * @param receivedResponse Response received from server
     * @param sendSeq Client endpoint sending sequence
     * @param outState out-state updated by reference (may change based on response)
     *
     * @return True if response validation passed (status code + schema), false otherwise (chain should break)
     */
    bool transformResponse( const std::string &requestUri,
                            const nghttp2::asio_http2::header_map &requestHeaders,
                            const ert::http2comm::Http2Client::response &receivedResponse,
                            std::uint64_t sendSeq,
                            std::string &outState,
                            std::map<std::string, std::string> &variables
                          );

    /**
     * Provision is being employed
     */
    void employ() {
        employed_ = true;
    }

    // setters:

    /**
     * Update dynamics configuration for triggering
     *
     * @param sequenceBegin Range initial sequence
     * @param sequenceEnd Range final sequence
     * @param cps Rate for sequences triggering (in calls/provisions per second)
     * @param repeat Repeat range processing when exhausted
     *
     * @return Operation success
     */
    bool updateTriggering(const std::string &sequenceBegin, const std::string &sequenceEnd, const std::string &cps, const std::string &repeat);

    /**
     * Starts timer-based triggering at configured cps rate
     *
     * @param ioContext Timer io_context
     * @param tickCallback Callback invoked on each tick (should call sendClientRequest)
     */
    void startTicking(boost::asio::io_context *ioContext, std::function<void()> tickCallback);

    /**
     * Stops timer-based triggering
     */
    void stopTicking();

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
     * Sets the admin data reference,
     * just in case it is used in SchemaId filter
     */
    void setAdminData(model::AdminData *p) {
        admin_data_ = p;
    }

    /**
     * Sets the configuration reference,
     * just in case it is used in event target
     */
    void setConfiguration(model::Configuration *p) {
        configuration_ = p;
    }

    /**
     * Sets the vault data reference,
     * just in case it is used in event source
     */
    void setVault(model::Vault *p) {
        vault_ = p;
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
        saveDynamics(); // refresh before returning
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
     * Provisioned in state
     *
     * @return In state
     */
    const std::string &getInState() const {
        return in_state_;
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

    /** Provisioned request schema reference
     *
     * @return Request schema to validate incoming traffic, nullptr if missing
     */
    std::shared_ptr<h2agent::model::AdminSchema> getRequestSchema();

    /** Provisioned response schema reference
     *
     * @return Response schema to validate outgoing traffic, nullptr if missing
     */
    std::shared_ptr<h2agent::model::AdminSchema> getResponseSchema();

    /**
     * Current sequence
     *
     * @return Provision sequence
     */
    const std::int64_t & getSeq() const {
        return seq_;
    }

    void setSeq(std::int64_t seq) {
        seq_ = seq;
    }

    /** Configured requests per second
     *
     * @return cps value
     */
    unsigned int getCps() const {
        return cps_;
    }

    /** Configured expected response status code
     *
     * @return expected status code (0 = not configured)
     */
    unsigned int getExpectedResponseStatusCode() const {
        return expected_response_status_code_;
    }

    /** Provision was employed
     *
     * @return Boolean about if this provision has been used
     */
    bool employed() const {
        return employed_;
    }

    /**
     * Checks if this provision references event-dependent transformation types
     *
     * @return True if any transformation requires stored events
     */
    bool needsStorage() const {
        for (const auto* vec : {&transformations_, &on_response_transformations_}) {
            for (const auto& t : *vec) {
                auto st = t->getSourceType();
                auto tt = t->getTargetType();
                if (st == Transformation::ServerEvent || st == Transformation::ClientEvent ||
                    tt == Transformation::ServerEventToPurge || tt == Transformation::ClientEventToPurge)
                    return true;
            }
        }
        return false;
    }
};

}
}

