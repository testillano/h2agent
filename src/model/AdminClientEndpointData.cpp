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

#include <string>
#include <regex>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminClientEndpointData.hpp>
#include <Configuration.hpp>


namespace h2agent
{
namespace model
{

AdminClientEndpointData::AdminClientEndpointData() {
    client_endpoint_schema_.setJson(h2agent::adminSchemas::client_endpoint); // won't fail
}

std::string AdminClientEndpointData::asJsonString() const {

    nlohmann::json result = nlohmann::json::array();

    read_guard_t guard(rw_mutex_);
    for (auto it = map_.begin(); it != map_.end(); it++) {
        //result.push_back(it->second->getJson());
        result.push_back(it->second->asJson()); // json_ is altered with connection status
    };

    // Client endpoint configuration is shown as an array regardless if there is 1 item, N items or none ([]):
    return (result.dump());
}

AdminClientEndpointData::LoadResult AdminClientEndpointData::loadSingle(const nlohmann::json &j, const common_resources_t &cr) {

    if (!client_endpoint_schema_.validate(j)) {
        return BadSchema;
    }

    // Client endpoint object to fill:
    auto clientEndpoint = std::make_shared<AdminClientEndpoint>();

    if (clientEndpoint->load(j)) {

        // Metrics data:
        clientEndpoint->setMetricsData(cr.MetricsPtr, cr.ResponseDelaySecondsHistogramBucketBoundaries, cr.MessageSizeBytesHistogramBucketBoundaries, cr.ApplicationName);

        // Push the key in the map:
        admin_client_endpoint_key_t key = clientEndpoint->getKey();

        // First find (avoid deadlock):
        auto registeredClientEndpoint = find(key);

        // Then write:
        write_guard_t guard(rw_mutex_);

        // host, port or secure changes implies re-creation for the client connection id:
        if (registeredClientEndpoint) {
            if (registeredClientEndpoint->getHost() != clientEndpoint->getHost() ||
                    registeredClientEndpoint->getPort() != clientEndpoint->getPort() ||
                    registeredClientEndpoint->getSecure() != clientEndpoint->getSecure()) {

                registeredClientEndpoint->load(j);
                if (!cr.ConfigurationPtr->getLazyClientConnection()) registeredClientEndpoint->connect(true /* from scratch */);
                return Accepted;
            }

            LOGINFORMATIONAL(
                bool updated = (registeredClientEndpoint->getPermit() != clientEndpoint->getPermit());
                ert::tracing::Logger::informational(ert::tracing::Logger::asString("Client endpoint '%s' has been updated%s", key.c_str(), updated ? "":" but no changes detected"), ERT_FILE_LOCATION);
            );
        }
        else {
            add(key, clientEndpoint);
            if (!cr.ConfigurationPtr->getLazyClientConnection()) clientEndpoint->connect();
        }

        return Success;

    }

    return BadContent;
}

AdminClientEndpointData::LoadResult AdminClientEndpointData::load(const nlohmann::json &j, const common_resources_t &cr) {

    if (j.is_array()) {
        bool oneAccepted = false;
        for (auto it : j) // "it" is of type json::reference and has no key() member
        {
            LoadResult result = loadSingle(it, cr);
            if (result != Success && result != Accepted)
                return result;
            if (result == Accepted) oneAccepted = true;
        }

        return oneAccepted ? Accepted:Success;
    }

    return loadSingle(j, cr);
}

bool AdminClientEndpointData::clear()
{
    write_guard_t guard(rw_mutex_);

    bool result = (size() != 0);

    map_.clear();

    return result;
}

std::shared_ptr<AdminClientEndpoint> AdminClientEndpointData::find(const admin_client_endpoint_key_t &id) const {

    read_guard_t guard(rw_mutex_);
    auto it = get(id);
    if (it != end())
        return it->second;

    return nullptr;
}

}
}

