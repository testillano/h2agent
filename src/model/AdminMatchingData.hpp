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

#include <common.hpp>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

class AdminMatchingData
{
public:
    AdminMatchingData();
    ~AdminMatchingData() = default;

    // Algorithm type
    enum AlgorithmType { FullMatching = 0, FullMatchingRegexReplace, PriorityMatchingRegex };

    // getters
    AlgorithmType getAlgorithm() const {
        read_guard_t guard(rw_mutex_);
        return algorithm_;
    }

    const std::string& getRgx() const {
        read_guard_t guard(rw_mutex_);
        return rgx_;
    }

    const std::string& getFmt() const {
        read_guard_t guard(rw_mutex_);
        return fmt_;
    }

    bool getSortUriPathQueryParameters() const {
        read_guard_t guard(rw_mutex_);
        return sort_uri_path_query_parameters_;
    }

    /**
     * Loads server matching operation data
     *
     * @param j Json document from operation body request
     *
     * @return Boolean about success operation
     */
    bool load(const nlohmann::json &j);

private:
    mutable mutex_t rw_mutex_;

    AlgorithmType algorithm_;
    std::string rgx_;
    std::string fmt_;
    bool sort_uri_path_query_parameters_;
};

}
}

