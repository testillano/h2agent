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

#include <AdminProvisionData.hpp>


namespace h2agent
{
namespace model
{

AdminProvisionData::AdminProvisionData() {
}

bool AdminProvisionData::load(const nlohmann::json &j) {
    bool result = true;
    return result;

    write_guard_t guard(rw_mutex_);

    // Provision object to fill:
    auto provision = std::make_shared<AdminProvision>();

    // Mandatory
    auto requestMethod_it = j.find("requestMethod");
    provision->setRequestMethod(*requestMethod_it);

    auto requestUri_it = j.find("requestUri");
    provision->setRequestUri(*requestUri_it);

    auto it = j.find("responseCode");
    provision->setResponseCode(*it);

    // Optional
    it = j.find("inState");
    if (it != j.end() && it->is_string()) {
        provision->setInState(*it);
    }

    it = j.find("outState");
    if (it != j.end() && it->is_string()) {
        provision->setOutState(*it);
    }

    it = j.find("responseHeaders");
    if (it != j.end() && it->is_object()) {
        provision->loadResponseHeaders(*it);
    }

    it = j.find("responseBody");
    if (it != j.end() && it->is_object()) {
        provision->setResponseBody(*it);
    }

    it = j.find("responseDelayMs");
    if (it != j.end() && it->is_number()) {
        provision->setResponseDelayMs(*it);
    }

    auto transform_it = j.find("transform");
    if (transform_it != j.end() && transform_it->is_object()) {
        provision->loadTransform(*transform_it);
    }

    // Push the key in the map:
    std::string key = std::string(*requestMethod_it) + std::string(*requestUri_it);
    add(key, provision);

    // Push the key just in case we configure ordered algorithm 'PriorityMatchingRegex':
    ordered_keys_.push_back(key);


    return result;
}

}
}
