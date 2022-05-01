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

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminSchemaData.hpp>


namespace h2agent
{
namespace model
{

AdminSchemaData::AdminSchemaData() {
    schema_schema_.setJson(h2agent::adminSchemas::schema); // won't fail
}

std::string AdminSchemaData::asJsonString() const {

    nlohmann::json result = nlohmann::json::array();

    for (auto it = map_.begin(); it != map_.end(); it++) {
        result.push_back(it->second->getJson());
    };

    // Schema configuration is shown as an array regardless if there is 1 item, N items or none ([]):
    return (result.dump());
}

AdminSchemaData::LoadResult AdminSchemaData::loadSingle(const nlohmann::json &j) {

    if (!schema_schema_.validate(j)) {
        return BadSchema;
    }

    // Schema object to fill:
    auto schema = std::make_shared<AdminSchema>();

    if (schema->load(j)) {

        // Push the key in the map:
        schema_key_t key = schema->getKey();
        add(key, schema);

        return Success;
    }

    return BadContent;
}

AdminSchemaData::LoadResult AdminSchemaData::load(const nlohmann::json &j) {

    if (j.is_array()) {
        for (auto it : j) // "it" is of type json::reference and has no key() member
        {
            LoadResult result = loadSingle(it);
            if (result != Success)
                return result;
        }

        return Success;
    }

    return loadSingle(j);
}

bool AdminSchemaData::clear()
{
    bool result = (size() != 0);

    map_.clear();

    return result;
}

std::shared_ptr<AdminSchema> AdminSchemaData::find(const std::string &id) const {
    auto it = get(id);
    if (it != end())
        return it->second;

    return nullptr;
}

}
}

