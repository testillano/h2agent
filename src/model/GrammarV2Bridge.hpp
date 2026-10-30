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

#include <nlohmann/json.hpp>

#include <GrammarMigrator.hpp>

namespace h2agent {
namespace model {

/**
 * TEMPORARY (phase [0.7a] of the grammar v2 proposal): provisions are
 * received in transformation grammar v2 and executed through the existing v1
 * path, so behaviour is unchanged by construction while the test suites are
 * migrated. Phase [0.7b] makes the engine parse v2 directly and removes this.
 *
 * Converts the 'transform'/'onResponseTransform' rules of one provision into
 * v1; every other field is copied untouched.
 *
 * @param v2 Provision in grammar v2 (already schema-validated).
 * @param v1 Equivalent provision in grammar v1.
 * @param error Reason when the provision uses v2 features without v1
 *        equivalent yet (e.g. a chain of several filters).
 * @return Conversion success.
 */
inline bool provisionToV1(const nlohmann::json &v2, nlohmann::json &v1, std::string &error) {
    try {
        const auto converted = h2agent::migrate::GrammarMigrator::provisionsToV1(h2agent::migrate::Json::parse(v2.dump()));
        v1 = nlohmann::json::parse(converted.dump());
        return true;
    } catch (const h2agent::migrate::MigrationError &e) {
        error = e.what();
        return false;
    }
}

}  // namespace model
}  // namespace h2agent
