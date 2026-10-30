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

#include <GrammarMigrator.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace h2agent {
namespace migrate {

namespace {

///////////////
// UTILITIES //
///////////////

[[noreturn]] void fail(const std::string &where, const std::string &msg) {
    throw MigrationError(where.empty() ? msg : where + ": " + msg);
}

bool startsWith(const std::string &s, const std::string &prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

/** Splits keeping empty tokens ("a||b|" -> a, "", b, ""), as v1 randomset does. */
std::vector<std::string> splitKeepEmpty(const std::string &s, char sep) {
    std::vector<std::string> result;
    std::size_t start = 0;
    for (;;) {
        std::size_t pos = s.find(sep, start);
        result.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return result;
}

/** v1 node paths after a key: '/' followed by at least one character. */
bool isV1NodePath(const std::string &s) {
    return s.size() > 1 && s[0] == '/';
}

/** Canonical unsigned decimal (no sign, no leading zeros): safe to turn into a json number and back. */
bool isCanonicalUnsigned(const std::string &s) {
    if (s.empty() || s.size() > 9) return false; // well below uint32 limits
    if (!std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isdigit(c);
    })) return false;
    return s == "0" || s[0] != '0';
}

/////////////////
// VOCABULARY  //
/////////////////

// Kinds without arguments (plain string in v2), v1 spelling -> v2 kind.
const std::vector<std::pair<std::string, std::string>> &plainSources() {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"request.uri", "requestUri"},
        {"request.uri.path", "requestUriPath"},
        {"request.headers", "requestHeaders"},
        {"response.headers", "responseHeaders"},
        {"response.statusCode", "responseStatusCode"},
        {"eraser", "eraser"},
        {"recvseq", "recvseq"},
        {"sendseq", "sendseq"},
        {"inState", "inState"},
    };
    return table;
}

const std::vector<std::pair<std::string, std::string>> &plainTargets() {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"response.body.string", "responseBodyText"},
        {"response.body.hexstring", "responseBodyHex"},
        {"request.body.string", "requestBodyText"},
        {"request.body.hexstring", "requestBodyHex"},
        {"response.statusCode", "responseStatusCode"},
        {"response.delayMs", "responseDelayMs"},
        {"request.uri", "requestUri"},
        {"request.method", "requestMethod"},
        {"request.delayMs", "requestDelayMs"},
        {"request.timeoutMs", "requestTimeoutMs"},
        {"outState", "outState"},
        {"break", "break"},
    };
    return table;
}

// Kinds whose whole argument is the text after a v1 prefix (prefix -> kind).
// 'emptyAllowed': v1 accepts an empty remainder (math., value.).
struct PrefixKind {
    const char *prefix;
    const char *kind;
    bool emptyAllowed;
};

const std::vector<PrefixKind> &prefixSources() {
    static const std::vector<PrefixKind> table = {
        {"request.uri.param.", "requestUriParam", false},
        {"request.body.", "requestBody", false},
        {"response.body.", "responseBody", false},
        {"request.header.", "requestHeader", false},
        {"response.header.", "responseHeader", false},
        {"math.", "math", true},
        {"strftime.", "strftime", false},
        {"var.", "var", false},
        {"value.", "value", true},
        {"serverEvent.", "serverEvent", false},
        {"clientEvent.", "clientEvent", false},
        {"txtFile.", "txtFile", false},
        {"binFile.", "binFile", false},
        {"command.", "command", false},
    };
    return table;
}

const std::vector<PrefixKind> &prefixTargets() {
    static const std::vector<PrefixKind> table = {
        {"response.header.", "responseHeader", false},
        {"request.header.", "requestHeader", false},
        {"var.", "var", false},
        {"txtFile.", "txtFile", false},
        {"binFile.", "binFile", false},
        {"serverEvent.", "serverEvent", false},
        {"clientEvent.", "clientEvent", false},
    };
    return table;
}

