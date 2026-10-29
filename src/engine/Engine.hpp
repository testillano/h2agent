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

#include "Evaluation.hpp"
#include "Ports.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ert {
namespace jsonflow {

/** Opaque parsed pipeline (AST). Built once, evaluated many times. */
class Pipeline;

/**
 * The transformation engine: parses a pipeline of `source -> filter -> target` rules
 * from JSON and evaluates it over an EvaluationContext, driving side effects
 * exclusively through the injected Ports.
 *
 * Design goals (proposal section 8): zero transport dependency, pure core,
 * dry-run as a first-class capability (evaluate with in-memory/no-op ports),
 * and reusability across hosts (h2agent, h2diagent, ...).
 *
 * THREAD-SAFETY (mirrors h2agent's model: stateless logic over thread-safe
 * stores).
 *   - A parsed Pipeline is immutable after parse(); it may be shared and
 * evaluated concurrently from many threads with no external locking.
 *   - evaluate() is const and keeps ALL per-event state in the caller-owned
 *     EvaluationContext (one per thread/request). The Engine holds no mutable
 *     state between calls, so it is reentrant.
 *   - Shared mutable state (the store, event history) lives behind the Ports.
 *     Concurrency safety is the PORT IMPLEMENTATION's responsibility, exactly
 * as h2agent delegates it to its sharded, shared_mutex-backed Vault/Map. The
 *     engine never adds its own locks around a port.
 *
 * Skeleton: declarations only. Implementations land during the parity-first
 * extraction from h2agent (phase [0.5], section 8.5).
 */
class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    /**
     * Parse a pipeline from its JSON array representation.
     *
     * @param j JSON array of rule objects.
     * @param error Human-readable error filled on failure.
     * @return Parsed pipeline, or nullptr on error.
     */
    std::shared_ptr<Pipeline> parse(const nlohmann::json &j, std::string &error) const;

    /**
     * Evaluate a parsed pipeline over a context.
     *
     * @param pipeline Parsed pipeline.
     * @param ctx Evaluation context (mutated in place).
     * @param ports Injected capabilities (any member may be null).
     * @param trace When true, populate result.executionTrace (dry-run / debug).
     * @return Evaluation result.
     */
    TransformResult evaluate(const std::shared_ptr<Pipeline> &pipeline, EvaluationContext &ctx, const Ports &ports,
                             bool trace = false) const;
};

}  // namespace jsonflow
}  // namespace ert
