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

#include <AdminClientProvisionData.hpp>
#include <functions.hpp>


namespace h2agent
{
namespace model
{

AdminClientProvisionData::AdminClientProvisionData() {
    client_provision_schema_.setJson(h2agent::adminSchemas::client_provision); // won't fail
}

std::string AdminClientProvisionData::asJsonString(bool getUnused) const {

    nlohmann::json result = nlohmann::json::array();

    this->forEach([&](const KeyType& k, const ValueType& value) {
        if (!(getUnused && value->employed())) {
            result.push_back(value->getJson());
        }
    });

    // Provision is shown as an array regardless if there is 1 item, N items or none ([]):
    return (result.dump());
}

AdminClientProvisionData::LoadResult AdminClientProvisionData::loadSingle(const nlohmann::json &j, const common_resources_t &cr) {

    std::string error{};
    if (!client_provision_schema_.validate(j, error)) {
        return BadSchema;
    }

    // Provision object to fill:
    auto provision = std::make_shared<AdminClientProvision>();

    if (provision->load(j)) {

        // Push the key in the map:
        admin_client_provision_key_t key = provision->getKey();

        // Push the key just in case we configure ordered algorithm 'RegexMatching'.
        // So, we always have both lists available; as each algorithm finds within the proper
        // list, we don't need to drop provisions when swaping the matching mode on the fly:
        add(key, provision);

        // Set common resources:
        provision->setAdminData(cr.AdminDataPtr);
        provision->setConfiguration(cr.ConfigurationPtr);
        provision->setGlobalVariable(cr.GlobalVariablePtr);
        provision->setFileManager(cr.FileManagerPtr);
        provision->setSocketManager(cr.SocketManagerPtr);
        provision->setMockClientData(cr.MockClientDataPtr);
        provision->setMockServerData(cr.MockServerDataPtr);

        return Success;
    }

    return BadContent;
}

AdminClientProvisionData::LoadResult AdminClientProvisionData::load(const nlohmann::json &j, const common_resources_t &cr) {

    if (j.is_array()) {
        for (auto it : j) // "it" is of type json::reference and has no key() member
        {
            LoadResult result = loadSingle(it, cr);
            if (result != Success)
                return result;
        }

        return Success;
    }

    return loadSingle(j, cr);
}

std::shared_ptr<AdminClientProvision> AdminClientProvisionData::find(const std::string &inState, const std::string &clientProvisionId) const {

    admin_client_provision_key_t key{};
    h2agent::model::calculateStringKey(key, inState, clientProvisionId);

    bool exists{};
    auto result = get(key, exists);

    return (exists ? result:nullptr);
}

}
}

