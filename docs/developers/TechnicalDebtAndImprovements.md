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
