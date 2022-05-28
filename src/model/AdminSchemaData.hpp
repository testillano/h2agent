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

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <AdminSchema.hpp>

#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>


namespace h2agent
{
namespace model
{

// Map key will be string which has a hash function.
class AdminSchemaData : public Map<schema_key_t, std::shared_ptr<AdminSchema>>
{
public:
    AdminSchemaData();
    ~AdminSchemaData() = default;

    // Load result
    enum LoadResult { Success = 0, BadSchema, BadContent };

    /**
     * Json string representation for class information (json array)
     *
     * @return Json string representation ('[]' for empty array).
     */
    std::string asJsonString() const;

    /**
     * Loads schema operation data
     *
     * @param j Json document from operation body request
     *
     * @return Load operation result
     */
    LoadResult load(const nlohmann::json &j);

    /** Clears internal data (map)
     *
     * @return True if something was removed, false if already empty
     */
    bool clear();

    /**
     * Finds schema item
     *
     * @param id Schema identifier
     *
     * @return Schema information or null if missing
     */
    std::shared_ptr<AdminSchema> find(const std::string &id) const;

    /**
    * Gets schema operation schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return schema_schema_;
    }

private:

    h2agent::jsonschema::JsonSchema schema_schema_{};

    LoadResult loadSingle(const nlohmann::json &j);

    mutable mutex_t rw_mutex_{};
};

}
}

