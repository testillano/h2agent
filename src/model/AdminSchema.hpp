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

#include <string>

#include <nlohmann/json.hpp>

#include <JsonSchema.hpp>


namespace h2agent
{
namespace model
{

// Schema key:
typedef std::string schema_key_t;


class AdminSchema
{
    nlohmann::json json_{}; // schema reference

    schema_key_t key_{};
    h2agent::jsonschema::JsonSchema schema_{};  // schema content

public:

    AdminSchema() {;}

    // setters:

    /**
     * Load schema information
     *
     * @param j Json schema object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j);

    // getters:

    /**
     * Gets the schema key (identifier)
     *
     * @return Schema key
     */
    const schema_key_t &getKey() const {
        return key_;
    }

    /** Json for class information
     *
     * @return Json object
     */
    const nlohmann::json &getJson() const {
        return json_;
    }

    /**
    * Validates json document against schem content.
    *
    * @param j document to validate
    * @param error Error passed by reference
    *
    * @return boolean about if json document is valid against json schema
    */
    bool validate(const nlohmann::json& j, std::string &error) const;
};

}
}