// v1 typed body targets: '<side>.body.json.<type>' -> v2 cast.
const std::vector<std::pair<std::string, std::string>> &jsonTypes() {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"string", "string"}, {"integer", "integer"}, {"unsigned", "unsigned"}, {"float", "float"},
        {"boolean", "boolean"}, {"object", "object"}, {"jsonstring", "json"},
    };
    return table;
}

const std::vector<std::string> &outStateMethods() {
    static const std::vector<std::string> methods = {"POST", "GET", "PUT", "DELETE", "HEAD"};
    return methods;
}

// Allowed sibling fields per object kind (besides the kind key itself).
const std::map<std::string, std::vector<std::string>> &fromSiblings() {
    static const std::map<std::string, std::vector<std::string>> table = {
        {"requestUriParam", {}}, {"requestBody", {}}, {"responseBody", {}}, {"requestHeader", {}},
        {"responseHeader", {}}, {"math", {}}, {"random", {}}, {"randomset", {}}, {"timestamp", {}},
        {"strftime", {}}, {"var", {}}, {"vault", {"path"}}, {"value", {}}, {"serverEvent", {}},
        {"clientEvent", {}}, {"txtFile", {}}, {"binFile", {}}, {"command", {}},
    };
    return table;
}

const std::map<std::string, std::vector<std::string>> &toSiblings() {
    static const std::map<std::string, std::vector<std::string>> table = {
        {"responseBody", {}}, {"requestBody", {}}, {"responseHeader", {}}, {"requestHeader", {}},
        {"var", {}}, {"vault", {"path"}}, {"foreignOutState", {"uri"}}, {"txtFile", {}}, {"binFile", {}},
        {"udpSocket", {"delayMs"}}, {"serverEvent", {}}, {"clientEvent", {}}, {"clientProvision", {"inState"}},
    };
    return table;
}

/**
 * Identifies the kind of a v2 'from'/'to' object: exactly one key must be a
 * known object kind, every other key must be one of its allowed siblings.
 */
std::string objectKind(const Json &v2, const std::map<std::string, std::vector<std::string>> &kinds,
                       const std::string &where) {
    std::string kind;
    for (const auto &item : v2.items()) {
        if (kinds.count(item.key()) == 0) continue;
        if (!kind.empty()) fail(where, "more than one kind ('" + kind + "', '" + item.key() + "')");
        kind = item.key();
    }
    if (kind.empty()) fail(where, "unknown kind in " + v2.dump());
    const auto &allowed = kinds.at(kind);
    for (const auto &item : v2.items()) {
        if (item.key() == kind) continue;
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end()) {
            std::string valid;
            for (const auto &a : allowed) valid += (valid.empty() ? "" : ", ") + a;
            fail(where, "unknown field '" + item.key() + "' for kind '" + kind + "' (valid: " +
                 (valid.empty() ? "none" : valid) + ")");
        }
    }
    return kind;
}

std::string stringArg(const Json &v2, const std::string &key, const std::string &where, bool emptyAllowed = false) {
    const auto it = v2.find(key);
    if (it == v2.end() || !it->is_string()) fail(where, "'" + key + "' must be a string");
    const std::string value = it->get<std::string>();
    if (value.empty() && !emptyAllowed) fail(where, "'" + key + "' must not be empty");
    return value;
}

/** v1 'vault.<key>[...]' key rule: non-empty, no dots. */
void checkVaultKey(const std::string &key, const std::string &where) {
    if (key.empty() || key.find('.') != std::string::npos) {
        fail(where, "vault key '" + key + "' must be non-empty and contain no dots");
    }
}

/////////////
// FILTERS //
/////////////

