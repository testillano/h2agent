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

bool contains(const std::vector<std::string> &v, const std::string &s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

std::string joined(const std::vector<std::string> &v) {
    std::string out;
    for (const auto &s : v) out += (out.empty() ? "" : ", ") + s;
    return out.empty() ? "none" : out;
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

/** Json pointer as written after a v1 '<prefix>.': '/' followed by anything. */
bool isPointer(const std::string &s) {
    return !s.empty() && s[0] == '/';
}

/** Canonical decimal integer (optional '-', no leading zeros): safe as a json number and back. */
bool isCanonicalInteger(const std::string &s, bool allowNegative) {
    const std::string digits = (allowNegative && !s.empty() && s[0] == '-') ? s.substr(1) : s;
    if (digits.empty() || digits.size() > 9) return false; // well below 32-bit limits
    if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) {
    return std::isdigit(c);
    })) return false;
    if (digits == "0") return s == "0"; // no "-0"
    return digits[0] != '0';
}

bool hasVariable(const std::string &s) {
    return s.find("@{") != std::string::npos;
}

/** Same decoding as the v1 runtime for event address values (Engine urlDecode). */
std::string urlDecode(const std::string &in) {
    std::string out;
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
                std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
            out.push_back(static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

/** Minimal encoding for a v1 event address value: only what the v1 parser would misread. */
std::string urlEncode(const std::string &in) {
    std::string out;
    for (char c : in) {
        if (c == '%') out += "%25";
        else if (c == '&') out += "%26";
        else if (c == '+') out += "%2B";
        else out.push_back(c);
    }
    return out;
}

/** Checks that 'obj' (a v2 kind object) only holds the discriminator and the allowed fields. */
void checkFields(const Json &obj, const std::string &discriminator, const std::string &kind,
                 const std::vector<std::string> &allowed, const std::vector<std::string> &required, const std::string &where) {
    for (const auto &item : obj.items()) {
        if (item.key() == discriminator || contains(allowed, item.key())) continue;
        fail(where, "unknown field '" + item.key() + "' for " + discriminator + " '" + kind + "' (valid: " + joined(allowed) + ")");
    }
    for (const auto &r : required) {
        if (!obj.contains(r)) fail(where, discriminator + " '" + kind + "' requires '" + r + "'");
    }
}

std::string textField(const Json &obj, const std::string &key, const std::string &where, bool emptyAllowed = false) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) fail(where, "'" + key + "' must be a string");
    const std::string value = it->get<std::string>();
    if (value.empty() && !emptyAllowed) fail(where, "'" + key + "' must not be empty");
    return value;
}

/** Text of a numeric field which may also be a string holding a variable ("@{seq}"). */
std::string numberOrVariable(const Json &obj, const std::string &key, bool allowNegative, const std::string &where) {
    const Json &v = obj.at(key);
    if (v.is_number_integer() && (allowNegative || v.get<long long>() >= 0)) return std::to_string(v.get<long long>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_string() && hasVariable(v.get<std::string>())) return v.get<std::string>();
    fail(where, "'" + key + "' must be " + (allowNegative ? "an integer" : "a non-negative integer") + " or a text with variables");
}

/** v1 'vault.<key>[...]' key rule: non-empty, no dots. */
void checkVaultKey(const std::string &key, const std::string &where) {
    if (key.empty() || key.find('.') != std::string::npos) {
        fail(where, "vault key '" + key + "' must be non-empty and contain no dots");
    }
}

////////////////
// VOCABULARY //
////////////////

// Kinds without fields, v1 spelling -> v2 kind.
const std::vector<std::pair<std::string, std::string>> &plainSources() {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"request.uri", "requestUri"}, {"request.uri.path", "requestUriPath"}, {"request.headers", "requestHeaders"},
        {"response.headers", "responseHeaders"}, {"response.statusCode", "responseStatusCode"}, {"eraser", "eraser"},
        {"recvseq", "recvseq"}, {"sendseq", "sendseq"}, {"inState", "inState"},
    };
    return table;
}

const std::vector<std::pair<std::string, std::string>> &plainTargets() {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"response.body.string", "responseBodyText"}, {"response.body.hexstring", "responseBodyHex"},
        {"request.body.string", "requestBodyText"}, {"request.body.hexstring", "requestBodyHex"},
        {"response.statusCode", "responseStatusCode"}, {"response.delayMs", "responseDelayMs"},
        {"request.uri", "requestUri"}, {"request.method", "requestMethod"}, {"request.delayMs", "requestDelayMs"},
        {"request.timeoutMs", "requestTimeoutMs"}, {"break", "stop"},
    };
    return table;
}

