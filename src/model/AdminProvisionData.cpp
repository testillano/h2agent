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

#include <AdminProvisionData.hpp>


namespace h2agent
{
namespace model
{

AdminProvisionData::AdminProvisionData() {
}

std::string AdminProvisionData::asJsonString(bool ordered) const {

    nlohmann::json result;

    if (ordered) {
        for (auto it = ordered_keys_.begin(); it != ordered_keys_.end(); it++) {
            auto element =  map_.find(*it);
            result.push_back(element->second->getJson());
        };
    }
    else {
        for (auto it = map_.begin(); it != map_.end(); it++) {
            result.push_back(it->second->getJson());
        };
    }

    // guarantee "null" if empty (nlohmann could change):
    return (result.empty() ? "null":result.dump());
}

bool AdminProvisionData::load(const nlohmann::json &j, bool priorityMatchingRegexConfigured) {

    // Provision object to fill:
    auto provision = std::make_shared<AdminProvision>();

    if (provision->load(j, priorityMatchingRegexConfigured)) {

        // Push the key in the map:
        admin_provision_key_t key = provision->getKey();
        add(key, provision);

        // Push the key just in case we configure ordered algorithm 'PriorityMatchingRegex':
        write_guard_t guard(rw_mutex_);
        ordered_keys_.push_back(key);

        return true;
    }

    return false;
}

bool AdminProvisionData::clear()
{
    bool result = (size() != 0);

    map_.clear();

    write_guard_t guard(rw_mutex_);
    ordered_keys_.clear();

    return result;
}

std::shared_ptr<AdminProvision> AdminProvisionData::find(const std::string &inState, const std::string &method, const std::string &uri) const {
    admin_provision_key_t key;
    calculateAdminProvisionKey(key, inState, method, uri);

    auto it = map_.find(key);
    if (it != end())
        return it->second;

    return nullptr;
}

std::shared_ptr<AdminProvision> AdminProvisionData::findWithPriorityMatchingRegex(const std::string &inState, const std::string &method, const std::string &uri) const {
    admin_provision_key_t key;
    calculateAdminProvisionKey(key, inState, method, uri);

    for (auto it = ordered_keys_.begin(); it != ordered_keys_.end(); it++) {
        auto provision = map_.find(*it)->second;
        if (std::regex_match(key, provision->getRegex()))
            return provision;
    };

    return nullptr;
}

}
}