Json filterToV2(const Json &v1Filter, const std::string &where) {
    if (v1Filter.is_string()) {
        if (v1Filter.get<std::string>() != "Size") fail(where, "unknown filter '" + v1Filter.get<std::string>() + "'");
        return v1Filter;
    }
    if (!v1Filter.is_object() || v1Filter.size() != 1) fail(where, "a filter is \"Size\" or an object with one key");
    const std::string name = v1Filter.begin().key();
    if (name == "Size") fail(where, "'Size' takes no arguments: write \"Size\"");
    if (name == "as") fail(where, "'as' is not a v1 filter");
    return v1Filter;
}

void filtersToV1(const Json &v2Filters, Json &v1Filter, std::string &asType, const std::string &where) {
    if (!v2Filters.is_array()) fail(where, "must be a list");
    std::vector<Json> plain;
    for (std::size_t i = 0; i < v2Filters.size(); i++) {
        const Json &f = v2Filters[i];
        const std::string at = where + "/" + std::to_string(i);
        if (f.is_object() && f.size() == 1 && f.contains("as")) {
            if (i + 1 != v2Filters.size()) fail(at, "'as' is only expressible in v1 as the last filter");
            if (!f["as"].is_string()) fail(at, "'as' must be a string");
            asType = f["as"].get<std::string>();
            continue;
        }
        plain.push_back(filterToV2(f, at)); // same shape check
    }
    if (plain.size() > 1) fail(where, "a chain of several filters is not expressible in v1");
    if (!plain.empty()) v1Filter = plain.front();
}

/** Applies the rule-level conversion for one v1 or v2 rule. */
void checkRuleKeys(const Json &rule, const std::vector<std::string> &allowed, const std::string &where) {
    if (!rule.is_object()) fail(where, "a rule must be an object");
    for (const auto &item : rule.items()) {
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end()) {
            fail(where, "unexpected field '" + item.key() + "'");
        }
    }
}

////////////////
// PROVISIONS //
////////////////

const std::vector<std::string> &transformKeys() {
    static const std::vector<std::string> keys = {"transform", "onResponseTransform"};
    return keys;
}

using RuleConverter = Json (*)(const Json &, const std::string &);

Json convertProvision(const Json &provision, RuleConverter convert, const std::string &where) {
    if (!provision.is_object()) fail(where, "a provision must be an object");
    Json result = provision;
    for (const auto &key : transformKeys()) {
        auto it = provision.find(key);
        if (it == provision.end()) continue;
        const std::string at = where + "/" + key;
        if (!it->is_array()) fail(at, "must be an array");
        Json rules = Json::array();
        for (std::size_t i = 0; i < it->size(); i++) rules.push_back(convert((*it)[i], at + "/" + std::to_string(i)));
        result[key] = rules;
    }
    return result;
}

Json convertDocument(const Json &doc, RuleConverter convert) {
    if (doc.is_array()) {
        Json result = Json::array();
        for (std::size_t i = 0; i < doc.size(); i++) result.push_back(convertProvision(doc[i], convert, "/" + std::to_string(i)));
        return result;
    }
    return convertProvision(doc, convert, "");
}

void detectRules(const Json &rules, bool &v1, bool &v2) {
    if (!rules.is_array()) return;
    for (const auto &rule : rules) {
        if (!rule.is_object()) continue;
        if (rule.contains("source") || rule.contains("target")) v1 = true;
        if (rule.contains("from") || rule.contains("to")) v2 = true;
        if (rule.contains("onFilterFail")) detectRules(rule["onFilterFail"], v1, v2);
    }
}

//////////
// SCAN //
//////////

bool readTextFile(const std::filesystem::path &file, std::string &content) {
    constexpr std::uintmax_t MaxScanBytes = 20 * 1024 * 1024;
    std::error_code ec;
    if (std::filesystem::file_size(file, ec) > MaxScanBytes || ec) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    content = ss.str();
    return content.find('\0') == std::string::npos; // binary
}

std::string trimmed(const std::string &line) {
    constexpr std::size_t MaxShown = 160;
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) return "";
    std::string s = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
    return s.size() > MaxShown ? s.substr(0, MaxShown) + "..." : s;
}

