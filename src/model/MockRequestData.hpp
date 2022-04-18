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

#include <vector>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <MockRequests.hpp>

#include <JsonSchema.hpp>


namespace h2agent
{
namespace model
{

/**
 * This class stores the requests history.
 *
 * This is useful to post-verify the internal data (content and schema validation) on testing system.
 * Also, the last request for an specific key, is used to know the state which is used to get the
 * corresponding provision information.
 */
class MockRequestData : public Map<mock_requests_key_t, std::shared_ptr<MockRequests>>
{
    h2agent::jsonschema::JsonSchema requests_schema_;

    bool checkSelection(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber) const;
    bool string2uint64(const std::string &input, std::uint64_t &output) const;

    mutable mutex_t rw_mutex_;

public:
    MockRequestData() {};
    ~MockRequestData() = default;

    /**
     * Loads request data
     *
     * @param pstate Previous request state
     * @param state Request state
     * @param method Request method
     * @param uri Request URI path
     * @param headers Request headers
     * @param body Request body
     *
     * @param responseStatusCode Response status code
     * @param responseHeaders Response headers
     * @param responseBody Response body
     * @param serverSequence Server sequence
     * @param responseDelayMs Response delay in milliseconds
     *
     * @param historyEnabled Requests complete history storage
     * @param virtualOriginComingFromMethod Marks event as virtual one, adding a field with the origin method which caused it. Non-virtual by default (empty parameter).
     */
    void loadRequest(const std::string &pstate, const std::string &state, const std::string &method, const std::string &uri, const nghttp2::asio_http2::header_map &headers, const std::string &body,
                     unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs,
                     bool historyEnabled, const std::string &virtualOriginComingFromMethod = "");

    /** Clears internal data
     *
     * @param somethingDeleted boolean to know if something was deleted, by reference
     * @param requestMethod Request method to filter selection
     * @param requestUri Request URI path to filter selection
     * @param requestNumber Request history number (1..N) to filter selection.
     * If empty, whole history is removed for method/uri provided.
     * If provided '-1', the latest event is selected.
     *
     * @return Boolean about success of operation (not related to the number of events removed)
     */
    bool clear(bool &somethingDeleted, const std::string &requestMethod = "", const std::string &requestUri = "", const std::string &requestNumber = "");

    /**
     * Json string representation for class information filtered
     *
     * @param requestMethod Request method to filter selection. Mandatory if 'requestUri' is provided:
     * @param requestUri Request URI path to filter selection. Mandatory if 'requestMethod' is provided.
     * @param requestNumber Request history number (1..N) to filter selection. Optional, but cannot be provided alone (needs 'requestUri' and 'requestMethod').
     * If empty, whole history is selected for method/uri provided.
     * If provided '-1' (unsigned long long max), the latest event is selected.
     * @param validQuery Boolean result passed by reference.
     *
     * @return Json string representation ('null' when nothing is found).
     */
    std::string asJsonString(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber, bool &validQuery) const;

    /**
     * Json string representation for class summary (total number of events and first/last/random keys).
     *
     * @param maxKeys Maximum number of keys to be displayed in the summary (protection for huge server data size). No limit by default.
     *
     * @return Json string representation.
     */
    std::string summary(const std::string &maxKeys = "") const;

    /**
     * Gets the mock request in specific position
     *
     * @param requestMethod Request method to filter selection
     * @param requestUri Request URI path to filter selection
     * @param requestNumber Request history number (1..N) to filter selection.
     * Value '-1' (unsigned long long max) selects the latest event.
     * Value '0' is not accepted, and null will be returned in this case.
     *
     * All the parameters are mandatory to select the single request event.
     * If any is missing, nullptr is returned.
     *
     * @return mock request pointer
     * @see size()
     */
    std::shared_ptr<MockRequest> getMockRequest(const std::string &requestMethod, const std::string &requestUri, const std::string &requestNumber) const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json asJson() const;

    /**
     * Finds most recent mock context entry.
     *
     * @param method Request method which was received
     * @param uri Request URI path which was received
     * @param state Request current state filled by reference. If nothing found, 'initial' will be set
     *
     * @return Boolean about if the request is found or not
     */
    bool findLastRegisteredRequest(const std::string &method, const std::string &uri, std::string &state) const;


    /**
     * Loads requests schema for optional validation
     *
     * @param schema Json schema document
     *
     * @return Sucessful operaiton
     */
    bool loadRequestsSchema(const nlohmann::json& schema);
};

}
}