// Kinds with a single text field holding everything after a v1 prefix.
struct OneField {
    const char *prefix;
    const char *kind;
    const char *field;
    bool emptyAllowed;  // v1 accepts an empty remainder (math., value.)
};

const std::vector<OneField> &oneFieldSources() {
    static const std::vector<OneField> table = {
        {"request.uri.param.", "requestUriParam", "name", false}, {"request.header.", "requestHeader", "name", false},
        {"response.header.", "responseHeader", "name", false}, {"math.", "math", "expression", true},
        {"strftime.", "strftime", "fmt", false}, {"var.", "var", "name", false}, {"value.", "literal", "value", true},
        {"txtFile.", "txtFile", "file", false}, {"binFile.", "binFile", "file", false}, {"command.", "command", "command", false},
    };
    return table;
}

const std::vector<OneField> &oneFieldTargets() {
    static const std::vector<OneField> table = {
        {"response.header.", "responseHeader", "name", false}, {"request.header.", "requestHeader", "name", false},
        {"var.", "var", "name", false}, {"txtFile.", "txtFile", "file", false}, {"binFile.", "binFile", "file", false},
    };
    return table;
}

// v1 typed body targets: '<side>.body.json.<type>' -> v2 cast type.
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

/////////////////////
// EVENT ADDRESSES //
/////////////////////

enum class FieldType { Text, Integer, Unsigned };

/** Accepted event address fields per domain and role (as the v1 runtime reads them). */
const std::vector<std::pair<std::string, FieldType>> &eventFields(const std::string &domain, bool source) {
    static const std::vector<std::pair<std::string, FieldType>> serverSource = {
        {"requestMethod", FieldType::Text}, {"requestUri", FieldType::Text}, {"requestUriStartsWith", FieldType::Text},
        {"eventNumber", FieldType::Integer}, {"eventPath", FieldType::Text}, {"recvseq", FieldType::Unsigned},
    };
    static const std::vector<std::pair<std::string, FieldType>> clientSource = {
        {"clientEndpointId", FieldType::Text}, {"requestMethod", FieldType::Text}, {"requestUri", FieldType::Text},
        {"requestUriStartsWith", FieldType::Text}, {"eventNumber", FieldType::Integer}, {"eventPath", FieldType::Text},
        {"sendseq", FieldType::Unsigned},
    };
    static const std::vector<std::pair<std::string, FieldType>> serverPurge = {
        {"requestMethod", FieldType::Text}, {"requestUri", FieldType::Text}, {"eventNumber", FieldType::Integer},
        {"recvseq", FieldType::Unsigned},
    };
    static const std::vector<std::pair<std::string, FieldType>> clientPurge = {
        {"clientEndpointId", FieldType::Text}, {"requestMethod", FieldType::Text}, {"requestUri", FieldType::Text},
        {"eventNumber", FieldType::Integer}, {"sendseq", FieldType::Unsigned},
    };
    if (domain == "serverEvent") return source ? serverSource : serverPurge;
    return source ? clientSource : clientPurge;
}

std::vector<std::string> fieldNames(const std::vector<std::pair<std::string, FieldType>> &fields) {
    std::vector<std::string> names;
    for (const auto &f : fields) names.push_back(f.first);
    return names;
}

/** v1 'k=v&k=v' address (values url-encoded) -> v2 fields appended to 'obj', in input order. */
void eventToV2(const std::string &query, const std::string &domain, bool source, Json &obj, const std::string &where) {
    const auto &allowed = eventFields(domain, source);
    if (query.empty()) fail(where, "empty " + domain + " address");
    for (const auto &pair : splitKeepEmpty(query, '&')) {
        const std::size_t eq = pair.find('=');
        const std::string key = pair.substr(0, eq);
        const std::string value = (eq == std::string::npos) ? "" : urlDecode(pair.substr(eq + 1));
        auto it = std::find_if(allowed.begin(), allowed.end(), [&](const auto &f) {
            return f.first == key;
        });
        if (it == allowed.end()) fail(where, "unknown " + domain + " address field '" + key + "' (valid: " + joined(fieldNames(allowed)) + ")");
        if (obj.contains(key)) fail(where, "repeated " + domain + " address field '" + key + "'");
        if (it->second == FieldType::Text) {
            obj[key] = value;
        } else if (isCanonicalInteger(value, it->second == FieldType::Integer)) {
            obj[key] = std::stoll(value);
        } else if (hasVariable(value)) {
            obj[key] = value;
        } else {
            fail(where, domain + " address field '" + key + "' must be a number or a text with variables, got '" + value + "'");
        }
    }
}