void scanFile(const std::filesystem::path &file, std::vector<GrammarMigrator::ScanHit> &hits) {
    std::string content;
    if (!readTextFile(file, content)) return;
    const std::string name = file.string();

    bool v1Provision = false;
    if (file.extension() == ".json") {
        try {
            const auto grammar = GrammarMigrator::detect(Json::parse(content));
            if (grammar == GrammarMigrator::Grammar::V1 || grammar == GrammarMigrator::Grammar::Mixed) {
                v1Provision = true;
                hits.push_back({name, 0, "v1-provision",
                                grammar == GrammarMigrator::Grammar::Mixed ? "mixed v1/v2 rules" : "migrate with --in"});
            }
        } catch (const std::exception &) {
            // not a json document (or a template): fall back to the line scan
        }
    }

    static const std::regex ruleLiteral(R"rgx("source"\s*:\s*")rgx", std::regex::optimize);
    std::istringstream lines(content);
    std::string line;
    for (std::size_t n = 1; std::getline(lines, line); n++) {
        if (line.find("/admin/v1") != std::string::npos) hits.push_back({name, n, "admin-v1-path", trimmed(line)});
        if (!v1Provision && std::regex_search(line, ruleLiteral)) hits.push_back({name, n, "v1-rule-literal", trimmed(line)});
    }
}

}  // namespace

/////////////
// SOURCES //
/////////////

Json GrammarMigrator::sourceToV2(const std::string &v1, const std::string &where) {
    for (const auto &entry : plainSources()) {
        if (v1 == entry.first) return entry.second;
    }
    if (v1 == "request.body") return Json{{"requestBody", ""}};
    if (v1 == "response.body") return Json{{"responseBody", ""}};

    if (startsWith(v1, "random.")) {
        static const std::regex range(R"(^random\.([-+]?[0-9]+)\.([-+]?[0-9]+)$)", std::regex::optimize);
        std::smatch m;
        if (!std::regex_match(v1, m, range)) fail(where, "v1 source 'random.<min>.<max>' expected, got '" + v1 + "'");
        try {
            return Json{{"random", Json::array({std::stoi(m.str(1)), std::stoi(m.str(2))})}};
        } catch (const std::exception &) {
            fail(where, "random range out of integer limits: '" + v1 + "'");
        }
    }
    if (startsWith(v1, "randomset.")) {
        const std::string rest = v1.substr(10);
        if (rest.empty()) fail(where, "empty randomset");
        Json set = Json::array();
        for (const auto &token : splitKeepEmpty(rest, '|')) set.push_back(token);
        return Json{{"randomset", set}};
    }
    if (startsWith(v1, "timestamp.")) {
        const std::string unit = v1.substr(10);
        if (unit != "s" && unit != "ms" && unit != "us" && unit != "ns") fail(where, "timestamp unit must be s, ms, us or ns: '" + v1 + "'");
        return Json{{"timestamp", unit}};
    }
    if (startsWith(v1, "vault.")) {
        const std::string rest = v1.substr(6);
        const std::size_t dot = rest.find('.');
        const std::string key = rest.substr(0, dot);
        checkVaultKey(key, where);
        if (dot == std::string::npos) return Json{{"vault", key}};
        const std::string path = rest.substr(dot + 1);
        if (!isV1NodePath(path)) fail(where, "vault node path must be '/<node>...': '" + v1 + "'");
        return Json{{"vault", key}, {"path", path}};
    }
    for (const auto &entry : prefixSources()) {
        if (!startsWith(v1, entry.prefix)) continue;
        const std::string arg = v1.substr(std::char_traits<char>::length(entry.prefix));
        if (arg.empty() && !entry.emptyAllowed) fail(where, "missing argument in v1 source '" + v1 + "'");
        return Json{{entry.kind, arg}};
    }
    fail(where, "cannot identify v1 source '" + v1 + "'");
}

