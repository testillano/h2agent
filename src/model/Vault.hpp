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
#include <JsonSchema.hpp>


namespace h2agent
{
namespace model
{

class WaitManager;

/**
 * Converts a json value to its string representation for variable substitution.
 * Strings are returned without quotes (backward compatible with former string storage).
 */
inline std::string jsonToString(const nlohmann::json &j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_null()) return "";
    return j.dump();
}

/**
 * This class stores the vault list.
 */
class Vault : public Map<std::string, nlohmann::json>
{
    h2agent::jsonschema::JsonSchema vault_schema_{};
    WaitManager *wait_manager_{};

public:
    Vault();
    ~Vault() = default;

    void setWaitManager(WaitManager *p) { wait_manager_ = p; }

    /**
     * Loads variable with a json value.
     *
     * @param variable variable name
     * @param value json value to store
     */
    void load(const std::string &variable, const nlohmann::json &value);

    /**
     * Loads a value at a specific path within an existing json variable.
     * Creates the variable as an empty object if it does not exist.
     *
     * @param variable variable name
     * @param path json pointer path (e.g. "/request/headers/auth")
     * @param value json value to set at path
     */
    void loadAtPath(const std::string &variable, const std::string &path, const nlohmann::json &value);

    /**
     * Loads server data global operation data
     *
     * @param j Json document from operation body request
     *
     * @return Load operation result
     */
    bool loadJson(const nlohmann::json &j);

    /**
     * Json string representation for class information (json object)
     *
     * @return Json string representation ('{}' for empty object).
     */
    std::string asJsonString() const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;

    /**
    * Gets vault schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return vault_schema_;
    }
};

}
}

