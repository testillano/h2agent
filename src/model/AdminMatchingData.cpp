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

#include <AdminMatchingData.hpp>


namespace h2agent
{
namespace model
{

AdminMatchingData::AdminMatchingData() {
    algorithm_ = FullMatching;
    //rgx_.clear();
    fmt_.clear();
    uri_path_query_parameters_filter_ = SortAmpersand;
    json_["algorithm"] = "FullMatching";
}

bool AdminMatchingData::load(const nlohmann::json &j) {

    bool result = true;

    write_guard_t guard(rw_mutex_);

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto algorithm_it = j.find("algorithm");

    // Optional
    bool hasRgx = false;
    bool hasFmt = false;
    auto rgx_it = j.find("rgx");
    if (rgx_it != j.end() && rgx_it->is_string()) {
        hasRgx = true;
        try {
            rgx_.assign(*rgx_it);
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            return false;
        }
    }

    auto fmt_it = j.find("fmt");
    if (fmt_it != j.end() && fmt_it->is_string()) {
        hasFmt = true;
        fmt_ = *fmt_it;
    }

    auto uriPathQueryParametersFilter_it = j.find("uriPathQueryParametersFilter");
    uri_path_query_parameters_filter_ = SortAmpersand; // default
    if (uriPathQueryParametersFilter_it != j.end() && uriPathQueryParametersFilter_it->is_string()) {
        if (*uriPathQueryParametersFilter_it == "SortSemicolon") {
            uri_path_query_parameters_filter_ = SortSemicolon;
        }
        else if (*uriPathQueryParametersFilter_it == "PassBy") {
            uri_path_query_parameters_filter_ = PassBy;
        }
        else if (*uriPathQueryParametersFilter_it == "Ignore") {
            uri_path_query_parameters_filter_ = Ignore;
        }
    }

    // Checkings
    if (*algorithm_it == "FullMatching") {
        if (hasRgx || hasFmt) {
            ert::tracing::Logger::error("FullMatching does not allow rgx and/or fmt fields", ERT_FILE_LOCATION);
            return false;
        }
        algorithm_ = FullMatching;
    }
    else if (*algorithm_it == "FullMatchingRegexReplace") {
        if (!hasRgx || !hasFmt) {
            ert::tracing::Logger::error("FullMatchingRegexReplace requires rgx and fmt fields", ERT_FILE_LOCATION);
            return false;
        }
        algorithm_ = FullMatchingRegexReplace;
    }
    else if (*algorithm_it == "PriorityMatchingRegex") {
        if (hasRgx || hasFmt) {
            ert::tracing::Logger::error("PriorityMatchingRegex does not allow rgx and/or fmt fields", ERT_FILE_LOCATION);
            return false;
        }
        algorithm_ = PriorityMatchingRegex;
    }


    return result;
}

}
}

