# Technical Debt and Improvements

## 1. Source/Target Addressing

The current regex-based addressing is the main pain point. `Transformation.cpp` has **39 static regex** and `AdminServerProvision.cpp` has **91 case branches**. Adding a new source/target type requires:
- Adding regex in `Transformation.cpp`
- Adding enum value in `Transformation.hpp`
- Adding case in `AdminServerProvision.cpp` (~1600 lines)
- Duplicating almost everything in `AdminClientProvision.cpp` (~1377 lines)

### Alternatives

- **Structured source/target (opt-in)**: accept both string and JSON object `{"type":"vault","key":"k","path":"/p"}`. Backward compatible — string parsed as today; object uses typed fields. Enables editor autocompletion and schema validation.

## 2. Server/Client Provision Duplication

**STATUS: DEFERRED**

`AdminServerProvision.cpp` (~1600 lines) and `AdminClientProvision.cpp` (~1377 lines) share ~90% identical switch cases (91 vs 89). Analysis revealed more complexity than initially estimated:
- `transform()` signatures are fundamentally different (server: request→response; client: generates request + separate `transformResponse()` for post-response).
- Client has extra logic: `saveDynamics`, `updateTriggering`, `startTicking`, `stopTicking`, `scheduleTick` (sequence/CPS system).
- The real duplication is in `processSources()`, `processFilters()`, `processTargets()`.
- Extracting those to free functions or a helper class would require passing many class member references as parameters.
- Risk of regression is high for a purely maintainability benefit. Adding a new source/target means touching both files — annoying but not catastrophic, and well covered by tests.

### 2.1 TransformContext struct
`processSources()`, `processFilters()`, `processTargets()` and `executeOnFilterFail()` share 15+ parameters passed individually. A `TransformContext` struct holding references to all of them would improve readability. Zero performance cost — compiler optimizes it the same way. Natural fit if the provision duplication refactor (section 2) is ever tackled.

## 3. Functional Gaps

### 3.1 Iteration Over Arrays
No way to iterate a JSON array and apply transforms to each element. Useful for generating dynamic responses from collections.

### 3.3 Provision Composition / Include
No DRY mechanism. If 20 provisions share the same 5 transforms, they are copied 20 times. A `"$ref"` or `"include"` would solve this.

### 3.3 Intermediate Variable Scope
Only `var` (local to one execution) and `vault` (global forever). No "session-scoped" or "request-chain-scoped" variable that survives triggers but is cleaned up at the end of the flow.

## 4. API and Operations

### 4.1 No Pagination
`GET /admin/v1/server-data` returns everything. With thousands of events in a load test, the response can be huge.

### 4.2 No Filtering
Cannot request "only events with status 500" without downloading everything.

### 4.3 WebSocket for Admin
The long-poll vault/wait mechanism works, but a WebSocket channel for real-time events (new traffic, vault changes, errors) would be more natural and efficient.

## 5. Testing

### 6.1 CT Cleanup
State can leak between tests. `h2agent_server_configuration()` now cleans vault, but each CT should be fully idempotent.

### 6.2 No Concurrency CTs
No test verifies behavior under concurrent load (race conditions in vault, provisions, etc.).

### 6.3 No Fuzzing
No tests with malformed JSON provisions, invalid paths, unicode edge cases, etc.

## 8. Architecture Notes

### What is already well done
- `Map` uses `shared_mutex` with `shared_lock` for reads and `unique_lock` for writes.
- All 39 regex in `Transformation.cpp` are static with `std::regex::optimize` — compiled once.
- Admin/traffic separation on distinct ports.
- The matching system (FullMatching, RegexMatching) is solid.
- The katas as a learning tool are excellent.
- The wait/long-poll for vault is elegant.
- Dynamic log level change via admin API.

### Minor optimization
~~`nlohmann::json` is passed by value in some `load()` paths. For large objects, ensuring `std::move` semantics everywhere could help (already used in `loadAtPath` but not in all paths).~~

**DONE** (v4.7.0): Added move overload `Vault::load(string, json&&)`. Callers use `std::move` on local temporals (`captureObj`, `val`). Const refs (`obj` from `getObject()`) remain as copies.

## Prioritization (impact / effort)

| Improvement | Impact | Effort |
|---|---|---|
| Include/ref in provisions | High | Low |
| Pagination on admin endpoints | Medium | Low |
| CT systematic cleanup | Medium | Low |
| Concurrency CTs | Medium | Medium |
| Provision duplication refactor | High | High |
| WebSocket admin channel | Medium | High |
