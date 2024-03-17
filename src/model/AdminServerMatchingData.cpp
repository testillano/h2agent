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
#include <regex>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminServerMatchingData.hpp>

namespace h2agent
{
namespace model
{

AdminServerMatchingData::AdminServerMatchingData() {
    algorithm_ = FullMatching;
    //rgx_.clear();
    fmt_.clear();
    uri_path_query_parameters_filter_ = Sort;
    uri_path_query_parameters_separator_ = Ampersand;
    json_["algorithm"] = "FullMatching";

    server_matching_schema_.setJson(h2agent::adminSchemas::server_matching); // won't fail
}

AdminServerMatchingData::LoadResult AdminServerMatchingData::load(const nlohmann::json &j) {

    std::string error{};
    if (!server_matching_schema_.validate(j, error)) {
        return BadSchema;
    }

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
            rgx_.assign(rgx_it->get<std::string>(), std::regex::optimize);
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            return BadContent;
        }
    }

    auto fmt_it = j.find("fmt");
    if (fmt_it != j.end() && fmt_it->is_string()) {
        hasFmt = true;
        fmt_ = *fmt_it;
    }

    auto uriPathQueryParameters_it = j.find("uriPathQueryParameters");
    if (uriPathQueryParameters_it != j.end()) {
        auto filter_it = (*uriPathQueryParameters_it).find("filter"); // mandatory
        if (filter_it->is_string()) {
            if (*filter_it == "Sort") {
                uri_path_query_parameters_filter_ = Sort;
            }
            else if (*filter_it == "PassBy") {
                uri_path_query_parameters_filter_ = PassBy;
            }
            else if (*filter_it == "Ignore") {
                uri_path_query_parameters_filter_ = Ignore;
            }
        }

        auto separator_it = (*uriPathQueryParameters_it).find("separator"); // optional
        if (separator_it != (*uriPathQueryParameters_it).end() && separator_it->is_string()) {
            if (*separator_it == "Ampersand") {
                uri_path_query_parameters_separator_ = Ampersand;
            }
            else if (*separator_it == "Semicolon") {
                uri_path_query_parameters_separator_ = Semicolon;
            }
        }
    }

    // Checkings
    if (*algorithm_it == "FullMatching") {
        if (hasRgx || hasFmt) {
            ert::tracing::Logger::error("FullMatching does not allow rgx and/or fmt fields", ERT_FILE_LOCATION);
            return BadContent;
        }
        algorithm_ = FullMatching;
    }
    else if (*algorithm_it == "FullMatchingRegexReplace") {
        if (!hasRgx || !hasFmt) {
            ert::tracing::Logger::error("FullMatchingRegexReplace requires rgx and fmt fields", ERT_FILE_LOCATION);
            return BadContent;
        }
        algorithm_ = FullMatchingRegexReplace;
    }
    else if (*algorithm_it == "RegexMatching") {
        if (hasRgx || hasFmt) {
            ert::tracing::Logger::error("RegexMatching does not allow rgx and/or fmt fields", ERT_FILE_LOCATION);
            return BadContent;
        }
        algorithm_ = RegexMatching;
    }


    return Success;
}

}
}

