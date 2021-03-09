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

#include <iostream>
#include <string>

// Project
#include <JsonSchema.hpp>

// External
//#include <nlohmann/json.hpp>

namespace h2agent
{
namespace jsonschema
{

void JsonSchema::set_schema_(const nlohmann::json& schema)
{

    // assign private member
    schema_ = schema;

    // json-parse the schema
    try
    {
        validator_.set_root_schema(schema);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Schema configuration failed, here is why: " << e.what() << "\n";
    }
}

void JsonSchema::setSchema(const nlohmann::json& schema)
{
    set_schema_(schema);
}

JsonSchema::JsonSchema(const nlohmann::json& schema)
{
    set_schema_(schema);
}


bool JsonSchema::validate(const nlohmann::json& json) const
{
    try
    {
        validator_.validate(
            json); // validate the document - uses the default throwing error-handler
    }
    catch (const std::exception& e)
    {
        std::cerr << "Validation failed, here is why: " << e.what() << "\n";
        return false;
    }

    return true;
}

std::string JsonSchema::asString() const
{
    return schema_.dump();
}

}
}