/** v2 event fields of 'obj' (besides the discriminator) -> v1 'k=v&k=v' address. */
std::string eventToV1(const Json &obj, const std::string &discriminator, const std::string &domain, bool source, const std::string &where) {
    const auto &allowed = eventFields(domain, source);
    checkFields(obj, discriminator, domain, fieldNames(allowed), {}, where);
    std::string query;
    for (const auto &item : obj.items()) {
        if (item.key() == discriminator) continue;
        auto it = std::find_if(allowed.begin(), allowed.end(), [&](const auto &f) {
            return f.first == item.key();
        });
        const std::string value = (it->second == FieldType::Text) ? textField(obj, item.key(), where, true)
                                  : numberOrVariable(obj, item.key(), it->second == FieldType::Integer, where);
        query += (query.empty() ? "" : "&") + item.key() + "=" + urlEncode(value);
    }
    if (query.empty()) fail(where, domain + " requires address fields");
    return query;
}

/////////////
// FILTERS //
/////////////

// v1 filters whose argument is a single value -> v2 field holding it.
struct ScalarFilter {
    const char *v1;
    const char *op;
    const char *field;
    bool numeric;  // Sum/Multiply take a number, the rest a string
};

const std::vector<ScalarFilter> &scalarFilters() {
    static const std::vector<ScalarFilter> table = {
        {"RegexCapture", "regexCapture", "rgx", false}, {"RegexKey", "regexKey", "rgx", false},
        {"Append", "append", "value", false}, {"Prepend", "prepend", "value", false},
        {"Sum", "sum", "value", true}, {"Multiply", "multiply", "value", true},
        {"EqualTo", "equalTo", "value", false},
        {"SchemaId", "schemaId", "id", false},
    };
    return table;
}

// v1 filters whose argument is an object: its fields are flattened as they are.
struct ObjectFilter {
    const char *v1;
    const char *op;
    std::vector<std::string> fields;
    std::vector<std::string> required;
};

const std::vector<ObjectFilter> &objectFilters() {
    static const std::vector<ObjectFilter> table = {
        {"RegexReplace", "regexReplace", {"rgx", "fmt"}, {"rgx", "fmt"}},
        {"Split", "split", {"size", "count", "sep", "filler", "numeric"}, {}},
        {"BaseConvert", "baseConvert", {"in", "out", "capital"}, {"in", "out"}},
        {"Strptime", "strptime", {"fmt", "unit"}, {"fmt"}},
        {"Strftime", "strftime", {"fmt", "unit"}, {"fmt"}},
    };
    return table;
}

/**
 * Sources which always yield a string: a cast to string on a json body target is
 * redundant for them (a body target without cast keeps the source type), so the
 * migrator does not produce it.
 */
const std::vector<std::string> &textSources() {
    static const std::vector<std::string> kinds = {
        "literal", "requestHeader", "requestUriParam", "requestUri", "requestUriPath", "strftime", "inState"
    };
    return kinds;
}

bool isBodyTarget(const Json &to) {
    return to["target"] == "responseBody" || to["target"] == "requestBody";
}

const std::vector<std::string> &castTypes() {
    static const std::vector<std::string> types = {"string", "integer", "unsigned", "float", "boolean", "object", "json"};
    return types;
}