std::string GrammarMigrator::sourceToV1(const Json &v2, const std::string &where) {
    if (v2.is_string()) {
        const std::string kind = v2.get<std::string>();
        for (const auto &entry : plainSources()) {
            if (kind == entry.second) return entry.first;
        }
        if (fromSiblings().count(kind)) fail(where, "kind '" + kind + "' requires an argument: {\"" + kind + "\": ...}");
        fail(where, "unknown source kind '" + kind + "'");
    }
    if (!v2.is_object()) fail(where, "must be a kind string or a one-kind object");

    const std::string kind = objectKind(v2, fromSiblings(), where);
    if (kind == "requestBody" || kind == "responseBody") {
        const std::string path = stringArg(v2, kind, where, true);
        return std::string(kind == "requestBody" ? "request.body" : "response.body") + (path.empty() ? "" : "." + path);
    }
    if (kind == "random") {
        const Json &r = v2[kind];
        if (!r.is_array() || r.size() != 2 || !r[0].is_number_integer() || !r[1].is_number_integer()) {
            fail(where, "'random' must be [<min integer>, <max integer>]");
        }
        return "random." + std::to_string(r[0].get<long long>()) + "." + std::to_string(r[1].get<long long>());
    }
    if (kind == "randomset") {
        const Json &set = v2[kind];
        if (!set.is_array() || set.empty()) fail(where, "'randomset' must be a non-empty list of strings");
        std::string joined;
        for (std::size_t i = 0; i < set.size(); i++) {
            if (!set[i].is_string()) fail(where, "'randomset' must be a non-empty list of strings");
            const std::string token = set[i].get<std::string>();
            if (token.find('|') != std::string::npos) fail(where, "randomset item with '|' is not expressible in v1");
            joined += (i ? "|" : "") + token;
        }
        return "randomset." + joined;
    }
    if (kind == "timestamp") {
        const std::string unit = stringArg(v2, kind, where);
        if (unit != "s" && unit != "ms" && unit != "us" && unit != "ns") fail(where, "timestamp unit must be s, ms, us or ns");
        return "timestamp." + unit;
    }
    if (kind == "vault") {
        const std::string key = stringArg(v2, kind, where);
        checkVaultKey(key, where);
        if (!v2.contains("path")) return "vault." + key;
        const std::string path = stringArg(v2, "path", where);
        if (!isV1NodePath(path)) fail(where, "vault 'path' must be '/<node>...'");
        return "vault." + key + "." + path;
    }
    for (const auto &entry : prefixSources()) {
        if (kind == entry.kind) return entry.prefix + stringArg(v2, kind, where, entry.emptyAllowed);
    }
    fail(where, "unknown source kind '" + kind + "'"); // unreachable: objectKind validated it
}

/////////////
// TARGETS //
/////////////

