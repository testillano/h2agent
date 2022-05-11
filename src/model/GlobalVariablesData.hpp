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

/**
 * This class stores the global variables list.
 */
class GlobalVariablesData : public Map<std::string, std::string>
{
    mutable mutex_t rw_mutex_;

    h2agent::jsonschema::JsonSchema global_variables_schema_;

public:
    GlobalVariablesData();
    ~GlobalVariablesData() = default;

    /**
     * Loads variable and value
     *
     * @param variable variable name
     * @param value variable value
     */
    void loadVariable(const std::string &variable, const std::string &value);

    /**
     * Loads server data global operation data
     *
     * @param j Json document from operation body request
     *
     * @return Load operation result
     */
    bool loadJson(const nlohmann::json &j);

    /** Clears list
     *
     * @return Boolean about success of operation (something removed, nothing removed: already empty)
     */
    bool clear();

    /**
     * Json string representation for class information (json object)
     *
     * @return Json string representation ('{}' for empty object).
     */
    std::string asJsonString() const;

    /**
     * Gets the variable value for the variable name provided
     *
     * @param variableName Variable name for which the query was performed
     * @param exists Variable was found (true) or missing (false)
     *
     * @return variable value
     */
    std::string getValue(const std::string &variableName, bool &exists) const;

    /**
     * Removes the variable name provided from map
     *
     * @param variableName Variable name to be removed
     */
    void removeVariable(const std::string &variableName);

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json asJson() const;

    /**
    * Gets global variables schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return global_variables_schema_;
    }
};

}
}

