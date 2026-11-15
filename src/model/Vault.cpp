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

#include <cctype>
#include <limits>
#include <string>

#include <ert/tracing/Logger.hpp>

#include <Vault.hpp>
#include <AdminSchemas.hpp>
#include <WaitManager.hpp>
#include <SseManager.hpp>
#include <JsonDump.hpp>

namespace h2agent
{
namespace model
{

Vault::Vault() {
    vault_schema_.setJson(h2agent::adminSchemas::vault); // won't fail
}

void Vault::load(const std::string &variable, const nlohmann::json &value) {
    add(variable, value);
    if (wait_manager_) wait_manager_->notify();
    if (sse_manager_) sse_manager_->notify(variable, value);
}

void Vault::load(const std::string &variable, nlohmann::json &&value) {
    add(variable, std::move(value));
    if (wait_manager_) wait_manager_->notify();
    if (sse_manager_) {
        nlohmann::json v;
        tryGet(variable, v);
        sse_manager_->notify(variable, v);
    }
}

void Vault::loadAtPath(const std::string &variable, const std::string &path, const nlohmann::json &value) {
    modifyOrInsert(variable, [&](nlohmann::json &current) {
        if (!current.is_object()) current = nlohmann::json::object();
        current[nlohmann::json::json_pointer(path)] = value;
    });
    if (wait_manager_) wait_manager_->notify();
    if (sse_manager_) {
        nlohmann::json v;
        tryGet(variable, v);
        sse_manager_->notify(variable, v);
    }
}

bool Vault::increment(const std::string &variable, long long delta, long long &result) {
    bool ok = false;
    modifyOrInsert(variable, [&](nlohmann::json &current) {
        long long base = 0;
        if (current.is_null()) base = 0;  // missing key (inserted as null by the map), or a stored null
        else if (current.is_number_integer()) base = current.get<long long>();
        else if (current.is_number_unsigned()) {
            const auto u = current.get<unsigned long long>();
            if (u > static_cast<unsigned long long>(std::numeric_limits<long long>::max())) return;
            base = static_cast<long long>(u);
        }
        else if (current.is_string()) {
            // the whole text, strictly: optional sign and digits (std::stoll alone
            // would also take leading blanks)
            const std::string &text = current.get_ref<const std::string &>();
            const std::size_t first = (!text.empty() && (text[0] == '-' || text[0] == '+')) ? 1 : 0;
            if (first == text.size()) return;
            for (std::size_t k = first; k < text.size(); k++)
                if (!std::isdigit(static_cast<unsigned char>(text[k]))) return;
            try {
                base = std::stoll(text);
            } catch (...) {
                return;  // out of range
            }
        }
        else return;  // float, boolean, object, array: not a counter
        if (__builtin_add_overflow(base, delta, &result)) return;
        current = result;
        ok = true;
    });
    if (!ok) return false;  // only an existing non counter value (kept) or an overflow: a missing key always succeeds
    if (wait_manager_) wait_manager_->notify();
    if (sse_manager_) sse_manager_->notify(variable, nlohmann::json(result));
    return true;
}

bool Vault::loadJson(const nlohmann::json &j) {

    std::string error{};
    if (!vault_schema_.validate(j, error)) {
        return false;
    }

    // Reject keys containing dots (reserved for JSON path navigation)
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key().find('.') != std::string::npos) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Vault entry key '%s' contains dots (not allowed)", it.key().c_str()), ERT_FILE_LOCATION));
            return false;
        }
    }

    // Convert: schema still validates as object, but values can be any JSON type.
    // Map::add(map_t) expects unordered_map<string, json>, which nlohmann::json
    // object iteration provides directly.
    for (auto it = j.begin(); it != j.end(); ++it) {
        add(it.key(), it.value());
        if (sse_manager_) sse_manager_->notify(it.key(), it.value());
    }
    if (wait_manager_) wait_manager_->notify();

    return true;
}

std::string Vault::asJsonString() const {

    return ((size() != 0) ? h2agent::model::dumpJson(getJson()) : "{}"); // server data is shown as an object
}

nlohmann::json Vault::getJson() const {
    return Map::getJson();
}

}
}

