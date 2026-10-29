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

#include <cstdint>
#include "Ports.hpp"
#include "Types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace ert {
namespace jsonflow {

/**
 * Transport metadata written (meta.* targets) and read (meta source) by the
 * pipeline. TYPED AND BY REFERENCE (performance over neutrality): members point
 * to the host's own variables, so there is no per-event allocation nor mapping
 * back. A null member means the host does not support it: the target is skipped
 * and the source is absent.
 */
struct Meta {
    unsigned int *statusCode{nullptr};
    unsigned int *delayMs{nullptr};
    unsigned int *timeoutMs{nullptr};
    std::string *uri{nullptr};
    std::string *method{nullptr};
    // Header writes in order (multimap semantics: the host emplaces every entry).
    std::vector<std::pair<std::string, std::string>> *headers{nullptr};
};

/**
 * Per-event evaluation context.
 *
 * BY REFERENCE: the context does not own the host data. `input`/`outputView`
 * are lazy read ports; `output`, `outBody`, the state strings and `meta` point
 * to the host's own objects, so no per-event copy is made and nothing has to be
 * mapped back; a value is only copied when a rule reads it (into the rule
 * working value, as any filter chain requires). All pointers must stay valid
 * during Engine::evaluate(). A null pointer means the capability is absent:
 * reads are "not found" (rule skipped) and writes are dropped.
 *
 * Input field names are a host convention (e.g. h2agent server input: uri,
 * uriPath, uriParams, body, headerMap, headers), addressed via json pointers.
 */
struct EvaluationContext {
    // Input (read-only):
    const Document *input{nullptr};
    std::int64_t sequence{0};             // generic monotonic event id
    const std::string *inState{nullptr};  // incoming state (data only: FSM correlation is the host's job)

    // Output being built:
    const Document *outputView{nullptr};  // how `output` sources read the body document
    nlohmann::json *output{nullptr};      // body document written by output.json.* targets
    std::string *outBody{nullptr};        // raw payload written by output.string/hexstring
    std::string *outState{nullptr};
    // Foreign out-state (outState.<METHOD>[.<uri>]): data only, no FSM logic.
    std::string *outStateMethod{nullptr};
    std::string *outStateUri{nullptr};
    Meta meta{};
};

/**
 * Self-contained context that OWNS its data (tests, dry-run). Its members
 * intentionally hide the base pointers, which are bound to them; pass it
 * wherever an EvaluationContext is expected. Not copyable (self-pointers).
 */
struct StandaloneContext : EvaluationContext {
    nlohmann::json input{};
    nlohmann::json output{};
    BodyView outBody{};
    std::string inState{};
    std::string outState{};
    std::string outStateMethod{};
    std::string outStateUri{};
    // Meta storage:
    unsigned int statusCode{0};
    unsigned int delayMs{0};
    unsigned int timeoutMs{0};
    std::string uri{};
    std::string method{};
    std::vector<std::pair<std::string, std::string>> headers{};

    StandaloneContext() {
        EvaluationContext::input = &input_doc_;
        EvaluationContext::inState = &inState;
        EvaluationContext::outputView = &output_doc_;
        EvaluationContext::output = &output;
        EvaluationContext::outBody = &outBody.raw;
        EvaluationContext::outState = &outState;
        EvaluationContext::outStateMethod = &outStateMethod;
        EvaluationContext::outStateUri = &outStateUri;
        meta.statusCode = &statusCode;
        meta.delayMs = &delayMs;
        meta.timeoutMs = &timeoutMs;
        meta.uri = &uri;
        meta.method = &method;
        meta.headers = &headers;
    }
    StandaloneContext(const StandaloneContext &) = delete;
    StandaloneContext &operator=(const StandaloneContext &) = delete;

    /** Last value written for header `name` ("" if none). */
    std::string header(const std::string &name) const {
        for (auto it = headers.rbegin(); it != headers.rend(); ++it)
            if (it->first == name) return it->second;
        return std::string();
    }

private:
    JsonDocument input_doc_{input};
    JsonDocument output_doc_{output};
};

/** One recorded step of the pipeline, enabling dry-run and traceability. */
struct TraceEntry {
    std::size_t index{0};  // rule position in the pipeline
    std::string source{};
    std::string filter{};
    std::string target{};
    bool applied{false};  // false when skipped
    std::string note{};   // why skipped / error detail
};

/** Outcome of evaluating a pipeline over a context. */
struct TransformResult {
    bool ok{true};
    bool broke{false};                         // a break rule interrupted the pipeline
    std::vector<TraceEntry> executionTrace{};  // populated when tracing is enabled
};

}  // namespace jsonflow
}  // namespace ert
