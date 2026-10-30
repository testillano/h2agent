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

namespace h2agent {
namespace model {

/**
 * Host vocabulary layer between the h2agent transformation grammar v2
 * (docs/developers/GrammarV2.md) and the transformation engine (src/engine,
 * h2agent::engine).
 *
 * Provisions arrive in grammar v2 (already validated by the provision schema).
 * Each rule is lowered ONCE at load time into the engine rule representation:
 * a {"source", "target", "filter"} object whose expressions address the
 * engine documents (input/output/meta), stores (local/global) and effects.
 * The engine representation is internal: it is never exposed to users.
 *
 * Neutral input document exposed by the host (see ServerInputDocument):
 *   {"uri": <url-decoded normalized uri>, "uriPath": <path>,
 *    "uriParams": {name: value}, "body": <json | string>,
 *    "headerMap": {name: first value}, "headers": [{"name":..,"value":..}]}
 *
 * Client request phase (ClientInputDocument): input = {"uri", "headerMap",
 * "headers", "requestBodyString"}; output = request body json being built;
 * meta bound to the live request uri/method/delayMs/timeoutMs (typed, in place).
 * Client response phase: same input plus "response": {"body", "headerMap",
 * "headers", "statusCode"}.
 *
 * Dependency-free (nlohmann only) so it is unit-testable in isolation.
 */
class EngineGrammarV2 {
public:
    /** Which pipeline is being lowered (applicable kinds differ). */
    enum class Mode {
        Server,         // server provision "transform"
        ClientRequest,  // client provision "transform" (builds the outgoing request)
        ClientResponse  // client provision "onResponseTransform" (reads the response)
    };

    /** Marker prefix for kinds not applicable in a mode: the engine classifies
     * it as Unsupported, so the rule is skipped and traced (v1 parity). */
    static constexpr const char *NotApplicable = "n/a.";

    /** Facts about a rule list needed by the host before evaluation (top-level rules only). */
    struct Summary {
        bool usesRequestBody{};     // some 'from' reads requestBody (request body must be decoded)
        bool usesJsonBodyTarget{};  // some 'to' writes the (server response | client request) json body
        bool needsStorage{};        // some rule reads or purges stored events
        bool endsWithStop{};        // last rule is a 'stop' target (illogical, warned)
    };

    static Summary summarize(const nlohmann::json &v2Rules, Mode mode);

    /**
     * Lowers a grammar v2 rule list (recursively, onFilterFail included).
     *
     * @param usesJsonBodyTarget ClientRequest only: whether the provision writes
     *        the request json body (requestBody then reads the body being built;
     *        otherwise it yields the whole template as a string, v1 parity).
     * @param error Reason when a rule uses a v2 feature not supported yet by the
     *        engine (several chained filters, a cast which is not the last filter
     *        of a json body or vault target, a conditional erase).
     * @return Engine rule array, or null json on error.
     */
    static nlohmann::json lowerPipeline(const nlohmann::json &v2Rules, Mode mode, bool usesJsonBodyTarget,
                                        std::string &error);

    /** Escapes a json-pointer reference token (RFC 6901: '~'->'~0', '/'->'~1'). */
    static std::string escapePointerToken(const std::string &token);
};

}  // namespace model
}  // namespace h2agent
