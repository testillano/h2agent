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


#include <set>

#include <ProvisionReferences.hpp>
#include <TypeConverter.hpp>
#include <Vault.hpp>

namespace h2agent
{
namespace model
{

namespace {

/** Items left literally, reported sorted ('#{...}' contents). */
struct Missing {
    std::set<std::string> references;  ///< no vault entry (key or pointer)
    std::set<std::string> notWhole;    ///< typed reference inside a longer text
    std::set<std::string> notCastable; ///< value the cast type cannot convert
};

const std::set<std::string> CAST_TYPES = {"string", "integer", "unsigned", "float", "boolean", "object", "json"};

/** Cast type prefix of a reference ("object" for "object:collection/x"), or empty. */
std::string castTypeOf(const std::string &ref, std::string &key) {
    const std::size_t colon = ref.find(':');
    if (colon == std::string::npos) return "";
    const std::string type = ref.substr(0, colon);
    if (CAST_TYPES.find(type) == CAST_TYPES.end()) return "";
    key = ref.substr(colon + 1);
    return type;
}

/** Vault value of "<key>[/<pointer>]", or false when it does not exist. */
bool lookupValue(const std::string &ref, const Vault *vault, nlohmann::json &value) {
    if (!vault) return false;
    const std::size_t slash = ref.find('/');
    const std::string key = (slash == std::string::npos) ? ref : ref.substr(0, slash);
    if (key.empty() || !vault->tryGet(key, value)) return false;
    if (slash != std::string::npos) {
        try {
            value = value.at(nlohmann::json::json_pointer(ref.substr(slash)));
        } catch (const std::exception &) {
            return false;
        }
    }
    return true;
}

std::string asText(const nlohmann::json &value) {
    return value.is_string() ? value.get<std::string>() : value.dump();
}

/** Converts a vault value as the cast filter does; false when not possible. */
bool cast(const nlohmann::json &value, const std::string &type, nlohmann::json &result) {
    if (type == "string") {
        result = asText(value);
        return true;
    }
    if (type == "object") {
        if (!value.is_structured()) return false;
        result = value;
        return true;
    }
    if (type == "json") {
        if (!value.is_string()) {
            result = value;
            return true;
        }
        try {
            result = nlohmann::json::parse(value.get<std::string>());
        } catch (const std::exception &) {
            return false;
        }
        return true;
    }
    if (value.is_structured() || value.is_null()) return false;
    TypeConverter tc;
    if (value.is_string()) tc.setString(value.get<std::string>());
    else if (value.is_boolean()) tc.setBoolean(value.get<bool>());
    else if (value.is_number_unsigned()) tc.setUnsigned(value.get<std::uint64_t>());
    else if (value.is_number_integer()) tc.setInteger(value.get<std::int64_t>());
    else tc.setFloat(value.get<double>());
    bool ok = false;
    if (type == "integer") result = tc.getInteger(ok);
    else if (type == "unsigned") result = tc.getUnsigned(ok);
    else if (type == "float") result = tc.getFloat(ok);
    else result = tc.getBoolean(ok);  // boolean
    return ok;
}

void resolveString(std::string &s, const Vault *vault, Missing &missing) {
    if (s.find("#{") == std::string::npos) return;
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '#' && i + 1 < s.size() && s[i + 1] == '{') {
            const std::size_t close = s.find('}', i + 2);
            if (close != std::string::npos) {
                std::string ref = s.substr(i + 2, close - (i + 2));
                std::string key;
                const std::string type = castTypeOf(ref, key);
                bool kept = true;
                if (type.empty() || type == "string") {
                    nlohmann::json value;
                    if (lookupValue(type.empty() ? ref : key, vault, value)) {
                        out += asText(value);
                        kept = false;
                    } else {
                        missing.references.insert(ref);
                    }
                } else {
                    missing.notWhole.insert(ref);  // a typed value needs the whole string
                }
                if (kept) out.append(s, i, close + 1 - i);  // kept literally
                i = close + 1;
                continue;
            }
        }
        out += s[i++];
    }
    s = std::move(out);
}

void resolveNode(nlohmann::json &node, const Vault *vault, Missing &missing) {
    if (node.is_string()) {
        const std::string &s = node.get_ref<const std::string &>();
        // whole-string typed reference: "#{<cast type>:<key>}"
        if (s.size() > 3 && s.compare(0, 2, "#{") == 0 && s.back() == '}' && s.find('}') == s.size() - 1) {
            const std::string ref = s.substr(2, s.size() - 3);
            std::string key;
            const std::string type = castTypeOf(ref, key);
            if (!type.empty()) {
                nlohmann::json value, result;
                if (!lookupValue(key, vault, value)) missing.references.insert(ref);
                else if (!cast(value, type, result)) missing.notCastable.insert(ref);
                else node = std::move(result);  // single pass: not scanned again
                return;
            }
        }
        resolveString(node.get_ref<std::string &>(), vault, missing);
        return;
    }
    if (node.is_structured()) {
        for (auto &child : node) resolveNode(child, vault, missing);
    }
}

}

bool resolveProvisionReferences(nlohmann::json &doc, const Vault *vault, std::string &warning) {
    Missing missing;
    resolveNode(doc, vault, missing);
    warning.clear();
    const auto section = [&warning](const std::set<std::string> &items, const std::string &title) {
        if (items.empty()) return;
        if (!warning.empty()) warning += "; ";
        warning += title + ":";
        for (const auto &item : items) warning += " #{" + item + "}";
    };
    section(missing.references, "unresolved provision references kept literally (missing vault entries at provision time)");
    section(missing.notWhole, "typed provision references kept literally (they must be the whole string value)");
    section(missing.notCastable, "typed provision references kept literally (vault value not convertible to the cast type)");
    return warning.empty();
}

}
}
