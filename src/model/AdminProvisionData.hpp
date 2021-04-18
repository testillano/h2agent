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
#include <mutex>
#include <shared_mutex>

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <AdminProvision.hpp>

namespace h2agent
{
namespace model
{

// Provision key:
typedef std::string provision_key_t;
// Future proof: instead of using a key = <method><uri>, we could agreggate them:
// typedef std::pair<std::string, std::string> provision_key_t;
// But in order to compile, we need to define a hash function for the unordered map:
// https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR)
// https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)
//

// Map key will be string which has a hash function.
// We will agregate method and uri in a single string for that.
class AdminProvisionData : public Map<provision_key_t, AdminProvision>
{
    std::vector<provision_key_t> ordered_keys_; // this is used to keep the insertion order which shall be used in PriorityMatchingRegex algorithm

public:
    AdminProvisionData();
    ~AdminProvisionData() = default;

    /**
     * Loads server provision operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    bool load(const nlohmann::json &j);

    /** Clear internal data (map and ordered keys vector) */
    void clear()
    {
        Map::clear();

        write_guard_t guard(rw_mutex_);
        ordered_keys_.clear();
    }

};

}
}