Json GrammarMigrator::targetToV2(const std::string &v1, std::string &asType, const std::string &where) {
    asType.clear();
    for (const auto &entry : plainTargets()) {
        if (v1 == entry.first) return entry.second;
    }
    for (const std::string side : {
                "response", "request"
            }) {
        const std::string prefix = side + ".body.json.";
        if (!startsWith(v1, prefix)) continue;
        const std::string rest = v1.substr(prefix.size());
        for (const auto &type : jsonTypes()) {
            if (!startsWith(rest, type.first)) continue;
            const std::string tail = rest.substr(type.first.size());
            if (!tail.empty() && (tail[0] != '.' || tail.size() == 1)) continue;
            asType = type.second;
            return Json{{side + "Body", tail.empty() ? "" : tail.substr(1)}};
        }
        fail(where, "unknown json type in v1 target '" + v1 + "'");
    }
    if (startsWith(v1, "vault.")) {
        static const std::regex typed(R"(^vault\.([^.]+)\.json\.(object|jsonstring)(?:\.(/.+))?$)", std::regex::optimize);
        static const std::regex plain(R"(^vault\.([^.]+)(?:\.(/.+))?$)", std::regex::optimize);
        std::smatch m;
        Json to;
        if (std::regex_match(v1, m, typed)) {
            asType = (m.str(2) == "object") ? "object" : "json";
            to = Json{{"vault", m.str(1)}};
            if (m[3].matched) to["path"] = m.str(3);
        } else if (std::regex_match(v1, m, plain)) {
            to = Json{{"vault", m.str(1)}};
            if (m[2].matched) to["path"] = m.str(2);
        } else {
            fail(where, "v1 target 'vault.<key>[.json.object|.json.jsonstring][./<node>...]' expected, got '" + v1 + "'");
        }
        return to;
    }
    if (startsWith(v1, "outState.")) {
        const std::string rest = v1.substr(9);
        const std::size_t dot = rest.find('.');
        const std::string method = rest.substr(0, dot);
        const auto &methods = outStateMethods();
        if (std::find(methods.begin(), methods.end(), method) == methods.end()) {
            fail(where, "v1 target 'outState.<POST|GET|PUT|DELETE|HEAD>[.<uri>]' expected, got '" + v1 + "'");
        }
        Json to = Json{{"foreignOutState", method}};
        if (dot != std::string::npos) {
            const std::string uri = rest.substr(dot + 1);
            if (uri.empty()) fail(where, "empty uri in v1 target '" + v1 + "'");
            to["uri"] = uri;
        }
        return to;
    }
    if (startsWith(v1, "udpSocket.")) {
        const std::string rest = v1.substr(10);
        const std::size_t bar = rest.find('|');
        const std::string path = rest.substr(0, bar);
        if (path.empty()) fail(where, "missing socket path in v1 target '" + v1 + "'");
        Json to = Json{{"udpSocket", path}};
        if (bar != std::string::npos) {
            // The delay may hold variables ("@{d}"): kept as text unless it is a plain number.
            const std::string delay = rest.substr(bar + 1);
            if (isCanonicalUnsigned(delay)) to["delayMs"] = std::stoul(delay);
            else to["delayMs"] = delay;
        }
        return to;
    }
    if (startsWith(v1, "clientProvision.")) {
        const std::string rest = v1.substr(16);
        const std::size_t dot = rest.find('.');
        const std::string id = rest.substr(0, dot);
        const std::string inState = (dot == std::string::npos) ? "" : rest.substr(dot + 1);
        if (id.empty() || inState.empty()) fail(where, "v1 target 'clientProvision.<id>.<inState>' expected, got '" + v1 + "'");
        return Json{{"clientProvision", id}, {"inState", inState}};
    }
    for (const auto &entry : prefixTargets()) {
        if (!startsWith(v1, entry.prefix)) continue;
        const std::string arg = v1.substr(std::char_traits<char>::length(entry.prefix));
        if (arg.empty()) fail(where, "missing argument in v1 target '" + v1 + "'");
        return Json{{entry.kind, arg}};
    }
    fail(where, "cannot identify v1 target '" + v1 + "'");
}

