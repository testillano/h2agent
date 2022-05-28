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

#include <regex>
#include <mutex>
#include <shared_mutex>

#include <common.hpp>

#include <nlohmann/json.hpp>

#include <JsonSchema.hpp>
#include <AdminSchemas.hpp>


namespace h2agent
{
namespace model
{

class AdminServerMatchingData
{
public:
    AdminServerMatchingData();
    ~AdminServerMatchingData() = default;

    // Algorithm type
    enum AlgorithmType { FullMatching = 0, FullMatchingRegexReplace, PriorityMatchingRegex };
    // UriPathQueryParametersFilter type
    enum UriPathQueryParametersFilterType { SortAmpersand = 0, SortSemicolon, PassBy, Ignore };

    // Load result
    enum LoadResult { Success = 0, BadSchema, BadContent };

    // getters
    AlgorithmType getAlgorithm() const {
        read_guard_t guard(rw_mutex_);
        return algorithm_;
    }

    const std::regex& getRgx() const {
        read_guard_t guard(rw_mutex_);
        return rgx_;
    }

    const std::string& getFmt() const {
        read_guard_t guard(rw_mutex_);
        return fmt_;
    }

    UriPathQueryParametersFilterType getUriPathQueryParametersFilter() const {
        read_guard_t guard(rw_mutex_);
        return uri_path_query_parameters_filter_;
    }

    /**
     * Json for class information
     *
     * @return Json object
     */
    const nlohmann::json &getJson() const {
        read_guard_t guard(rw_mutex_);

        return json_;
    }

    /**
     * Loads server matching operation data
     *
     * @param j Json document from operation body request
     *
     * @return Load operation result
     */
    LoadResult load(const nlohmann::json &j);

    /**
    * Gets matching schema
    */
    const h2agent::jsonschema::JsonSchema& getSchema() const {
        return server_matching_schema_;
    }

private:
    h2agent::jsonschema::JsonSchema server_matching_schema_{};

    mutable mutex_t rw_mutex_{};

    nlohmann::json json_{};

    AlgorithmType algorithm_{};
    std::regex rgx_{};
    std::string fmt_{};
    UriPathQueryParametersFilterType uri_path_query_parameters_filter_{};
};

}
}

