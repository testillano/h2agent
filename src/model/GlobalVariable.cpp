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

#include <ert/tracing/Logger.hpp>

#include <GlobalVariable.hpp>
#include <AdminSchemas.hpp>

namespace h2agent
{
namespace model
{

GlobalVariable::GlobalVariable() {
    global_variable_schema_.setJson(h2agent::adminSchemas::server_data_global); // won't fail
}

void GlobalVariable::loadVariable(const std::string &variable, const std::string &value) {
    add(variable, value);
}

bool GlobalVariable::loadJson(const nlohmann::json &j) {

    if (!global_variable_schema_.validate(j)) {
        return false;
    }

    add(j);

    return true;
}

bool GlobalVariable::clear()
{
    write_guard_t guard(rw_mutex_);
    bool result = (map_.size() > 0); // something deleted

    map_.clear();

    return result;
}

std::string GlobalVariable::asJsonString() const {

    if (map_.size() == 0)
        return "{}"; // nothing found to be built

    return getJson().dump();
}

std::string GlobalVariable::getValue(const std::string &variableName, bool &exists) const {

    read_guard_t guard(rw_mutex_);

    auto it = get(variableName);
    exists = (it != end());

    return (exists ? (it->second):"");
}

void GlobalVariable::removeVariable(const std::string &variableName) {
    remove(variableName);
}

nlohmann::json GlobalVariable::getJson() const {
    return get();
}

}
}

