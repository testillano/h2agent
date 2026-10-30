# Transformation engine (`src/engine`)

## What it is

The provision transformation pipeline (`transform` / `onResponseTransform`: source -> filter -> target) is executed by an internal subsystem, `src/engine` (CMake library `h2agent-engine`, namespace `h2agent::engine`). It used to live, duplicated, inside `AdminServerProvision` and `AdminClientProvision` (`processSources` / `processFilters` / `processTargets`); now both provisions share one implementation.

The engine knows nothing about HTTP/2, nghttp2 or the h2agent model: it reads and writes through **ports** and a per-event **context**, implemented by the host (`src/model/EngineHost.*`). Users write provisions in grammar v2 (`docs/developers/GrammarV2.md`); the host lowers them once, at load time, into the engine rule representation, which is internal.

```
 provision (v2 JSON) --load--> EngineGrammarV2 (lower)     --> Engine::parse --> immutable Pipeline
                                                                                      |
 request/response  --event--> EngineHost (ports + context, by reference) --> Engine::evaluate
```

## Pieces

| File | Role |
|---|---|
| `src/engine/Engine.{hpp,cpp}` | `parse()` (classify + precompile rules) and `evaluate()` (run them). |
| `src/engine/Rule.hpp` | Parsed rule: source/filter/target kinds and everything precompiled at load. |
| `src/engine/Evaluation.hpp` | `EvaluationContext` (per event), `Meta`, `TraceEntry`, `TransformResult`, `StandaloneContext`. |
| `src/engine/Ports.hpp` | `Document`, `Store`, `EventSource`, `FileAccess`, `SocketWriter`, `Clock`, `Shell`, `Trigger`, `SchemaValidator`. |
| `src/model/EngineGrammarV2.*` | Load-time lowering of the grammar v2 rules (`{"source": "requestBody"}`, `{"target": "responseStatusCode"}`, `var`, `vault`...) into the engine representation (`input./body`, `meta.statusCode`, `local.`, `global.`...), per mode (server, client request, client response), plus the facts the host needs before evaluation (request body read, json body target, stored events). |
| `src/model/EngineHost.*` | Port implementations over h2agent resources (vault, chain variables, events, files, sockets, schemas) and the lazy input documents. |

## Design rules (keep them when touching the engine)

* **Parse once, evaluate many.** Everything that does not depend on the event is decided in `parse()` (provision load), never per event:
  * regex filters (`std::regex`);
  * `math` expressions: compiled once with `@{var}` bound as exprtk variables (constant expressions are folded). Values that would not behave identically to textual substitution (signed, non-numeric, glued to a token) fall back to the v1 textual path;
  * static target json pointers and source pointers (`CompiledPointer`: head field + rest);
  * `@{}` templates: no allocation when there is nothing to substitute.
* **By reference, no per-event copies.** The context points to the host objects: input/output are lazy `Document` ports, `output`/`outBody`/state/`Meta` point to the response (server) or request (client) variables, written in place. A value is only copied when a rule reads it.
* **Thread safety.** A `Pipeline` is immutable and shared by all workers; `evaluate()` is const and keeps per-event state in the caller context. Shared state lives behind the ports (sharded `Vault`/`Map`). The only mutable piece in a pipeline is the per-rule pool of compiled `math` instances (locked acquire/release).
* **Grammar semantics belong to the host.** E.g. "an empty node read by path is absent" is a v1 rule (kept in v2) for body paths only; it is applied by `V1BodyDocument`, not by the engine (`request.headers` with no headers is an empty array, as in v1).

## Differences with the former implementation

The behavior is the same as before (all original UT/CT unchanged), except for two `math` error cases where the former code was buggy, kept on purpose and covered by UT:

* **Expression that does not compile** (e.g. `math.2*/`): the former code evaluated the uncompiled expression anyway and wrote its meaningless result to the target. Now the rule is skipped and the target is left untouched, as any other source that cannot be resolved.
* **Whole result beyond the `int` range** (e.g. `math.3000000000`): the former code tested `result == (int)result` (overflow) and stored it as a float. Now it is stored as an integer (`3000000000`).

Request header writes in client provisions are visible to later `requestHeader`/`requestHeaders` reads in the same pipeline, exactly as before (the header map is a multimap: a rewrite adds a value and the lookup keeps returning the first one).

## Tests

* `ut/engine/Engine_test.cpp`: every source, filter and target in isolation with in-memory port doubles (`StandaloneContext` owns its documents).
* `ut/model/EngineGrammarV2/lower.cpp`: lowering per mode, and v2 features the engine does not run yet.
* `ut/model/Transformation/{transform,clientTransform}.cpp`: the provision suites over real (v2) provisions, migrated from the original v1 ones with `h2agent-migrate`: they are the parity contract with v1.

## Performance check

`benchmark/compare.sh` compares two image tags on the same profiles (interleaved rounds, medians, CPU time per request, range-overlap verdict, correctness guard on non-2xx/failed responses):

```bash
$ cd benchmark
$ SERVER_CPUS=2-3 LOAD_CPUS=4-5 ./compare.sh --base <reference tag> --cand <candidate tag> --rounds 5
```

Use it for any change in the engine hot path. Profiles: `server/light` (no transforms), `server/default` (includes `math`), `server/ipv4-with-regexreplace`, `server/many-rules` (21 cheap rules, per-rule cost) and `client/default`. Results at the end of the refactor (CPU per request vs the former implementation): `default` -84% (about 6x throughput, `math` precompiled), `many-rules` -8%, others unchanged.
