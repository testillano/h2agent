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
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace h2agent {
namespace model {

/**
 * Host vocabulary layer between h2agent v1 provision grammar and the
 * transport-neutral transformation engine (src/engine, h2agent::engine).
 *
 * The alias map is HARDCODED here on purpose (proposal 8.7): the engine knows
 * only neutral names (input/output/meta/local/global/sequence); h2agent maps its
 * HTTP vocabulary onto them. Rule: aliases only RENAME, never remodel structure.
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
class EngineGrammarV1 {
public:
    /** Which v1 pipeline is being translated (sources/targets differ). */
    enum class Mode {
        Server,         // server provision "transform"
        ClientRequest,  // client provision "transform" (builds the outgoing request)
        ClientResponse  // client provision "onResponseTransform" (reads the response)
    };

    /** Marker prefix for v1 expressions that are not applicable in the given
     * mode (v1 silently ignores/skips them). The engine classifies anything with
     * this prefix as Unsupported, so the rule is skipped and traced. */
    static constexpr const char *NotApplicable = "n/a.";

    /**
     * Translate one v1 source expression to the engine grammar.
     * @param usesJsonBodyTarget ClientRequest only: whether the provision has a
     *        request.body.json.* target (v1 then reads the body being built;
     *        otherwise request.body yields the whole template as a string).
     */
    static std::string translateSource(const std::string &v1, Mode mode, bool usesJsonBodyTarget = false);

    /** Translate one v1 target expression to the engine grammar. */
    static std::string translateTarget(const std::string &v1, Mode mode);

    /**
     * Translate a whole v1 transform array (each item: source/target/filter/
     * onFilterFail). Filters are grammar-identical and pass through untouched;
     * onFilterFail is translated recursively. Non-array input yields an empty
     * array.
     */
    static nlohmann::json translatePipeline(const nlohmann::json &v1Transforms, Mode mode,
                                            bool usesJsonBodyTarget = false);

    /**
     * Escape a single json-pointer reference token (RFC 6901: '~'->'~0',
     * '/'->'~1'). Used for header / query-parameter names placed under input.
     */
    static std::string escapePointerToken(const std::string &token);

    /**
     * Alias table (v1 prefix -> engine prefix) for the given mode, dumped at
     * startup for diagnostics (proposal 8.7: raw layer + alias dump).
     */
    static std::vector<std::pair<std::string, std::string>> aliases(Mode mode);
};

}  // namespace model
}  // namespace h2agent
