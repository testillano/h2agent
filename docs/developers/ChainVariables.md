# Chain Variables (scoped variables internals)

## Terminology

- **User-facing (docs/api)**: "scoped variable" — contrasts with "vault entry" and describes the behavior: `var.*` has a defined lifetime scope (the `outState` chain), unlike `vault.*` which persists indefinitely.
- **Code internals**: `chain_variables_`, `chainVariables`, `getChainVariables()` — describes the mechanism: these are the variables that persist along the `outState` chain.

The keyword remains `var.*` — no API change.

## Motivation

Before this change, `var.*` was provision-scoped: created and destroyed within a single provision execution. To pass data across `outState` links, the only option was `vault.*`, which has two problems:

1. **Namespace pollution**: concurrent flows share the global namespace, requiring `seq`-based naming (`vault.flow_@{seq}_myData`) to avoid collisions.
2. **Manual cleanup**: vault are never freed automatically, requiring explicit `eraser` transformations at the end of every chain to prevent memory growth.

## Design decision: promote `var` vs new `chainVar` keyword

We chose to promote `var.*` to chain-scoped rather than introduce a new `chainVar.*` keyword:

- **Simpler UX**: the user writes `var.foo` and it just works across the chain. No new concept.
- **Backward compatible**: without `outState`, `var` behaves exactly as before. The only change is for chains, where vars now survive across links — but nobody relies on vars being *lost* between links.
- **No schema changes**: no new source/target types, no regex updates.

The alternative (`chainVar.*`) was considered but rejected as it adds cognitive load without meaningful benefit. It is documented in `chain-scoped-variable.md` at the project root for historical reference.

## Chain scope per mode

### Client mode (explicit chain)

The chain is explicit: `sendClientRequest()` calls itself recursively on state progression. A `shared_ptr<map<string,string>>` is created on the first call and captured in the async lambda, flowing through:

```
sendClientRequest(provision, inState, endpoint, chainVariables=nullptr)
  → chainVariables = make_shared<map>()   // first call only
  → provision->transform(..., *chainVariables)
  → asyncSend → on response callback:
      → provision->transformResponse(..., *chainVariables)
      → if state progression:
          → sendClientRequest(nextProvision, finalOutState, endpoint, chainVariables)  // same shared_ptr
```

Variables set in `transform()` (request phase) are visible in `transformResponse()` (response phase) of the same link, and in both phases of all subsequent links.

**This is where scoped variables add the most value.** Chain links often hit different URIs (e.g., auth → data → update), making `clientEvent` addressing verbose and tightly coupled. Example:

```json
[
  {
    "id": "authenticate", "endpoint": "backend",
    "requestMethod": "POST", "requestUri": "/auth",
    "outState": "authenticated",
    "onResponseTransform": [
      {"source": "response.body./token", "target": "var.token"}
    ]
  },
  {
    "id": "fetchData", "endpoint": "backend",
    "requestMethod": "GET", "requestUri": "/data",
    "inState": "authenticated",
    "requestHeaders": {"authorization": "Bearer @{var.token}"}
  }
]
```

Without scoped vars, link 2 would need: `clientEvent.clientEndpointId=backend&requestMethod=POST&requestUri=/auth&eventNumber=-1&eventPath=/responseBody/token`.

### Server mode (implicit chain)

There is no explicit chain loop. Each HTTP request is handled independently. The "chain" is implicit: successive requests to the same `(method, uri)` are correlated by state stored in `MockEventsHistory`.

Chain variables are stored alongside the state in `MockEventsHistory`, which is keyed by `DataKey` (method + uri):

```
Request 1: GET /foo
  → findLastRegisteredRequestState(GET, /foo) → inState="initial", chainVars={}
  → provision(inState=initial).transform(..., chainVars)
  → chainVars may now contain {total: "100"}
  → loadEvent(...)  // stores event with outState
  → storeChainVariables(GET, /foo, chainVars)

Request 2: GET /foo
  → findLastRegisteredRequestState(GET, /foo) → inState="step2", chainVars={total: "100"}
  → provision(inState=step2).transform(..., chainVars)
  → reads var.total → "100"
```

**Important**: the chain variables are scoped to the `DataKey` (method + uri), not to a specific `inState`. All state transitions for the same `(method, uri)` share the same variables map. This is consistent with how the state itself works — there is one state per `DataKey`, not per provision.

In server mode, `serverEvent` already covers most cross-link data needs (since each link's method+URI is known). Scoped vars are a convenience for accumulation patterns (e.g., running totals) where iterating through all previous events would be cumbersome.

## Concurrency consideration

In both modes, concurrent flows to the same `DataKey` share the same chain variables. This is the same limitation that already exists for the state: if two clients send requests to `(GET, /foo)` concurrently, they share the state progression. The chain variables inherit this design constraint.

For concurrent isolation, users should use `vault.*` with `seq`-based naming (the existing pattern).

## Performance impact

Negligible:
1. Heap allocation of the variables map once per chain start (shared_ptr in client mode, stored in MockEventsHistory in server mode).
2. Shared_ptr copy at each chain link (client mode) or map copy on state retrieval (server mode).
3. Cleanup when the chain ends.

This is trivial compared to the HTTP/2 I/O overhead of each chain link.

## Files involved

| File | Role |
|------|------|
| `MockEventsHistory.hpp` | Storage: `chain_variables_` member, getter/setter with mutex |
| `MockData.hpp/cpp` | Access: `findLastRegisteredRequestState` overload (with chain vars), `storeChainVariables` |
| `AdminServerProvision.hpp/cpp` | Server transform: `variables` parameter (by reference) |
| `AdminClientProvision.hpp/cpp` | Client transform/transformResponse: `variables` parameter (by reference) |
| `MyTrafficHttp2Server.cpp` | Server flow: retrieve → transform → store |
| `MyAdminHttp2Server.hpp/cpp` | Client flow: shared_ptr through recursive sendClientRequest |
| `TypeConverter.hpp` | Comments: "local variables" → "scoped variables" |
| `docs/api/README.md` | User docs: "local variable" → "scoped variable" |
