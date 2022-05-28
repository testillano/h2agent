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

//#include <string>
#include <mutex>
#include <shared_mutex>

#include <nlohmann/json.hpp>

#include <AdminServerMatchingData.hpp>
#include <AdminServerProvisionData.hpp>
#include <AdminSchemaData.hpp>

namespace h2agent
{
namespace model
{

class AdminData
{
    AdminServerMatchingData matching_data_{};
    AdminServerProvisionData provision_data_{};
    AdminSchemaData schema_data_{};

public:

    /** Empty constructor */
    AdminData() {;}

    /**
     * Loads admin matching operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    AdminServerMatchingData::LoadResult loadMatching(const nlohmann::json &j) {
        return matching_data_.load(j);
    }

    /**
     * Loads admin provision operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    AdminServerProvisionData::LoadResult loadProvision(const nlohmann::json &j) {
        return provision_data_.load(j, (matching_data_.getAlgorithm() == AdminServerMatchingData::AlgorithmType::PriorityMatchingRegex));
    }

    /**
     * Loads admin schema operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    AdminSchemaData::LoadResult loadSchema(const nlohmann::json &j) {
        return schema_data_.load(j);
    }

    /**
     * Clears admin provisions data
     *
     * @return True if something was removed, false if already empty
     */
    bool clearProvisions() {
        return provision_data_.clear();
    }

    /**
     * Clears admin schemas data
     *
     * @return True if something was removed, false if already empty
     */
    bool clearSchemas() {
        return schema_data_.clear();
    }

    /**
     * Gets admin matching data
     */
    const AdminServerMatchingData& getMatchingData() const {
        return matching_data_;
    }

    /**
     * Gets admin provision data
     */
    const AdminServerProvisionData& getProvisionData() const {
        return provision_data_;
    }

    /**
     * Gets admin schema data
     */
    const AdminSchemaData& getSchemaData() const {
        return schema_data_;
    }
};

}
}

