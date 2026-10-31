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

#include <EngineGrammarV2.hpp>

#include <vector>

namespace h2agent {
namespace model {

namespace {

using Mode = EngineGrammarV2::Mode;

/** Raised while lowering a rule with a v2 feature not supported yet by the engine. */
struct NotSupported {
    std::string reason;
};

std::string text(const nlohmann::json &o, const char *key) {
    auto it = o.find(key);
    return (it != o.end() && it->is_string()) ? it->get<std::string>() : "";
}

/** Numeric field as text (it may also be a text holding a variable). */
std::string number(const nlohmann::json &v) {
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_string()) return v.get<std::string>();
    return v.dump();
}

/** "<base>" or "<base>.<path>" (path is a json pointer, absent means the whole document). */
std::string withPath(const std::string &base, const nlohmann::json &o) {
    const std::string path = text(o, "path");
    return path.empty() ? base : base + "." + path;
}

/** Only what the engine event address parser would misread is encoded (it url-decodes values). */
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

std::string eventAddress(const nlohmann::json &o, const char *discriminator) {
    std::string query;
    for (const auto &item : o.items()) {
        if (item.key() == discriminator) continue;
        query += (query.empty() ? "" : "&") + item.key() + "=" + urlEncode(number(item.value()));
    }
    return query;
}

std::string na(const std::string &kind) {
    return std::string(EngineGrammarV2::NotApplicable) + kind;
}

std::string lowerSource(const nlohmann::json &from, Mode mode, bool usesJsonBodyTarget) {
    const std::string kind = text(from, "source");

    // Common to every mode:
    if (kind == "var") return withPath("local." + text(from, "name"), from);
    if (kind == "vault") return withPath("global." + text(from, "key"), from);
    if (kind == "eraser") return "eraser";
    if (kind == "math") return "math." + text(from, "expression");
    if (kind == "random") return "random." + number(from["min"]) + "." + number(from["max"]);
    if (kind == "randomset") {
        std::string values;
        bool first = true;
        for (const auto &v : from["values"]) {
            const std::string token = v.get<std::string>();
            if (token.find('|') != std::string::npos) throw NotSupported{"randomset value with '|'"};
            values += (first ? "" : "|") + token;
            first = false;
        }
        return "randomset." + values;
    }
    if (kind == "timestamp") return "timestamp." + text(from, "unit");
    if (kind == "strftime") return "strftime." + text(from, "fmt");
    if (kind == "literal") return "value." + text(from, "value");
    if (kind == "inState") return "inState";
    if (kind == "serverEvent" || kind == "clientEvent") return kind + "." + eventAddress(from, "source");
    if (kind == "txtFile" || kind == "binFile") return kind + "." + text(from, "file");
    if (kind == "command") return "command." + text(from, "command");
    if (kind == "requestHeader") return "input./headerMap/" + EngineGrammarV2::escapePointerToken(text(from, "name"));
    if (kind == "requestHeaders") return "input./headers";

    if (mode == Mode::Server) {
        if (kind == "recvseq") return "sequence";
        if (kind == "requestUri") return "input./uri";
        if (kind == "requestUriPath") return "input./uriPath";
        if (kind == "requestUriParam") return "input./uriParams/" + EngineGrammarV2::escapePointerToken(text(from, "name"));
        if (kind == "requestBody") return "input./body" + text(from, "path");  // json pointer appended: "input./body/a"
        if (kind == "responseBody") return withPath("output", from);
        return na(kind);  // sendseq, response headers/status code
    }

    if (kind == "sendseq") return "sequence";
    if (mode == Mode::ClientRequest) {
        if (kind == "requestUri") return "meta./uri";  // live value (requestUri targets update it)
        if (kind == "requestBody") return usesJsonBodyTarget ? withPath("output", from) : "input./requestBodyString";
        return na(kind);  // recvseq, uri path/params, response.*
    }

    // Mode::ClientResponse
    if (kind == "requestUri") return "input./uri";
    if (kind == "requestBody") return "input./requestBodyString";
    if (kind == "responseBody") return "input./response/body" + text(from, "path");
    if (kind == "responseHeader")
        return "input./response/headerMap/" + EngineGrammarV2::escapePointerToken(text(from, "name"));
    if (kind == "responseHeaders") return "input./response/headers";
    if (kind == "responseStatusCode") return "input./response/statusCode";
    return na(kind);  // recvseq, uri path/params
}

/** v2 cast type -> engine typed body target kind. */
std::string bodyKind(const std::string &castType) {
    return castType == "json" ? "jsonstring" : castType;
}

/**
 * A final cast which the engine runs as a typed target: json body targets (any type) and vault targets (object/json).
 * They keep the v1 typed write semantics (merge at root, erase flavours). Any other cast runs as a chain filter.
 */
bool typedTarget(const nlohmann::json &to, const std::string &castType, Mode mode) {
    const std::string kind = text(to, "target");
    if ((mode == Mode::Server && kind == "responseBody") || (mode == Mode::ClientRequest && kind == "requestBody")) return true;
    return kind == "vault" && (castType == "object" || castType == "json");
}

/** `castType` is only given for typed targets (see typedTarget). */
std::string lowerTarget(const nlohmann::json &to, const std::string &castType, Mode mode) {
    const std::string kind = text(to, "target");
    const bool bodyJson = (mode == Mode::Server && kind == "responseBody") || (mode == Mode::ClientRequest && kind == "requestBody");

    if (bodyJson) return withPath(castType.empty() ? "output.json" : "output.json." + bodyKind(castType), to);
    if (kind == "vault") {
        std::string base = "global." + text(to, "key");
        if (castType == "object") base += ".json.object";
        else if (castType == "json") base += ".json.jsonstring";
        return withPath(base, to);
    }

    // Common to every mode:
    if (kind == "var") return withPath("local." + text(to, "name"), to);
    if (kind == "txtFile" || kind == "binFile") return kind + "." + text(to, "file");
    if (kind == "udpSocket") return "udpSocket." + text(to, "socket") + (to.contains("delayMs") ? "|" + number(to["delayMs"]) : "");
    if (kind == "stop") return "break";

    if (mode == Mode::Server) {
        if (kind == "responseBodyText") return "output.string";
        if (kind == "responseBodyHex") return "output.hexstring";
        if (kind == "responseHeader") return "meta.header." + text(to, "name");
        if (kind == "responseStatusCode") return "meta.statusCode";
        if (kind == "responseDelayMs") return "meta.delayMs";
        if (kind == "outState") {
            if (!to.contains("method")) return "outState";
            return "outState." + text(to, "method") + (to.contains("uri") ? "." + text(to, "uri") : "");
        }
        if (kind == "serverEvent") return "serverEvent." + eventAddress(to, "target");
        if (kind == "clientProvision") return "clientProvision." + text(to, "id") + "." + text(to, "inState");
        return na(kind);  // request.*, clientEvent
    }

    // Client modes: server-only targets are no-ops (v1 parity).
    if (kind == "outState") return to.contains("method") ? na(kind) : "outState";
    if (kind == "clientEvent") return "clientEvent." + eventAddress(to, "target");
    if (mode == Mode::ClientRequest) {
        if (kind == "requestBodyText") return "output.string";
        if (kind == "requestBodyHex") return "output.hexstring";
        if (kind == "requestHeader") return "meta.header." + text(to, "name");
        if (kind == "requestUri") return "meta.uri";
        if (kind == "requestMethod") return "meta.method";
        if (kind == "requestDelayMs") return "meta.delayMs";
        if (kind == "requestTimeoutMs") return "meta.timeoutMs";
    }
    return na(kind);  // response.*, serverEvent, clientProvision (and request.* in the response phase)
}

/** v2 filter item -> engine filter (the v1 filter shape). */
nlohmann::json lowerFilter(const nlohmann::json &f) {
    const std::string op = text(f, "op");
    if (op == "size") return "Size";
    if (op == "conditionVar") return {{"ConditionVar", (f.value("not", false) ? "!" : "") + text(f, "name")}};
    if (op == "regexCapture") return {{"RegexCapture", f["rgx"]}};
    if (op == "regexKey") return {{"RegexKey", f["rgx"]}};
    if (op == "schemaId") return {{"SchemaId", f["id"]}};
    if (op == "equalTo") return {{f.value("not", false) ? "DifferentFrom" : "EqualTo", f["value"]}};
    static const std::vector<std::pair<std::string, std::string>> scalar = {
        {"append", "Append"}, {"prepend", "Prepend"}, {"sum", "Sum"}, {"multiply", "Multiply"},
        {"jsonConstraint", "JsonConstraint"},
    };
    for (const auto &s : scalar) {
        if (op == s.first) return {{s.second, f["value"]}};
    }
    static const std::vector<std::pair<std::string, std::string>> object = {
        {"regexReplace", "RegexReplace"}, {"split", "Split"}, {"baseConvert", "BaseConvert"},
        {"strptime", "Strptime"}, {"strftime", "Strftime"},
    };
    for (const auto &o : object) {
        if (op != o.first) continue;
        nlohmann::json arg = nlohmann::json::object();
        for (const auto &item : f.items()) {
            if (item.key() != "op") arg[item.key()] = item.value();
        }
        return {{o.second, arg}};
    }
    throw NotSupported{"filter '" + op + "'"};
}

nlohmann::json lowerRules(const nlohmann::json &rules, Mode mode, bool usesJsonBodyTarget);

nlohmann::json lowerRule(const nlohmann::json &rule, Mode mode, bool usesJsonBodyTarget) {
    std::vector<nlohmann::json> filters;
    auto fit = rule.find("filter");
    if (fit != rule.end()) {
        for (const auto &f : *fit) {
            if (text(f, "op") == "cast") filters.push_back({{"Cast", text(f, "type")}});
            else filters.push_back(lowerFilter(f));
        }
    }
    // A final cast on a typed target is the target typing itself (v1 order: filter first, target typing last):
    std::string castType;
    if (!filters.empty() && filters.back().contains("Cast")) {
        const std::string lastCast = filters.back()["Cast"].get<std::string>();
        if (typedTarget(rule["to"], lastCast, mode)) {
            castType = lastCast;
            filters.pop_back();
        }
    }

    nlohmann::json out = nlohmann::json::object();
    out["source"] = lowerSource(rule["from"], mode, usesJsonBodyTarget);
    out["target"] = lowerTarget(rule["to"], castType, mode);
    if (filters.size() == 1) out["filter"] = filters.front();  // the engine single filter form
    else if (!filters.empty()) out["filter"] = filters;  // chain, in list order
    auto off = rule.find("onFilterFail");
    if (off != rule.end()) out["onFilterFail"] = lowerRules(*off, mode, usesJsonBodyTarget);
    return out;
}

nlohmann::json lowerRules(const nlohmann::json &rules, Mode mode, bool usesJsonBodyTarget) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto &rule : rules) out.push_back(lowerRule(rule, mode, usesJsonBodyTarget));
    return out;
}

}  // namespace

