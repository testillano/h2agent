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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>
#include <memory>
#include <mutex>
#include <regex>
#include <set>

#include <arashpartow/exprtk.hpp>

#include "Engine.hpp"
#include "Rule.hpp"

namespace h2agent {
namespace engine {

/**
 * Parsed pipeline: an ordered vector of classified rules. Built once by
 * Engine::parse(), evaluated many times by Engine::evaluate().
 */
class Pipeline {
public:
    std::vector<Rule> rules{};
};

namespace {

// Value of a @{} reference: "<name>" or "<name>/<json pointer>" (the first '/'
// starts the pointer into the stored value, e.g. "doc/a/b" or "caps/1"), looked
// up in the local scope first, then in the global one (h2agent's local-vs-vault
// priority). Absent when unknown or when the pointer does not exist.
std::optional<nlohmann::json> lookupVariable(const std::string &ref, const Ports &ports) {
    if (!ports.store) return std::nullopt;
    const std::size_t slash = ref.find('/');
    const std::string name = (slash == std::string::npos) ? ref : ref.substr(0, slash);
    const std::string pointer = (slash == std::string::npos) ? std::string() : ref.substr(slash);
    auto v = ports.store->get(Store::Scope::Local, name, pointer);
    if (!v) v = ports.store->get(Store::Scope::Global, name, pointer);
    return v;
}

// Resolve @{name} / @{name/pointer} substitutions (see lookupVariable): strings
// are inserted as they are and any other value as its compact json text. An
// unknown name or pointer is kept literally (v1 behavior: only existing variables
// are replaced). A bare value (no @{...}) is returned unchanged.
std::string substituteVariables(const std::string &in, const Ports &ports) {
    std::string out;
    out.reserve(in.size());

    for (std::size_t i = 0; i < in.size();) {
        if (in[i] == '@' && i + 1 < in.size() && in[i + 1] == '{') {
            std::size_t close = in.find('}', i + 2);
            if (close != std::string::npos) {
                auto v = lookupVariable(in.substr(i + 2, close - (i + 2)), ports);
                if (v) out += v->is_string() ? v->get_ref<const std::string &>() : v->dump();
                else out.append(in, i, close + 1 - i);  // unknown: kept literally (v1)
                i = close + 1;
                continue;
            }
        }
        out += in[i];
        ++i;
    }
    return out;
}

// @{} template resolution without allocating when there is nothing to
// substitute (the common case): returns `in` itself, or the substituted text
// held in `scratch`.
const std::string &resolveTemplate(const std::string &in, std::string &scratch, const Ports &ports) {
    if (in.find("@{") == std::string::npos) return in;
    scratch = substituteVariables(in, ports);
    return scratch;
}

// Coerce a json value to its string representation (string verbatim, others
// dumped).
std::string asString(const nlohmann::json &v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_null()) return "";
    return v.dump();
}

// Validate that `received` contains the `expected` subset (parity with
// h2agent::model::jsonConstraint). Object mode: every key/value in `expected`
// must exist in `received` (recursing into nested objects/arrays). Array mode:
// every element in `expected` must be present somewhere in `received`
// (order-independent, extras allowed). On failure, `failReport` is set to a
// human-readable reason and false is returned.
bool jsonConstraint(const nlohmann::json &received, const nlohmann::json &expected, std::string &failReport) {
    if (expected.is_array()) {
        if (!received.is_array()) {
            failReport = "JsonConstraint FAILED: expected array but received non-array";
            return false;
        }
        for (std::size_t i = 0; i < expected.size(); ++i) {
            bool found = false;
            for (std::size_t j = 0; j < received.size(); ++j) {
                if (expected[i].is_object() && received[j].is_object()) {
                    std::string dummy;
                    if (jsonConstraint(received[j], expected[i], dummy)) {
                        found = true;
                        break;
                    }
                } else if (received[j] == expected[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                failReport = "JsonConstraint FAILED: expected element at index '" + std::to_string(i) +
                             "' not found in validated source array: " + expected[i].dump();
                return false;
            }
        }
        return true;
    }

    for (auto &el : expected.items()) {
        const std::string &key = el.key();
        const nlohmann::json &val = el.value();
        if (!received.is_object() || !received.contains(key)) {
            failReport = "JsonConstraint FAILED: expected key '" + key + "' is missing in validated source";
            return false;
        }
        if (val.is_object()) {
            if (!jsonConstraint(received[key], val, failReport)) return false;
        } else if (val.is_array() && received[key].is_array()) {
            if (!jsonConstraint(received[key], val, failReport)) return false;
        } else {
            if (received[key] != val) {
                failReport = "JsonConstraint FAILED: expected value for key '" + key +
                             "' differs regarding validated source";
                return false;
            }
        }
    }
    return true;
}

// --- typed coercions (parity with h2agent TypeConverter getters) ---
//
// The engine value is already a typed nlohmann::json, so the "native type" is
// the json type. Coercion follows h2agent exactly: objects/arrays never coerce
// to a scalar (except via asString), strings are parsed numerically (stoll /
// stoull / stod) and fail the rule when not parseable, and booleans map to 0/1.
// Each helper returns false to SKIP the rule (h2agent returns false) when the
// value cannot be coerced to the requested type.

bool toInteger(const nlohmann::json &v, std::int64_t &out) {
    if (v.is_number_integer() || v.is_number_unsigned()) {
        out = v.get<std::int64_t>();
        return true;
    }
    if (v.is_number_float()) {
        out = static_cast<std::int64_t>(v.get<double>());
        return true;
    }
    if (v.is_boolean()) {
        out = v.get<bool>() ? 1 : 0;
        return true;
    }
    if (v.is_string()) {
        try {
            out = std::stoll(v.get<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;  // object/array/null
}

bool toUnsigned(const nlohmann::json &v, std::uint64_t &out) {
    if (v.is_number_unsigned()) {
        out = v.get<std::uint64_t>();
        return true;
    }
    if (v.is_number_integer()) {
        out = static_cast<std::uint64_t>(v.get<std::int64_t>());
        return true;
    }
    if (v.is_number_float()) {
        out = static_cast<std::uint64_t>(v.get<double>());
        return true;
    }
    if (v.is_boolean()) {
        out = v.get<bool>() ? 1u : 0u;
        return true;
    }
    if (v.is_string()) {
        try {
            out = std::stoull(v.get<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;  // object/array/null
}

bool toFloat(const nlohmann::json &v, double &out) {
    if (v.is_number()) {
        out = v.get<double>();
        return true;
    }
    if (v.is_boolean()) {
        out = v.get<bool>() ? 1.0 : 0.0;
        return true;
    }
    if (v.is_string()) {
        try {
            out = std::stod(v.get<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;  // object/array/null
}

bool toBoolean(const nlohmann::json &v, bool &out) {
    if (v.is_boolean()) {
        out = v.get<bool>();
        return true;
    }
    if (v.is_number_integer()) {
        out = v.get<std::int64_t>() != 0;
        return true;
    }
    if (v.is_number_unsigned()) {
        out = v.get<std::uint64_t>() != 0;
        return true;
    }
    if (v.is_number_float()) {
        out = v.get<double>() != 0.0;
        return true;
    }
    if (v.is_string()) {
        out = !v.get<std::string>().empty();  // non-empty string -> true
        return true;
    }
    return false;  // object/array/null
}

// Decode a hex string (optional "0x" prefix) into raw bytes. Parity with
// h2agent's fromHexString: odd length or any non-hex digit fails (returns
// false -> skip rule). Accepts lower-case and upper-case hex digits.
bool fromHexString(const std::string &input, std::string &output) {
    bool has0x = (input.rfind("0x", 0) == 0);
    if ((input.length() % 2) != 0) return false;  // odd length

    output.clear();
    auto nibble = [](char c, int &v) -> bool {
        if (c >= '0' && c <= '9') {
            v = c - '0';
            return true;
        }
        if (c >= 'a' && c <= 'f') {
            v = (c - 'a') + 0x0a;
            return true;
        }
        if (c >= 'A' && c <= 'F') {
            v = (c - 'A') + 0x0a;
            return true;
        }
        return false;
    };

    for (std::size_t i = (has0x ? 2 : 0); i < input.length(); i += 2) {
        int hi = 0, lo = 0;
        if (!nibble(input[i], hi) || !nibble(input[i + 1], lo)) return false;
        output += static_cast<char>((hi << 4) | lo);
    }
    return true;
}

// --- source classification / parsing ---

bool startsWith(const std::string &s, const std::string &p) {
    return s.rfind(p, 0) == 0;
}

// Split a store expression "<key>[./<json-pointer>]" on the FIRST "./" (dot
// immediately followed by the pointer's leading '/'). Everything before is the
// key, so keys MAY contain dots (h2agent v1 var names do, e.g. RegexCapture
// groups "caps.1"); everything after the dot is the pointer (leading '/').
void splitKeyPtr(const std::string &rest, std::string &key, std::string &ptr) {
    std::size_t sep = rest.find("./");
    if (sep == std::string::npos) {
        key = rest;
        ptr = "";
    } else {
        key = rest.substr(0, sep);
        ptr = rest.substr(sep + 1);  // includes the leading '/', e.g. "/a/b"
    }
}

// URL-decode (%XX and '+'), parity with ert::http2comm::URLFunctions::decode.
std::string urlDecode(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char ch = in[i];
        if (ch == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
                std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
            out.push_back(static_cast<char>(std::strtol(in.substr(i + 1, 2).c_str(), nullptr, 16)));
            i += 2;
        } else if (ch == '+') {
            out.push_back(' ');
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

// Tokenize a "k=v&k=v" event address ONCE (parse time), URL-decoding each value.
// A repeated key yields an empty map (parity with h2agent extractQueryParameters).
std::map<std::string, std::string> parseEventAddress(const std::string &query) {
    std::map<std::string, std::string> out;
    if (query.empty()) return out;
    std::size_t pos = 0;
    while (pos != std::string::npos) {
        std::size_t amp = query.find('&', pos);
        std::string pair = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        std::size_t eq = pair.find('=');
        std::string k = pair.substr(0, eq);
        std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
        if (out.count(k) != 0) return {};
        out.emplace(k, urlDecode(v));
        pos = (amp == std::string::npos) ? std::string::npos : amp + 1;
    }
    return out;
}

// Build the address object handed to the EventSource port: @{var} substituted
// PER VALUE at evaluation time (after parse-time tokenization), so variable
// values containing '&' or '=' cannot corrupt the address (h2agent parity).
nlohmann::json eventAddressJson(const std::map<std::string, std::string> &params, const Ports &ports) {
    nlohmann::json addr = nlohmann::json::object();
    for (const auto &kv : params) addr[kv.first] = substituteVariables(kv.second, ports);
    return addr;
}

}  // namespace

////////////////////////
// PRECOMPILED MATH   //
////////////////////////

// A math source compiled ONCE at parse time (provision load), as the regex
// filters are. Each distinct @{name} becomes an exprtk variable, so evaluate()
// only assigns values and calls value(): no per-event compilation.
//
// Thread safety: an exprtk expression evaluates over its bound variables, so a
// compiled instance cannot be shared by concurrent evaluations. The program
// keeps a small pool of instances (one is compiled at parse; more are compiled
// only under real concurrency and then reused): acquire/release is a short
// uncontended lock, and the pool dies with the pipeline (no leaks on reload).
struct MathProgram {
    struct Instance {
        std::vector<double> vars;  // sized before binding: never reallocated
        exprtk::symbol_table<double> symbols;
        exprtk::expression<double> expression;
    };

    bool constant{false};  // no @{} at all: folded to `constantValue` at parse
    double constantValue{0};
    std::string bound{};                    // expression with @{name} -> identifier
    std::vector<std::string> names{};       // variable name per slot
    std::vector<std::string> identifiers{};  // exprtk identifier per slot

    std::unique_ptr<Instance> compile() const {
        auto in = std::make_unique<Instance>();
        in->vars.assign(names.size(), 0.0);
        for (std::size_t i = 0; i < names.size(); ++i) in->symbols.add_variable(identifiers[i], in->vars[i]);
        in->expression.register_symbol_table(in->symbols);
        exprtk::parser<double> parser;
        if (!parser.compile(bound, in->expression)) return nullptr;
        return in;
    }

    std::unique_ptr<Instance> acquire() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!pool_.empty()) {
                auto in = std::move(pool_.back());
                pool_.pop_back();
                return in;
            }
        }
        return compile();  // concurrent use beyond the pool size: compile one more
    }

    void release(std::unique_ptr<Instance> in) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(in));
    }

    // Build the program for `expr`, or null when it cannot be bound with exact
    // textual-substitution semantics (the caller then keeps the v1 path).
    static std::shared_ptr<MathProgram> build(const std::string &expr) {
        auto program = std::make_shared<MathProgram>();
        auto isWordChar = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
        };
        std::string out;
        for (std::size_t i = 0; i < expr.size();) {
            if (expr[i] == '@' && i + 1 < expr.size() && expr[i + 1] == '{') {
                const std::size_t close = expr.find('}', i + 2);
                if (close == std::string::npos) return nullptr;
                // Glued to a token ("2@{x}", "@{x}e3"): textual concatenation has no
                // variable equivalent.
                if ((i > 0 && isWordChar(expr[i - 1])) || (close + 1 < expr.size() && isWordChar(expr[close + 1])))
                    return nullptr;
                const std::string name = expr.substr(i + 2, close - (i + 2));
                std::size_t slot = 0;
                while (slot < program->names.size() && program->names[slot] != name) ++slot;
                if (slot == program->names.size()) {
                    std::string id = "mathvar_" + std::to_string(slot);
                    while (expr.find(id) != std::string::npos) id = "x" + id;  // never collide with user text
                    program->names.push_back(name);
                    program->identifiers.push_back(id);
                }
                out += program->identifiers[slot];
                i = close + 1;
                continue;
            }
            out += expr[i++];
        }
        program->bound = std::move(out);

        auto first = program->compile();
        if (!first) return nullptr;  // invalid expression: keep v1 behavior (per-event path)
        if (program->names.empty()) {
            program->constant = true;
            program->constantValue = first->expression.value();
        } else {
            program->pool_.push_back(std::move(first));
        }
        return program;
    }

private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<Instance>> pool_;
};

namespace {

// Strict unsigned decimal ("12", "1.5", ".5", "1e3"): the only values whose
// binding is identical to textual substitution. Signs are excluded on purpose:
// "@{x}^2" with x="-3" is -(3^2) textually but (-3)^2 bound.
bool plainUnsignedDecimal(const std::string &s, double &out) {
    std::size_t i = 0, digits = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i, ++digits;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i, ++digits;
    }
    if (digits == 0) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        std::size_t j = i + 1;
        if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
        std::size_t expDigits = 0;
        while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j, ++expDigits;
        if (expDigits == 0) return false;
        i = j;
    }
    if (i != s.size()) return false;
    out = std::strtod(s.c_str(), nullptr);
    return true;
}


SourceType classifySource(const std::string &expr, Rule &rule) {
    if (startsWith(expr, "value.")) {
        rule.sourceArg = expr.substr(6);
        return SourceType::Value;
    }
    if (expr == "sequence") return SourceType::Sequence;
    if (startsWith(expr, "timestamp.")) {
        rule.sourceArg = expr.substr(10);  // s|ms|us|ns
        if (rule.sourceArg == "s" || rule.sourceArg == "ms" || rule.sourceArg == "us" || rule.sourceArg == "ns")
            return SourceType::Timestamp;
        return SourceType::Unsupported;
    }
    if (startsWith(expr, "strftime.")) {
        // strftime.<format>: current UTC time formatted via strftime. The format
        // is free text (errors are captured at evaluation); it admits @{var}
        // substitution on the resulting string (parity with h2agent).
        rule.sourceArg = expr.substr(9);
        return SourceType::Strftime;
    }
    if (startsWith(expr, "random.")) {
        // random.<min>.<max> (min/max may be signed)
        std::string rest = expr.substr(7);
        std::size_t dot = rest.find('.', (rest[0] == '-' || rest[0] == '+') ? 1 : 0);
        if (dot == std::string::npos) return SourceType::Unsupported;
        try {
            rule.sourceI1 = std::stoll(rest.substr(0, dot));
            rule.sourceI2 = std::stoll(rest.substr(dot + 1));
        } catch (...) {
            return SourceType::Unsupported;
        }
        if (rule.sourceI2 < rule.sourceI1) return SourceType::Unsupported;
        return SourceType::Random;
    }
    if (startsWith(expr, "randomset.")) {
        // randomset.<v1|v2|..|vN>: tokenize the pipe-separated list, preserving
        // empty parts (leading/trailing/consecutive pipes -> empty-string tokens).
        // Parity with h2agent, which appends a trailing pipe before splitting so
        // the final (possibly empty) token is kept; here we split manually so that
        // 'a||b' => {"a","","b"} and 'a|' => {"a",""} and '' (bare "randomset.")
        // => {""} (a single empty token). The chosen token admits @{var}
        // substitution at evaluation.
        std::string rest = expr.substr(10);  // everything after "randomset."
        rule.sourceTokens.clear();
        std::size_t start = 0;
        while (true) {
            std::size_t bar = rest.find('|', start);
            if (bar == std::string::npos) {
                rule.sourceTokens.push_back(rest.substr(start));
                break;
            }
            rule.sourceTokens.push_back(rest.substr(start, bar - start));
            start = bar + 1;
        }
        return SourceType::RandomSet;
    }
    if (expr == "eraser") return SourceType::Eraser;
    if (startsWith(expr, "math.")) {
        // math.<expression>: store the raw expression (admits @{var} substitution
        // before evaluation). See the Math case in evaluate() for the deferred
        // evaluator decision made in this batch.
        rule.sourceArg = expr.substr(5);
        rule.math = MathProgram::build(rule.sourceArg);  // compiled once, at load
        return SourceType::Math;
    }
    if (startsWith(expr, "local.")) {
        splitKeyPtr(expr.substr(6), rule.sourceKey, rule.sourcePtr);
        return SourceType::StoreLocal;
    }
    if (startsWith(expr, "global.")) {
        splitKeyPtr(expr.substr(7), rule.sourceKey, rule.sourcePtr);
        return SourceType::StoreGlobal;
    }
    if (expr == "inState") return SourceType::InState;
    // Neutral v2 payload sources: input[./json-pointer] / output[./json-pointer].
    // Bare "input"/"output" means the whole document (ptr ""); a ".<pointer>"
    // suffix (e.g. input./a/b) navigates into it. The host maps its transport
    // vocabulary (HTTP request/response, Diameter message, ...) into these two
    // neutral json documents, so the engine stays transport-agnostic.
    if (expr == "input") {
        rule.sourcePtr = "";
        return SourceType::Input;
    }
    if (startsWith(expr, "input.")) {
        rule.sourcePtr = expr.substr(6);  // the /pointer part (leading '/')
        return SourceType::Input;
    }
    if (expr == "meta") {
        rule.sourcePtr = "";
        return SourceType::Meta;
    }
    if (startsWith(expr, "meta.")) {
        rule.sourcePtr = expr.substr(5);  // the /pointer part (leading '/')
        return SourceType::Meta;
    }
    if (expr == "output") {
        rule.sourcePtr = "";
        return SourceType::Output;
    }
    if (startsWith(expr, "output.")) {
        rule.sourcePtr = expr.substr(7);  // the /pointer part (leading '/')
        return SourceType::Output;
    }
    if (startsWith(expr, "serverEvent.")) {
        rule.sourceArg = expr.substr(12);  // raw address kept for traces
        rule.sourceEventDomain = "serverEvent";
        rule.sourceEventParams = parseEventAddress(rule.sourceArg);
        return SourceType::ServerEvent;
    }
    if (startsWith(expr, "clientEvent.")) {
        rule.sourceArg = expr.substr(12);
        rule.sourceEventDomain = "clientEvent";
        rule.sourceEventParams = parseEventAddress(rule.sourceArg);
        return SourceType::ClientEvent;
    }
    if (expr == "sendseq") return SourceType::Sendseq;
    if (startsWith(expr, "txtFile.")) {
        rule.sourceArg = expr.substr(8);  // path (admits @{var})
        return SourceType::TxtFile;
    }
    if (startsWith(expr, "binFile.")) {
        rule.sourceArg = expr.substr(8);
        return SourceType::BinFile;
    }
    if (startsWith(expr, "command.")) {
        rule.sourceArg = expr.substr(8);  // shell command (admits @{var})
        return SourceType::Command;
    }
    return SourceType::Unsupported;
}

// Match a "output.json.<kind>[./json-pointer]" target. On a match, fills
// rule.targetPath with the pointer ("" for the bare form) and returns true.
// Mirrors h2agent, where typed json targets all share the optional ./ptr tail.
// Match a typed store write "<scopePrefix><key>.json.<kind>[./ptr]" (e.g.
// "global.doc.json.object./a"). Splits the key and the optional json-pointer.
// The key may not contain dots (same rule as the plain store key/path split).
bool matchStoreJsonTyped(const std::string &expr, const std::string &scopePrefix, const std::string &kind,
                         Rule &rule) {
    if (!startsWith(expr, scopePrefix)) return false;
    std::string rest = expr.substr(scopePrefix.size());  // "<key>.json.<kind>[./ptr]"
    // The key may contain dots (v1 var names), so locate the ".json.<kind>" marker
    // followed by end-of-string or "./" (the optional pointer).
    const std::string marker = ".json." + kind;
    std::size_t from = 0;
    while (true) {
        std::size_t m = rest.find(marker, from);
        if (m == std::string::npos || m == 0) return false;
        std::size_t after = m + marker.size();
        if (after == rest.size()) {
            rule.targetKey = rest.substr(0, m);
            rule.targetPtr = "";
            return true;
        }
        if (rest.compare(after, 2, "./") == 0) {
            rule.targetKey = rest.substr(0, m);
            rule.targetPtr = rest.substr(after + 1);  // the /pointer
            return true;
        }
        from = m + 1;
    }
}

bool matchJsonTypedTarget(const std::string &expr, const std::string &kind, Rule &rule) {
    const std::string base = "output.json." + kind;
    if (expr == base) {
        rule.targetPath = "";  // root
        return true;
    }
    if (startsWith(expr, base + ".")) {
        rule.targetPath = expr.substr(base.size() + 1);  // the /pointer part
        return true;
    }
    return false;
}

TargetType classifyTarget(const std::string &expr, Rule &rule) {
    if (expr == "output.string") return TargetType::OutputString;
    if (expr == "output.hexstring") return TargetType::OutputHexString;
    if (expr == "output.json") return TargetType::OutputJson;  // root ("" path)
    if (startsWith(expr, "output.json./")) {
        rule.targetPath = expr.substr(12);  // the /pointer part
        return TargetType::OutputJson;
    }
    if (matchJsonTypedTarget(expr, "string", rule)) return TargetType::OutputJsonString;
    if (matchJsonTypedTarget(expr, "integer", rule)) return TargetType::OutputJsonInteger;
    if (matchJsonTypedTarget(expr, "unsigned", rule)) return TargetType::OutputJsonUnsigned;
    if (matchJsonTypedTarget(expr, "float", rule)) return TargetType::OutputJsonFloat;
    if (matchJsonTypedTarget(expr, "boolean", rule)) return TargetType::OutputJsonBoolean;
    if (matchJsonTypedTarget(expr, "object", rule)) return TargetType::OutputJsonObject;
    if (matchJsonTypedTarget(expr, "jsonstring", rule)) return TargetType::OutputJsonJsonString;
    // Typed store writes must be checked BEFORE the generic local./global. so the
    // ".json.object"/".json.jsonstring" suffixes are not swallowed by the generic key.
    if (matchStoreJsonTyped(expr, "local.", "object", rule)) return TargetType::StoreLocalJsonObject;
    if (matchStoreJsonTyped(expr, "local.", "jsonstring", rule)) return TargetType::StoreLocalJsonString;
    if (matchStoreJsonTyped(expr, "global.", "object", rule)) return TargetType::StoreGlobalJsonObject;
    if (matchStoreJsonTyped(expr, "global.", "jsonstring", rule)) return TargetType::StoreGlobalJsonString;
    if (startsWith(expr, "local.")) {
        splitKeyPtr(expr.substr(6), rule.targetKey, rule.targetPtr);
        return TargetType::StoreLocal;
    }
    if (startsWith(expr, "global.")) {
        splitKeyPtr(expr.substr(7), rule.targetKey, rule.targetPtr);
        return TargetType::StoreGlobal;
    }
    if (expr == "outState") return TargetType::OutState;
    if (startsWith(expr, "outState.")) {
        // outState.<METHOD>[.<uri>] : split METHOD (up to next dot) and optional uri.
        std::string rest = expr.substr(9);
        std::size_t dot = rest.find('.');
        if (dot == std::string::npos) {
            rule.targetMethod = rest;
        } else {
            rule.targetMethod = rest.substr(0, dot);
            rule.targetUri = rest.substr(dot + 1);
        }
        return TargetType::OutStateForeign;
    }
    if (startsWith(expr, "meta.header.")) {
        rule.targetName = expr.substr(12);  // header name (admits @{var})
        return TargetType::MetaHeader;
    }
    if (expr == "meta.statusCode") return TargetType::MetaStatusCode;
    if (expr == "meta.delayMs") return TargetType::MetaDelayMs;
    if (expr == "meta.uri") return TargetType::MetaUri;
    if (expr == "meta.method") return TargetType::MetaMethod;
    if (expr == "meta.timeoutMs") return TargetType::MetaTimeoutMs;
    if (startsWith(expr, "txtFile.")) {
        rule.targetFilePath = expr.substr(8);
        return TargetType::TxtFile;
    }
    if (startsWith(expr, "binFile.")) {
        rule.targetFilePath = expr.substr(8);
        return TargetType::BinFile;
    }
    if (startsWith(expr, "udpSocket.")) {
        // udpSocket.<path>[|<ms>]
        std::string rest = expr.substr(10);
        std::size_t bar = rest.find('|');
        if (bar == std::string::npos) {
            rule.targetSocketPath = rest;
        } else {
            rule.targetSocketPath = rest.substr(0, bar);
            try {
                rule.targetSocketDelayMs = static_cast<unsigned int>(std::stoul(rest.substr(bar + 1)));
            } catch (...) {
                rule.targetSocketDelayMs = 0;
            }
        }
        return TargetType::UdpSocket;
    }
    if (startsWith(expr, "serverEvent.")) {
        rule.targetEventDomain = "serverEvent";
        rule.targetEventParams = parseEventAddress(expr.substr(12));
        return TargetType::EventPurge;
    }
    if (startsWith(expr, "clientEvent.")) {
        rule.targetEventDomain = "clientEvent";
        rule.targetEventParams = parseEventAddress(expr.substr(12));
        return TargetType::EventPurge;
    }
    if (startsWith(expr, "clientProvision.")) {
        // clientProvision.<id>.<inState>
        std::string rest = expr.substr(16);
        std::size_t dot = rest.find('.');
        // v1 parity: both <id> and <inState> are mandatory; otherwise the
        // transformation item is discarded (here: Unsupported -> skipped).
        if (dot == std::string::npos || dot == 0 || dot + 1 >= rest.size()) return TargetType::Unsupported;
        rule.targetProvisionId = rest.substr(0, dot);
        rule.targetProvisionInState = rest.substr(dot + 1);
        return TargetType::ClientProvisionTrigger;
    }
    if (expr == "break") return TargetType::Break;
    return TargetType::Unsupported;
}

FilterType classifyFilter(const nlohmann::json &filterNode, Filter &filter, Rule &rule) {
    // Parameterless string filter: {"filter":"Size"} (parity with h2agent).
    if (filterNode.is_string() && filterNode.get<std::string>() == "Size") {
        filter.raw = "Size";
        return FilterType::Size;
    }
    if (!filterNode.is_object()) return FilterType::None;
    if (filterNode.contains("Append") && filterNode["Append"].is_string()) {
        filter.arg = filterNode["Append"].get<std::string>();
        filter.raw = "Append";
        return FilterType::Append;
    }
    if (filterNode.contains("Prepend") && filterNode["Prepend"].is_string()) {
        filter.arg = filterNode["Prepend"].get<std::string>();
        filter.raw = "Prepend";
        return FilterType::Prepend;
    }
    if (filterNode.contains("Sum") && filterNode["Sum"].is_number()) {
        filter.number = filterNode["Sum"].get<double>();
        filter.raw = "Sum";
        return FilterType::Sum;
    }
    if (filterNode.contains("Multiply") && filterNode["Multiply"].is_number()) {
        filter.number = filterNode["Multiply"].get<double>();
        filter.raw = "Multiply";
        return FilterType::Multiply;
    }
    // Regex filters: precompile the pattern ONCE here (never in evaluate, per the
    // performance rule). std::regex::optimize mirrors h2agent, which relies on it
    // so matching cannot throw at evaluation time. An invalid pattern cannot be
    // honored, so the whole rule is marked Unsupported (skipped + traced) rather
    // than throwing.
    if (filterNode.contains("RegexReplace") && filterNode["RegexReplace"].is_object()) {
        const auto &rr = filterNode["RegexReplace"];
        if (rr.contains("rgx") && rr["rgx"].is_string() && rr.contains("fmt") && rr["fmt"].is_string()) {
            filter.raw = "RegexReplace";
            filter.arg = rr["fmt"].get<std::string>();  // replacement format
            try {
                filter.regex = std::regex(rr["rgx"].get<std::string>(), std::regex::optimize);
            } catch (const std::regex_error &) {
                rule.targetType = TargetType::Unsupported;  // invalid pattern -> skip rule
                return FilterType::None;
            }
            return FilterType::RegexReplace;
        }
    }
    if (filterNode.contains("RegexCapture") && filterNode["RegexCapture"].is_string()) {
        filter.raw = "RegexCapture";
        try {
            filter.regex = std::regex(filterNode["RegexCapture"].get<std::string>(), std::regex::optimize);
        } catch (const std::regex_error &) {
            rule.targetType = TargetType::Unsupported;  // invalid pattern -> skip rule
            return FilterType::None;
        }
        return FilterType::RegexCapture;
    }
    // ConditionVar: {"ConditionVar":"[!]varname"}. Stores the (possibly '!'-
    // prefixed) variable name verbatim; evaluation reads the store and inverts.
    if (filterNode.contains("ConditionVar") && filterNode["ConditionVar"].is_string()) {
        filter.raw = "ConditionVar";
        filter.arg = filterNode["ConditionVar"].get<std::string>();
        return FilterType::ConditionVar;
    }
    if (filterNode.contains("EqualTo") && filterNode["EqualTo"].is_string()) {
        filter.raw = "EqualTo";
        filter.arg = filterNode["EqualTo"].get<std::string>();  // admits @{var}
        return FilterType::EqualTo;
    }
    if (filterNode.contains("DifferentFrom") && filterNode["DifferentFrom"].is_string()) {
        filter.raw = "DifferentFrom";
        filter.arg = filterNode["DifferentFrom"].get<std::string>();  // admits @{var}
        return FilterType::DifferentFrom;
    }
    // JsonConstraint: {"JsonConstraint":<obj|array>}. Keep the expected subset.
    if (filterNode.contains("JsonConstraint") &&
            (filterNode["JsonConstraint"].is_object() || filterNode["JsonConstraint"].is_array())) {
        filter.raw = "JsonConstraint";
        filter.object = filterNode["JsonConstraint"];
        return FilterType::JsonConstraint;
    }
    if (filterNode.contains("SchemaId") && filterNode["SchemaId"].is_string()) {
        filter.raw = "SchemaId";
        filter.arg = filterNode["SchemaId"].get<std::string>();  // schema id
        return FilterType::SchemaId;
    }
    // Split: {"Split":{size,count,sep,filler,numeric}} with defaults 2/4/'.'/'0'/false.
    if (filterNode.contains("Split") && filterNode["Split"].is_object()) {
        const auto &sp = filterNode["Split"];
        filter.raw = "Split";
        filter.i1 = sp.contains("size") && sp["size"].is_number() ? sp["size"].get<long long>() : 2;
        filter.i2 = sp.contains("count") && sp["count"].is_number() ? sp["count"].get<long long>() : 4;
        filter.arg = sp.contains("sep") && sp["sep"].is_string() ? sp["sep"].get<std::string>() : ".";
        filter.filler = sp.contains("filler") && sp["filler"].is_string() ? sp["filler"].get<std::string>() : "0";
        filter.flag = sp.contains("numeric") && sp["numeric"].is_boolean() && sp["numeric"].get<bool>();
        return FilterType::Split;
    }
    // BaseConvert: {"BaseConvert":{in,out,capital}}. in/out are bases 2-36.
    if (filterNode.contains("BaseConvert") && filterNode["BaseConvert"].is_object()) {
        const auto &bc = filterNode["BaseConvert"];
        if (bc.contains("in") && bc["in"].is_number() && bc.contains("out") && bc["out"].is_number()) {
            filter.raw = "BaseConvert";
            filter.i1 = bc["in"].get<long long>();
            filter.i2 = bc["out"].get<long long>();
            filter.flag = bc.contains("capital") && bc["capital"].is_boolean() && bc["capital"].get<bool>();
            return FilterType::BaseConvert;
        }
    }
    // Strptime / Strftime: {"...":{fmt,unit}} with unit s|ms|us|ns (default s).
    auto parseUnit = [](const nlohmann::json &n) -> int {
        std::string u = (n.contains("unit") && n["unit"].is_string()) ? n["unit"].get<std::string>() : "s";
        return (u == "ms") ? 1 : (u == "us") ? 2 : (u == "ns") ? 3 : 0;
    };
    if (filterNode.contains("Strptime") && filterNode["Strptime"].is_object()) {
        const auto &sp = filterNode["Strptime"];
        if (sp.contains("fmt") && sp["fmt"].is_string()) {
            filter.raw = "Strptime";
            filter.arg = sp["fmt"].get<std::string>();
            filter.unit = parseUnit(sp);
            return FilterType::Strptime;
        }
    }
    if (filterNode.contains("Strftime") && filterNode["Strftime"].is_object()) {
        const auto &sf = filterNode["Strftime"];
        if (sf.contains("fmt") && sf["fmt"].is_string()) {
            filter.raw = "Strftime";
            filter.arg = sf["fmt"].get<std::string>();
            filter.unit = parseUnit(sf);
            return FilterType::Strftime;
        }
    }
    // RegexKey: {"RegexKey":"<pattern>"}. Pattern precompiled; NO @{} subst.
    if (filterNode.contains("RegexKey") && filterNode["RegexKey"].is_string()) {
        filter.raw = "RegexKey";
        try {
            filter.regex = std::regex(filterNode["RegexKey"].get<std::string>(), std::regex::optimize);
        } catch (const std::regex_error &) {
            rule.targetType = TargetType::Unsupported;  // invalid pattern -> skip rule
            return FilterType::None;
        }
        return FilterType::RegexKey;
    }
    // Cast: {"Cast":"<type>"} (grammar v2 cast which is not a typed target).
    if (filterNode.contains("Cast") && filterNode["Cast"].is_string()) {
        static const std::set<std::string> types = {"string", "integer", "unsigned", "float", "boolean", "object", "json"};
        filter.arg = filterNode["Cast"].get<std::string>();
        if (types.count(filter.arg) == 0) return FilterType::None;
        filter.raw = "Cast";
        return FilterType::Cast;
    }
    return FilterType::None;  // unknown/absent filter -> none
}

// Removes the node addressed by `pointer` from `doc` (its parent key or array
// item); an empty pointer is the whole document, which becomes an empty object.
// A missing node is a no-op.
void eraseNode(nlohmann::json &doc, const std::string &pointer) {
    if (pointer.empty()) {
        doc = nlohmann::json::object();
        return;
    }
    const std::size_t lastSlash = pointer.find_last_of('/');
    try {
        const nlohmann::json::json_pointer node(pointer);
        if (!doc.contains(node)) return;
        nlohmann::json &parent = doc[nlohmann::json::json_pointer(pointer.substr(0, lastSlash))];
        const std::string child = node.back();
        if (parent.is_object()) parent.erase(child);
        else if (parent.is_array()) parent.erase(std::stoul(child));
    } catch (const std::exception &) {
        // malformed pointer or array index: nothing to erase
    }
}

// Single boolean criterion of the grammar (conditionVar, stop, clientProvision):
// null, false, numeric zero and the empty string are false; anything else (any
// non-empty string -- "0" and "false" included --, non-zero numbers, true, any
// object or array) is true. An absent value never reaches it (the rule is skipped).
bool truthy(const nlohmann::json &v) {
    if (v.is_string()) return !v.get_ref<const std::string &>().empty();
    if (v.is_number_integer()) return v.get<std::int64_t>() != 0;
    if (v.is_number_unsigned()) return v.get<std::uint64_t>() != 0;
    if (v.is_number_float()) return v.get<double>() != 0.0;
    if (v.is_boolean()) return v.get<bool>();
    return !v.is_null();
}

// ConditionVar test "[!]varname" ("varname" may be "<name>/<pointer>", as in
// @{}): the store variable (local first, then global) is truthy (absent is
// false), inverted by a leading '!'.
bool conditionHolds(const std::string &arg, const Ports &ports) {
    std::string varname = arg;
    bool reverse = (!varname.empty() && varname[0] == '!');
    if (reverse) varname.erase(0, 1);
    std::optional<nlohmann::json> v = lookupVariable(varname, ports);
    const bool isTrue = v && truthy(*v);
    return reverse ? !isTrue : isTrue;
}

// Cast filter: converts `value` to the type (see FilterType::Cast). False when
// the value cannot be converted (value untouched).
bool castValue(const std::string &type, nlohmann::json &value) {
    if (type == "string") {
        value = asString(value);
        return true;
    }
    if (type == "integer") {
        std::int64_t n;
        if (!toInteger(value, n)) return false;
        value = n;
        return true;
    }
    if (type == "unsigned") {
        std::uint64_t n;
        if (!toUnsigned(value, n)) return false;
        value = n;
        return true;
    }
    if (type == "float") {
        double n;
        if (!toFloat(value, n)) return false;
        value = n;
        return true;
    }
    if (type == "boolean") {
        bool b;
        if (!toBoolean(value, b)) return false;
        value = b;
        return true;
    }
    if (type == "object") return value.is_object() || value.is_array();
    try {  // "json": parse the value text
        value = nlohmann::json::parse(asString(value));
    } catch (...) {
        return false;
    }
    return true;
}

// Applies one filter of the rule chain over `value`. `last` tells whether it is
// the last filter of the chain: only then capture groups take the store target
// layout (otherwise the value goes on as the whole match / the matched key value).
// Returns false when the filter fails (the rule is skipped and its onFilterFail
// runs), with the reason in `note`.
bool applyFilter(const Filter &f, bool last, const Rule &rule, nlohmann::json &value, const Ports &ports,
                 std::vector<std::string> &localGroups, std::size_t &firstGroup, std::string &note) {
    if (f.type == FilterType::Append) {
        std::string argBuf;
        value = asString(value) + resolveTemplate(f.arg, argBuf, ports);
    } else if (f.type == FilterType::Prepend) {
        std::string argBuf;
        value = resolveTemplate(f.arg, argBuf, ports) + asString(value);
    } else if (f.type == FilterType::Sum || f.type == FilterType::Multiply) {
        // Coerce source to a number (string numbers accepted), operate, keep
        // numeric.
        double n = 0.0;
        bool ok = true;
        if (value.is_number()) {
            n = value.get<double>();
        } else if (value.is_string()) {
            try {
                n = std::stod(value.get<std::string>());
            } catch (...) {
                ok = false;
            }
        } else {
            ok = false;
        }
        if (ok) {
            double r = (f.type == FilterType::Sum) ? (n + f.number) : (n * f.number);
            // Preserve integer-ness when the result is whole (nicer than 3.0).
            if (r == static_cast<long long>(r))
                value = static_cast<long long>(r);
            else
                value = r;
        }
        // On non-numeric source the filter is a no-op (value stays as-is),
        // matching h2agent's "should not happen (protected by schema)" lenient
        // behavior.
    } else if (f.type == FilterType::RegexReplace) {
        // std::regex_replace returns the source unchanged when there is no
        // match (intended fallback). Result is always a string. Regex was
        // precompiled at parse time (never here).
        value = std::regex_replace(asString(value), f.regex, f.arg);
    } else if (f.type == FilterType::RegexCapture) {
        // Full-match semantics (std::regex_match). No full match -> skip the
        // whole rule (parity with h2agent returning false). On match, the
        // value becomes the whole match (group 0); capture groups are exposed
        // as a single JSON object with numeric string keys
        // {"0": whole, "1": g1, ...} so a store target can navigate them by
        // json-pointer (e.g. local.caps./1).
        std::smatch matches;
        std::string subject = asString(value);
        if (std::regex_match(subject, matches, f.regex) && !matches.empty()) {
            // Main resolved value is the whole match (group 0), used by string
            // targets. For store targets we instead write a groups object with
            // numeric string keys {"0": whole, "1": g1, ...}, navigable by
            // json-pointer (e.g. local.caps./1).
            value = matches.str(0);
            if (last && rule.targetType == TargetType::StoreGlobal) {
                // v1 vault layout: single object {"0": whole, "1": g1, ...}
                nlohmann::json groups = nlohmann::json::object();
                for (std::size_t g = 0; g < matches.size(); ++g) groups[std::to_string(g)] = matches.str(g);
                value = groups;
            } else if (last && rule.targetType == TargetType::StoreLocal) {
                // v1 var layout: key = whole match, "<key>.<n>" = group n (n>=1)
                for (std::size_t g = 0; g < matches.size(); ++g) localGroups.push_back(matches.str(g));
                firstGroup = 1;
            }
        } else {
            note = "regex capture did not fully match; rule skipped";
            return false;
        }
    } else if (f.type == FilterType::ConditionVar) {
        // Transfer source->target only if the named store var is truthy; when
        // the condition is false the whole rule is skipped (no target assignment).
        if (!conditionHolds(f.arg, ports)) {
            note = "ConditionVar is false; rule skipped";
            return false;
        }
        // Pass: value stays as the resolved source (string parity with h2agent).
    } else if (f.type == FilterType::EqualTo || f.type == FilterType::DifferentFrom) {
        std::string cmp = substituteVariables(f.arg, ports);  // admits @{var}
        bool equal = (asString(value) == cmp);
        bool pass = (f.type == FilterType::EqualTo) ? equal : !equal;
        if (!pass) {
            note = (f.type == FilterType::EqualTo) ? "EqualTo is false; rule skipped" : "DifferentFrom is false; rule skipped";
            return false;
        }
        // Pass: value stays as the resolved source string (h2agent parity).
    } else if (f.type == FilterType::JsonConstraint) {
        // Validate the json source contains the expected subset. On success
        // value becomes "1"; otherwise it becomes the fail-report string
        // (parity with h2agent: the rule still applies, carrying the report).
        std::string failReport;
        if (jsonConstraint(value, f.object, failReport))
            value = std::string("1");
        else
            value = failReport;
    } else if (f.type == FilterType::SchemaId) {
        // Validate the json source against a registered schema id via the
        // SchemaValidator port. '1' on success else ''. If the port is null
        // the rule is skipped (validation ignored, parity with h2agent's
        // "missing schema -> validation ignored").
        // v1 parity: "1" when valid, the fail report when invalid; an unknown
        // schema id (or no validator) leaves the source value unchanged.
        if (ports.schema) {
            std::string failReport;
            std::optional<bool> ok = ports.schema->validate(f.arg, value, failReport);
            if (ok) value = *ok ? std::string("1") : failReport;
        }
    } else if (f.type == FilterType::Split) {
        // Take the last size*count chars (left-pad with filler when shorter),
        // split into 'count' groups of 'size', joined by 'sep'; when numeric,
        // strip leading zeros per group. Parity with h2agent Split.
        long long size = f.i1;
        long long count = f.i2;
        const std::string &sep = f.arg;
        const std::string &filler = f.filler;
        bool numeric = f.flag;
        std::size_t totalLen = static_cast<std::size_t>(size) * static_cast<std::size_t>(count);
        std::string padded = asString(value);
        if (padded.size() > totalLen) {
            padded = padded.substr(padded.size() - totalLen);
        } else if (padded.size() < totalLen && !filler.empty()) {
            std::string padding;
            while (padding.size() + padded.size() < totalLen) padding += filler;
            if (padding.size() + padded.size() > totalLen)
                padding = padding.substr(padding.size() + padded.size() - totalLen);
            padded = padding + padded;
        }
        std::string out;
        for (long long i = 0; i < count; ++i) {
            if (i > 0) out += sep;
            std::string group = padded.substr(static_cast<std::size_t>(i * size), static_cast<std::size_t>(size));
            if (numeric) {
                try {
                    out += std::to_string(std::stoull(group));
                } catch (...) {
                    out += group;
                }
            } else {
                out += group;
            }
        }
        value = out;
    } else if (f.type == FilterType::BaseConvert) {
        // Reinterpret the source as a number in base 'in' and render in base
        // 'out' (2-36). 'capital' selects upper-case digits. On any error the
        // source is kept unchanged (parity with h2agent).
        int baseIn = static_cast<int>(f.i1);
        int baseOut = static_cast<int>(f.i2);
        bool capital = f.flag;
        std::string src = asString(value);
        std::string out;
        try {
            unsigned long long val = std::stoull(src, nullptr, baseIn);
            if (baseOut == 10) {
                out = std::to_string(val);
            } else {
                if (val == 0) {
                    out = "0";
                } else {
                    while (val > 0) {
                        int d = static_cast<int>(val % static_cast<unsigned long long>(baseOut));
                        out += (d < 10) ? static_cast<char>('0' + d)
                               : static_cast<char>((capital ? 'A' : 'a') + d - 10);
                        val /= static_cast<unsigned long long>(baseOut);
                    }
                    std::reverse(out.begin(), out.end());
                }
            }
        } catch (...) {
            out = src;  // fallback to source on error
        }
        value = out;
    } else if (f.type == FilterType::Strptime) {
        // Parse the source date string into a UNIX epoch via strptime+timegm,
        // scaled by the unit. Parse failure skips the rule (h2agent parity).
        std::string src = asString(value);
        std::tm tmv{};
        if (strptime(src.c_str(), f.arg.c_str(), &tmv) == nullptr) {
            note = "Strptime filter failed to parse source; rule skipped";
            return false;
        }
        std::int64_t epoch = static_cast<std::int64_t>(timegm(&tmv));
        switch (f.unit) {
        case 1:
            epoch *= 1000LL;
            break;
        case 2:
            epoch *= 1000000LL;
            break;
        case 3:
            epoch *= 1000000000LL;
            break;
        default:
            break;
        }
        value = epoch;
    } else if (f.type == FilterType::Strftime) {
        // Format a numeric epoch source (interpreted in the given unit) into a
        // string via gmtime_r+strftime. Non-numeric source or format failure
        // skips the rule (h2agent parity).
        std::int64_t epoch = 0;
        if (!toInteger(value, epoch)) {
            note = "Strftime filter requires a numeric source (epoch); rule skipped";
            return false;
        }
        switch (f.unit) {
        case 1:
            epoch /= 1000LL;
            break;
        case 2:
            epoch /= 1000000LL;
            break;
        case 3:
            epoch /= 1000000000LL;
            break;
        default:
            break;
        }
        std::time_t t = static_cast<std::time_t>(epoch);
        std::tm tmv{};
        gmtime_r(&t, &tmv);
        char buf[256] = {0};
        std::size_t n = std::strftime(buf, sizeof(buf), f.arg.c_str(), &tmv);
        if (n == 0) {
            note = "Strftime filter failed to format epoch; rule skipped";
            return false;
        }
        value = std::string(buf, n);
    } else if (f.type == FilterType::RegexKey) {
        // The source must be a json object; match its keys and resolve to the
        // value of the FIRST matching key. No object or no match skips the rule
        // (parity with h2agent). Regex precompiled at parse (never here).
        if (!value.is_object()) {
            note = "RegexKey filter requires a JSON object source; rule skipped";
            return false;
        }
        bool matched = false;
        std::smatch km;
        for (auto it = value.begin(); it != value.end(); ++it) {
            std::string key = it.key();
            if (std::regex_match(key, km, f.regex)) {
                value = it.value();  // resolve to the first matching key's value
                if (last && (rule.targetType == TargetType::StoreLocal || rule.targetType == TargetType::StoreGlobal)) {
                    for (std::size_t g = 0; g < km.size(); ++g) localGroups.push_back(km.str(g));
                    firstGroup = 0;  // "<key>.0" = matched key, ".1".. = groups
                }
                matched = true;
                break;
            }
        }
        if (!matched) {
            note = "RegexKey filter: no key matched; rule skipped";
            return false;
        }
    } else if (f.type == FilterType::Size) {
        // Number of elements/keys (object/array) or characters (string) as a
        // numeric string (parity with h2agent Size).
        if (value.is_object() || value.is_array())
            value = std::to_string(value.size());
        else
            value = std::to_string(asString(value).size());
    } else if (f.type == FilterType::Cast) {
        if (!castValue(f.arg, value)) {
            note = "value could not be cast to '" + f.arg + "'; rule skipped";
            return false;
        }
    }
    return true;
}


}  // namespace

// Parse one rule (and, recursively, its onFilterFail fallbacks). `where` labels
// errors with the rule position (e.g. "rule 3" or "rule 3.onFilterFail[0]").
static bool parseRule(const nlohmann::json &item, const std::string &where, Rule &rule, std::string &error) {
    if (!item.is_object() || !item.contains("source") || !item.contains("target") || !item["source"].is_string() ||
            !item["target"].is_string()) {
        error = where + " must be an object with string 'source' and 'target'";
        return false;
    }
    rule.rawSource = item["source"].get<std::string>();
    rule.rawTarget = item["target"].get<std::string>();
    rule.sourceType = classifySource(rule.rawSource, rule);
    rule.targetType = classifyTarget(rule.rawTarget, rule);
    if ((rule.sourceType == SourceType::Input || rule.sourceType == SourceType::Output) &&
            rule.sourcePtr.find("@{") == std::string::npos)
        rule.sourceCompiledStatic = CompiledPointer::compile(rule.sourcePtr, rule.sourceCompiled);
    if (rule.targetPath.find("@{") == std::string::npos) {
        try {
            rule.targetJsonPtr = nlohmann::json::json_pointer(rule.targetPath);
            rule.targetJsonPtrStatic = true;
        } catch (const std::exception &) {
            // malformed: keep the per-event build, which skips the rule (v1 parity)
        }
    }
    // "filter": one filter, or a chain of filters run in list order.
    auto fit = item.find("filter");
    if (fit != item.end()) {
        const bool chain = fit->is_array();
        const std::size_t count = chain ? fit->size() : 1;
        for (std::size_t k = 0; k < count; ++k) {
            Filter filter;
            filter.type = classifyFilter(chain ? (*fit)[k] : *fit, filter, rule);
            if (filter.type == FilterType::None) continue;  // unknown filter -> none (v1 parity)
            if (!rule.rawFilter.empty()) rule.rawFilter += ",";
            rule.rawFilter += filter.raw;
            rule.filters.push_back(std::move(filter));
        }
    }
    auto off = item.find("onFilterFail");
    if (off != item.end()) {
        if (!off->is_array()) {
            error = where + ".onFilterFail must be an array of rule objects";
            return false;
        }
        rule.onFilterFail.reserve(off->size());
        for (std::size_t k = 0; k < off->size(); ++k) {
            Rule fb;
            if (!parseRule((*off)[k], where + ".onFilterFail[" + std::to_string(k) + "]", fb, error)) return false;
            rule.onFilterFail.push_back(std::move(fb));
        }
    }
    return true;
}

std::shared_ptr<Pipeline> Engine::parse(const nlohmann::json &j, std::string &error) const {
    if (!j.is_array()) {
        error = "pipeline must be a JSON array of rule objects";
        return nullptr;
    }

    auto pipeline = std::make_shared<Pipeline>();
    pipeline->rules.reserve(j.size());

    for (std::size_t i = 0; i < j.size(); ++i) {
        Rule rule;
        if (!parseRule(j[i], "rule " + std::to_string(i), rule, error)) return nullptr;
        pipeline->rules.push_back(std::move(rule));
    }

    return pipeline;
}

// meta source over the typed host metadata: "" = whole metadata as an object
// (diagnostics), "/uri", "/method", "/statusCode", "/delayMs", "/timeoutMs",
// "/headers/<name>" (last written value). Absent when the host lacks the field.
static std::optional<nlohmann::json> metaGet(const Meta &meta, const std::string &ptr) {
    if (ptr.empty()) {
        nlohmann::json all = nlohmann::json::object();
        for (const char *k : {
                    "/uri", "/method", "/statusCode", "/delayMs", "/timeoutMs"
                })
            if (auto v = metaGet(meta, k)) all[k + 1] = std::move(*v);
        if (meta.headers && !meta.headers->empty()) {
            nlohmann::json h = nlohmann::json::object();
            for (const auto &kv : *meta.headers) h[kv.first] = kv.second;
            all["headers"] = std::move(h);
        }
        return all;
    }
    if (ptr == "/uri") return meta.uri ? std::optional<nlohmann::json>(*meta.uri) : std::nullopt;
    if (ptr == "/method") return meta.method ? std::optional<nlohmann::json>(*meta.method) : std::nullopt;
    if (ptr == "/statusCode") return meta.statusCode ? std::optional<nlohmann::json>(*meta.statusCode) : std::nullopt;
    if (ptr == "/delayMs") return meta.delayMs ? std::optional<nlohmann::json>(*meta.delayMs) : std::nullopt;
    if (ptr == "/timeoutMs") return meta.timeoutMs ? std::optional<nlohmann::json>(*meta.timeoutMs) : std::nullopt;
    static const std::string headersPrefix = "/headers/";
    if (meta.headers && ptr.compare(0, headersPrefix.size(), headersPrefix) == 0) {
        const std::string name = ptr.substr(headersPrefix.size());
        for (auto it = meta.headers->rbegin(); it != meta.headers->rend(); ++it)
            if (it->first == name) return nlohmann::json(it->second);
    }
    return std::nullopt;
}

// Evaluate a rule list. Returns false when a break interrupted processing (it
// propagates through onFilterFail fallbacks, as h2agent does); true otherwise.
static bool evalRules(const std::vector<Rule> &rules, EvaluationContext &ctx, const Ports &ports, bool trace,
                      TransformResult &result, const std::string &prefix = "") {
    // Host-owned output objects; when the host provides none, writes land in
    // these scratch sinks and are dropped.
    nlohmann::json outputSink;
    std::string outBodySink;
    nlohmann::json &output = ctx.output ? *ctx.output : outputSink;
    std::string &outBody = ctx.outBody ? *ctx.outBody : outBodySink;
    std::string outStateSink, outStateMethodSink, outStateUriSink;
    std::string &outState = ctx.outState ? *ctx.outState : outStateSink;
    std::string &outStateMethod = ctx.outStateMethod ? *ctx.outStateMethod : outStateMethodSink;
    std::string &outStateUri = ctx.outStateUri ? *ctx.outStateUri : outStateUriSink;

    for (std::size_t i = 0; i < rules.size(); ++i) {
        const Rule &rule = rules[i];

        // Trace data is only built when tracing was requested.
        TraceEntry entry;
        if (trace) {
            entry.index = i;
            entry.path = prefix + std::to_string(i);
            entry.source = rule.rawSource;
            entry.target = rule.rawTarget;
            entry.filter = rule.rawFilter;
        }

        // json paths admit @{var} substitution (h2agent parity), resolved per event.
        std::string sPtrBuf, tPathBuf, tPtrBuf;
        const std::string &sPtr = resolveTemplate(rule.sourcePtr, sPtrBuf, ports);
        const std::string &tPath = resolveTemplate(rule.targetPath, tPathBuf, ports);
        const std::string &tPtr = resolveTemplate(rule.targetPtr, tPtrBuf, ports);
        // Target json pointer: the one compiled at parse, or built for this event.
        nlohmann::json::json_pointer dynamicTargetPtr;
        auto targetPointer = [&]() -> const nlohmann::json::json_pointer & {
            if (rule.targetJsonPtrStatic) return rule.targetJsonPtr;
            dynamicTargetPtr = nlohmann::json::json_pointer(tPath);  // may throw: rule skipped
            return dynamicTargetPtr;
        };

        if (rule.sourceType == SourceType::Unsupported || rule.targetType == TargetType::Unsupported) {
            entry.applied = false;
            if (trace) entry.note = "unsupported source or target (not yet ported)";
            if (trace) result.executionTrace.push_back(std::move(entry));
            continue;
        }

        // --- source resolution (typed via json) ---
        nlohmann::json value;
        bool sourceResolved = true;
        // `eraser` marks the special "reset/remove the target" source. When true,
        // the resolved value is irrelevant and the target application branches to
        // an erase/clear/empty behavior instead of assigning (h2agent parity).
        bool eraser = false;
        switch (rule.sourceType) {
        case SourceType::Value:
        {
            std::string buf;
            value = resolveTemplate(rule.sourceArg, buf, ports);
        }
        break;
        case SourceType::Sequence:
            value = static_cast<std::uint64_t>(ctx.sequence);
            break;
        case SourceType::Timestamp: {
            std::int64_t epoch = ports.clock ? ports.clock->nowEpoch(rule.sourceArg) : 0;
            value = epoch;
            break;
        }
        case SourceType::Strftime: {
            // Current LOCAL time formatted by strftime (parity with h2agent,
            // which calls time()/localtime()). A formatting failure yields an
            // empty string (strftime returns 0). The result admits @{var}
            // substitution (h2agent parity).
            std::time_t now = std::time(nullptr);
            std::tm tmv{};
            localtime_r(&now, &tmv);
            char buf[256] = {0};
            std::size_t n = std::strftime(buf, sizeof(buf), rule.sourceArg.c_str(), &tmv);
            value = substituteVariables(std::string(buf, n), ports);
            break;
        }
        case SourceType::Random: {
            long long range = rule.sourceI2 - rule.sourceI1 + 1;
            value = rule.sourceI1 + (std::rand() % range);
            break;
        }
        case SourceType::RandomSet: {
            // Pick one token uniformly at random; the chosen token admits
            // @{var} substitution. sourceTokens always has >=1 element (parse
            // keeps empty parts), so the modulo is safe. Result is a string.
            const std::string &chosen = rule.sourceTokens[std::rand() % rule.sourceTokens.size()];
            value = substituteVariables(chosen, ports);
            break;
        }
        case SourceType::Eraser:
            // No value is produced; flag the target application to erase/reset.
            eraser = true;
            break;
        case SourceType::Math: {
            // Evaluate the arithmetic expression via Arash Partow exprtk, at
            // parity with h2agent. @{var} are substituted first (math admits
            // runtime variables), then compiled and evaluated. A whole result
            // becomes an integer, otherwise a float. A compile failure skips
            // the rule. The expression is precompiled at parse (MathProgram);
            // the per-event compilation only remains as the exact v1 fallback.
            double result = 0;
            bool done = false;
            if (rule.math && rule.math->constant) {
                result = rule.math->constantValue;  // folded at load
                done = true;
            } else if (rule.math) {
                // Bound variables: same lookup as @{} substitution (local, then
                // global). Only plain unsigned decimals bind; anything else
                // keeps the textual path below, so semantics never change.
                auto in = rule.math->acquire();
                bool bindable = (in != nullptr);
                for (std::size_t k = 0; bindable && k < rule.math->names.size(); ++k) {
                    std::optional<nlohmann::json> v = lookupVariable(rule.math->names[k], ports);
                    bindable = v && plainUnsignedDecimal(v->is_string() ? v->get<std::string>() : v->dump(),
                                                         in->vars[k]);
                }
                if (bindable) {
                    result = in->expression.value();
                    done = true;
                }
                if (in) rule.math->release(std::move(in));
            }
            if (!done) {
                // v1 path: textual @{} substitution, compiled for this event.
                std::string expr = substituteVariables(rule.sourceArg, ports);
                exprtk::expression<double> expression;
                exprtk::parser<double> parser;
                if (!parser.compile(expr, expression)) {
                    sourceResolved = false;
                    if (trace) entry.note = "math: expression did not compile; rule skipped";
                    break;
                }
                result = expression.value();
            }
            if (result == static_cast<long long>(result))
                value = static_cast<long long>(result);
            else
                value = result;
            break;
        }
        case SourceType::StoreLocal:
        case SourceType::StoreGlobal: {
            auto scope = (rule.sourceType == SourceType::StoreLocal) ? Store::Scope::Local : Store::Scope::Global;
            std::string keyBuf;
            const std::string &key = resolveTemplate(rule.sourceKey, keyBuf, ports);
            std::optional<nlohmann::json> v;
            if (ports.store) v = ports.store->get(scope, key, sPtr);
            // v1 TypeConverter::setObject parity: a path that extracts an empty
            // value (null / {} / []) counts as "not found".
            if (v && !sPtr.empty() && v->empty()) v.reset();
            if (v) {
                value = *v;
            } else {
                // Missing source: skip the whole rule (parity with h2agent, which
                // returns false and does not touch the target).
                sourceResolved = false;
            }
            break;
        }
        case SourceType::InState:
            value = ctx.inState ? *ctx.inState : std::string();
            break;
        case SourceType::Input:
        case SourceType::Output: {
            // Host documents: the host decides what is absent (the engine adds
            // no grammar-specific rule). Absent -> skip the whole rule.
            const Document *doc = (rule.sourceType == SourceType::Input) ? ctx.input : ctx.outputView;
            std::optional<nlohmann::json> v;
            if (doc) v = rule.sourceCompiledStatic ? doc->get(rule.sourceCompiled) : doc->get(sPtr);
            if (v)
                value = std::move(*v);
            else
                sourceResolved = false;
            break;
        }
        case SourceType::Meta: {
            std::optional<nlohmann::json> v = metaGet(ctx.meta, sPtr);
            if (v)
                value = std::move(*v);
            else
                sourceResolved = false;  // metadata not supported by the host: skip the rule
            break;
        }
        case SourceType::ServerEvent:
        case SourceType::ClientEvent: {
            // Resolve via the EventSource port. The engine passes the raw
            // address (after @{} substitution); the host adapter interprets the
            // query-param form (method/uri/eventNumber/path/seq/prefix/endpoint).
            std::optional<nlohmann::json> v;
            if (ports.events)
                v = ports.events->resolve(rule.sourceEventDomain, eventAddressJson(rule.sourceEventParams, ports));
            if (v)
                value = *v;
            else
                sourceResolved = false;  // not found / port null -> skip
            break;
        }
        case SourceType::Sendseq:
            value = static_cast<std::uint64_t>(ctx.sequence);
            break;
        case SourceType::TxtFile:
        case SourceType::BinFile: {
            if (!ports.files) {
                sourceResolved = false;
                if (trace) entry.note = "file source: no FileAccess port; rule skipped";
                break;
            }
            std::string path = substituteVariables(rule.sourceArg, ports);
            auto content = ports.files->read(path, rule.sourceType == SourceType::BinFile);
            value = content ? *content : std::string{};  // missing file => "" (v1 parity)
            break;
        }
        case SourceType::Command: {
            if (!ports.shell) {
                sourceResolved = false;
                if (trace) entry.note = "command source: no Shell port; rule skipped";
                break;
            }
            auto res = ports.shell->run(substituteVariables(rule.sourceArg, ports));
            value = res.output;
            // v1 parity: exit code published as local variable "rc".
            if (ports.store) ports.store->set(Store::Scope::Local, "rc", std::to_string(res.returnCode));
            break;
        }
        default:
            break;
        }

        if (!sourceResolved) {
            entry.applied = false;
            if (trace && entry.note.empty()) entry.note = "source not found; rule skipped";
            if (trace) result.executionTrace.push_back(std::move(entry));
            continue;
        }

        // Eraser is a value-less source: its only filters are ConditionVar (the
        // target is erased only when every condition holds) and Cast (nothing to
        // convert, ignored). Any other filter needs a real source value, so the
        // rule is skipped (v1 parity; the v2 schema rejects it).
        if (eraser && !rule.filters.empty()) {
            const char *skipNote = nullptr;
            for (const Filter &f : rule.filters) {
                if (f.type == FilterType::Cast) continue;
                if (f.type != FilterType::ConditionVar) {
                    skipNote = "eraser with a filter other than ConditionVar is not allowed; rule skipped";
                    break;
                }
                if (!conditionHolds(f.arg, ports)) {
                    skipNote = "ConditionVar is false; rule skipped";
                    break;
                }
            }
            if (skipNote) {
                entry.applied = false;
                if (trace) entry.note = skipNote;
                if (trace) result.executionTrace.push_back(std::move(entry));
                if (!evalRules(rule.onFilterFail, ctx, ports, trace, result, prefix + std::to_string(i) + "/onFilterFail/")) return false;  // onFilterFail
                continue;
            }
        }

        // Eraser short-circuits to the target application: the erase/clear/empty
        // behavior runs (the guard above already checked its conditions).
        if (eraser) {
            switch (rule.targetType) {
            case TargetType::StoreLocal:
            case TargetType::StoreGlobal: {
                // Erase the store key entirely, or only the node at its pointer.
                auto scope =
                    (rule.targetType == TargetType::StoreLocal) ? Store::Scope::Local : Store::Scope::Global;
                std::string keyBuf;
                const std::string &key = resolveTemplate(rule.targetKey, keyBuf, ports);
                if (!ports.store) break;
                if (tPtr.empty()) {
                    ports.store->erase(scope, key);
                } else if (auto current = ports.store->get(scope, key)) {
                    eraseNode(*current, tPtr);
                    ports.store->set(scope, key, *current);
                }
                break;
            }
            case TargetType::OutputJson:  // grammar v2 body target without cast
            case TargetType::OutputJsonObject:
                // Remove the addressed json node; at root, clear the whole body
                // (parity with h2agent's eraser on output.json.object).
                eraseNode(output, tPath);
                break;
            case TargetType::TxtFile:
            case TargetType::BinFile:
                // v1 parity: eraser into a file target empties the file.
                if (ports.files) ports.files->truncate(substituteVariables(rule.targetFilePath, ports));
                break;
            case TargetType::EventPurge: {
                // eraser + serverEvent/clientEvent target -> purge via the port.
                if (ports.events) {
                    ports.events->purge(rule.targetEventDomain, eventAddressJson(rule.targetEventParams, ports));
                }
                break;
            }
            default:
                // Any other string-like target: reset to empty string (parity
                // with h2agent, where eraser yields an empty source string).
                switch (rule.targetType) {
                case TargetType::OutputString:
                case TargetType::OutputHexString:
                    outBody = "";
                    break;
                case TargetType::OutputJsonString:
                    try {
                        output[targetPointer()] = "";
                    } catch (const std::exception &) {
                        // malformed pointer: neutral no-op
                    }
                    break;
                case TargetType::OutState:
                    outState = "";
                    break;
                default:
                    break;
                }
                break;
            }
            entry.applied = true;
            if (trace) result.executionTrace.push_back(std::move(entry));
            continue;
        }

        // Capture groups to publish as flat LOCAL store keys "<key>.<n>" (h2agent
        // v1 parity for var targets of RegexCapture, and for RegexKey on any store
        // target). firstGroup is 1 for RegexCapture (key itself holds group 0) and
        // 0 for RegexKey (".0" is the matched key).
        std::vector<std::string> localGroups;
        std::size_t firstGroup = 0;

        // --- filters (optional chain, in list order) ---
        std::string filterNote;
        bool filtersPassed = true;
        for (std::size_t k = 0; filtersPassed && k < rule.filters.size(); ++k)
            filtersPassed = applyFilter(rule.filters[k], k + 1 == rule.filters.size(), rule, value, ports, localGroups,
                                        firstGroup, filterNote);
        if (!filtersPassed) {
            entry.applied = false;
            if (trace) entry.note = std::move(filterNote);
            if (trace) result.executionTrace.push_back(std::move(entry));
            if (!evalRules(rule.onFilterFail, ctx, ports, trace, result, prefix + std::to_string(i) + "/onFilterFail/")) return false;  // onFilterFail
            continue;
        }

        // --- target application ---
        // `coerced` tracks typed targets that require a successful coercion of
        // the resolved value. On failure the rule is SKIPPED (parity with
        // h2agent returning false and not touching the target).
        bool coerced = true;
        try {
            switch (rule.targetType) {
            case TargetType::OutputString:
                outBody = asString(value);
                break;
            case TargetType::OutputHexString: {
                std::string raw;
                if (fromHexString(asString(value), raw))
                    outBody = raw;
                else
                    coerced = false;
                break;
            }
            case TargetType::OutputJsonString: {
                const nlohmann::json::json_pointer &ptr = targetPointer();  // "" => whole document
                output[ptr] = asString(value);
                break;
            }
            case TargetType::OutputJson:
                output[targetPointer()] = value;  // native type of the source ("" => whole document)
                break;
            case TargetType::OutputJsonInteger: {
                std::int64_t n;
                if (toInteger(value, n)) {
                    output[targetPointer()] = n;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::OutputJsonUnsigned: {
                std::uint64_t n;
                if (toUnsigned(value, n)) {
                    output[targetPointer()] = n;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::OutputJsonFloat: {
                double n;
                if (toFloat(value, n)) {
                    output[targetPointer()] = n;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::OutputJsonBoolean: {
                bool b;
                if (toBoolean(value, b)) {
                    output[targetPointer()] = b;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::OutputJsonObject: {
                // Prefer a real object/array. Otherwise best-effort convert to the
                // first coercible scalar with h2agent's priority: string, integer,
                // unsigned, float, boolean. A bare "object" target at root merges
                // (merge_patch) to preserve existing fields, exactly as h2agent does.
                const nlohmann::json::json_pointer &ptr = targetPointer();
                if (value.is_object() || value.is_array()) {
                    if (tPath.empty())
                        output.merge_patch(value);
                    else
                        output[ptr] = value;
                } else if (value.is_primitive() && !value.is_null()) {
                    // v1 parity: a scalar keeps its native type (string, integer,
                    // unsigned, float, boolean); at root it becomes the whole body.
                    output[ptr] = value;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::OutputJsonJsonString: {
                // Parse the source string as JSON and set the resulting node. Invalid
                // JSON -> skip the rule (parity with h2agent parseJsonContent failing).
                nlohmann::json parsed;
                bool ok = true;
                try {
                    parsed = nlohmann::json::parse(asString(value));
                } catch (...) {
                    ok = false;
                }
                if (ok) {
                    if (tPath.empty())
                        output.merge_patch(parsed);  // merge at root (h2agent parity)
                    else
                        output[targetPointer()] = parsed;
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::StoreLocal:
            case TargetType::StoreGlobal: {
                auto scope = (rule.targetType == TargetType::StoreLocal) ? Store::Scope::Local : Store::Scope::Global;
                std::string keyBuf;
                const std::string &key = resolveTemplate(rule.targetKey, keyBuf, ports);
                // v1 parity for validation filters on var targets: a non-"1"
                // result is a fail report -> "<key>.fail" = report, key = "".
                if (rule.targetType == TargetType::StoreLocal &&
                        !rule.filters.empty() &&
                        (rule.filters.back().type == FilterType::JsonConstraint ||
                         rule.filters.back().type == FilterType::SchemaId) &&
                        asString(value) != "1") {
                    if (ports.store) ports.store->set(Store::Scope::Local, key + ".fail", asString(value));
                    value = std::string();
                }
                // Native json in both scopes: the value keeps the type the source (or a cast) yields.
                if (ports.store) {
                    if (tPtr.empty() && value.is_string())
                        ports.store->setString(scope, key, std::move(value.get_ref<std::string &>()));  // no json round trip
                    else
                        ports.store->set(scope, key, value, tPtr);
                    // Flat local group keys "<key>.<n>" (v1 parity; always LOCAL scope,
                    // also for vault targets of RegexKey, exactly as h2agent does).
                    for (std::size_t g = firstGroup; g < localGroups.size(); ++g)
                        ports.store->set(Store::Scope::Local, key + "." + std::to_string(g), localGroups[g]);
                }
                break;
            }
            case TargetType::StoreLocalJsonObject:
            case TargetType::StoreGlobalJsonObject: {
                // Require an object (or array) source; otherwise skip (parity with
                // h2agent TGVarJson_Object strict typing).
                if (!value.is_object() && !value.is_array()) {
                    coerced = false;
                    break;
                }
                auto scope = (rule.targetType == TargetType::StoreLocalJsonObject) ? Store::Scope::Local
                             : Store::Scope::Global;
                std::string keyBuf;
                const std::string &key = resolveTemplate(rule.targetKey, keyBuf, ports);
                if (ports.store) ports.store->set(scope, key, value, tPtr);
                break;
            }
            case TargetType::StoreLocalJsonString:
            case TargetType::StoreGlobalJsonString: {
                // Parse a JSON string from the source; invalid -> skip.
                nlohmann::json parsed;
                try {
                    parsed = nlohmann::json::parse(asString(value));
                } catch (...) {
                    coerced = false;
                    break;
                }
                auto scope = (rule.targetType == TargetType::StoreLocalJsonString) ? Store::Scope::Local
                             : Store::Scope::Global;
                std::string keyBuf;
                const std::string &key = resolveTemplate(rule.targetKey, keyBuf, ports);
                if (ports.store) ports.store->set(scope, key, parsed, tPtr);
                break;
            }
            case TargetType::OutState:
                outState = asString(value);
                outStateMethod.clear();  // v1: regular outState empties method/uri
                outStateUri.clear();
                break;
            case TargetType::OutStateForeign:
                // Data only, no FSM logic: record the out-state plus the foreign
                // method/uri so the host can create a virtual event.
                outState = asString(value);
                outStateMethod = rule.targetMethod;
                outStateUri = substituteVariables(rule.targetUri, ports);
                break;
            case TargetType::MetaHeader: {
                // Appended in order: the host emplaces every write (v1 multimap semantics).
                std::string name = substituteVariables(rule.targetName, ports);
                if (ctx.meta.headers)
                    ctx.meta.headers->emplace_back(std::move(name), asString(value));
                else
                    coerced = false;
                break;
            }
            case TargetType::MetaStatusCode: {
                std::uint64_t n;
                if (ctx.meta.statusCode && toUnsigned(value, n))
                    *ctx.meta.statusCode = static_cast<unsigned int>(n);
                else
                    coerced = false;
                break;
            }
            case TargetType::MetaUri:
                if (ctx.meta.uri)
                    *ctx.meta.uri = asString(value);
                else
                    coerced = false;
                break;
            case TargetType::MetaMethod:
                if (ctx.meta.method)
                    *ctx.meta.method = asString(value);
                else
                    coerced = false;
                break;
            case TargetType::MetaTimeoutMs: {
                std::uint64_t n;
                if (ctx.meta.timeoutMs && toUnsigned(value, n))
                    *ctx.meta.timeoutMs = static_cast<unsigned int>(n);
                else
                    coerced = false;
                break;
            }
            case TargetType::MetaDelayMs: {
                std::uint64_t n;
                if (ctx.meta.delayMs && toUnsigned(value, n))
                    *ctx.meta.delayMs = static_cast<unsigned int>(n);
                else
                    coerced = false;
                break;
            }
            case TargetType::TxtFile:
            case TargetType::BinFile: {
                // Side effect via the FileAccess port; skip if the port is absent.
                if (ports.files) {
                    std::string path = substituteVariables(rule.targetFilePath, ports);
                    ports.files->write(path, asString(value), rule.targetType == TargetType::BinFile,
                                       rule.targetFilePath.find("@{") != std::string::npos);
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::UdpSocket: {
                if (ports.sockets) {
                    std::string path = substituteVariables(rule.targetSocketPath, ports);
                    ports.sockets->write(path, asString(value), rule.targetSocketDelayMs);
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::EventPurge: {
                // Used with eraser source: purge events at the given address.
                if (ports.events) {
                    ports.events->purge(rule.targetEventDomain, eventAddressJson(rule.targetEventParams, ports));
                } else {
                    coerced = false;
                }
                break;
            }
            case TargetType::ClientProvisionTrigger: {
                // Conditional gate: fires on a true value (see truthy()).
                if (truthy(value) && ports.trigger) {
                    std::string id = substituteVariables(rule.targetProvisionId, ports);
                    std::string inState = substituteVariables(rule.targetProvisionInState, ports);
                    ports.trigger->trigger(id, inState);
                }
                // false value or null port => silently skip firing (not an error)
                break;
            }
            case TargetType::Break: {
                // Stop the pipeline on a true value (see truthy()); a false or
                // undefined source is a no-op.
                if (truthy(value)) {
                    entry.applied = true;
                    if (trace) entry.note = "break: pipeline interrupted";
                    result.broke = true;
                    if (trace) result.executionTrace.push_back(std::move(entry));
                    return false;  // break propagates to callers (incl. onFilterFail)
                }
                break;
            }
            default:
                break;
            }
        } catch (const std::exception &) {
            // e.g. malformed json-pointer: skip the rule instead of throwing into the host
            coerced = false;
        }

        if (!coerced) {
            entry.applied = false;
            if (trace) entry.note = "value could not be coerced to the target type; rule skipped";
            if (trace) result.executionTrace.push_back(std::move(entry));
            continue;
        }

        entry.applied = true;
        if (trace) result.executionTrace.push_back(std::move(entry));
    }

    return true;
}

TransformResult Engine::evaluate(const std::shared_ptr<Pipeline> &pipeline, EvaluationContext &ctx, const Ports &ports,
                                 bool trace) const {
    TransformResult result;
    if (!pipeline) {
        result.ok = false;
        return result;
    }
    evalRules(pipeline->rules, ctx, ports, trace, result);
    return result;
}

}  // namespace engine
}  // namespace h2agent