std::string GrammarMigrator::targetToV1(const Json &v2, const std::string &asType, const std::string &where) {
    const auto noCast = [&](const std::string &kind) {
        if (!asType.empty()) fail(where, "{\"as\": \"" + asType + "\"} on target '" + kind + "' is not expressible in v1");
    };

    if (v2.is_string()) {
        const std::string kind = v2.get<std::string>();
        for (const auto &entry : plainTargets()) {
            if (kind == entry.second) {
                noCast(kind);
                return entry.first;
            }
        }
        if (toSiblings().count(kind)) fail(where, "kind '" + kind + "' requires an argument: {\"" + kind + "\": ...}");
        fail(where, "unknown target kind '" + kind + "'");
    }
    if (!v2.is_object()) fail(where, "must be a kind string or a one-kind object");

    const std::string kind = objectKind(v2, toSiblings(), where);
    if (kind == "responseBody" || kind == "requestBody") {
        const std::string path = stringArg(v2, kind, where, true);
        if (asType.empty()) fail(where, "target '" + kind + "' needs a final {\"as\": <type>} to be expressible in v1");
        std::string v1Type;
        for (const auto &type : jsonTypes()) {
            if (type.second == asType) v1Type = type.first;
        }
        if (v1Type.empty()) fail(where, "unknown cast {\"as\": \"" + asType + "\"}");
        return std::string(kind == "responseBody" ? "response" : "request") + ".body.json." + v1Type + (path.empty() ? "" : "." + path);
    }
    if (kind == "vault") {
        const std::string key = stringArg(v2, kind, where);
        checkVaultKey(key, where);
        std::string v1 = "vault." + key;
        if (asType == "object") v1 += ".json.object";
        else if (asType == "json") v1 += ".json.jsonstring";
        else noCast(kind);
        if (v2.contains("path")) {
            const std::string path = stringArg(v2, "path", where);
            if (!isV1NodePath(path)) fail(where, "vault 'path' must be '/<node>...'");
            v1 += "." + path;
        }
        return v1;
    }
    noCast(kind);
    if (kind == "foreignOutState") {
        const std::string method = stringArg(v2, kind, where);
        const auto &methods = outStateMethods();
        if (std::find(methods.begin(), methods.end(), method) == methods.end()) fail(where, "method must be POST, GET, PUT, DELETE or HEAD");
        return "outState." + method + (v2.contains("uri") ? "." + stringArg(v2, "uri", where) : "");
    }
    if (kind == "udpSocket") {
        std::string v1 = "udpSocket." + stringArg(v2, kind, where);
        if (v2.contains("delayMs")) {
            const Json &delay = v2["delayMs"];
            if (delay.is_number_unsigned() || (delay.is_number_integer() && delay.get<long long>() >= 0)) v1 += "|" + std::to_string(delay.get<unsigned long long>());
            else if (delay.is_string()) v1 += "|" + delay.get<std::string>();
            else fail(where, "'delayMs' must be a non-negative integer or a text with variables");
        }
        return v1;
    }
    if (kind == "clientProvision") {
        const std::string id = stringArg(v2, kind, where);
        if (id.find('.') != std::string::npos) fail(where, "client provision id must contain no dots");
        if (!v2.contains("inState")) fail(where, "'clientProvision' requires 'inState'");
        return "clientProvision." + id + "." + stringArg(v2, "inState", where);
    }
    for (const auto &entry : prefixTargets()) {
        if (kind == entry.kind) return entry.prefix + stringArg(v2, kind, where);
    }
    fail(where, "unknown target kind '" + kind + "'"); // unreachable: objectKind validated it
}

///////////
// RULES //
///////////

Json GrammarMigrator::ruleToV2(const Json &v1Rule, const std::string &where) {
    if (v1Rule.is_object() && (v1Rule.contains("from") || v1Rule.contains("to"))) fail(where, "rule is already v2");
    checkRuleKeys(v1Rule, {"source", "target", "filter", "onFilterFail"}, where);
    if (!v1Rule.contains("source") || !v1Rule["source"].is_string()) fail(where + "/source", "missing or not a string");
    if (!v1Rule.contains("target") || !v1Rule["target"].is_string()) fail(where + "/target", "missing or not a string");

    Json rule = Json::object();
    rule["from"] = sourceToV2(v1Rule["source"].get<std::string>(), where + "/source");
    std::string asType;
    rule["to"] = targetToV2(v1Rule["target"].get<std::string>(), asType, where + "/target");

    Json filters = Json::array();
    if (v1Rule.contains("filter")) filters.push_back(filterToV2(v1Rule["filter"], where + "/filter"));
    if (!asType.empty()) filters.push_back(Json{{"as", asType}});
    if (!filters.empty()) rule["filter"] = filters;

    if (v1Rule.contains("onFilterFail")) {
        const Json &off = v1Rule["onFilterFail"];
        if (!off.is_array()) fail(where + "/onFilterFail", "must be an array");
        Json rules = Json::array();
        for (std::size_t i = 0; i < off.size(); i++) rules.push_back(ruleToV2(off[i], where + "/onFilterFail/" + std::to_string(i)));
        rule["onFilterFail"] = rules;
    }
    return rule;
}