/** Applies the rule-level key check for one v1 or v2 rule. */
void checkRuleKeys(const Json &rule, const std::vector<std::string> &allowed, const std::string &where) {
    if (!rule.is_object()) fail(where, "a rule must be an object");
    for (const auto &item : rule.items()) {
        if (!contains(allowed, item.key())) fail(where, "unexpected field '" + item.key() + "'");
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

    // v1 rule literals: a dotted (or v1-only) value right after "source"/"target". v2 kind names
    // never contain dots, so '{"source": "requestBody"}' does not match.
    static const std::regex ruleLiteral(
        R"rgx("source"\s*:\s*"(eraser"|(request|response|var|vault|value|math|random|randomset|timestamp|strftime|serverEvent|clientEvent|txtFile|binFile|command)\.)|"target"\s*:\s*"(break"|(response|request|var|vault|outState|txtFile|binFile|udpSocket|serverEvent|clientEvent|clientProvision)\.))rgx",
        std::regex::optimize);
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
        if (v1 == entry.first) return Json{{"source", entry.second}};
    }
    for (const std::string side : {
                "request", "response"
            }) {
        const std::string whole = side + ".body";
        if (v1 == whole) return Json{{"source", side + "Body"}};
        if (!startsWith(v1, whole + ".")) continue;
        const std::string path = v1.substr(whole.size() + 1);
        if (!isPointer(path)) fail(where, "body path must be a json pointer ('/<node>...'): '" + v1 + "'");
        return Json{{"source", side + "Body"}, {"path", path}};
    }
    if (startsWith(v1, "random.")) {
        static const std::regex range(R"(^random\.([-+]?[0-9]+)\.([-+]?[0-9]+)$)", std::regex::optimize);
        std::smatch m;
        if (!std::regex_match(v1, m, range)) fail(where, "v1 source 'random.<min>.<max>' expected, got '" + v1 + "'");
        try {
            return Json{{"source", "random"}, {"min", std::stoi(m.str(1))}, {"max", std::stoi(m.str(2))}};
        } catch (const std::exception &) {
            fail(where, "random range out of integer limits: '" + v1 + "'");
        }
    }
    if (startsWith(v1, "randomset.")) {
        const std::string rest = v1.substr(10);
        if (rest.empty()) fail(where, "empty randomset");
        Json values = Json::array();
        for (const auto &token : splitKeepEmpty(rest, '|')) values.push_back(token);
        return Json{{"source", "randomset"}, {"values", values}};
    }
    if (startsWith(v1, "timestamp.")) {
        const std::string unit = v1.substr(10);
        if (unit != "s" && unit != "ms" && unit != "us" && unit != "ns") fail(where, "timestamp unit must be s, ms, us or ns: '" + v1 + "'");
        return Json{{"source", "timestamp"}, {"unit", unit}};
    }
    if (startsWith(v1, "vault.")) {
        const std::string rest = v1.substr(6);
        const std::size_t dot = rest.find('.');
        const std::string key = rest.substr(0, dot);
        checkVaultKey(key, where);
        if (dot == std::string::npos) return Json{{"source", "vault"}, {"key", key}};
        const std::string path = rest.substr(dot + 1);
        if (path.size() < 2 || !isPointer(path)) fail(where, "vault node path must be '/<node>...': '" + v1 + "'");
        return Json{{"source", "vault"}, {"key", key}, {"path", path}};
    }
    for (const std::string domain : {
                "serverEvent", "clientEvent"
            }) {
        if (!startsWith(v1, domain + ".")) continue;
        Json from = Json{{"source", domain}};
        eventToV2(v1.substr(domain.size() + 1), domain, true, from, where);
        return from;
    }
    for (const auto &entry : oneFieldSources()) {
        if (!startsWith(v1, entry.prefix)) continue;
        const std::string arg = v1.substr(std::char_traits<char>::length(entry.prefix));
        if (arg.empty() && !entry.emptyAllowed) fail(where, "missing argument in v1 source '" + v1 + "'");
        return Json{{"source", entry.kind}, {entry.field, arg}};
    }
    fail(where, "cannot identify v1 source '" + v1 + "'");
}

std::string GrammarMigrator::sourceToV1(const Json &v2, const std::string &where) {
    if (!v2.is_object() || !v2.contains("source") || !v2["source"].is_string()) fail(where, "must be an object with a 'source' kind");
    const std::string kind = v2["source"].get<std::string>();

    for (const auto &entry : plainSources()) {
        if (kind != entry.second) continue;
        checkFields(v2, "source", kind, {}, {}, where);
        return entry.first;
    }
    if (kind == "requestBody" || kind == "responseBody") {
        checkFields(v2, "source", kind, {"path"}, {}, where);
        const std::string base = (kind == "requestBody") ? "request.body" : "response.body";
        if (!v2.contains("path")) return base;
        const std::string path = textField(v2, "path", where);
        if (!isPointer(path)) fail(where, "'path' must be a json pointer ('/<node>...')");
        return base + "." + path;
    }
    if (kind == "random") {
        checkFields(v2, "source", kind, {"min", "max"}, {"min", "max"}, where);
        if (!v2["min"].is_number_integer() || !v2["max"].is_number_integer()) fail(where, "'min' and 'max' must be integers");
        return "random." + std::to_string(v2["min"].get<long long>()) + "." + std::to_string(v2["max"].get<long long>());
    }
    if (kind == "randomset") {
        checkFields(v2, "source", kind, {"values"}, {"values"}, where);
        const Json &values = v2["values"];
        if (!values.is_array() || values.empty()) fail(where, "'values' must be a non-empty list of strings");
        std::string joinedValues;
        for (std::size_t i = 0; i < values.size(); i++) {
            if (!values[i].is_string()) fail(where, "'values' must be a non-empty list of strings");
            const std::string token = values[i].get<std::string>();
            if (token.find('|') != std::string::npos) fail(where, "randomset value with '|' is not expressible in v1");
            joinedValues += (i ? "|" : "") + token;
        }
        return "randomset." + joinedValues;
    }
    if (kind == "timestamp") {
        checkFields(v2, "source", kind, {"unit"}, {"unit"}, where);
        const std::string unit = textField(v2, "unit", where);
        if (unit != "s" && unit != "ms" && unit != "us" && unit != "ns") fail(where, "'unit' must be s, ms, us or ns");
        return "timestamp." + unit;
    }
    if (kind == "vault") {
        checkFields(v2, "source", kind, {"key", "path"}, {"key"}, where);
        const std::string key = textField(v2, "key", where);
        checkVaultKey(key, where);
        if (!v2.contains("path")) return "vault." + key;
        const std::string path = textField(v2, "path", where);
        if (path.size() < 2 || !isPointer(path)) fail(where, "vault 'path' must be '/<node>...'");
        return "vault." + key + "." + path;
    }
    if (kind == "serverEvent" || kind == "clientEvent") {
        return kind + "." + eventToV1(v2, "source", kind, true, where);
    }
    for (const auto &entry : oneFieldSources()) {
        if (kind != entry.kind) continue;
        checkFields(v2, "source", kind, {entry.field}, {entry.field}, where);
        return entry.prefix + textField(v2, entry.field, where, entry.emptyAllowed);
    }
    fail(where, "unknown source '" + kind + "'");
}

/////////////
// TARGETS //
/////////////

Json GrammarMigrator::targetToV2(const std::string &v1, std::string &castType, const std::string &where) {
    castType.clear();
    for (const auto &entry : plainTargets()) {
        if (v1 == entry.first) return Json{{"target", entry.second}};
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
            if (!tail.empty() && tail[0] != '.') continue;
            castType = type.second;
            Json to = Json{{"target", side + "Body"}};
            if (tail.empty()) return to;
            const std::string path = tail.substr(1);
            if (!isPointer(path)) fail(where, "body path must be a json pointer ('/<node>...'): '" + v1 + "'");
            to["path"] = path;
            return to;
        }
        fail(where, "unknown json type in v1 target '" + v1 + "'");
    }
    if (startsWith(v1, "vault.")) {
        static const std::regex typed(R"(^vault\.([^.]+)\.json\.(object|jsonstring)(?:\.(/.+))?$)", std::regex::optimize);
        static const std::regex plain(R"(^vault\.([^.]+)(?:\.(/.+))?$)", std::regex::optimize);
        std::smatch m;
        Json to;
        if (std::regex_match(v1, m, typed)) {
            castType = (m.str(2) == "object") ? "object" : "json";
            to = Json{{"target", "vault"}, {"key", m.str(1)}};
            if (m[3].matched) to["path"] = m.str(3);
        } else if (std::regex_match(v1, m, plain)) {
            to = Json{{"target", "vault"}, {"key", m.str(1)}};
            if (m[2].matched) to["path"] = m.str(2);
        } else {
            fail(where, "v1 target 'vault.<key>[.json.object|.json.jsonstring][./<node>...]' expected, got '" + v1 + "'");
        }
        return to;
    }
    if (v1 == "outState") return Json{{"target", "outState"}};
    if (startsWith(v1, "outState.")) {
        const std::string rest = v1.substr(9);
        const std::size_t dot = rest.find('.');
        const std::string method = rest.substr(0, dot);
        if (!contains(outStateMethods(), method)) fail(where, "v1 target 'outState.<POST|GET|PUT|DELETE|HEAD>[.<uri>]' expected, got '" + v1 + "'");
        Json to = Json{{"target", "outState"}, {"method", method}};
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
        const std::string socket = rest.substr(0, bar);
        if (socket.empty()) fail(where, "missing socket path in v1 target '" + v1 + "'");
        Json to = Json{{"target", "udpSocket"}, {"socket", socket}};
        if (bar != std::string::npos) {
            // The delay may hold variables ("@{d}"): kept as text unless it is a canonical number.
            const std::string delay = rest.substr(bar + 1);
            if (isCanonicalInteger(delay, false)) to["delayMs"] = std::stoul(delay);
            else if (!delay.empty()) to["delayMs"] = delay;
            else fail(where, "empty delay in v1 target '" + v1 + "'");
        }
        return to;
    }
    if (startsWith(v1, "clientProvision.")) {
        const std::string rest = v1.substr(16);
        const std::size_t dot = rest.find('.');
        const std::string id = rest.substr(0, dot);
        const std::string inState = (dot == std::string::npos) ? "" : rest.substr(dot + 1);
        if (id.empty() || inState.empty()) fail(where, "v1 target 'clientProvision.<id>.<inState>' expected, got '" + v1 + "'");
        return Json{{"target", "clientProvision"}, {"id", id}, {"inState", inState}};
    }
    for (const std::string domain : {
                "serverEvent", "clientEvent"
            }) {
        if (!startsWith(v1, domain + ".")) continue;
        Json to = Json{{"target", domain}};
        eventToV2(v1.substr(domain.size() + 1), domain, false, to, where);
        return to;
    }
    for (const auto &entry : oneFieldTargets()) {
        if (!startsWith(v1, entry.prefix)) continue;
        const std::string arg = v1.substr(std::char_traits<char>::length(entry.prefix));
        if (arg.empty()) fail(where, "missing argument in v1 target '" + v1 + "'");
        return Json{{"target", entry.kind}, {entry.field, arg}};
    }
    fail(where, "cannot identify v1 target '" + v1 + "'");
}

std::string GrammarMigrator::targetToV1(const Json &v2, const std::string &castType, const std::string &where) {
    if (!v2.is_object() || !v2.contains("target") || !v2["target"].is_string()) fail(where, "must be an object with a 'target' kind");
    const std::string kind = v2["target"].get<std::string>();
    const auto noCast = [&]() {
        if (!castType.empty()) fail(where, "cast to '" + castType + "' on target '" + kind + "' is not expressible in v1");
    };

    for (const auto &entry : plainTargets()) {
        if (kind != entry.second) continue;
        checkFields(v2, "target", kind, {}, {}, where);
        noCast();
        return entry.first;
    }
    if (kind == "responseBody" || kind == "requestBody") {
        checkFields(v2, "target", kind, {"path"}, {}, where);
        if (castType.empty()) fail(where, "target '" + kind + "' needs a final cast filter to be expressible in v1");
        std::string v1Type;
        for (const auto &type : jsonTypes()) {
            if (type.second == castType) v1Type = type.first;
        }
        if (v1Type.empty()) fail(where, "unknown cast type '" + castType + "'");
        std::string v1 = std::string(kind == "responseBody" ? "response" : "request") + ".body.json." + v1Type;
        if (!v2.contains("path")) return v1;
        const std::string path = textField(v2, "path", where);
        if (!isPointer(path)) fail(where, "'path' must be a json pointer ('/<node>...')");
        return v1 + "." + path;
    }
    if (kind == "vault") {
        checkFields(v2, "target", kind, {"key", "path"}, {"key"}, where);
        const std::string key = textField(v2, "key", where);
        checkVaultKey(key, where);
        std::string v1 = "vault." + key;
        if (castType == "object") v1 += ".json.object";
        else if (castType == "json") v1 += ".json.jsonstring";
        else noCast();
        if (v2.contains("path")) {
            const std::string path = textField(v2, "path", where);
            if (path.size() < 2 || !isPointer(path)) fail(where, "vault 'path' must be '/<node>...'");
            v1 += "." + path;
        }
        return v1;
    }
    noCast();
    if (kind == "outState") {
        checkFields(v2, "target", kind, {"method", "uri"}, {}, where);
        if (!v2.contains("method")) {
            if (v2.contains("uri")) fail(where, "'uri' requires 'method'");
            return "outState";
        }
        const std::string method = textField(v2, "method", where);
        if (!contains(outStateMethods(), method)) fail(where, "'method' must be POST, GET, PUT, DELETE or HEAD");
        return "outState." + method + (v2.contains("uri") ? "." + textField(v2, "uri", where) : "");
    }
    if (kind == "udpSocket") {
        checkFields(v2, "target", kind, {"socket", "delayMs"}, {"socket"}, where);
        std::string v1 = "udpSocket." + textField(v2, "socket", where);
        if (v2.contains("delayMs")) {
            const Json &delay = v2["delayMs"];
            if (delay.is_number_unsigned() || (delay.is_number_integer() && delay.get<long long>() >= 0)) v1 += "|" + std::to_string(delay.get<unsigned long long>());
            else if (delay.is_string() && !delay.get<std::string>().empty()) v1 += "|" + delay.get<std::string>();
            else fail(where, "'delayMs' must be a non-negative integer or a text with variables");
        }
        return v1;
    }
    if (kind == "clientProvision") {
        checkFields(v2, "target", kind, {"id", "inState"}, {"id", "inState"}, where);
        const std::string id = textField(v2, "id", where);
        if (id.find('.') != std::string::npos) fail(where, "client provision 'id' must contain no dots");
        return "clientProvision." + id + "." + textField(v2, "inState", where);
    }
    if (kind == "serverEvent" || kind == "clientEvent") {
        return kind + "." + eventToV1(v2, "target", kind, false, where);
    }
    for (const auto &entry : oneFieldTargets()) {
        if (kind != entry.kind) continue;
        checkFields(v2, "target", kind, {entry.field}, {entry.field}, where);
        return entry.prefix + textField(v2, entry.field, where);
    }
    fail(where, "unknown target '" + kind + "'");
}

/////////////
// FILTERS //
/////////////

Json GrammarMigrator::filterToV2(const Json &v1, const std::string &where) {
    if (v1.is_string()) {
        if (v1.get<std::string>() != "Size") fail(where, "unknown filter '" + v1.get<std::string>() + "'");
        return Json{{"op", "size"}};
    }
    if (!v1.is_object() || v1.size() != 1) fail(where, "a v1 filter is \"Size\" or an object with one key");
    const std::string name = v1.begin().key();
    const Json &arg = v1.begin().value();

    if (name == "ConditionVar") {
        if (!arg.is_string()) fail(where, "ConditionVar argument must be a string");
        std::string var = arg.get<std::string>();
        const bool negated = !var.empty() && var[0] == '!';
        if (negated) var = var.substr(1);
        if (var.empty()) fail(where, "ConditionVar needs a variable name");
        Json item = Json{{"op", "conditionVar"}, {"name", var}};
        if (negated) item["not"] = true;
        return item;
    }
    if (name == "DifferentFrom") {  // v2 keeps one comparison filter: equalTo, negated
        if (!arg.is_string()) fail(where, "DifferentFrom argument must be a string");
        return Json{{"op", "equalTo"}, {"value", arg}, {"not", true}};
    }
    if (name == "JsonConstraint") {
        if (!arg.is_object() && !arg.is_array()) fail(where, "JsonConstraint argument must be an object or an array");
        return Json{{"op", "jsonConstraint"}, {"value", arg}};
    }
    for (const auto &f : scalarFilters()) {
        if (name != f.v1) continue;
        if (f.numeric ? !arg.is_number() : !arg.is_string()) fail(where, name + " argument must be " + (f.numeric ? "a number" : "a string"));
        return Json{{"op", f.op}, {f.field, arg}};
    }
    for (const auto &f : objectFilters()) {
        if (name != f.v1) continue;
        if (!arg.is_object()) fail(where, name + " argument must be an object");
        Json item = Json{{"op", f.op}};
        for (const auto &field : arg.items()) {
            if (!contains(f.fields, field.key())) fail(where, "unknown " + name + " field '" + field.key() + "' (valid: " + joined(f.fields) + ")");
            item[field.key()] = field.value();
        }
        for (const auto &r : f.required) {
            if (!arg.contains(r)) fail(where, name + " requires '" + r + "'");
        }
        return item;
    }
    if (name == "Size") fail(where, "'Size' takes no arguments: write \"Size\"");
    fail(where, "unknown filter '" + name + "'");
}

Json GrammarMigrator::filterToV1(const Json &v2, const std::string &where) {
    if (!v2.is_object() || !v2.contains("op") || !v2["op"].is_string()) fail(where, "a filter must be an object with an 'op'");
    const std::string op = v2["op"].get<std::string>();

    if (op == "size") {
        checkFields(v2, "op", op, {}, {}, where);
        return "Size";
    }
    if (op == "conditionVar") {
        checkFields(v2, "op", op, {"name", "not"}, {"name"}, where);
        const std::string name = textField(v2, "name", where);
        bool negated = false;
        if (v2.contains("not")) {
            if (!v2["not"].is_boolean()) fail(where, "'not' must be a boolean");
            negated = v2["not"].get<bool>();
        }
        return Json{{"ConditionVar", (negated ? "!" : "") + name}};
    }
    if (op == "equalTo") {
        checkFields(v2, "op", op, {"value", "not"}, {"value"}, where);
        if (!v2["value"].is_string()) fail(where, "'value' must be a string");
        if (v2.contains("not") && !v2["not"].is_boolean()) fail(where, "'not' must be a boolean");
        return Json{{v2.value("not", false) ? "DifferentFrom" : "EqualTo", v2["value"]}};
    }
    if (op == "jsonConstraint") {
        checkFields(v2, "op", op, {"value"}, {"value"}, where);
        if (!v2["value"].is_object() && !v2["value"].is_array()) fail(where, "'value' must be an object or an array");
        return Json{{"JsonConstraint", v2["value"]}};
    }
    for (const auto &f : scalarFilters()) {
        if (op != f.op) continue;
        checkFields(v2, "op", op, {f.field}, {f.field}, where);
        const Json &arg = v2[f.field];
        if (f.numeric ? !arg.is_number() : !arg.is_string()) fail(where, std::string("'") + f.field + "' must be " + (f.numeric ? "a number" : "a string"));
        return Json{{f.v1, arg}};
    }
    for (const auto &f : objectFilters()) {
        if (op != f.op) continue;
        checkFields(v2, "op", op, f.fields, f.required, where);
        Json arg = Json::object();
        for (const auto &field : v2.items()) {
            if (field.key() != "op") arg[field.key()] = field.value();
        }
        return Json{{f.v1, arg}};
    }
    if (op == "cast") fail(where, "a cast is only expressible in v1 as the last filter");
    fail(where, "unknown filter op '" + op + "'");
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

    // v1 skips any eraser rule carrying a filter (running onFilterFail instead): in v2 a
    // conditional erase does erase, so the behaviour cannot be kept by a syntax change.
    if (rule["from"]["source"] == "eraser" && v1Rule.contains("filter")) {
        fail(where + "/filter", "v1 never applies an 'eraser' rule with a filter (it is skipped): remove the rule, or rewrite it as a v2 conditional erase");
    }

    std::string castType;
    Json to = targetToV2(v1Rule["target"].get<std::string>(), castType, where + "/target");
    if ((to["target"] == "serverEvent" || to["target"] == "clientEvent") && rule["from"]["source"] != "eraser") {
        fail(where + "/source", "v1 never applies an event purge target with a source other than 'eraser' (it is skipped): remove the rule");
    }
    if ((to["target"] == "stop" || to["target"] == "clientProvision") && rule["from"]["source"] == "eraser") {
        fail(where + "/source", "v1 never fires a 'break' or 'clientProvision' target fed by 'eraser' (it does nothing): remove the rule");
    }

    if (castType == "string" && isBodyTarget(to) && contains(textSources(), rule["from"]["source"].get<std::string>())) {
        castType.clear();  // redundant: the source already yields a string
    }

    Json filters = Json::array();
    if (v1Rule.contains("filter")) filters.push_back(filterToV2(v1Rule["filter"], where + "/filter"));
    if (!castType.empty()) filters.push_back(Json{{"op", "cast"}, {"type", castType}});
    if (!filters.empty()) rule["filter"] = filters;
    rule["to"] = to;

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
    checkRuleKeys(v2Rule, {"from", "filter", "to", "onFilterFail"}, where);
    if (!v2Rule.contains("from")) fail(where + "/from", "missing");
    if (!v2Rule.contains("to")) fail(where + "/to", "missing");

    Json v1Filter;
    std::string castType;
    if (v2Rule.contains("filter")) {
        const Json &filters = v2Rule["filter"];
        const std::string at = where + "/filter";
        if (!filters.is_array() || filters.empty()) fail(at, "must be a non-empty list");
        std::vector<Json> plain;
        for (std::size_t i = 0; i < filters.size(); i++) {
            const Json &f = filters[i];
            const std::string fat = at + "/" + std::to_string(i);
            if (f.is_object() && f.contains("op") && f["op"] == "cast") {
                if (i + 1 != filters.size()) fail(fat, "a cast is only expressible in v1 as the last filter");
                checkFields(f, "op", "cast", {"type"}, {"type"}, fat);
                castType = textField(f, "type", fat);
                if (!contains(castTypes(), castType)) fail(fat, "unknown cast type '" + castType + "'");
                continue;
            }
            plain.push_back(filterToV1(f, fat));
        }
        if (plain.size() > 1) fail(at, "a chain of several filters is not expressible in v1");
        if (!plain.empty()) v1Filter = plain.front();
    }

    // A json body target without cast keeps the source type: v1 typed it as a string for text sources.
    if (castType.empty() && v2Rule["to"].is_object() && v2Rule["to"].contains("target") && isBodyTarget(v2Rule["to"]) &&
            v2Rule["from"].is_object() && v2Rule["from"].contains("source") && v2Rule["from"]["source"].is_string() &&
            contains(textSources(), v2Rule["from"]["source"].get<std::string>())) {
        castType = "string";
    }

    Json rule = Json::object();
    rule["source"] = sourceToV1(v2Rule["from"], where + "/from");
    if (rule["source"] == "eraser" && !v1Filter.is_null()) fail(where + "/filter", "a conditional erase is not expressible in v1");
    rule["target"] = targetToV1(v2Rule["to"], castType, where + "/to");
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
            if (contains(skipped, it->path().filename().string())) it.disable_recursion_pending();
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
