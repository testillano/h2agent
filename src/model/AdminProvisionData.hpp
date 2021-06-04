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
#include <AdminProvision.hpp>

namespace h2agent
{
namespace model
{

// Map key will be string which has a hash function.
// We will agregate method and uri in a single string for that.
class AdminProvisionData : public Map<admin_provision_key_t, AdminProvision>
{
    std::vector<admin_provision_key_t> ordered_keys_; // this is used to keep the insertion order which shall be used in PriorityMatchingRegex algorithm

public:
    AdminProvisionData();
    ~AdminProvisionData() = default;

    /**
     * Json string representation for class information
     *
     * @param ordered Print json array elements following the insertion order
     *
     * @return Json string representation
     */
    std::string asJsonString(bool ordered = false) const;

    /**
     * Loads server provision operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    bool load(const nlohmann::json &j);

    /** Clears internal data (map and ordered keys vector)
     *
     * @return True if something was removed, false if already empty
     */
    bool clear();

    /**
     * Finds provision item for traffic reception. Previously, mock dynamic data should be checked to
     * know if current state exists for the reception.
     *
     * @param inState Request input state if proceeed
     * @param method Request method received
     * @param uri Request URI path received
     *
     * @return Provision information or null if missing
     */
    std::shared_ptr<AdminProvision> find(const std::string &inState, const std::string &method, const std::string &uri) const;

    /**
    * Finds provision item for traffic reception. Previously, mock dynamic data should be checked to
    * know if current state exists for the reception.
    * The algorithm is PriorityMatchingRegex, so ordered search is applied.
    *
    * @param inState Request input state if proceeed
    * @param method Request method received
    * @param uri Request URI path received
    *
    * @return Provision information or null if missing
    */
    std::shared_ptr<AdminProvision> findWithPriorityMatchingRegex(const std::string &inState, const std::string &method, const std::string &uri) const;


};

}
}