Json GrammarMigrator::ruleToV1(const Json &v2Rule, const std::string &where) {
    if (v2Rule.is_object() && (v2Rule.contains("source") || v2Rule.contains("target"))) fail(where, "rule is already v1");
    checkRuleKeys(v2Rule, {"from", "to", "filter", "onFilterFail"}, where);
    if (!v2Rule.contains("from")) fail(where + "/from", "missing");
    if (!v2Rule.contains("to")) fail(where + "/to", "missing");

    Json v1Filter;
    std::string asType;
    if (v2Rule.contains("filter")) filtersToV1(v2Rule["filter"], v1Filter, asType, where + "/filter");

    Json rule = Json::object();
    rule["source"] = sourceToV1(v2Rule["from"], where + "/from");
    rule["target"] = targetToV1(v2Rule["to"], asType, where + "/to");
    if (!v1Filter.is_null()) rule["filter"] = v1Filter;

    if (v2Rule.contains("onFilterFail")) {
        const Json &off = v2Rule["onFilterFail"];
        if (!off.is_array()) fail(where + "/onFilterFail", "must be an array");
        Json rules = Json::array();
        for (std::size_t i = 0; i < off.size(); i++) rules.push_back(ruleToV1(off[i], where + "/onFilterFail/" + std::to_string(i)));
        rule["onFilterFail"] = rules;
    }
    return rule;
}

////////////////
// PROVISIONS //
////////////////

Json GrammarMigrator::provisionsToV2(const Json &doc) {
    return convertDocument(doc, [](const Json &rule, const std::string &where) {
        return GrammarMigrator::ruleToV2(rule, where);
    });
}

Json GrammarMigrator::provisionsToV1(const Json &doc) {
    return convertDocument(doc, [](const Json &rule, const std::string &where) {
        return GrammarMigrator::ruleToV1(rule, where);
    });
}

GrammarMigrator::Grammar GrammarMigrator::detect(const Json &doc) {
    bool v1 = false, v2 = false;
    const auto provision = [&](const Json &p) {
        if (!p.is_object()) return;
        for (const auto &key : transformKeys()) {
            if (p.contains(key)) detectRules(p[key], v1, v2);
        }
    };
    if (doc.is_array()) {
        for (const auto &p : doc) provision(p);
    } else {
        provision(doc);
    }
    if (v1 && v2) return Grammar::Mixed;
    if (v1) return Grammar::V1;
    if (v2) return Grammar::V2;
    return Grammar::None;
}

//////////
// SCAN //
//////////

std::vector<GrammarMigrator::ScanHit> GrammarMigrator::scan(const std::string &path) {
    namespace fs = std::filesystem;
    std::vector<ScanHit> hits;
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) {
        scanFile(path, hits);
        return hits;
    }
    if (!fs::is_directory(path, ec)) throw MigrationError("cannot scan '" + path + "': not a file or directory");

    static const std::vector<std::string> skipped = {".git", "build", "node_modules"};
    fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) {
            if (std::find(skipped.begin(), skipped.end(), it->path().filename().string()) != skipped.end()) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec)) scanFile(it->path(), hits);
    }
    std::sort(hits.begin(), hits.end(), [](const ScanHit &a, const ScanHit &b) {
        return a.file != b.file ? a.file < b.file : a.line < b.line;
    });
    return hits;
}

}  // namespace migrate
}  // namespace h2agent
