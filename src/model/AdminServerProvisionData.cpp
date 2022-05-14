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

#include <AdminServerProvisionData.hpp>


namespace h2agent
{
namespace model
{

AdminServerProvisionData::AdminServerProvisionData() {
    server_provision_schema_.setJson(h2agent::adminSchemas::server_provision); // won't fail
}

std::string AdminServerProvisionData::asJsonString(bool ordered) const {

    nlohmann::json result = nlohmann::json::array();

    if (ordered) {
        for (auto it = ordered_keys_.begin(); it != ordered_keys_.end(); it++) {
            auto element =  get(*it);
            result.push_back(element->second->getJson());
        };
    }
    else {
        for (auto it = map_.begin(); it != map_.end(); it++) {
            result.push_back(it->second->getJson());
        };
    }

    // Provision is shown as an array regardless if there is 1 item, N items or none ([]):
    return (result.dump());
}

AdminServerProvisionData::LoadResult AdminServerProvisionData::loadSingle(const nlohmann::json &j, bool priorityMatchingRegexConfigured) {

    if (!server_provision_schema_.validate(j)) {
        return BadSchema;
    }

    // Provision object to fill:
    auto provision = std::make_shared<AdminServerProvision>();

    if (provision->load(j, priorityMatchingRegexConfigured)) {

        // Push the key in the map:
        admin_server_provision_key_t key = provision->getKey();
        add(key, provision);

        // Push the key just in case we configure ordered algorithm 'PriorityMatchingRegex'.
        // So, we always have both lists available; as each algorithm finds within the proper
        // list, we don't need to drop provisions when swaping the matching mode on the fly:
        write_guard_t guard(rw_mutex_);
        ordered_keys_.push_back(key);

        return Success;
    }

    return BadContent;
}

AdminServerProvisionData::LoadResult AdminServerProvisionData::load(const nlohmann::json &j, bool priorityMatchingRegexConfigured) {

    if (j.is_array()) {
        for (auto it : j) // "it" is of type json::reference and has no key() member
        {
            LoadResult result = loadSingle(it, priorityMatchingRegexConfigured);
            if (result != Success)
                return result;
        }

        return Success;
    }

    return loadSingle(j, priorityMatchingRegexConfigured);
}

bool AdminServerProvisionData::clear()
{
    bool result = (size() != 0);

    map_.clear();

    write_guard_t guard(rw_mutex_);
    ordered_keys_.clear();

    return result;
}

std::shared_ptr<AdminServerProvision> AdminServerProvisionData::find(const std::string &inState, const std::string &method, const std::string &uri) const {
    admin_server_provision_key_t key;
    calculateAdminServerProvisionKey(key, inState, method, uri);

    auto it = get(key);
    if (it != end())
        return it->second;

    return nullptr;
}

std::shared_ptr<AdminServerProvision> AdminServerProvisionData::findWithPriorityMatchingRegex(const std::string &inState, const std::string &method, const std::string &uri) const {
    admin_server_provision_key_t key;
    calculateAdminServerProvisionKey(key, inState, method, uri);

    for (auto it = ordered_keys_.begin(); it != ordered_keys_.end(); it++) {
        auto provision = get(*it)->second;
        if (std::regex_match(key, provision->getRegex()))
            return provision;
    };

    return nullptr;
}

}
}

