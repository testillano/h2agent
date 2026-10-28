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

#include <EngineGrammarV1.hpp>

namespace h2agent {
namespace model {

namespace {

bool startsWith(const std::string &s, const std::string &p) {
    return s.rfind(p, 0) == 0;
}

}  // namespace

std::string EngineGrammarV1::escapePointerToken(const std::string &token) {
    std::string out;
    out.reserve(token.size());
    for (char c : token) {
        if (c == '~')
            out += "~0";
        else if (c == '/')
            out += "~1";
        else
            out += c;
    }
    return out;
}

namespace {

std::string na(const std::string &v1) {
    return std::string(EngineGrammarV1::NotApplicable) + v1;
}

}  // namespace

std::string EngineGrammarV1::translateSource(const std::string &v1, Mode mode, bool usesJsonBodyTarget) {
    // Storage aliases are common to every mode.
    if (startsWith(v1, "var.")) return "local." + v1.substr(4);
    if (startsWith(v1, "vault.")) return "global." + v1.substr(6);

    if (mode == Mode::Server) {
        if (v1 == "recvseq") return "sequence";
        if (v1 == "sendseq") return na(v1);  // v1: not applicable in server context
        // Order matters: most specific request.uri forms first.
        if (v1 == "request.uri") return "input./uri";
        if (v1 == "request.uri.path") return "input./uriPath";
        if (startsWith(v1, "request.uri.param.")) return "input./uriParams/" + escapePointerToken(v1.substr(18));
        if (v1 == "request.body") return "input./body";
        if (startsWith(v1, "request.body.")) return "input./body" + v1.substr(13);  // "/a/b"
        if (v1 == "response.body") return "output";
        if (startsWith(v1, "response.body.")) return "output." + v1.substr(14);  // "output./a"
        if (startsWith(v1, "request.header.")) return "input./headerMap/" + escapePointerToken(v1.substr(15));
        if (v1 == "request.headers") return "input./headers";
        if (startsWith(v1, "response.header") || v1 == "response.statusCode") return na(v1);
        return v1;
    }

    // Client modes (request build and response processing)
    if (v1 == "sendseq") return "sequence";
    if (v1 == "recvseq" || startsWith(v1, "request.uri.")) return na(v1);  // v1: return false
    if (startsWith(v1, "request.header.")) return "input./headerMap/" + escapePointerToken(v1.substr(15));
    if (v1 == "request.headers") return "input./headers";

    if (mode == Mode::ClientRequest) {
        if (v1 == "request.uri") return "meta./uri";  // live value (request.uri targets update it)
        if (v1 == "request.body" || startsWith(v1, "request.body.")) {
            if (!usesJsonBodyTarget) return "input./requestBodyString";  // v1: whole template string
            return v1 == "request.body" ? "output" : "output." + v1.substr(13);
        }
        if (startsWith(v1, "response.")) return na(v1);  // no response yet (v1 returns false)
        return v1;
    }

    // Mode::ClientResponse
    if (v1 == "request.uri") return "input./uri";
    if (v1 == "request.body" || startsWith(v1, "request.body.")) return "input./requestBodyString";
    if (v1 == "response.body") return "input./response/body";
    if (startsWith(v1, "response.body.")) return "input./response/body" + v1.substr(14);
    if (startsWith(v1, "response.header."))
        return "input./response/headerMap/" + escapePointerToken(v1.substr(16));
    if (v1 == "response.headers") return "input./response/headers";
    if (v1 == "response.statusCode") return "input./response/statusCode";
    return v1;
}

std::string EngineGrammarV1::translateTarget(const std::string &v1, Mode mode) {
    if (startsWith(v1, "var.")) return "local." + v1.substr(4);
    if (startsWith(v1, "vault.")) return "global." + v1.substr(6);

    if (mode == Mode::Server) {
        if (v1 == "response.body.string") return "output.string";
        if (v1 == "response.body.hexstring") return "output.hexstring";
        if (startsWith(v1, "response.body.json.")) return "output.json." + v1.substr(19);
        if (startsWith(v1, "response.header.")) return "meta.header." + v1.substr(16);
        if (v1 == "response.statusCode") return "meta.statusCode";
        if (v1 == "response.delayMs") return "meta.delayMs";
        if (startsWith(v1, "request.") || startsWith(v1, "clientEvent.")) return na(v1);
        // outState[.<METHOD>[.<uri>]], txtFile., binFile., udpSocket.,
        // serverEvent. (purge), clientProvision., break: identical.
        return v1;
    }

    // Client modes: server-only targets are no-ops in v1.
    if (startsWith(v1, "response.") || startsWith(v1, "serverEvent.") || startsWith(v1, "clientProvision.") ||
            startsWith(v1, "outState."))
        return na(v1);

    if (mode == Mode::ClientRequest) {
        if (v1 == "request.body.string") return "output.string";
        if (v1 == "request.body.hexstring") return "output.hexstring";
        if (startsWith(v1, "request.body.json.")) return "output.json." + v1.substr(18);
        if (startsWith(v1, "request.header.")) return "meta.header." + v1.substr(15);
        if (v1 == "request.uri") return "meta.uri";
        if (v1 == "request.method") return "meta.method";
        if (v1 == "request.delayMs") return "meta.delayMs";
        if (v1 == "request.timeoutMs") return "meta.timeoutMs";
        return v1;  // var/vault/outState/txtFile/binFile/udpSocket/clientEvent/break
    }

    // Mode::ClientResponse: request.* targets write throw-away copies in v1.
    if (startsWith(v1, "request.")) return na(v1);
    return v1;
}

nlohmann::json EngineGrammarV1::translatePipeline(const nlohmann::json &v1Transforms, Mode mode,
        bool usesJsonBodyTarget) {
    nlohmann::json out = nlohmann::json::array();
    if (!v1Transforms.is_array()) return out;
    for (const auto &item : v1Transforms) {
        if (!item.is_object()) {
            out.push_back(item);  // let the engine parser report the malformed rule
            continue;
        }
        nlohmann::json t = item;  // filter and any other key pass through untouched
        auto s = item.find("source");
        if (s != item.end() && s->is_string())
            t["source"] = translateSource(s->get<std::string>(), mode, usesJsonBodyTarget);
        auto g = item.find("target");
        if (g != item.end() && g->is_string()) t["target"] = translateTarget(g->get<std::string>(), mode);
        auto f = item.find("onFilterFail");
        if (f != item.end()) t["onFilterFail"] = translatePipeline(*f, mode, usesJsonBodyTarget);
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> EngineGrammarV1::aliases(Mode mode) {
    std::vector<std::pair<std::string, std::string>> a = {{"var.<id>", "local.<id>"}, {"vault.<key>", "global.<key>"}};
    if (mode == Mode::Server) {
        a.insert(a.end(), {{"recvseq", "sequence"},
            {"request.uri", "input./uri"},
            {"request.uri.path", "input./uriPath"},
            {"request.uri.param.<n>", "input./uriParams/<n>"},
            {"request.body[./ptr]", "input./body[/ptr]"},
            {"response.body[./ptr]", "output[./ptr]"},
            {"request.header.<h>", "input./headerMap/<h>"},
            {"request.headers", "input./headers"},
            {"response.body.string", "output.string"},
            {"response.body.hexstring", "output.hexstring"},
            {"response.body.json.<kind>[./ptr]", "output.json.<kind>[./ptr]"},
            {"response.header.<h>", "meta.header.<h>"},
            {"response.statusCode", "meta.statusCode"},
            {"response.delayMs", "meta.delayMs"}
        });
    } else if (mode == Mode::ClientRequest) {
        a.insert(a.end(), {{"sendseq", "sequence"},
            {"request.uri (source)", "meta./uri"},
            {"request.body[./ptr] (source)", "output[./ptr] | input./requestBodyString"},
            {"request.header.<h> (source)", "input./headerMap/<h>"},
            {"request.headers", "input./headers"},
            {"request.body.string", "output.string"},
            {"request.body.hexstring", "output.hexstring"},
            {"request.body.json.<kind>[./ptr]", "output.json.<kind>[./ptr]"},
            {"request.header.<h> (target)", "meta.header.<h>"},
            {"request.uri (target)", "meta.uri"},
            {"request.method", "meta.method"},
            {"request.delayMs", "meta.delayMs"},
            {"request.timeoutMs", "meta.timeoutMs"}
        });
    } else {
        a.insert(a.end(), {{"sendseq", "sequence"},
            {"request.uri", "input./uri"},
            {"request.body[./ptr]", "input./requestBodyString"},
            {"request.header.<h>", "input./headerMap/<h>"},
            {"request.headers", "input./headers"},
            {"response.body[./ptr]", "input./response/body[/ptr]"},
            {"response.header.<h>", "input./response/headerMap/<h>"},
            {"response.headers", "input./response/headers"},
            {"response.statusCode", "input./response/statusCode"}
        });
    }
    return a;
}

}  // namespace model
}  // namespace h2agent