std::string EngineGrammarV2::escapePointerToken(const std::string &token) {
    std::string out;
    out.reserve(token.size());
    for (char c : token) {
        if (c == '~') out += "~0";
        else if (c == '/') out += "~1";
        else out += c;
    }
    return out;
}

EngineGrammarV2::Summary EngineGrammarV2::summarize(const nlohmann::json &v2Rules, Mode mode) {
    Summary s;
    if (!v2Rules.is_array()) return s;
    const std::string bodyTarget = (mode == Mode::Server) ? "responseBody" : "requestBody";
    for (const auto &rule : v2Rules) {
        const std::string from = text(rule.value("from", nlohmann::json::object()), "source");
        const std::string to = text(rule.value("to", nlohmann::json::object()), "target");
        if (from == "requestBody") s.usesRequestBody = true;
        if (mode != Mode::ClientResponse && to == bodyTarget) s.usesJsonBodyTarget = true;
        if (from == "serverEvent" || from == "clientEvent" || to == "serverEvent" || to == "clientEvent") s.needsStorage = true;
        s.endsWithStop = (to == "stop");
    }
    return s;
}

nlohmann::json EngineGrammarV2::lowerPipeline(const nlohmann::json &v2Rules, Mode mode, bool usesJsonBodyTarget,
        std::string &error) {
    if (!v2Rules.is_array()) return nlohmann::json::array();
    try {
        return lowerRules(v2Rules, mode, usesJsonBodyTarget);
    } catch (const NotSupported &e) {
        error = e.reason + " is not supported yet";
    } catch (const std::exception &e) {
        error = std::string("malformed rule: ") + e.what();  // the schema should have rejected it
    }
    return nlohmann::json();
}

}  // namespace model
}  // namespace h2agent
