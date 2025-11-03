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
#include <string>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <AdminClientProvision.hpp>

#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>


namespace h2agent
{
namespace model
{

// Map key will be string which has a hash function.
// We will agregate method and uri in a single string for that.
class AdminClientProvisionData : public Map<admin_client_provision_key_t, std::shared_ptr<AdminClientProvision>>
{
public:
    AdminClientProvisionData();
    ~AdminClientProvisionData() = default;

    using KeyType = admin_client_provision_key_t;
    using ValueType = std::shared_ptr<AdminClientProvision>;

    // Load result
    enum LoadResult { Success = 0, BadSchema, BadContent };

    /**
     * Json string representation for class information (json array)
     *
     * @param getUnused Print json array elements which was not used (false by default)
     *
     * @return Json string representation ('[]' for empty array).
     */
    std::string asJsonString(bool getUnused = false) const;

    /**
     * Loads client provision operation data
     *
     * @param j json document from operation body request
     * @param cr common resources references (general configuration, global variables, file manager, mock client events data)
     *
     * @return Load operation result
     */
    LoadResult load(const nlohmann::json &j, const common_resources_t &cr);

    /**
     * Finds provision item for traffic requirement.
     *
     * @param inState provision input state
     * @param clientProvisionId Provision identifier
     *
     * @return Provision information or null if missing
     */
    std::shared_ptr<AdminClientProvision> find(const std::string &inState, const std::string &clientProvisionId) const;

    /**
    * Gets provision schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return client_provision_schema_;
    }

private:

    h2agent::jsonschema::JsonSchema client_provision_schema_{};

    LoadResult loadSingle(const nlohmann::json &j, const common_resources_t &cr);
};

}
}

