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
#include <Vault.hpp>

namespace h2agent
{
namespace model
{

namespace {

/** Vault value of "<key>[/<pointer>]" as text, or false when it does not exist. */
bool lookup(const std::string &ref, const Vault *vault, std::string &text) {
    if (!vault) return false;
    const std::size_t slash = ref.find('/');
    const std::string key = (slash == std::string::npos) ? ref : ref.substr(0, slash);
    nlohmann::json value;
    if (key.empty() || !vault->tryGet(key, value)) return false;
    if (slash != std::string::npos) {
        try {
            value = value.at(nlohmann::json::json_pointer(ref.substr(slash)));
        } catch (const std::exception &) {
            return false;
        }
    }
    text = value.is_string() ? value.get<std::string>() : value.dump();
    return true;
}

void resolveString(std::string &s, const Vault *vault, std::set<std::string> &missing) {
    if (s.find("#{") == std::string::npos) return;
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '#' && i + 1 < s.size() && s[i + 1] == '{') {
            const std::size_t close = s.find('}', i + 2);
            if (close != std::string::npos) {
                const std::string ref = s.substr(i + 2, close - (i + 2));
                std::string text;
                if (lookup(ref, vault, text)) {
                    out += text;
                } else {
                    out.append(s, i, close + 1 - i);  // unresolved: kept literally
                    missing.insert(ref);
                }
                i = close + 1;
                continue;
            }
        }
        out += s[i++];
    }
    s = std::move(out);
}

void resolveNode(nlohmann::json &node, const Vault *vault, std::set<std::string> &missing) {
    if (node.is_string()) {
        resolveString(node.get_ref<std::string &>(), vault, missing);
    } else if (node.is_structured()) {
        for (auto &child : node) resolveNode(child, vault, missing);
    }
}

}

bool resolveProvisionReferences(nlohmann::json &doc, const Vault *vault, std::string &warning) {
    std::set<std::string> missing;
    resolveNode(doc, vault, missing);
    if (missing.empty()) return true;
    warning = "unresolved provision references kept literally (missing vault entries at provision time):";
    for (const auto &ref : missing) warning += " #{" + ref + "}";
    return false;
}

}
}
