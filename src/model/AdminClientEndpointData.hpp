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
#include <mutex>
#include <shared_mutex>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <AdminClientEndpoint.hpp>

#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>


namespace h2agent
{
namespace model
{

// Map key will be string which has a hash function.
class AdminClientEndpointData : public Map<admin_client_endpoint_key_t, std::shared_ptr<AdminClientEndpoint>>
{
public:
    AdminClientEndpointData();
    ~AdminClientEndpointData() = default;

    // Load result
    enum LoadResult { Success = 0, Accepted, BadSchema, BadContent };

    /**
     * Json string representation for class information (json array)
     *
     * @return Json string representation ('[]' for empty array).
     */
    std::string asJsonString() const;

    /**
     * Loads client endpoint operation data
     *
     * @param j json document from operation body request
     * @param cr common resources references (general configuration, global variables, file manager, mock client events data)
     *
     * @return Load operation result
     */
    LoadResult load(const nlohmann::json &j, const common_resources_t &cr);

    /** Clears internal data (map and ordered keys vector)
     *
     * @return True if something was removed, false if already empty
     */
    bool clear();

    /**
     * Finds client endpoint item.
     *
     * @param id Client endpoint identifier
     *
     * @return Client endpoint information or null if missing
     */
    std::shared_ptr<AdminClientEndpoint> find(const admin_client_endpoint_key_t &id) const;

    /**
    * Gets client endpoint schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return client_endpoint_schema_;
    }

private:

    h2agent::jsonschema::JsonSchema client_endpoint_schema_{};

    LoadResult loadSingle(const nlohmann::json &j, const common_resources_t &cr);

    mutable mutex_t rw_mutex_{};
};

}
}

