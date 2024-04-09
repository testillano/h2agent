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

// Standard
#include <string>

// Project
//#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>


namespace h2agent
{
namespace jsonschema
{

class JsonSchema
{
    bool available_{};

    nlohmann::json json_{};
    nlohmann::json_schema::json_validator validator_{};

public:
    /**
    * Default constructor
    */
    JsonSchema() : available_(false) {;}
    ~JsonSchema() {;}

    // setters

    /**
    * Set json document schema
    *
    * @param j Json document schema
    *
    * @return Successful if a valid schema was configured
    */
    bool setJson(const nlohmann::json& j);

    // getters

    /**
    * Returns successful if a valid schema was configured
    *
    * @return Boolean about successful schema load
    */
    bool isAvailable() const {
        return available_;
    }

    /**
    * Get json document schema
    *
    * @return Json document schema
    */
    const nlohmann::json& getJson() const
    {
        return json_;
    }

    /**
    * Validates json document against schema.
    *
    * @param j Json document to validate
    * @param error Error passed by reference
    *
    * @return boolean about if json document is valid against json schema
    */
    bool validate(const nlohmann::json& j, std::string &error) const;
};

}
}

