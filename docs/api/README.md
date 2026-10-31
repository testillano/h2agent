# H2Agent Management API - User Guide

This guide covers the concepts, behavior and usage patterns of the h2agent management API.
For the endpoint reference (parameters, schemas, status codes), see the [OpenAPI specification](./openapi.yaml)
or the [interactive documentation](https://testillano.github.io/h2agent/api/).

## Table of contents

- [Overview](#overview)
- [General operations](#general-operations)
  - [API schemas](#api-schemas)
  - [Schemas](#schemas)
  - [Vault](#vaults)
    - [Blocking wait](#blocking-wait)
  - [Files](#files)
  - [Logging](#logging)
  - [Health](#health)
  - [Capabilities](#capabilities)
  - [Configuration](#configuration)
- [Matching algorithms](#matching-algorithms)
  - [URI path query parameters](#uri-path-query-parameters)
  - [Regex configuration (rgx & fmt)](#regex-configuration-rgx--fmt)
  - [Algorithm selection](#algorithm-selection)
- [Server provisions](#server-provisions)
  - [State machine (inState/outState)](#state-machine-instateoutstate)
  - [Server provision fields](#server-provision-fields)
  - [Variable types: `var` vs `vault`](#variable-types-var-vs-vault)
  - [Transformation pipeline](#transformation-pipeline)
  - [Multiple provisions](#multiple-provisions)
  - [Unused provisions](#unused-provisions)
- [Server data](#server-data)
  - [Storage configuration](#storage-configuration)
  - [Querying server data](#querying-server-data)
  - [Server data summary](#server-data-summary)
  - [Deleting server data](#deleting-server-data)
- [Client endpoints](#client-endpoints)
- [Client provisions](#client-provisions)
  - [Client state machine](#client-state-machine)
  - [Client provision fields](#client-provision-fields)
  - [Client transformation differences](#client-transformation-differences)
  - [Multiple client provisions](#multiple-client-provisions)
  - [Unused client provisions](#unused-client-provisions)
  - [Triggering](#triggering)
    - [Partial chain execution](#partial-chain-execution)
    - [Server-triggered client flows](#server-triggered-client-flows)
- [Client data](#client-data)
  - [Client storage configuration](#client-storage-configuration)
  - [Querying client data](#querying-client-data)
  - [Client data summary](#client-data-summary)
  - [Deleting client data](#deleting-client-data)

## Overview

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. All operations are served over *URI* path `/admin/v2/`.

The former `/admin/v1/` API is retired: any request to it is answered with `410 Gone` and a `cause` pointing to the same resource under `/admin/v2/` (provisions written for the v1 grammar must be migrated with the [h2agent-migrate](../../README.md#execution-of-h2agent-migrate-utility) tool).

The API is organized in three groups:

**General** mock operations:

* Schemas: define validation schemas used in further provisions to check the incoming and outgoing traffic.
* Vault: shared variables between different provision contexts and flows. Extra feature to solve some situations by other means, and also used for built-in response condition mechanism: [Dynamic response delays](../../README.md#dynamic-response-delays).
* Logging: dynamic logger configuration (update and check).
* General configuration (server).

**Traffic server mock** features:

* Server matching configuration: classification algorithms to split the incoming traffic and access to the final procedure which will be applied.
* Server provision configuration: here we will define the mock behavior regarding the request received, and the transformations done over it to build the final response and evolve, if proceed, to another state for further receptions.
* Server data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

**Traffic client mock** features:

* Client endpoints configuration: remote server addresses configured to be used by client provisions.
* Client provision configuration: here we will define the mock behavior regarding the request sent, and the transformations done over it to build the final request and evolve, if proceed, to another flow for further sendings.
* Client data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

## General operations

### API schemas

`GET /admin/v2/api-schema` lists the administrative resources whose json schema is published, and `GET /admin/v2/api-schema/<resource>` returns that schema (`server-matching`, `server-provision`, `client-endpoint`, `client-provision`, `schema`, `vault`), with its `$id` set to the request URI. These are the schemas the agent validates admin requests against (for example, the transformation grammar of the provisions). The former `GET /admin/v2/<resource>/schema` paths are retired (`410 Gone` with a hint).

Do not confuse them with the [schemas](#schemas) below, which are user schemas registered to validate traffic.

### Schemas

Loads schema(s) (`POST /admin/v2/schema`) for future event validation. Added schemas could be referenced within provision configurations by mean their string identifier.

If you have a `json` schema (from file `schema.json`) and want to build the `h2agent` schema configuration (into file `h2agent_schema.json`), you may perform automations like this *bash script* example:

```bash
$ jq --arg id "theSchemaId" '. | { id: $id, schema: . }' schema.json > h2agent_schema.json
```

Also *python* or any other language could do the job:

```python
>>> schema = {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":True,"properties":{"foo":{"type":"string"}},"required":["foo"]}
>>> print({ "id":"theSchemaId", "schema":schema })
```

Load of a set of schemas through an array object is allowed. So, instead of launching *N* schema loads separately, you could group them:

```json
[
  {
    "id": "myRequestsSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "foo": { "type": "string" }
      },
      "required": [ "foo" ]
    }
  },
  {
    "id": "myResponsesSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "bar": { "type": "number" }
      },
      "required": [ "bar" ]
    }
  }
]
```

A schema set fails with the first failed item, giving a 'pluralized' version of the single load failed response message.

### Vault

Vault (`POST /admin/v2/vault`) can be created dynamically from provisions execution (to be used there in later transformations steps or from any other different provision, due to the global scope), but they also can be loaded through this *REST API* operation. This operation is mainly focused on the use of vaults as constants for the whole execution (although they could be updated or reset from provisions).

Vault are created as string-value, which will be interpreted as numbers or any other data type, depending on the transformation involved.

When querying (`GET /admin/v2/vault`), without the `name` query parameter, the whole list of vaults is returned as a JSON object. With the `name` parameter, the specific variable value is returned as a plain string. A variable used as memory bucket could store even binary data and it may be obtained with this operation.

#### Blocking wait

The endpoint `GET /admin/v2/vault/<key>/wait` blocks until a variable changes, replacing polling loops with a single call. Two modes are supported:

- **Any change** (no `value` parameter): returns when the variable differs from its value at the time the request was received.
- **Specific value** (`value=V`): returns when the variable equals `V`. Returns immediately if already satisfied. Scalars are compared by their text, so the barrier does not depend on how the value was stored: `value=60` matches both `60` and `"60"`, and `value=true` matches `true` and `"true"` (text is case-sensitive). JSON objects and arrays (`value={"a":1}`, URL-encoded) are compared as JSON.

Parameters: `timeoutMs` (default 30000, max 300000). Returns `200` on success, `408` on timeout, `429` if too many concurrent waiters (max 32).

#### Thread safety with worker pool

Individual vault operations (`load`, `loadAtPath`, `tryGet`) are thread-safe. However, **read-modify-write patterns at the provision level are not atomic**. For example, a common pattern to count errors:

```json
{"from": {"source": "math", "expression": "@{MY_COUNTER} + 1"}, "to": {"target": "vault", "key": "MY_COUNTER"}}
```

The variable read (`@{MY_COUNTER}`) and the write (vault key `MY_COUNTER`) are separate operations. When multiple worker threads execute this concurrently, increments can be lost. This only affects shared (global) vault keys accessed from multiple subscribers simultaneously.

Per-subscriber vault keys (e.g., `flow_@{sequence}_status`) are naturally partitioned and safe. For global counters, consider aggregating per-subscriber values after the test completes instead of incrementing a shared counter during execution.

#### Vault concurrency and sharding

The vault is written on the traffic hot path: every `vault` transformation target (from both the client and server roles) takes a write lock on the vault. To keep this scalable when a provision touches **many** vault keys under **high concurrency**, the vault is backed by an **N-way sharded map** (16 shards): each key is routed to a shard by hashing, and a per-key write locks **only that shard**. Writes to independent keys from different threads therefore do not serialize against each other.

Notes:

- The gain applies to **distinct keys** (the common per-subscriber case): contention on the vault drops by roughly `1/N`. A single **shared** hot key still serializes on its one shard (and still has the read-modify-write race described above -- sharding does not make `{"source": "math", "expression": "@{K}+1"}` -> vault key `K` atomic).
- Whole-vault admin operations (`GET /admin/v2/vault`, `DELETE /admin/v2/vault`) lock **all** shards (in a fixed order) to produce a consistent snapshot / clear. This is by design: those are administrative paths, not the traffic hot path.
- This is transparent to provisions and to the REST API; no configuration is needed. It is an internal property of the vault store (`src/model/Map.hpp`, opt-in via the `Shards` template parameter; only the vault uses `> 1`). A native benchmark is provided at `benchmark/native/vault_sharding/`.

### Files

The files operation (`GET /admin/v2/files`) retrieves the whole list of files processed and their current status. For example:

```json
[
  {
    "bytes": 1791,
    "closeDelayUsecs": 1000000,
    "path": "Mozart.txt",
    "state": "closed"
  },
  {
    "bytes": 1770,
    "closeDelayUsecs": 1000000,
    "path": "Beethoven.txt",
    "state": "opened"
  }
]
```

An already managed file could be externally removed or corrupted. In that case, the state "missing" will be shown.

### Logging

The logging level (`GET /admin/v2/logging`) retrieves the current level: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

To change it (`PUT /admin/v2/logging?level=<level>`), provide any of the valid log levels as query parameter. This can be also configured on start as described in the [command line](../../README.md#command-line) section.

### Health

`GET /admin/v2/health` returns `{"status":"healthy"}` with HTTP 200. This endpoint is intended for Kubernetes liveness and readiness probes. It is always available as long as the admin server is running.

### Capabilities

`GET /admin/v2/capabilities` returns diagnostics about this binary: admin API version, program version and the provision transformation grammar it accepts. It is informative only (clients use the API version they were written for):

```json
{ "apiVersion": "v2", "programVersion": "v5.0.0", "grammar": "v2" }
```

### Configuration

The general process configuration (`GET /admin/v2/configuration`) returns:

```json
{
    "longTermFilesCloseDelayUsecs": 1000000,
    "shortTermFilesCloseDelayUsecs": 0,
    "lazyClientConnection": true
}
```

The server configuration (`GET /admin/v2/server/configuration`) returns:

```json
{
    "preReserveRequestBody": true,
    "receiveRequestBody": true
}
```

Request body reception can be disabled (`PUT /admin/v2/server/configuration`). This is useful to optimize the server processing in case that request body content is not actually needed by planned provisions:

* `receiveRequestBody=true`: data received will be processed.
* `receiveRequestBody=false`: data received will be ignored.

The `h2agent` starts with request body reception enabled by default, but you could also disable this through command-line (`--traffic-server-ignore-request-body`).

Also, request body memory pre-reservation could be disabled to be dynamic. This simplifies the model behind (`http2comm` library) disabling the default optimization which minimizes reallocations done when data chunks are processed:

* `preReserveRequestBody=true`: pre reserves memory for the expected request body (with the maximum message size received in a given moment).
* `preReserveRequestBody=false`: allocates memory dynamically during append operations for data chunks processed.

The `h2agent` starts with memory pre reservation enabled by default, but you could also disable this through command-line (`--traffic-server-dynamic-request-body-allocation`).

## Matching algorithms

The server matching configuration (`POST /admin/v2/server-matching`) defines how incoming traffic is classified towards provisions.

> **Client-only processes.** When the process is started without a traffic server (`--traffic-server-port 0`), `POST /admin/v2/server-matching` is rejected with `404 Not Found` (`"server is disabled (process started without a traffic server)"`), for the same reason as server provisions above.

### URI path query parameters

Optional object used to specify the transformation used for traffic classification, of query parameters received in the *URI* path. It contains two fields, a mandatory _filter_ and an optional _separator_:

* *filter*:
  * *Sort*: this is the <u>default behavior</u>, which sorts, if received, query parameters to make provisions predictable for unordered inputs.
  * *PassBy*: if received, query parameters are used to classify without modifying the received *URI* path (query parameters are kept as received).
  * *Ignore*: if received, query parameters are ignored during classification (removed from *URI* path and not taken into account to match provisions). Note that query parameters are stored to be accessible on provision transformations, because this filter is only considered to classify traffic.
* *separator*:
  * *Ampersand*: this is the <u>default behavior</u> (if whole _uriPathQueryParameters_ object is not configured) and consists in split received query parameters keys using *ampersand* (`'&'`) as separator for key-value pairs.
  * *Semicolon*: using *semicolon* (`';'`) as query parameters pairs separator is rare but still applies on older systems.

### Regex configuration (rgx & fmt)

Optional arguments used in `FullMatchingRegexReplace` algorithm.
Regular expressions used by `h2agent` are based on `std::regex` and built with default `ECMAScript` option type from [available](https://en.cppreference.com/w/cpp/regex/syntax_option_type) ones.
Also, `std::regex_replace` and `std::regex_match` algorithms use default format (`ECMAScript` [rules](https://262.ecma-international.org/5.1/#sec-15.5.4.11)) from [available](https://en.cppreference.com/w/cpp/regex/match_flag_type) ones.

### Algorithm selection

The fundamental distinction is between **deterministic lookup** and **sequential search**:

- `FullMatching` / `FullMatchingRegexReplace`: the incoming URI (optionally transformed via `rgx`/`fmt`) becomes a map key. One URI → one key → one provision. Order does not matter. O(1) lookup.
- `RegexMatching`: each provision's `requestUri` is treated as a regex pattern. The incoming URI is tested against them sequentially. First match wins. O(n) search.

When to use each:

- Use `FullMatching` when all URIs are fixed and predictable (e.g. `/api/v1/users`, `/api/v1/orders`).
- Use `FullMatchingRegexReplace` when URIs have variable parts but a single `rgx`/`fmt` transformation can normalize them all into predictable keys (e.g. stripping timestamps, trimming IDs). The key insight is that one global transformation must be enough to classify all traffic.
- Use `RegexMatching` when URIs are too heterogeneous for a single transformation to normalize — a mix of different path structures where each provision needs its own pattern (e.g. `/api/v1/config/...`, `/api/v2/users/.../profile`, `/health`, `/metrics`). In this case, sequential regex matching against individual provision patterns is the only viable approach.

There are three classification algorithms. Two of them classify the traffic by single identification of the provision key (`method`, `uri` and `inState`): `FullMatching` matches directly, and `FullMatchingRegexReplace` matches directly after transformation. The other one, `RegexMatching` is not matching by identification but for regular expression.

In summary:

* `FullMatching`: when <u>all</u> the *method/URIs* are completely <u>predictable</u>.
* `FullMatchingRegexReplace`: when <u>some</u> *URIs* should be <u>transformed</u> to get <u>predictable</u> ones (for example, timestamps trimming, variables in path or query parameters, etc.), and <u>other</u> *URIs* are disjoint with them, but also <u>predictable</u>.
* `RegexMatching`: when we have a <u>mix</u> of <u>unpredictable</u> *URIs* in our test plan.

#### FullMatching

Arguments `rgx` and `fmt` are not used here, so not allowed. The incoming request is fully translated into key without any manipulation, and then searched in internal provision map.

This is the default algorithm. Internal provision is stored in a map indexed with real requests information to compose an aggregated key (normally containing the requests *method* and *URI*, but as future proof, we could add `expected request` fingerprint). Then, when a request is received, the map key is calculated and retrieved directly to be processed.

This algorithm is very good and easy to use for predictable functional tests (as it is accurate), also giving internally better performance for provision selection.

#### FullMatchingRegexReplace

Both `rgx` and `fmt` arguments are required. This algorithm is based in [regex-replace](http://www.cplusplus.com/reference/regex/regex_replace/) transformation. The first one (*rgx*) is the matching regular expression, and the second one (*fmt*) is the format specifier string which defines the transformation. Previous *full matching* algorithm could be simulated here using empty strings for `rgx` and `fmt`, but having obviously a performance degradation due to the filter step.

For example, you could trim an *URI* received in different ways:

`URI` example:

```
uri = "/ctrl/v2/id-555112233/ts-1615562841"
```

* Remove last *timestamp* path part (`/ctrl/v2/id-555112233`):

```
rgx = "(/ctrl/v2/id-[0-9]+)/(ts-[0-9]+)"
fmt = "$1"
```

* Trim last four digits (`/ctrl/v2/id-555112233/ts-161556`):

```
rgx = "(/ctrl/v2/id-[0-9]+/ts-[0-9]+)[0-9]{4}"
fmt = "$1"
```

So, this `regex-replace` algorithm is flexible enough to cover many possibilities (even *tokenize* path query parameters, because <u>the whole received `uri` is processed</u>, including that part). As future proof, other fields could be added, like algorithm flags defined in underlying C++ `regex` standard library used.

Also, `regex-replace` could act as a virtual *full matching* algorithm when the transformation fails (the result will be the original tested key), because it can be used as a <u>fall back to cover non-strictly matched receptions</u>. The limitation here is when those unmatched receptions have variable parts (it is impossible/unpractical to provision all the possibilities). So, this fall back has sense to provision constant reception keys (fixed and predictable *URIs*), and of course, strict provision keys matching the result of `regex-replace` transformation on their reception keys which does not fit the other fall back ones.

#### RegexMatching

Arguments `rgx` and `fmt` are not used here, so not allowed. Provision keys are in this case, regular expressions to match reception keys. As we cannot search the real key in the provision map, we must check the reception sequentially against the list of regular expressions, and this is done assuming the first match as the valid one. So, this identification algorithm relies in the configured provision order to match the receptions and select the first valid occurrence.

This algorithm allows to provision with priority. For example, consider 3 provision operations which are provided sequentially in the following order:

1. `/ctrl/v2/id-55500[0-9]{4}/ts-[0-9]{10}`
2. `/ctrl/v2/id-5551122[0-9]{2}/ts-[0-9]{10}`
3. `/ctrl/v2/id-555112244/ts-[0-9]{10}`

If the `URI` "*/ctrl/v2/id-555112244/ts-1615562841*" is received, the second one is the first positive match and then, selected to mock the provisioned answer. Even being the third one more accurate, this algorithm establish an ordered priority to match the information.

Note: in case of large provisions, this algorithm could be not recommended (sequential iteration through provision keys is slower that map search performed in *full matching* procedures).


## Server provisions

Server provisions (`POST /admin/v2/server-provision`) define the response behavior for incoming requests. This section covers the conceptual aspects of provisioning.

> **Client-only processes.** When the process is started without a traffic server (`--traffic-server-port 0`), it is a pure client and its mode cannot change at runtime. In that case `POST /admin/v2/server-provision` is rejected with `404 Not Found` (`"server is disabled (process started without a traffic server)"`), consistent with `PUT /admin/v2/server-data/configuration`. Use client provisions instead.

### State machine (inState/outState)

We could label a provision specification to take advantage of internal *FSM* (finite state machine) for matched occurrences. When a reception matches a provision specification, the real context is searched internally to get the current state ("**initial**" if missing or empty string provided) and then get the `inState` provision for that value. Then, the specific provision is processed and the new state will get the `outState` provided value. This makes possible to program complex flows which depends on some conditions, not only related to matching keys, but also consequence from [transformation filters](#filters) which could manipulate those states.

These arguments are configured by default with the label "**initial**", used by the system when a reception does not match any internal occurrence (as the internal state is unassigned). This conforms a default rotation for further occurrences because the `outState` is again the next `inState` value. It is important to understand that if there is not at least 1 provision with `inState` = "**initial**" the matched occurrences won't never be processed. Also, if the next state configured (`outState` provisioned or transformed) has not a corresponding `inState` value, the flow will be broken/stopped.

So, "**initial**" is a reserved value which is mandatory to debut any kind of provisioned transaction. Remember that an empty string will be also converted to this special state for both `inState` and `outState` fields, and character `#` is not allowed (check [this](../../docs/developers/AggregatedKeys.md) document for developers).

#### Example

* Provision *X* (match *m*, `inState`="*initial*"): `outState`="*second*", `response` *XX*
* Provision *Y* (match *m*, `inState`="*second*"): `outState`="*initial*", `response` *YY*
* Reception matches *m* and internal context map (server data) is empty: as we assume state "*initial*", we look for this `inState` value for match *m*, which is provision *X*.
* Response *XX* is sent. Internal state will take the provision *X* `outState`, which is "*second*".
* Reception matches *m* and internal context map stores state "*second*", we look for this `inState` value for match *m*, which is provision Y.
* Response *YY* is sent. Internal state will take the provision *Y* `outState`, which is "*initial*".

Further similar matches (*m*), will repeat the cycle again and again.

<u>Important note</u>: match *m* refers to matching key, that is to say: provision `method` and `uri`, but states are linked to real *URIs* received (coincide with match key `uri` for *FullMatching* classification algorithm, but not for others). So, there is a different state machine definition for each specific provision and so, a different current state for each specific events fulfilling such provision (this is much better that limiting the whole mock configuration with a global *FSM*, as for example, some events could fail due to *SUT* bugs and states would evolve different for their corresponding keys). If your mock receives several requests with different *URIs* for an specific test stage name, consider to name their provision states with the same identifier (with the stage name, for example), because different provisions will evolve at the "same time" and those names does not collide because they are different state machines (different matches). This could ease the flow understanding as those requests are received in a known test stage.

#### Special purge state

The keyword '*purge*' is a reserved out-state used to indicate that server data for the current data key (method + URI) must be dropped. It should be configured at the last stage of a scenario to control memory consumption in long-term load tests. Incomplete chains (due to timeouts or SUT failures) retain their full event history for forensics, since purge only fires at the chain's final step.

### Extra fields (description)

Provision objects accept an optional `description` field to annotate the purpose of each entry. This is useful because JSON does not support comments:

```json
{
  "description": "Returns 200 with session token for POST /api/v1/sessions",
  "requestMethod": "POST",
  "requestUri": "/api/v1/sessions",
  "responseCode": 200,
  "responseBody": { "token": "abc123" }
}
```

This field has no effect on matching or processing — it is purely informational. Unknown fields are rejected by the schema to catch typos early (e.g. `responseDelay` instead of `responseDelayMs`). See the [benchmark test profiles](../../benchmark/tests) for real-world examples using `description`.

### Server provision fields

#### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

The modern `HTTP` specification (RFC 9110) states that all general-purpose `HTTP` servers must implement at least the `GET` and `HEAD` methods. The implementation of `HEAD` must be equivalent to `GET` for the same resource, with the sole and crucial difference that the `HEAD` response must never contain a message body (entity). However, the "mock server" mode of this application allows simulating `HEAD` responses that are incorrect or do not follow the specification. In fact, by default, the `HEAD` response corresponding to a `GET` provision configured is not automatically implemented; instead, the user must actively do this when setting up the mock.

#### requestUri

Request *URI* path (percent-encoded) to match depending on the algorithm selected. It includes possible query parameters, depending on matching filters provided for them.

<u>*Empty string is accepted*</u>, and is reserved to configure an optional default provision, something which could be specially useful to define the fall back provision if no matching entry is found. So, you could configure defaults for each method, just putting an empty *request URI* or omitting this optional field. Default provisions could evolve through states (in/out) but at least "initial" is again mandatory to be processed.

#### requestSchemaId

We could optionally validate requests against a `json` schema. Schemas are identified by string name and configured through [command line](../../README.md#command-line) or REST API. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### responseHeaders

Header fields for the response. For example:

```json
"responseHeaders":
{
  "content-type": "application/json"
}
```

#### responseCode

Response status code. It can also be used to transport `RST_STREAM` and `GOAWAY` [error codes](https://datatracker.ietf.org/doc/html/rfc7540#section-7):

| Hex value | Name | Description |
|-----------|------|-------------|
| 0x0 | NO_ERROR | Normal graceful close GOAWAY |
| 0x1 | PROTOCOL_ERROR | Protocol error at frame level |
| 0x2 | INTERNAL_ERROR | Internal error |
| 0x3 | FLOW_CONTROL_ERROR | Flow control error |
| 0x7 | REFUSED_STREAM | Frame rejected before processing |
| 0x8 | CANCEL | Stream cancelled by application |
| 0xB | STREAM_CLOSED | Reception of frame already closed |

Those values are also defined in `nghttp2_error_code` enum type within `https://nghttp2.org/documentation/nghttp2.h.html`.

Any value lesser than 100 will cancel the ongoing stream with the corresponding error value. For values greater or equal than 100, it will be interpreted as `HTTP Status Codes`.

Error codes are registered as vaults with key `'__core:stream-error-traffic-server:<recvseq>-<method>-<uri>'` and value `'<error code>'`.

#### responseBody

Response body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

#### responseDelayMs

Optional response delay simulation in milliseconds.

#### responseSchemaId

We could optionally validate built responses against a `json` schema. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

### Variable types: `var` vs `vault`

There are two variable stores available in transformations. Both hold native `json` values (string, number, boolean, null, object, array) with the type the source (or a `cast` filter) yields, and both navigate them with a json pointer `path`; they differ in scope:

| Aspect | `var` (local) | `vault` (global) |
|--------|---------------|----------------------|
| **Scope** | Per-provision `outState` chain. Destroyed when the chain ends. | Process-level. Persists until explicitly erased or deleted via REST API. |
| **Storage type** | Any `json` type | Any `json` type |
| **Name/key** | `name`, may contain dots (e.g. `captures.1`) | `key`, dots forbidden |
| **Path navigation** | `{"source": "var", "name": "NAME", "path": "/path/to/field"}` (json pointer) | `{"source": "vault", "key": "KEY", "path": "/path/to/field"}` (json pointer) |
| **REST API** | None (internal only) | Full CRUD: `GET`/`POST`/`DELETE` on `/admin/v2/vault` |
| **CLI loading** | Not supported | `--vault file.json` |
| **Blocking wait** | Not supported | `GET /admin/v2/vault/<key>/wait` |

#### regexCapture behavior difference

When a `regexCapture` filter is applied, the way capture groups are stored depends on the target variable type:

**`var` (local)** — creates separate flat variables with dotted suffixes:

```
to: {"target": "var", "name": "uri_parts"}
-> uri_parts   = "/app/v1/foo/bar/1"   (full match)
-> uri_parts.1 = "foo"                 (group 1)
-> uri_parts.2 = "1"                   (group 2)

Access: {"source": "var", "name": "uri_parts.1"} -> "foo"   (the group is a variable of its own)
```

> **Gotcha — dotted capture variables in a vault key.** A capture-group variable
> like `uri_parts.1` is a perfectly valid variable, and `@{uri_parts.1}` substitutes
> fine in a **value** or a `var` name. BUT a vault `key` cannot contain dots, and the
> provision schema rejects it even inside a variable pattern, e.g.
> `"key": "RCVD_@{uri_parts.1}"`. Either capture into a vault and use the pointer form
> (`"key": "RCVD_@{uri_parts/1}"`), or copy the group to a dot-free variable first and
> use that in the key:
>
> ```
> {"from": {"source": "var", "name": "uri_parts.1"}, "to": {"target": "var", "name": "group1"}},
> {"from": {"source": "literal", "value": "0"}, "to": {"target": "vault", "key": "RCVD_@{group1}"}}
> ```

**`vault` (global)** — stores a single JSON object with numbered keys:

```
to: {"target": "vault", "key": "uri_parts"}
-> uri_parts = {"0": "/app/v1/foo/bar/1", "1": "foo", "2": "1"}

Access: {"source": "vault", "key": "uri_parts", "path": "/1"} -> "foo"   (json pointer path navigation)
```

#### Value types

A value keeps its type when it is copied between variables and vaults (no serialization): a request body object stored in a `var` is written as an object by a `{"op": "cast", "type": "object"}` body target, and its nodes can be read with `path`. A number (`recvseq`, `math`, `timestamp`, `random`, `sum`...) is stored as a number, and a `literal` as a string. When a value is substituted in a text (`@{name}`), strings are inserted as they are and any other value as its compact json text (`60`, `true`, `{"a":1}`).

> **Changed in 5.0.0:** a `var` held strings only, and a vault stored numbers and booleans as strings. Values now keep their type, which affects: a `var` object written to a body (now an object, formerly its json text), the vault values listed by the REST API (`2` instead of `"2"`), and the `conditionVar` filter (a numeric `0` or a boolean `false` is now a false condition, formerly a non-empty, true one). Add `{"op": "cast", "type": "string"}` before the store target to keep a string.

Note that a `path` is a json pointer (`"path": "/1"`), while a dotted `var` name is a plain variable name (`"name": "uri_parts.1"`). Paths work the same way for `requestBody` and `responseBody`.

If the vault already existed (regardless of its previous type — string, number, or object), the `regexCapture` result **fully replaces** it. To write captures into a subtree of an existing object, specify a path in the target: `{"target": "vault", "key": "MY_OBJ", "path": "/captures"}` will only replace the `/captures` field, preserving the rest of the object.

### Provision references: `#{key}`

A provision may reference vault entries that are resolved **once, when the provision is loaded** (`POST /admin/v2/server-provision`, `POST /admin/v2/client-provision`, or the startup `--traffic-server-provision` / `--traffic-client-provision` files, which are loaded after `--vault`):

* `#{key}` is replaced by the vault entry, and `#{key/<json pointer>}` by a node of it (the first `/` starts the pointer, as in `@{}`). Strings are inserted as they are, any other value as its compact json text.
* Every string value of the provision is resolved (`requestUri`, `responseBody`, headers, transformation fields...); object keys are not.
* A reference which does not exist at load time (key or pointer) is **kept literally**, as an unknown `@{}` variable is, and reported as a warning (in the logs and in the `warning` field of the `POST` response).
* References are consumed: the provision stored (and returned by `GET`) holds the resolved values, not the references. They are never dynamic: a later vault change is not observed until the provision is loaded again.

Typical use is sharing constant values among many provisions, specially regular expressions, which are compiled at load and cannot hold runtime `@{}` variables:

```bash
$ curl --http2-prior-knowledge -XPOST http://localhost:8074/admin/v2/vault -H'content-type:application/json' \
       -d'{"patterns": {"imsi": "([0-9]{15})", "msisdn": "(34[0-9]{9})"}}'
```

```json
{"from": {"source": "requestUriPath"}, "filter": [{"op": "regexCapture", "rgx": "/subscribers/imsi-#{patterns/imsi}"}], "to": {"target": "var", "name": "imsi"}}
```

Compared with `@{}`: `@{name}` is a **runtime** substitution (variables and vaults, every event, the provision keeps the template), while `#{key}` is a **load-time** one (vaults only, once). Both can coexist in a provision. There is no escape syntax: a `#{...}` (or `@{...}`) text which does not match any vault entry (or variable) is left as it is, and the resolution is a single pass, so a resolved text is never scanned again.

### Boolean criterion

Every place where the grammar takes a decision on a value uses the same criterion:

| Value | Result |
|---|---|
| `null`, `false`, `0` (any numeric zero: `0`, `0.0`), `""` (empty string) | **false** |
| any non-empty string (including `"0"`, `"false"`, `" "`), any non-zero number, `true`, any object or array (including `{}` and `[]`) | **true** |

Note that `"0"` and `"false"` are texts, so they are true: only their numeric (`0`) and boolean (`false`) values are false. Values keep their json type in variables and vaults (see [value types](#value-types)), so a `recvseq`, a `math` result or a body node `false` are evaluated with their type, while a `literal`, a header or a uri part are always texts.

How each context applies it, and what happens with an absent value (unknown variable, source or pointer which does not resolve):

| Context | True | False | Absent |
|---|---|---|---|
| `conditionVar` filter (`{"op": "conditionVar", "name": "<name>[/<pointer>]"}`) | the filter passes | the filter fails: target untouched, `onFilterFail` runs | false (the filter fails) |
| `conditionVar` filter with `"not": true` | the filter fails | the filter passes | true (the filter passes) |
| `stop` target (server, client request and client response transformations) | the transformation list is interrupted | ignored, the list goes on | the rule is skipped (the list goes on) |
| `clientProvision` target (server transformations) | the client provision is triggered | not triggered | the rule is skipped (not triggered) |
| `{"op": "cast", "type": "boolean"}` filter | `true` | `false` (`null`, objects and arrays fail the cast) | the rule is skipped |

The `eraser` source provides no value, so it cannot feed `stop` nor `clientProvision` (the schema rejects it). The validation filters `jsonConstraint` and `schemaId` produce `"1"` on success (true) and, on a variable target, an empty string (false) plus the `<name>.fail` report, so their result can drive any of the contexts above.

> **Changed in 5.0.0:** v1 tested the text of the value (non-empty is true) and stored numbers and booleans as texts. A numeric `0` or a boolean `false` is now false; texts behave as before.

### Transformation pipeline

Sorted list of transformation items to modify incoming information and build the dynamic response to be sent.

Each transformation has three stages, written in this order:

```json
{"from": {"source": "<kind>", ...}, "filter": [{"op": "<op>", ...}], "to": {"target": "<kind>", ...}}
```

`from` captures a value (the **source**), the optional `filter` list processes it, and `to` writes it (the **target**). `source`, `target` and `op` select the kind; every other argument is a named field (`path`, `name`, `key`, `value`...), and a missing `path` means the whole document. <u>The filters are applied over sources and sent to targets</u>. An optional `onFilterFail` list holds the transformations executed when a filter fails. The exact mapping from the former (v1) grammar is documented in [GrammarV2.md](../developers/GrammarV2.md), every change from 4.x with before/after examples in the [migration guide](../MigrationGuide.md), and the `h2agent-migrate` tool converts v1 provisions automatically.

The value reaching a json body target (`responseBody`, or `requestBody` in client provisions) keeps the type the source yields; a final `{"op": "cast", "type": "string|integer|unsigned|float|boolean|object|json"}` filter converts it (`json` parses a json text). The cast may appear at any position of the filter list and with any target (see [filters](#filter-chains)).

A best effort is done to transform and convert information to final target vaults, and when something wrong happens, a logging error is thrown and the transformation filter is skipped going to the next one to be processed. For example, a source detected as *json* object cannot be assigned to a number or string target, but could be set into another *json* object.

Let's start describing the available sources of data: regardless the native or normal representation for every kind of target, the fact is that conversions may be done to almost every other type:

- *string* to *number* and *boolean* (true if non empty).

- *number* to *string* and *boolean* (true if different than zero).

- *boolean*: there is no source for boolean type, but you could create a non-empty string or non-zeroed number to represent *true* on a boolean target (the `{"op": "cast", "type": "boolean"}` filter follows the [boolean criterion](#boolean-criterion) for strings and numbers; `null`, objects and arrays cannot be cast to boolean) (only response body nodes could include a boolean).

- *json object*: request body node (whole document is indeed the root node) when being an object itself (note that it may be also a number or string). This data type can only be transfered into targets which support json objects like response body json node.



*Variables substitution:*

Before describing sources and targets (and filters), just to clarify that in some situations it is allowed the insertion of variables in the form `@{var id}` which will be replaced if exist, by scoped provision variables and vaults. In that case we will add the comment "**admits variables substitution**". At certain sources and targets, substitutions are not allowed because have no sense or they are rarely needed:

> **Important**: the `@{name}` pattern uses the bare variable name or vault key. For example, a value stored via `"to": {"target": "vault", "key": "mySeq"}` is referenced as `@{mySeq}`.
>
> A json pointer may follow the name to navigate into the stored value: `@{name/<pointer>}` (the first `/` starts the pointer). For example `@{doc/user/id}` for a stored object, `@{list/0}` for an array item, or `@{uri_parts/1}` for a capture group held in a vault. Local variables are searched before vaults, strings are inserted as they are and any other value as its compact json text; an unknown name or pointer is kept literally (`@{name}` stays in the text, as in v1). The same reference is accepted by the `conditionVar` filter (`"name": "doc/enabled"`).



The **source** of information is classified after parsing the following possible expressions:

- `{"source": "requestUri"}`: whole `url-decoded` and **normalized** request *URI* (path together with possible query parameters **sorted**). Not necessarily the same as the classification *URI* (which could ignore those query parameters, or even pass them by from *URI* with no order guaranteed) and in the same way, not necessarily the same as the original *URI* due to the same reason: query parameters order. Normalization makes the source more predictable, something useful to extract specific *URI* parts. For example, consider the *URI* `/composer?city=Bonn&author=Beethoven`, which normalized would turn into `/composer?author=Beethoven&city=Bonn` (because query parameters are sorted). Assuming that source format, you may use the regular expression `(/composer\?author=)([a-zA-Z]*)(&city=)([a-zA-Z]*)`, to extract the author name or city from that predictable normalized *URI* (second or fourth capture group) . This kind of transformation is very usual regardless if query parameters are processed or not.

- `{"source": "requestUriPath"}`: `url-decoded` request *URI* path part.

- `{"source": "requestUriParam", "name": "<name>"}`: request URI specific parameter `<name>`.

- `{"source": "requestBody"}`: request body received. Should be interpreted depending on the request content type. In case of `json`, it will be the document from *root*. In case of `multipart` reception, a proprietary `json` structure is built to ease accessibility, for example:

  ```json
  {
    "multipart.1": {
      "content": {
        "foo": "bar"
      },
      "headers": {
        "Content-Type": "application/json"
      }
    },
    "multipart.2": {
      "content": "0x268aff26",
      "headers": {
        "Content-Type": "application/octet-stream"
      }
    }
  }
  ```

  So, every part is labeled as `multipart.<number>` with nested `content` and `headers` (the content representation depends again on the content type received in the nested headers field). The drawback for `multipart` reception is that we cannot access the original raw data through this `requestBody` source because it is transformed into `json` nature as an usability assumption. Anyway, proprietary structure is more useful and probable to be needed, so future proof for raw access is less priority.

- `{"source": "requestBody", "path": "/<node1>/../<nodeN>"}`: request body node `json` path. This source path **admits variables substitution**. Leading slash is needed as first node is considered the `json` Also, `multipart` content can be accessed to retrieve any of the nested parts in the proprietary `json` representation commented above.

- `{"source": "responseBody"}`: response body as template. Should be interpreted depending on the response content type. The use of provisioned response as template reference is rare but could ease the build of structures for further transformations, In case of `json` it will be the document from *root*.

  As the transformation steps modify this data container, its value as a source is likewise updated.

- `{"source": "responseBody", "path": "/<node1>/../<nodeN>"}`: response body node `json` path. This source path **admits variables substitution**. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations. Consider using vaults with `json` values (`{"source": "vault", "key": "<key>"}`) as a cleaner alternative for shared templates, since they decouple the template data from the response being built.

  As the transformation steps modify this data container, its value as a source is likewise updated.

- `{"source": "requestHeader", "name": "<hname>"}`: request header component (i.e. *content-type*). Take into account that header fields values are received [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- `{"source": "requestHeaders"}`: all request headers as a JSON array of `{"name": "<key>", "value": "<val>"}` objects. Useful with `jsonConstraint` filter to validate a set of headers. An array is used instead of an object because HTTP headers may have duplicate keys.

- `{"source": "responseHeaders"}`: all response headers as a JSON array (same format as `requestHeaders`). Only available in client provision `onResponseTransform`.

- `{"source": "eraser"}`: provides no value; it **removes what lives at the location given by the target**:

  | Target | Effect |
  |---|---|
  | `{"target": "var", "name": ...}` / `{"target": "vault", "key": ...}` | removes the entry; with `path`, only that node of the stored value |
  | `{"target": "responseBody"}` (`requestBody` in client provisions), with or without `{"op": "cast", "type": "object"}` | removes the node at `path` (object key or array item), or clears the whole body without `path` |
  | same body target with `{"op": "cast", "type": "string"}` | sets an empty string at `path` (v1 erase flavour of string targets) |
  | `{"target": "responseBodyText"}`, `{"target": "outState"}` | empty string |
  | `{"target": "txtFile", "file": ...}` / `{"target": "binFile", "file": ...}` | empties the file |
  | `{"target": "serverEvent", <event fields>}` / `{"target": "clientEvent", <event fields>}` | purges the stored events at that address (events are read-only for transformations, so erasing them is the only write they accept, and `eraser` is the only source those targets accept) |

  A missing node, entry or event is a no-op (no null node is created). Any other target ignores the eraser, and `stop` / `clientProvision` reject it (they act on a true value, see [boolean criterion](#boolean-criterion)).

  **Conditional erase**: the only filters accepted with `eraser` are `conditionVar` (one or several, optionally negated with `"not": true`): the target is erased only when every condition holds, otherwise it is kept and `onFilterFail` runs. For example, purge an event once a flow is done:

  ```json
  {"from": {"source": "eraser"}, "filter": [{"op": "conditionVar", "name": "done"}], "to": {"target": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/item/1"}}
  ```

  Typical uses: remove the vaults of a flow after its last step (vaults are process-wide, so this avoids memory growth in load tests), purge events to control the storage growth, or remove auxiliary nodes from a response body used as a template (although storing templates in vaults and reading them with `{"source": "vault", "key": "<key>", "path": "<path>"}` is recommended: it does not pollute the response body).

- `{"source": "math", "expression": "<expression>"}`: this source is based in [Arash Partow's exprtk](https://github.com/ArashPartow/exprtk) math library compilation. There are many possibilities (calculus, control and logical expressions, trigonometry, logic, string processing, etc.), so check [here](https://github.com/ArashPartow/exprtk/blob/master/readme.txt) for more information. This source specification **admits variables substitution** (third-party library variable substitutions are not needed, so they are not supported). Some simple examples could be: "2*sqrt(2)", "sin(3.141592/2)", "max(16,25)", "1 and 1", etc. You may implement a simple arithmetic server (check [this](./kata/09.Arithmetic_Server/README.md) kata exercise to deepen the topic).

- `{"source": "random", "min": <min>, "max": <max>}`: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.

- `{"source": "randomset", "values": ["<value1>", .., "<valueN>"]}`: random string value between pipe-separated labels provided. This source specification **admits variables substitution**. Note that both leading and trailing pipes would add empty parts (`'|foo|bar'`, `'foo|bar|'` and `'foo||bar'` become three parts, `'foo'`, `'bar'` and empty string).

- `{"source": "timestamp", "unit": "<unit>"}`: UNIX epoch time in `s` (seconds), `ms` (milliseconds), `us` (microseconds) or `ns` (nanoseconds).

- `{"source": "strftime", "fmt": "<format>"}`: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/). This source format **admits variables substitution**.

- `{"source": "recvseq"}`: sequence id number increased for every mock reception (starts on *1* when the *h2agent* is started).

- `{"source": "var", "name": "<id>"[, "path": "/<path>"]}`: general purpose variable (readable at transformation chain, scoped to the `outState` chain). Values can be strings, numbers, booleans, arrays or `json` objects; a `path` (json pointer) navigates into the stored value. This source variable identifier **admits variables substitution**. Variables set in one provision are automatically available in the next provision linked via `outState`, and destroyed when the chain ends.

- `{"source": "vault", "key": "<key>"[, "path": "/<path>"]}`: general purpose vault (readable from anywhere, process-level scope). Values can be strings, numbers, booleans, arrays or `json` objects. When a `path` is provided (json pointer format), the source navigates into the stored `json` value (e.g. `{"source": "vault", "key": "TPL", "path": "/request/headers/auth"}` extracts the `auth` field). Without path, the whole value is used. For string values, the behavior is backward compatible with the former string-only storage. This source variable identifier **admits variables substitution** (on the key part). Vault are useful to store dynamic information to be used in a different provision instance. For example you could split a request `URI` in the form `/update/<id>/<timestamp>` and store a variable with the name `<id>` and value `<timestamp>`. That variable could be queried later just providing `<id>` which is probably enough in such context. Thus, we could parse other provisions (access to events addressed with dynamic elements), simulate advanced behaviors, or just parse mock invariant globals over configured provisions (although this seems to be less efficient than hard-coding them, it is true that it drives provisions adaptation "on the fly" if you update such globals when needed). **Note:** key names cannot contain dots.

- `{"source": "literal", "value": "<value>"}`: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string, see [boolean criterion](#boolean-criterion)), will be converted to the target type. Empty value is allowed, for example, to set an empty string, just type: `"value."`. This source value **admits variables substitution**. Also, special characters are allowed ('\n', '\t', etc.).

- `{"source": "serverEvent", <event fields>}`: access server context indexed by request *method* (`requestMethod`), *URI* (`requestUri`), events *number* (`eventNumber`) and events number *path* (`eventPath`), where event fields are:

  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list, as a json integer (or a text holding a variable, e.g. `"@{n}"`). Mandatory unless `recvseq` is provided.
  - *eventPath*: `json` document path within selection. Optional.
  - *recvseq*: receive sequence identifier for stable event addressing. Optional: when provided, `eventNumber` is ignored and the specific event matching this sequence is accessed.
  - *requestUriStartsWith*: **literal URI prefix** used instead of the exact `requestUri`. Optional, and mutually exclusive with `requestUri` (when present, it takes precedence over `requestUri`/`recvseq`). Instead of addressing one key by its exact *URI*, it selects **all** stored keys for the given `requestMethod` whose *URI* starts with this prefix, merges their events into a single list ordered by reception timestamp, and lets `eventNumber` pick within that merged set (so `"eventNumber": -1` means "the most recent event whose *URI* starts with this prefix, whatever its unknown trailing part is"). The match is a **plain string comparison**, not a regular expression: characters like `|`, `.`, `{` or `[` are compared verbatim (there is nothing to escape). This is designed for the case where the unknown part of the *URI* is a **trailing suffix** (e.g. a server-generated identifier appended to a known path); an unknown part in the middle is not covered by prefix mode.

  > **Concurrency note**: `eventNumber` is a positional index into the events list. It is stable when each key (method + URI) is owned by a single flow (e.g. URIs containing unique identifiers like `/resources/{id}`). However, when multiple concurrent flows share the same key (e.g. a fixed URI like `/resources`), positions may shift unpredictably as events are inserted or deleted by other flows, making positional addressing unreliable in that scenario. Use `recvseq` for reliable event access under concurrency.

  Event addressing will retrieve a `json` object corresponding to a single event (given by `requestMethod`, `requestUri` and `eventNumber` or `recvseq`) and optionally a node within that event object (given by `eventPath` to narrow the selection).

  For example, `{"source": "serverEvent", "requestMethod": "GET", "requestUri": "/foo/bar", "eventNumber": 3, "eventPath": "/requestHeaders"}` searches the third (event number 3) `GET /foo/bar` request and `/requestHeaders` path, as part of event definition, gives the request headers that was received. The particular case of empty event path extracts the whole event structure, and in general, paths are [json pointers](https://tools.ietf.org/html/rfc6901), which are powerful enough to cover addressing needs.

  Field values are plain json strings, so the request *URI* is written as it is, even when it holds its own query parameters (no URL-encoding is needed): for the request `/app/v1/stock/madrid?loc=123&id=2`, the source is:

  ```json
  {"source": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/stock/madrid?loc=123&id=2", "eventNumber": -1, "eventPath": "/requestBody"}
  ```

  Every field **admits variables substitution**, so the address can also be built from previous transformations, e.g. `"requestUri": "@{requestUri}"`.

  **Addressing by URI prefix (`requestUriStartsWith`)**: sometimes the mock does not know the full *URI* in advance because the system under test appends a value at request time (a generated identifier, a creation timestamp, a correlation token) to a known path. In that case the exact `requestUri` cannot be built. Prefix mode solves it: provide the stable leading part of the *URI* and let `eventNumber` select across every matching key.

  For example, a client first creates a resource at a *URI* ending with a server-generated id, and later asks the mock to echo back the body of the most recently created resource. The mock cannot know the id beforehand, but it knows the path prefix `/app/v1/resources/`:

  ```json
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/last-created",
    "responseCode": 200,
    "transform": [
      {
        "from": {
          "source": "serverEvent",
          "requestMethod": "POST",
          "requestUriStartsWith": "/app/v1/resources/",
          "eventNumber": -1,
          "eventPath": "/requestBody"
        },
        "filter": [{"op": "cast", "type": "object"}],
        "to": {"target": "responseBody"}
      }
    ]
  }
  ```

  This returns the request body of the latest `POST` whose *URI* starts with `/app/v1/resources/` (e.g. `/app/v1/resources/8a1f`, `/app/v1/resources/42`, ...), regardless of the unknown trailing id. Note that the comparison is literal (not a regular expression), so no metacharacter escaping is involved; and that prefix mode only addresses an unknown **trailing** part of the *URI*.

  <u>Server events history</u> should be kept enabled allowing to access events. So, imagine the following current server data map:

  ```json
  [
    {
      "method": "POST",
      "events": [
        {
          "requestBody": {
            "engine": "tdi",
            "model": "audi",
            "year": 2021
          },
          "requestHeaders": {
            "accept": "*/*",
            "content-length": "52",
            "content-type": "application/x-www-form-urlencoded",
            "user-agent": "curl/7.77.0"
          },
          "previousState": "initial",
          "receptionTimestampUs": 1626039610709978,
          "responseDelayMs": 0,
          "responseStatusCode": 201,
          "recvseq": 116,
          "state": "initial"
        }
      ],
      "uri": "/app/v1/stock/madrid?loc=123&id=2"
    }
  ]
  ```

  Then, the source commented above would store this `json` object, which is the request body for the last (`"eventNumber": -1`) event registered:

  ```json
  {
    "engine": "tdi",
    "model": "audi",
    "year": 2021
  }
  ```

- `{"source": "inState"}`: current processing state.

- `{"source": "clientEvent", <event fields>}`: access client context indexed by client endpoint identifier (`clientEndpointId`), request *method* (`requestMethod`), *URI* (`requestUri`), events *number* (`eventNumber`) and events number *path* (`eventPath`), where event fields are:

  - *clientEndpointId*: client endpoint identifier. Mandatory.
  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list, as a json integer (or a text holding a variable). Mandatory unless `sendseq` is provided.
  - *eventPath*: `json` document path within selection. Optional.
  - *sendseq*: send sequence identifier for stable event addressing. Optional: when provided, `eventNumber` is ignored and the specific event matching this sequence is accessed.
  - *requestUriStartsWith*: **literal URI prefix** used instead of the exact `requestUri` (same semantics as described for `serverEvent`): it selects all client events for the given `clientEndpointId` and `requestMethod` whose *URI* starts with this prefix, merges them ordered by sending timestamp, and `eventNumber` picks within the merged set. Optional and mutually exclusive with `requestUri`. Plain string comparison, not a regular expression.

  The same concurrency considerations as `serverEvent` apply. Use `sendseq` for reliable event access under concurrency.

  Event addressing will retrieve a `json` object corresponding to a single client event (given by `clientEndpointId`, `requestMethod`, `requestUri` and `eventNumber` or `sendseq`) and optionally a node within that event object (given by `eventPath` to narrow the selection).

  For example, `{"source": "clientEvent", "clientEndpointId": "myBackend", "requestMethod": "GET", "requestUri": "/api/v1/data", "eventNumber": -1, "eventPath": "/responseBody"}` searches the last client event for `GET /api/v1/data` sent through the `myBackend` endpoint, and `/responseBody` path gives the response body that was received.

  Every field **admits variables substitution**, as for `serverEvent`.

  <u>Client events history</u> should be kept enabled allowing to access events. This source is available in both server and client provision transformations, enabling server provisions to read data from previous client interactions (e.g., to build responses based on data fetched from a backend).

- `{"source": "txtFile", "file": "<path>"}`: reads text content from file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing files will fail to open. This source enables the `h2agent` capability to serve files.

- `{"source": "binFile", "file": "<path>"}`: same as `txtFile` but reading binary data.

- `{"source": "command", "command": "<command>"}`: executes command on process shell and captures the standard output/error ([popen](https://man7.org/linux/man-pages/man3/popen.3.html)() is used behind). Also, the return code is saved into scoped variable `rc`. You may call external scripts or executables, and do whatever needed as if you would be using the shell environment.

  - Important notes:
    - **Be aware about security problems**, as you could provision via `REST API` any instruction accessible by a running `h2agent` to extract information or break things without interface restriction (remember anyway that `h2agent` supports [secured connection](#Execution-with-TLS-support)).
    - **This operation could impact performance** as external procedures will block the working thread during execution (it is different than response delays which are managed asynchronously), so perhaps you should increase the number of working threads (check [command line](#Command-line)). This operation is mainly designed to run administrative procedures within the testing flow, but not as part of regular provisions to define mock behavior. So, having an additional working thread (`--traffic-server-worker-threads 2`) should be enough to handle dedicated `URIs` for that kind of work reserving another thread for normal traffic.

  - Examples:
    - `/any/procedure 2>&1`: `stderr` is also captured together with standard output (if not, the `h2agent` process will show the error message in console).
    - `ls /the/file 2>/dev/null || /bin/true`: always success (`rc` stores 0) even if file is missing. Path captured when the file path exists.
    - `/opt/tools/checkCondition &>/dev/null && echo fulfilled`: prepare transformation to capture non-empty content ("fulfilled") when condition is successful.
    - `/path/to/getJpg >/var/log/image.jpg 2>/var/log/getJpg.err`: arbitrary procedure executed and standard output/error dumped into files which can be read in later step by mean `binFile`/`txtFile` sources.
    - Shell commands accessible on environment path: security considerations are important but this functionality is worth it as it even allows us to simulate exceptional conditions within our test system. For example, we could provision a special `uri` to provoke the mock server crash using command source: `pkill -SIGSEGV h2agent` (suicide command).




The **target** of information is classified after parsing the following possible expressions (between *[square brackets]* we denote the potential data types allowed):

- `{"target": "responseBodyText"}` *[string]*: response body storing expected string processed.

- `{"target": "responseBodyHex"}` *[string]*: response body storing expected string processed from hexadecimal representation, for example `0x8001` (prefix `0x` is optional).

- `{"target": "responseBody"}` + `{"op": "cast", "type": "string"}` *[string]*: response body document storing expected string at *root*.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "integer"}` *[integer]*: response body document storing expected integer at *root*.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "unsigned"}` *[unsigned integer]*: response body document storing expected unsigned integer at *root*.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "float"}` *[float number]*: response body document storing expected float number at *root*.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "boolean"}` *[boolean]*: response body document storing expected boolean at *root*.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "object"}` *[json object]*: response body document storing expected object as *root* node.

- `{"target": "responseBody"}` + `{"op": "cast", "type": "json"}` *[json string]*: response body document storing expected object, extracted from json-parsed string, as *root* node.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "string"}` *[string]*: response body node path storing expected string. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "integer"}` *[integer]*: response body node path storing expected integer. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "unsigned"}` *[unsigned integer]*: response body node path storing expected unsigned integer. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "float"}` *[float number]*: response body node path storing expected float number. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "boolean"}` *[boolean]*: response body node path storing expected booblean. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "object"}` *[json object]*: response body node path storing expected object under provided path. If source origin is not an object, there will be a best effort to convert to string, number, unsigned number, float number and boolean, in this specific priority order. This target path **admits variables substitution**.

- `{"target": "responseBody", "path": "/<node1>/../<nodeN>"}` + `{"op": "cast", "type": "json"}` *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path. This target path **admits variables substitution**.

- `{"target": "responseHeader", "name": "<hname>"}` *[string (or number as string)]*: response header component (i.e. *location*). This target name **admits variables substitution**. Take into account that header fields values are sent [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- `{"target": "responseStatusCode"}` *[unsigned integer]*: response status code.

- `{"target": "responseDelayMs"}` *[unsigned integer]*: simulated delay to respond: although you can configure a fixed value for this property on provision document, this transformation target overrides it.

- `{"target": "var", "name": "<id>"[, "path": "/<path>"]}` *[any json type]*: general purpose variable (writable at transformation chain, scoped to the `outState` chain). The idea of *variable* vaults is to optimize transformations when multiple transfers are going to be done (for example, complex operations like regular expression filters, are dumped to a variable, and then, we drop its value over many targets without having to repeat those complex algorithms again). Variables set here are automatically available in subsequent provisions linked via `outState`. The value keeps the type the source yields; with a `path` (json pointer) only that node is written, preserving the rest of the stored object. This target variable identifier **admits variables substitution**.

- `{"target": "vault", "key": "<key>"[, "path": "/<path>"]}` *[any json type]*: general purpose vault (writable at transformation chain and intended to be used later, as source, from anywhere as process-level scope). Values can be strings, numbers, booleans, arrays or `json` objects. This target variable identifier **admits variables substitution** (on the key part).

  When a `path` is provided (json pointer format), only the specified field within the stored `json` object is modified, preserving the rest of the document (e.g. `{"target": "vault", "key": "TPL", "path": "/status"}` sets only the `status` field). Without path, the entire variable value is replaced.

  When the source is a `json` object (e.g. from `requestBody`), it is stored natively. When the source is a string, it is stored as a `json` string value (backward compatible).

  **regexCapture behavior**: when a `regexCapture` filter is used with a `vault` target, the capture groups are stored as a single `json` object with numbered keys: `{"0": "full match", "1": "group1", "2": "group2", ...}`. This differs from local variables (`var`), where separate variables are created (`name`, `name.1`, `name.2`). With vaults, the captures are accessed via path navigation: `"path": "/1"`, `"path": "/2"`, etc. If the variable already existed (string or object), it is **fully replaced** by the capture object (unless a path is specified in the target, in which case only that subtree is replaced).

  **Note:** key names cannot contain dots. To reset a vault, use the `eraser` source.

- `{"target": "vault", "key": "<key>"[, "path": "/<path>"]}` + `{"op": "cast", "type": "object"}` *[json object]*: stores the source as a native `json` object in the vault. Unlike the plain `vault` target without cast (which attempts object extraction with string fallback), this target **requires** the source to be a valid `json` object — the transform is skipped if it is not. Useful when you want strict typing. The optional `path` works the same as for the plain `vault` target.

- `{"target": "vault", "key": "<key>"[, "path": "/<path>"]}` + `{"op": "cast", "type": "json"}` *[json string → json object]*: parses a JSON-encoded string from the source and stores the resulting native `json` object in the vault. This is the vault equivalent of a `responseBody` target with the `json` cast: it takes a string like `"{\"a\":1,\"b\":2}"` and stores `{"a":1,"b":2}` as a navigable object. The transform is skipped if the string is not valid `json`. The optional `path` stores the parsed object at a sub-path within the vault entry.

  Example use case — extracting fields from an embedded JSON string:

  ```json
  [
    {
      "from": {
        "source": "serverEvent",
        "requestMethod": "POST",
        "requestUri": "/api/v1/accounts",
        "eventNumber": -1,
        "eventPath": "/requestBody/balanceDetails"
      },
      "filter": [{"op": "cast", "type": "json"}],
      "to": {"target": "vault", "key": "balance"}
    },
    {
      "from": {"source": "vault", "key": "balance", "path": "/currentDebt"},
      "filter": [{"op": "cast", "type": "unsigned"}],
      "to": {"target": "responseBody", "path": "/debt"}
    }
  ]
  ```

- `{"target": "outState"}` *[string (or number as string)]*: next processing state. This overrides the default provisioned one.

- `{"target": "outState", "method": "POST|GET|PUT|DELETE|HEAD"[, "uri": "<uri>"]}` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.

  You could, for example, simulate a database where a *DELETE* for an specific entry could infer through its provision an *out-state* for a foreign method like *GET*, so when getting that *URI* you could obtain a *404* (assumed this provision for the new *working-state* = *in-state* = *out-state* = "id-deleted"). By default, the same `uri` is used from the current event to the foreign method, but it could also be provided optionally giving more flexibility to generate virtual events with specific states.

- `{"target": "txtFile", "file": "<path>"}` *[string]*: dumps source (as string) over text file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing directories will fail to open (the process does not create tree hierarchy). It is considered long term file (file is closed 1 second after last write, by default) when a constant path is configured, because this is normally used for specific log files. On the other hand, when any substitution may took place in the path provided (it has variables in the form `@{varname}`) it is considered as a dynamic name, so understood as short term file (file is opened, written and closed without delay, by default). **Note:** you can force short term type inserting a variable, for example with empty value: `txtFile./path/to/short-term-file.txt@{empty}`. Delays in microseconds are configurable on process startup. Check  [command line](#Command-line) for `--long-term-files-close-delay-usecs` and `--short-term-files-close-delay-usecs` options.

  This target can also be used to write named pipes (previously created: `mkfifo /tmp/mypipe && chmod 0666 /tmp/mypipe`), with the following restriction: writes must close the file descriptor everytime, so long/short term delays for close operations must be zero depending on which of them applies: variable paths zeroes the delay by default, but constant ones shall be zeroed too by command-line (`--long-term-files-close-delay-usecs 0`). Just like with regular UNIX pipes (`|`), when the writer closes, the pipe is torn down, so fast operations writting named pipes could provoke data looses (some writes missed). In that case, it is more recommended to use UDP through unix socket target (`udpSocket./tmp/udp.sock`).

- `{"target": "binFile", "file": "<path>"}` *[string]*: same as `txtFile` but writting binary data.

- `{"target": "udpSocket", "socket": "<path>"[, "delayMs": <milliseconds>]}` *[string]*: sends source (as string) via UDP datagram through a unix socket with the path provided, with an optional delay in milliseconds. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. UDP is a transport layer protocol in the TCP/IP suite, which provides a simple, connectionless, and unreliable communication service. It is a lightweight protocol that does not guarantee the delivery or order of data packets. Instead, it allows applications to send individual datagrams (data packets) to other hosts over the network without establishing a connection first. UDP is often used where low latency is crucial. In `h2agent` is useful to signal external applications to do associated tasks sharing specific data for the transactions processed. Use `./tools/udp-server` program to play with it or even better `./tools/udp-server-h2client` to generate HTTP/2 requests UDP-driven (this will be covered when full `h2agent` client capabilities are ready).

- `{"target": "serverEvent", <event fields>}`: this target is always used in conjunction with the `eraser` source acting as an alternative purge method to the purge `outState`. The main difference is that states-driven purge method acts over processed events key (`method` and `uri` for the provision in which the purge state is planned), so not all the test scenarios may be covered with that constraint if they need to remove events registered for different transactions. In this case, event addressing is defined by request *method* (`requestMethod`), *URI* (`requestUri`), and events *number* (`eventNumber`): events number *path* (`eventPath`) is not accepted, as this operation just remove specific events or whole history, like REST API for server-data deletion:

  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Optional: if not provided, all the history may be purged.
  - *recvseq*: receive sequence identifier for stable event addressing. Optional: when provided, `eventNumber` is ignored and the specific event matching this sequence is removed.

  > **Concurrency note**: `eventNumber` is a positional index. When multiple concurrent flows share the same key, positions may shift as events are inserted or deleted by other flows, making positional addressing unreliable in that scenario. Use `recvseq` for reliable event removal under concurrency.

  This target, as its source counterpart, **admits variables substitution**.

- `{"target": "clientEvent", <event fields>}`: analogous to the `serverEvent` purge target above, but for client events. Event addressing is defined by client endpoint identifier (`clientEndpointId`), request *method* (`requestMethod`), *URI* (`requestUri`), and events *number* (`eventNumber`):

  - *clientEndpointId*: client endpoint identifier. Mandatory.
  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Optional: if not provided, all the history may be purged.
  - *sendseq*: send sequence identifier for stable event addressing. Optional: when provided, `eventNumber` is ignored and the specific event matching this sequence is removed.

  The same concurrency considerations as `serverEvent` apply. Use `sendseq` for reliable event removal under concurrency. This target **admits variables substitution**.

- `{"target": "clientProvision", "id": "<clientProvisionId>", "inState": "<inState>"}` *[string]*: triggers a client provision flow (fire-and-forget) from within a server transformation. Both the identifier and the `inState` **admit variables substitution** (e.g. `"id": "@{flowId}", "inState": "@{myState}"`). Multiple `clientProvision` targets can be specified in the same transformation list and all of them will be triggered. Triggers are collected during the transformation pipeline and executed asynchronously after the server response is fully built, so they do not block or delay the server response. This is the mechanism to connect server and client modes: when the server receives a request, it can trigger one or more outgoing client flows as a side effect.

  The source acts as a **conditional gate**: the trigger only fires on a **true** value (see [boolean criterion](#boolean-criterion)). A false or undefined source (e.g. a variable that does not exist) silently skips the trigger. This follows the same convention as the `stop` target and the `conditionVar` filter.

  ```json
  {
    "from": {"source": "literal", "value": "1"},
    "to": {"target": "clientProvision", "id": "myNotificationFlow", "inState": "initial"}
  }
  ```

  Conditional triggering based on a flag variable (no `conditionVar` filter needed):

  ```json
  {
    "from": {"source": "var", "name": "triggerFlow"},
    "to": {"target": "clientProvision", "id": "myNotificationFlow", "inState": "initial"}
  }
  ```

  Here, if the variable `triggerFlow` exists and is true (for example `"yes"` or `1`, but not `0`, `false` or `""`), the trigger fires. If the variable does not exist, the trigger is skipped.

  The triggered client provision must be previously configured (via `POST /admin/v2/client-provision`) along with its associated client endpoint (via `POST /admin/v2/client-endpoint`). If the provision or endpoint is not found, or the endpoint is disabled, an error is logged and the trigger is silently skipped.

- `{"target": "stop"}`: when a **true** value (see [boolean criterion](#boolean-criterion)) is transferred, the transformations list is interrupted. A false value (or undefined source) ignores the action. Note that placing `stop` as the **last** item in a transformation list is illogical — there are no further items to interrupt. A warning is logged at provision time when this is detected.



There are several **filter** methods, but remember that filter node is optional, so you could directly transfer source to target without modification, just omitting filter, for example:

```json
{"from": {"source": "random", "min": 25, "max": 35}, "to": {"target": "responseDelayMs"}}
```

In the case above, *delay* will take the absolute value for the random generated (just in case the user configures a range with possible negative result).

Filters give you the chance to make complex transformations:

<a name="filter-chains"></a>The `filter` list is a **chain**: filters run in list order, each one over the output of the previous one, and the first failing filter stops the chain (the target is not touched and `onFilterFail` runs). For example, extract a number from the uri, add an offset and write it as an integer:

```json
{
  "from": {"source": "requestUriPath"},
  "filter": [
    {"op": "regexReplace", "rgx": "/app/v1/id-([0-9]+)", "fmt": "$1"},
    {"op": "cast", "type": "integer"},
    {"op": "sum", "value": 1000}
  ],
  "to": {"target": "responseBody", "path": "/id"}
}
```

The `cast` filter converts the value to `string`, `integer`, `unsigned`, `float`, `boolean`, `object` (an object or array is required) or `json` (the value text is parsed); a value which cannot be converted fails the filter. Capture groups of `regexCapture`/`regexKey` (`<id>.k` variables, vault group objects) are only published when that filter is the last one of the chain; otherwise the whole match (or the matched key value) goes on to the next filter.



- `regexCapture`: this filter provides a regular expression, including optionally capture groups which will be applied to the source and stored in the target. This filter is designed specially for general purpose variables, because each captured group *k* will be mapped to a new variable named `<id>.k` where `<id>` is the original source variable name. Also, the variable "as is" will store the entire match, same for any other type of target (used together with boolean target it is useful to write the match condition). Let's see some examples:

  ```json
  {
    "from": {"source": "requestUriPath"},
    "filter": [{"op": "regexCapture", "rgx": "/api/v2/id-([0-9]+)/category-([a-z]+)"}],
    "to": {"target": "var", "name": "id_cat"}
  }
  ```

  In this case, if the source received is *"/api/v2/id-28/category-animal"*, then we have 2 captured groups, so, we will have the variables *id_cat.1="28"* and *id_cat.2="animal"*. Also, the specified variable name *"as is"* will store the entire match: *id_cat="/api/v2/id-28/category-animal"*.

  Other example:

  ```json
  {
    "from": {"source": "requestUriPath"},
    "filter": [{"op": "regexCapture", "rgx": "/api/v2/id-[0-9]+/category-([a-z]+)"}],
    "to": {"target": "responseBody", "path": "/category"}
  }
  ```

  In this example, it is not important to notice that we only have 1 captured group (we removed the brackets of the first one from the previous example). This is because the target is a path within the response body, not a variable, so, only the entire match (if proceed) will be transferred. Assuming we receive the same source from previous example, that value will be the entire *URI* path. If we would use a variable as target, such variable would store the same entire match, and also we would have *animal* as `<variable name>.1`.

  If you want to move directly the captured group (`animal`) to a non-variable target, you may use the next filter:



- `regexReplace`: this is similar to the matching algorithm based in regular expressions and replace procedure (even the fact that it *falls back to source information when not matching is done*, something that differs from former `regexCapture` algorithm which builds an empty string when regular expression is not fully matched). We provide `rgx` and `fmt` to transform the source into the target:

  ```json
  {
    "from": {"source": "requestUriPath"},
    "filter": [
      {"op": "regexReplace", "rgx": "(/ctrl/v2/id-[0-9]+/)ts-([0-9]+)", "fmt": "$2"},
      {"op": "cast", "type": "unsigned"}
    ],
    "to": {"target": "responseBody", "path": "/data/timestamp"}
  }
  ```

  For example, if the source received is "*/ctrl/v2/id-555112233/ts-1615562841*", then we will replace/create a node "*data.timestamp*" within the response body, with the value formatted: *1615562841*.

  In this algorithm, the obtained value will be a string.

- `split`: splits the source string into fixed-size groups joined by a separator. The algorithm takes the last `size × count` characters from the source, pads on the left with `filler` if shorter, and optionally strips leading zeros per group when `numeric` is enabled.

  Parameters (all optional):

  | Parameter | Type | Default | Description |
  |-----------|------|---------|-------------|
  | `size` | integer (≥1) | 2 | Characters per group |
  | `count` | integer (≥1) | 4 | Number of groups |
  | `sep` | string | `"."` | Separator between groups |
  | `filler` | string | `"0"` | Left-padding string repeated to reach `size × count` length. Set to `""` to disable padding |
  | `numeric` | boolean | false | Strip leading zeros in each group (via integer conversion) |

  Example — transform a phone number into an IPv4-like address:

  ```json
  {
    "from": {"source": "requestBody", "path": "/phone"},
    "filter": [{"op": "split", "numeric": true}],
    "to": {"target": "var", "name": "ipv4"}
  }
  ```

  With defaults (`size`: 2, `count`: 4, `sep`: `"."`), this takes the last 8 digits, splits into 4 groups of 2, and strips leading zeros. For phone `555112233`, the result is `55.11.22.33`.

  The `filler` parameter controls left-padding when the source is shorter than `size × count`. For example, with source `"42"` and defaults, the padded input becomes `"00000042"`, producing `0.0.0.42`. With `"filler": ""`, no padding is applied and the result depends on the actual source length.

  Note: the `split` approach produces octets in the range 0-99 (2 digits). For realistic
  IPv4 addresses with full 0-255 octets, use math expressions to decompose a 32-bit
  integer into its four bytes:

  ```json
  [
    {
      "from": {"source": "var", "name": "sequence"},
      "filter": [{"op": "sum", "value": 167772161}],
      "to": {"target": "var", "name": "ipnum"}
    },
    {
      "from": {"source": "math", "expression": "floor(@{ipnum} / 16777216)"},
      "to": {"target": "var", "name": "o1"}
    },
    {
      "from": {"source": "math", "expression": "floor((@{ipnum} % 16777216) / 65536)"},
      "to": {"target": "var", "name": "o2"}
    },
    {
      "from": {"source": "math", "expression": "floor((@{ipnum} % 65536) / 256)"},
      "to": {"target": "var", "name": "o3"}
    },
    {
      "from": {"source": "math", "expression": "floor(@{ipnum} % 256)"},
      "to": {"target": "var", "name": "o4"}
    },
    {
      "from": {"source": "literal", "value": "@{o1}.@{o2}.@{o3}.@{o4}"},
      "to": {"target": "requestBody", "path": "/ipv4Address"}
    }
  ]
  ```

  This converts `ipnum` into a dotted-quad by dividing by powers of 256 (2^24, 2^16, 2^8).
  Offset `167772161` = 10.0.0.1 in decimal (10*256^3 + 1), chosen so that IPs start in the
  10.0.0.0/8 private range (RFC 1918). This offset is arbitrary and can be changed to any
  value that produces a valid starting IP. For example:
  - `167772161` -> starts at 10.0.0.1 (class A private)
  - `3232235521` -> starts at 192.168.0.1 (class C private)
  - `1` -> starts at 0.0.0.1 (if range validity is irrelevant for the test)

- `baseConvert`: converts the source string between numeric bases (2–36). Parameters `in` and `out` are required. On conversion error, the source is returned unchanged.

  | Parameter | Type | Required | Default | Description |
  |-----------|------|----------|---------|-------------|
  | `in` | integer (2–36) | yes | — | Input base |
  | `out` | integer (2–36) | yes | — | Output base |
  | `capital` | boolean | no | false | Use uppercase letters for digits above 9 |

  ```json
  {
    "from": {"source": "requestBody", "path": "/id"},
    "filter": [{"op": "baseConvert", "in": 10, "out": 16, "capital": true}],
    "to": {"target": "var", "name": "hex_id"}
  }
  ```

  With source `"255"`, the result is `"FF"`. With `"capital": false` (default), it would be `"ff"`.

- `strptime` (*str-p-time*: string **p**arse time): parses a date/time string into a numeric epoch timestamp. Uses the POSIX `strptime` function with `timegm` (UTC). The result is an integer suitable for arithmetic (e.g. `sum`) and later formatting back with `strftime`.

  | Parameter | Type | Required | Default | Description |
  |-----------|------|----------|---------|-------------|
  | `fmt` | string | yes | — | Format string (POSIX strptime specifiers: `%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, etc.) |
  | `unit` | string | no | `"s"` | Output unit: `s` (seconds), `ms` (milliseconds), `us` (microseconds), `ns` (nanoseconds) |

  ```json
  {
    "from": {"source": "literal", "value": "2026-03-30T16:00:00"},
    "filter": [{"op": "strptime", "fmt": "%Y-%m-%dT%H:%M:%S"}],
    "to": {"target": "var", "name": "epoch"}
  }
  ```

  Stores `1774987200` (epoch seconds for that UTC date) in the variable `epoch`. With `"unit": "ms"`, it would store `1774987200000`.

- `strftime` (*str-f-time*: string **f**ormat time): formats a numeric epoch timestamp into a date/time string. Uses `gmtime_r` + `strftime` (UTC). The source must be numeric (integer epoch in the specified unit).

  | Parameter | Type | Required | Default | Description |
  |-----------|------|----------|---------|-------------|
  | `fmt` | string | yes | — | Format string (POSIX strftime specifiers: `%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, etc.) |
  | `unit` | string | no | `"s"` | Input unit: `s` (seconds), `ms` (milliseconds), `us` (microseconds), `ns` (nanoseconds) |

  ```json
  {
    "from": {"source": "var", "name": "epoch"},
    "filter": [{"op": "strftime", "fmt": "%Y-%m-%dT%H:%M:%S"}, {"op": "cast", "type": "string"}],
    "to": {"target": "responseBody", "path": "/date"}
  }
  ```

  With `epoch = 1774987200`, stores `"2026-03-30T16:00:00"` in the response body.

  **Combined example — current time + 1 hour, formatted:**

  ```json
  [
    {"from": {"source": "timestamp", "unit": "s"}, "to": {"target": "var", "name": "epoch"}},
    {
      "from": {"source": "var", "name": "epoch"},
      "filter": [{"op": "sum", "value": 3600}],
      "to": {"target": "var", "name": "epoch"}
    },
    {
      "from": {"source": "var", "name": "epoch"},
      "filter": [{"op": "strftime", "fmt": "%Y-%m-%dT%H:%M:%SZ"}, {"op": "cast", "type": "string"}],
      "to": {"target": "responseBody", "path": "/expiresAt"}
    }
  ]
  ```

  **Round-trip example — parse, shift, reformat:**

  ```json
  [
    {
      "from": {"source": "requestBody", "path": "/startDate"},
      "filter": [{"op": "strptime", "fmt": "%d/%m/%Y %H:%M"}],
      "to": {"target": "var", "name": "epoch"}
    },
    {
      "from": {"source": "var", "name": "epoch"},
      "filter": [{"op": "sum", "value": 86400}],
      "to": {"target": "var", "name": "epoch"}
    },
    {
      "from": {"source": "var", "name": "epoch"},
      "filter": [{"op": "strftime", "fmt": "%Y-%m-%d"}, {"op": "cast", "type": "string"}],
      "to": {"target": "responseBody", "path": "/nextDay"}
    }
  ]
  ```

  Parses `"30/03/2026 16:00"` from the request, adds 24 hours, and responds with `"2026-03-31"`.

- `regexKey`: searches the keys of a JSON object source and returns the value of the first key that matches the provided regex. The source must be a JSON object — the filter is skipped if it is not. If no key matches, the transform is skipped. This filter **does not admit variables substitution** (`@{name}` patterns are treated as literal regex text) because the regular expression is precompiled at provision time.

  This is useful when JSON keys contain dynamic parts (timestamps, unique IDs, etc.) that cannot be addressed with static json pointers.

  ```json
  {
    "from": {"source": "requestBody", "path": "/accounts"},
    "filter": [{"op": "regexKey", "rgx": "acct-([0-9]{10})-checking"}],
    "to": {"target": "vault", "key": "matchedAccount"}
  }
  ```

  Given a source object like `{"acct-1774945487-checking": {"balance": 1500, "currency": "EUR"}, "acct-1774945487-savings": {"balance": 30000, "currency": "EUR"}}`, the filter matches the key `acct-1774945487-checking` by regex and stores its value in the vault `matchedAccount`. You can then navigate it with json pointers:

  ```json
  {"from": {"source": "vault", "key": "matchedAccount", "path": "/balance"}, "filter": [{"op": "cast", "type": "integer"}], "to": {"target": "responseBody", "path": "/balance"}}
  ```

  **Capture groups**: when the regex contains capture groups, the matched key and its groups are stored in variables following the same `.N` convention as `regexCapture`:

  ```
  -> vault matchedAccount  = {"balance": 1500, "currency": "EUR"}   (the value)
  -> var matchedAccount.0  = "acct-1774945487-checking"              (matched key)
  -> var matchedAccount.1  = "1774945487"                            (group 1)
  ```

  This works regardless of whether the target is `vault` or `var`. When the target is a `var`, the value is stored as serialized JSON in the bare target variable, and the key capture groups go to `.0`, `.1`, etc.

  Another example — extracting from a response where order IDs contain timestamps:

  ```json
  [
    {
      "from": {"source": "responseBody", "path": "/orders"},
      "filter": [{"op": "regexKey", "rgx": "order-(2026[0-9]{4})-.*-(express)"}],
      "to": {"target": "vault", "key": "order"}
    },
    {
      "from": {"source": "vault", "key": "order", "path": "/status"},
      "to": {"target": "var", "name": "orderStatus"}
    },
    {
      "from": {"source": "var", "name": "order.1"},
      "filter": [{"op": "cast", "type": "string"}],
      "to": {"target": "responseBody", "path": "/orderDate"}
    },
    {
      "from": {"source": "var", "name": "order.2"},
      "filter": [{"op": "cast", "type": "string"}],
      "to": {"target": "responseBody", "path": "/shippingType"}
    }
  ]
  ```

- `size`: returns the number of elements in the source value as a numeric string. For JSON objects it returns the number of keys, for JSON arrays the number of elements, and for strings the number of characters. This filter takes no arguments: `{"op": "size"}`.

  ```json
  {"from": {"source": "responseBody", "path": "/items"}, "filter": [{"op": "size"}], "to": {"target": "var", "name": "itemCount"}}
  ```

  If the response body `/items` is `["a", "b", "c"]`, then the variable `itemCount` will be `"3"`. The result is always a string, usable in math expressions:

  ```json
  [
    {
      "from": {"source": "responseBody", "path": "/items"},
      "filter": [{"op": "size"}],
      "to": {"target": "var", "name": "itemCount"}
    },
    {
      "from": {"source": "math", "expression": "@{itemCount} == 3"},
      "to": {"target": "var", "name": "itemCountOK"}
    }
  ]
  ```

- `append`: this appends the provided information to the source. This filter, **admits variables substitution**.

  ```json
  {
    "from": {"source": "literal", "value": "telegram"},
    "filter": [{"op": "append", "value": ".teslayout.com"}],
    "to": {"target": "var", "name": "site"}
  }
  ```

  In the example above we will have the variable *site="telegram.teslayout.com"*.

  This could be done also with the `regexReplace` filter, but this has better performance.

  In this algorithm, the obtained value will be a string.

  The advantage against "value-type source with variables replace", is that we can operate directly any source type without need to store auxiliary variable to be replaced.



- `prepend`: this prepends the provided information to the source. This filter, **admits variables substitution**.

  ```json
  {
    "from": {"source": "literal", "value": "teslayout.com"},
    "filter": [{"op": "prepend", "value": "www."}],
    "to": {"target": "var", "name": "site"}
  }
  ```

  In the example above we will have the variable *site="telegram.teslayout.com"*.

  This could be done also with the `regexReplace` filter, but this has better performance.

  In this algorithm, the obtained value will be a string.

  The advantage against "value-type source with variables replace", is that we can operate directly any source type without need to store auxiliary variable to be replaced.



- `sum`: adds the source (if numeric conversion is possible) to the value provided (which <u>also could be negative or float</u>):

  ```json
  {
    "from": {"source": "random", "min": 0, "max": 99999999},
    "filter": [{"op": "sum", "value": 123456789012345}],
    "to": {"target": "var", "name": "mysum"}
  }
  ```

  In this example, the random range limitation (integer numbers) is uncaged through the addition operation. Using this together with other filter algorithms should complete most of the needs. For more complex operations, you may use the `math` source.

  This filter is also useful to sequence a subscriber number:

  ```json
  {
    "from": {"source": "recvseq"},
    "filter": [{"op": "sum", "value": 555000000}],
    "to": {"target": "var", "name": "subscriber"}
  }
  ```

  It is not valid to provide algebraic expressions (like 1/3, 2^5, etc.). For more complex operations, you may use the `math` source.



- `multiply`: multiplies the source (if numeric conversion is possible) by the value provided (which <u>also could be negative to change sign, or lesser than 1 to divide</u>):

  ```json
  {
    "from": {"source": "literal", "value": "-10"},
    "filter": [{"op": "multiply", "value": -0.1}],
    "to": {"target": "var", "name": "value-of-one"}
  }
  ```

  In this example, we operate `-10 * -0.1 = 1`. It is not valid to provide algebraic expressions (like 1/3, 2^5, etc.). For more complex operations, you may use the `math` source.



- `conditionVar`: conditional transfer from source to target based on the boolean interpretation of the value stored in the variable (both local and vaults are searched, giving <u>priority to local ones</u>; the name admits a json pointer, `"name": "<name>/<pointer>"`), following the [boolean criterion](#boolean-criterion):

  - **False** condition for cases:
    - <u>Undefined</u> variable.
    - <u>Empty</u> string, number <u>zero</u>, boolean <u>false</u> or <u>null</u>.

  - **True** condition for the rest of cases:
    - Non-empty string, non-zero number, boolean true, any object or array: note that the strings "0", "false" or any other "apparently false" non-empty string are true condition variables (only their numeric or boolean values are false).
      Also, variable name in `conditionVar` filter, can be preceded by <u>exclamation mark (!)</u>  in order to <u>invert the condition</u>.

  Transfer procedure consists in <u>source copy over target</u> only when condition is **true**. For the **false** branch, you can use `onFilterFail` to provide alternative transforms that execute when the filter condition is not met:

  ```json
  {
    "from": {"source": "literal", "value": "value when id is true"},
    "filter": [{"op": "conditionVar", "name": "id"}],
    "to": {"target": "responseBodyText"},
    "onFilterFail": [
      {
        "from": {"source": "literal", "value": "value when id is false"},
        "to": {"target": "responseBodyText"}
      }
    ]
  }
  ```

  The `onFilterFail` field is an optional array of **full transforms** — not just alternative values. Each fallback item goes through the complete `source → filter → target` pipeline, so you can do anything a regular transform does: write response headers, store files, send UDP datagrams, update vault entries, trigger breaks, etc. It works with any filter type (`conditionVar`, `equalTo`, `equalTo` with `"not": true`, etc.).

  Nesting is fully recursive — each fallback transform can have its own `onFilterFail`, enabling chained if/else-if/else decision trees:

  ```json
  {
    "from": {"source": "vault", "key": "level"},
    "filter": [{"op": "equalTo", "value": "critical"}, {"op": "cast", "type": "string"}],
    "to": {"target": "responseBody", "path": "/category"},
    "onFilterFail": [
      {
        "from": {"source": "vault", "key": "level"},
        "filter": [{"op": "equalTo", "value": "warning"}, {"op": "cast", "type": "string"}],
        "to": {"target": "responseBody", "path": "/category"},
        "onFilterFail": [
          {
            "from": {"source": "literal", "value": "info"},
            "to": {"target": "responseBody", "path": "/category"}
          },
          {
            "from": {"source": "literal", "value": "level-unknown"},
            "to": {"target": "udpSocket", "socket": "/tmp/alerts.sock"}
          }
        ]
      },
      {
        "from": {"source": "literal", "value": "warning-detected"},
        "to": {"target": "responseHeader", "name": "x-alert"}
      }
    ]
  }
  ```

  In this example: if the vault `level` is `"critical"`, the category is set directly. If not, it checks for `"warning"` — and if that matches, it also sets a response header. If neither matches, it falls through to the default `"info"` category and sends a UDP notification.

  The legacy approach using inverted `conditionVar` in separate items still works:

  ```json
  {
    "from": {"source": "literal", "value": "value when id is true"},
    "filter": [{"op": "conditionVar", "name": "id"}],
    "to": {"target": "responseBodyText"}
  },
  {
    "from": {"source": "literal", "value": "value when id is false"},
    "filter": [{"op": "conditionVar", "name": "id", "not": true}],
    "to": {"target": "responseBodyText"}
  }
  ```

  Normally, we generate condition variables by mean regular expression filters, because non-matched sources skips target assignment (undefined is *false* condition) and matched ones copy the source (matched) into the target (variable) which will be a compliant condition variable (non-empty string is *true* condition):

  ```json
  {
    "from": {"source": "requestBody", "path": "/must/be/number"},
    "filter": [{"op": "regexCapture", "rgx": "([0-9]+)"}],
    "to": {"target": "var", "name": "isNumber"}
  }
  ```

  In that example `isNumber` will be undefined (**false** as condition variable) if the request body node value at `/must/be/number` is not a number, and will hold that numeric value, so non-empty value (**true** as condition variable), when it is actually a number (guaranteed by regular expression filter). Then, we can use it as condition variable:

  ```json
  {
    "from": {"source": "literal", "value": "number received !"},
    "filter": [{"op": "conditionVar", "name": "isNumber"}],
    "to": {"target": "responseBodyText"}
  }
  ```

  Condition variables may also be created **automatically** by some transformations into variable targets, to be used later in this `conditionVar` filter. The best example are `jsonConstraint` and `schemaId` filters (explained later) working together with variable target, as it outputs "1" when validation is successful and "" when fails.

  There are some other transformations that are mainly used to create condition variables to be used later. This is the case of *equalTo* (optionally negated):



- `equalTo`: conditional transfer from source to target based in string comparison between the source (as text) and the provided value: the filter passes when they are equal, or when they differ with `"not": true` (`{"op": "equalTo", "value": "200", "not": true}`; v1 `DifferentFrom`). Otherwise the filter fails (target untouched, `onFilterFail` runs). The value **admits variables substitution**.

  ```json
  {
    "from": {"source": "requestBody"},
    "filter": [{"op": "equalTo", "value": "{\"foo\":1}"}],
    "to": {"target": "var", "name": "expectedBody"}
  },
  {
    "from": {"source": "literal", "value": "400"},
    "filter": [{"op": "conditionVar", "name": "expectedBody", "not": true}],
    "to": {"target": "responseStatusCode"}
  }
  ```

  We could also <u>insert the whole condition in the source</u> using for example math library functions `like` and `ilike` (case insensitive variant), having a normalized output ("0": false, "1": true) to compare with filter value:

  ```json
  {
    "from": {"source": "math", "expression": "'@{name1}' ilike 'word'"},
    "filter": [{"op": "equalTo", "value": "1"}],
    "to": {"target": "var", "name": "iequal"}
  }
  ```

  Math library also supports wild-cards for string comparisons and many advanced operations, but normally `regexCapture` is a better alternative (for example: "`[w|W][o|O][r|R][d|D]`" matches "word" as well as "wOrD" or any other combination) because it is more efficient: math library is always used with dynamic variables, so it needs to be compiled on-the-fly, but regular expressions used in `h2agent` are always compiled at provision stage.

  Perhaps, the only use cases that require math library are those related to numeric comparisons:

  In the following example, we translate a logical math expression (which results in value of `1` (true) or `0` (false)) into conditional variable, because it will hold the value "1" or nothing (remember: conditional transfer):

  ```json
  {"from": {"source": "recvseq"}, "to": {"target": "var", "name": "recvseq"}},
  {
    "from": {"source": "math", "expression": "@{recvseq} > 10"},
    "filter": [{"op": "equalTo", "value": "1"}],
    "to": {"target": "var", "name": "greater"}
  },
  {
    "from": {"source": "literal", "value": "Sequence @{recvseq} is lesser or equal than 10"},
    "to": {"target": "responseBodyText"}
  },
  {
    "from": {"source": "literal", "value": "Sequence @{recvseq} is greater than 10"},
    "filter": [{"op": "conditionVar", "name": "greater"}],
    "to": {"target": "responseBodyText"}
  }
  ```

  We could also generate conditional variables from logical expressions using math library and `equalTo` filter to normalize the result into a compliant conditional variable:

  ```json
  {
    "from": {"source": "math", "expression": "@{A}*@{B}"},
    "filter": [{"op": "equalTo", "value": "1"}],
    "to": {"target": "var", "name": "A_and_B"}
  },
  {
    "from": {"source": "math", "expression": "max(@{A},@{B})"},
    "filter": [{"op": "equalTo", "value": "1"}],
    "to": {"target": "var", "name": "A_or_B"}
  },
  {
    "from": {"source": "math", "expression": "abs(@{A}-@{B})"},
    "filter": [{"op": "equalTo", "value": "1"}],
    "to": {"target": "var", "name": "A_xor_B"}
  }
  ```

  Note that `A_xor_B` could be also obtained using source `(@{A}-@{B})^2` or `(@{A}+@{B})%2`.



- `jsonConstraint`: performs a `json` validation between the source (must be a valid document) and the provided filter `json` object.

  - If validation **succeed**, the string "1" is stored in selected target.
  - If validation **fails**, the validation report detail is stored in selected target. <u>If the target is a variable</u> (recommended use), the validation report is stored in `<varname>.fail` variable, and `<varname>` will be emptied. So we could use `!<varname>` or `<varname>.fail` as equivalent condition variables to detect the validation error.

  ```json
  {
    "from": {"source": "requestBody"},
    "filter": [{"op": "jsonConstraint", "value": {"foo": 1}}],
    "to": {"target": "var", "name": "expectedBody"}
  },
  {
    "from": {"source": "literal", "value": "400"},
    "filter": [{"op": "conditionVar", "name": "expectedBody", "not": true}],
    "to": {"target": "responseStatusCode"}
  },
  {
    "from": {"source": "var", "name": "expectedBody.fail"},
    "filter": [{"op": "conditionVar", "name": "expectedBody.fail"}],
    "to": {"target": "responseBodyText"}
  },
  {"from": {"source": "var", "name": "expectedBody.fail"}, "to": {"target": "stop"}}
  ```

  Validation algorithm consists in object reference restriction over source (which must be an object or an array). For **objects**, everything included in the filter must exist and be equal to source, but could miss information (for which it would be non-restrictive). So, an empty object '{}' always matches (although it has no sense to be used). In the example above, `{"foo":1}` is validated, but also `{"foo":1,"bar":2}` does. When a value within the constraint is an **array**, the same "contains" logic applies recursively: every element in the expected array must exist somewhere in the received array, order-independent and allowing extras. Use `schemaId` for strict positional array validation if needed.

  Examples for nested arrays:

  | received | expected | result |
  |---|---|---|
  | `{"tags":["a","b"]}` | `{"tags":["b","a"]}` | SUCCEED (order irrelevant) |
  | `{"tags":["a","b","c"]}` | `{"tags":["a","b"]}` | SUCCEED (extras allowed) |
  | `{"tags":["a"]}` | `{"tags":["a","b"]}` | FAIL ("b" missing) |

  For **arrays**, every element in the filter array must exist somewhere in the source array. When elements are objects, partial matching is used (same recursive constraint logic), so you only need to specify the fields you care about. This is especially useful with `request.headers` source to validate mandatory headers:

  ```json
  {
    "from": {"source": "requestHeaders"},
    "filter": [
      {
        "op": "jsonConstraint",
        "value": [{"name": "x-mandatory", "value": "required-value"}, {"name": "content-type"}]
      }
    ],
    "to": {"target": "var", "name": "validatedHeaders"}
  }
  ```

  In this example, the request must contain a header `x-mandatory` with value `required-value` and a header `content-type` (any value). If validation fails, the report indicates which expected element index was not found.

  To understand better, imagine the source as the 'received' body, and the json constraint filter object as the 'expected' one, so the restriction is ruled by 'expected' acting as a subset which could miss/ignore nodes actually received without problem (less restrictive), but those ones specified there, must exist and be equal to the ones received.

  Take into account that filter provides an static object where variables search/replace is not possible, so those elements which could be non-trivial should be validated separately, for example:

  ```json
  {
    "from": {"source": "requestBody", "path": "/here/the/id"},
    "filter": [{"op": "equalTo", "value": "@{id}"}],
    "to": {"target": "var", "name": "idMatches"}
  }
  ```

  And finally, we should aggregate condition results related to the event analyzed, to compute a global validation result.

  The amount of transformation items is approximately the same as if we could adapt the json constraint (as we would need items to transfer dynamic data like `id` in the example, to the corresponding object node), indeed it seems more intuitive to use `jsonConstraint` for static references:

  Many times, dynamic values are node keys instead of values, so we could still use `jsonConstraint` if nested information is static/predictable.

  ```json
  {
    "from": {"source": "requestBody", "path": "/data/@{phone}"},
    "filter": [{"op": "jsonConstraint", "value": {"model": "samsung", "color": "blue"}}],
    "to": {"target": "var", "name": "expectedPhoneNodeWithinBody"}
  }
  ```

  Often, most of the needed validation documents will be known *a priori* within certain testing conditions, so dynamic validations by mean other filters should be minimized.

  Multiple validations in different tree locations with different filter objects could be chained. Imagine that we received this one:

  ```json
  {
    "foo": 1,
    "timestamp": 1680710820,
    "data": {
      "555555555": {
        "model": "samsung",
        "color": "blue"
      }
    }
  }
  ```

  Then, these could be the whole validation logic in our provision:

  ```json
  {
    "from": {"source": "requestBody"},
    "filter": [{"op": "jsonConstraint", "value": {"foo": 1}}],
    "to": {"target": "var", "name": "rootDataOK"}
  },
  {"from": {"source": "requestUriParam", "name": "phone"}, "to": {"target": "var", "name": "phone"}},
  {
    "from": {"source": "requestBody", "path": "/data/@{phone}"},
    "filter": [{"op": "jsonConstraint", "value": {"model": "samsung", "color": "blue"}}],
    "to": {"target": "var", "name": "phoneDataOK"}
  },
  {
    "from": {"source": "literal", "value": "@{rootDataOK}@{phoneDataOK}"},
    "filter": [{"op": "equalTo", "value": "11"}],
    "to": {"target": "var", "name": "allOK"}
  }
  ```

  Where the time-stamp received from the client is omitted as unpredictable in the first validation, and the phone (`555555555`), supposed (in the example) to be provided in the request query parameters list, is validated through its nested content against the corresponding request node path (`/data/555555555`).

  To finish, just to remark that a mock server used for functional tests can also be inspected through *REST API*, retrieving any event related data to be externally validated, so we will not need to make complicated provisions to do that internally, or at least we could make a compromise between internal and external validations. The difference is the fact that self-contained provisions could "make the day" against scattered information between those provisions and test orchestrator. Also remember that schema validation is supported, so you could provide an OpenAPI restriction for your project interfaces.

  Provisions identification through method and *URI* is normally enough to decide rejecting with 501 (not implemented), although this can be enforced with `jsonConstraint` filter in order to be more accurate if needed. In the case of load testing, normally we are not so strict in favor of performance regarding flow validations. Definitely, this filter is mainly used to validate responses in client mock mode.



- `schemaId`: performs a `json` schema validation between the source (must be a valid document) and the provided filter which is a registered schema for the given identifier. Same logic than `jsonConstraint` is applied here:

  - If validation **succeed**, the string "1" is stored in selected target.

  - If validation **fails**, the validation report detail is stored in selected target. <u>If the target is a variable</u> (recommended use), the validation report is stored in `<varname>.fail` variable, and `<varname>` will be emptied. So we could use `!<varname>` or `<varname>.fail` as equivalent condition variables to detect the validation error.



  Both the `jsonConstraint` and `schemaId` filters **serve as more specific supplementary validations** to enhance event schemas (request and response validation schemas).



Finally, after possible transformations, we could validate the response body (although this may be considered overkilling because the mock is expected to build the response according with a known response schema):



### Multiple provisions

Provision of a set of provisions through an array object is allowed. So, instead of launching *N* provisions separately, you could group them:

```json
[
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/1",
    "responseCode": 200,
    "responseBody": { "foo": "bar-1" },
    "responseHeaders": { "content-type": "application/json", "x-version": "1.0.0" }
  },
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/2",
    "responseCode": 200,
    "responseBody": { "foo": "bar-2" },
    "responseHeaders": { "content-type": "application/json", "x-version": "1.0.0" }
  }
]
```

A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### Unused provisions

`GET /admin/v2/server-provision/unused` retrieves all the provisions configured that were not used yet. This is useful for troubleshooting (during tests implementation or *SUT* updates) to filter unnecessary provisions configured: when the test is executed, just identify unused items and then remove them from test configuration.
The 'unused' status is initialized at creation time (`POST` operation) or when the provision is overwritten.

## Server data

### Storage configuration

Both server and client data storage share the same configuration model (`PUT /admin/v2/server-data/configuration`).

There are three valid combinations:

* `discard=true&discardKeyHistory=true`: nothing is stored.
* `discard=false&discardKeyHistory=true`: no key history stored (only the last event for a key, except for unprovisioned events, which history is always respected for troubleshooting purposes).
* `discard=false&discardKeyHistory=false`: everything is stored: events and key history.

The combination `discard=true&discardKeyHistory=false` is incoherent, as it is not possible to store requests history with general events discarded. In this case, an status code *400 (Bad Request)* is returned.

The `h2agent` starts with full data storage enabled by default, but you could also disable this through command-line (`--discard-data` / `--discard-data-key-history`).

And regardless the previous combinations, you could enable or disable the purge execution when this reserved state is reached for a specific provision:

* `disablePurge=true`: provisions with `purge` state will ignore post-removal operation when this state is reached.
* `disablePurge=false`: provisions with `purge` state will process post-removal operation when this state is reached.

The `h2agent` starts with purge stage enabled by default, but you could also disable this through command-line (`--disable-purge`).

And, also regardless the storage combinations, the transformation pipeline **execution trace** can be stored within each event (disabled by default):

* `executionTrace=true`: every stored event includes an `executionTrace` array, with one entry per evaluated transformation rule, in evaluation order.
* `executionTrace=false`: no trace is built nor stored.

Each entry addresses its rule with a json pointer into the provision (`/transform/<n>`, `/onResponseTransform/<n>` for client response transformations, and `.../onFilterFail/<m>` for fallbacks), tells whether it was applied, and why not:

```json
"executionTrace": [
  {"rule": "/transform/0", "applied": true},
  {"rule": "/transform/1", "applied": false, "note": "EqualTo is false; rule skipped"},
  {"rule": "/transform/1/onFilterFail/0", "applied": true}
]
```

It is the way to find out why a rule did not apply (a filter failed, a source or path did not resolve, a value could not be cast...) without debug logs. It needs the event storage (`discard=false`), and it adds a small cost per event, so it is meant for test development and troubleshooting, not for load testing.

Be careful using this `PUT` operation in the middle of traffic load, because it could interfere and make unpredictable the server data information during tests. Indeed, some provisions with transformations based in event sources, could identify the requests within the history for an specific event assuming that a particular server data configuration is guaranteed.

`GET /admin/v2/server-data/configuration` retrieves the current configuration:

```json
{
    "executionTrace": false,
    "needsStorage": false,
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

The `needsStorage` field is a computed boolean: `true` when any loaded provision contains transformation items with source type `serverEvent`/`clientEvent` or target type `serverEvent`/`clientEvent` (with `eraser` source, used for event deletion). This allows runners to auto-detect whether storage must be enabled for the current test scenario.

#### Stateful detection warnings

When a provision is loaded (`POST`) that references event-dependent transformations but `storeEvents` is currently disabled, the response includes a `warning` field:

```json
{
    "result": "true",
    "response": "server-provision operation; valid schema and server provision data received",
    "warning": "provision references serverEvent/clientEvent but storeEvents is disabled"
}
```

Similarly, when storage is disabled via `PUT /admin/v2/server-data/configuration?discard=true` but loaded provisions require it, the response includes a warning body:

```json
{
    "result": "true",
    "response": "server-data configuration updated",
    "warning": "active provisions require storeEvents (serverEvent/clientEvent references found)"
}
```

Both are advisory only — the operation proceeds normally.

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### Querying server data

`GET /admin/v2/server-data` retrieves the current server internal data (requests received, their states and other useful information like timing or global order). Events received are stored <u>even if no provisions were found</u> for them (the agent answered with `501`, not implemented), being useful to troubleshoot possible configuration mistakes in the tests design. By default, the `h2agent` stores the whole history of events (for example requests received for the same `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the received `uri` normalized</u> (having for example, a better predictable query parameters order during server data events search), not the `classification uri` (which could dispense with query parameters or variable parts depending on the matching algorithm), and could also be slightly different in some cases (specially when query parameters are involved) than original `uri` received on HTTP/2 interface.

<u>Without query parameters</u> (`GET /admin/v2/server-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/requestBody`

The `json` document response shall contain three main nodes: `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `method/uri` combination.

Both *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a requests schema file to validate incoming receptions through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking), and also permits to match the requests content that the agent received.

**Important note**: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). Use `./tools/url.sh` helper to encode:

`/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/requestBody`

Once internally decoded, the request *URI* will be matched against the `uri` <u>normalized</u> as commented above, so encoding must be also done taking this normalization into account (query parameters order).

When provided *method* and *uri*, server data will be filtered with that key. If event number is provided too, the single event object, if exists, will be returned. Same for event path (if nothing found, empty document is returned but status code will be 200, not 204). When no query parameters are provided, the whole internal data organized by key (*method* + *uri*) together with their events arrays are returned.

The query also accepts the two addressing extensions of the `serverEvent` transformation source, with the same semantics, so any `serverEvent` address can be tried here before being used in a provision (and vice versa):

* *requestUriStartsWith*: **literal** URI prefix instead of the exact `requestUri` (which is ignored when both are present). Selects among all the events, for the given *method*, whose *URI* starts with the prefix, merged in reception order; *requestMethod* and *eventNumber* are mandatory (`eventNumber=-1` is the most recent one). For example: `/admin/v2/server-data?requestMethod=POST&requestUriStartsWith=/app/v1/resources/&eventNumber=-1&eventPath=/requestBody`.
* *recvseq*: receive sequence for stable addressing within the *method* + *uri* key (both mandatory); *eventNumber* is ignored. For example: `/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&recvseq=27`.

Both return the single event object (or the *eventPath* node within it): 200 when found, 204 when not, 400 when the addressing is incomplete or invalid. They are also available in the helpers: `server_data --starts-with <method> <uri prefix> <[-]event number> [event path]` and `server_data --recvseq <method> <uri> <recvseq> [event path]`.

#### Server event fields

The information collected for a server event item is:

* `virtualOrigin`: special field for virtual entries coming from provisions which established an *out-state* for a foreign method/uri. This entry is necessary to simulate complexes states but you should ignore from the post-verification point of view. The rest of *json* fields will be kept with the original event information, just in case the history is disabled, to allow tracking the maximum information possible. This node holds a `json` nested object containing the `method` and `uri` for the real event which generated this virtual register.
* `receptionTimestampUs`: event reception *timestamp* (request in).
* `sendingTimestampUs`: event sending *timestamp* (response out). Set after the response is fully sent, including any configured or dynamic delays. Useful for measuring actual server processing time (`sendingTimestampUs - receptionTimestampUs`).
* `state`: working/current state for the event (provision `outState` or target state modified by transformation filters).
* `requestHeaders`: object containing the list of request headers.
* `requestBody`: object containing the request body.
* `previousState`: original provision state which managed this request (provision `inState`).
* `responseBody`: response which was sent.
* `responseDelayMs`: delay which was processed.
* `responseStatusCode`: status code which was sent.
* `responseHeaders`: object containing the list of response headers which were sent.
* `recvseq`: current server monotonically increased sequence for every reception (`1..N`). In case of a virtual register (if it contains the field `virtualOrigin`), this sequence is actually not increased for the server data entry shown, only for the original event which caused this one.

### Server data summary

`GET /admin/v2/server-data/summary` — when a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the server data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of receptions or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of server data summary operation is try to guide the user to narrow and prepare an efficient query.

The summary response fields:

* `displayedKeys`: the summary could also be too big to be displayed, so query parameter *maxKeys* will limit the number (`amount`) of displayed keys in the whole response. Each key in the `list` is given by the *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: this includes possible virtual events, although normally this kind of configuration is not usual and the value matches the total number of real receptions.
* `totalKeys`: total different keys (method/uri) registered.

Example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/1?name=test" },
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/2?name=test" },
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/3?name=test" }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### Deleting server data

`DELETE /admin/v2/server-data` deletes the server data given by query parameters defined in the same way as the *GET* operation. For example:

`/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *method* and *URI* are provided together and *eventNumber* restricts optionally them.

The *recvseq* addressing is accepted too (same as the `serverEvent` purge target: *method* and *URI* mandatory, *eventNumber* ignored), for example `/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&recvseq=27`. Deletion by *requestUriStartsWith* is **not** supported (bad request), to avoid removing more events than expected.

## Client endpoints

Client endpoints (`POST /admin/v2/client-endpoint`) define remote server connection information where `h2agent` may connect during test execution.

By default, created endpoints will connect the defined remote server (except for lazy connection mode: `--remote-servers-lazy-connection`) but no reconnection procedure is implemented in case of fail. Instead, they will be reconnected on demand when a request is processed through such endpoint.

Mandatory fields are `id`, `host` and `port`. Optional `secure` field is used to indicate the scheme used, *http* (default) or *https*, and `permit` field is used to process (default) or ignore a request through the client endpoint regardless if the connection is established or not (when permitted, a closed connection will be lazily restarted). Using `permit`, flows may be interrupted without having to disconnect the carrier.

Endpoints could be updated through further *POST* requests to the same identifier `id`. When `host`, `port` and/or `secure` are modified for an existing endpoint, connection shall be dropped and re-created again towards the corresponding updated address. In this case, status code *Accepted* (202) will be returned.

Configuration of a set of client endpoints through an array object is allowed:

```json
[
  { "id": "myServer1", "host": "localhost1", "port": 8000 },
  { "id": "myServer2", "host": "localhost2", "port": 8000 }
]
```

A client endpoint set fails with the first failed item, giving a 'pluralized' version of the single configuration failed response message although previous valid client endpoints will be added.

## Client provisions

Client provisions (`POST /admin/v2/client-provision`) are a fundamental part of the client mode configuration. Unlike server provisions, they are identified by the mandatory `id` identifier (in server mode, the primary identifier was the `method/uri` key) and the optional `inState` field (which defaults to "initial" when missing). In client mode, there are no classification algorithms because the provisions are actively triggered through the *REST API*.

### Client state machine

In client mode, the meaning of `inState` is slightly different and represents the evolution for a given <u>identifier understood as specific test scenario</u>: the state shall transition for each of its stages (`outState` dictates the next provision key to be processed).

Example:

* `id`="*scenario1*", `inState`="*initial*", `outState`="*second*"
* `id`="*scenario1*", `inState`="*second*", `outState`="*third*"
* `id`="*scenario1*", `inState`="*third*", `outState`="*purge*"

When *scenario1* is triggered, its current state is searched assuming "initial" when nothing is found in client data storage. So it will be processed and next stage is triggered automatically for the new combination `id` + `outState` when the response is received (timeout is a kind of response but normally user stops the scenario in this case). System test is possible because those stages are replicated by mean different instances of the same scenario evolving separately: this is driven by an internal sequence identifier which is used to calculate real request *method* and *uri*, the ones stored in the data base.

Each stage (chain link) may target a **different client endpoint** through its own [`endpoint`](#endpoint) field, so a single scenario can mix remote servers across its states (see the fallback rules and example there).

The `outState` holds a reserved default value of `road-closed` for any provision when it is not explicitly configured. This is because here, the provision is not reset and must be guided by the flow execution. This `outState` can be configured on request transformation before sending and after response is received so new flows can be triggered with different stages, but they are unset by default (`road-closed`). This special value is not accepted for `inState` field to guarantee its reserved meaning.

#### Chain cycle prevention

Client provision chains progress automatically: when a provision completes, the `outState` is used to find the next provision with matching `inState`. This creates a risk of infinite loops if the chain forms a cycle. The following safeguards are in place:

**Reserved values**:

- `outState` = `"initial"` is **rejected** at provision time. Since `"initial"` is the default entry point for chains, allowing it as an output state would trivially create cycles. A chain that needs to "restart" should use a different design (e.g. dynamics with `repeat`).
- `inState` = `"road-closed"` is **rejected** at provision time. This value is reserved as the default `outState` (chain terminator) and no provision should be reachable through it.

**Self-loop detection**: at runtime, state progression requires `outState` to differ from `inState`. If they are equal, the chain stops. This prevents a single provision from re-triggering itself.

**Multi-step cycles are not detected**: cycles involving two or more provisions (e.g. A→B→C→A where none of the individual `outState` values equal their own `inState`) will result in an infinite chain — unbounded event accumulation in `client-data` (memory growth) and endless traffic to the target server. This is a deliberate simplification: detecting arbitrary graph cycles at runtime would add complexity for a scenario that is unlikely in practice and typically indicates a design error. Note that chains can start at any `inState` (not just `"initial"`), so cycles can form between any set of states. Be mindful of your `outState` transitions to avoid circular paths.

<u>Special **purge** state</u>: the keyword '*purge*' clears all events accumulated during the chain — including events from previous steps with different endpoints, methods or URIs. This is where chain-aware purge is most valuable: client chains are identified by provision `id` + `inState`, and each step typically targets a different method+URI, so without it only the last step's events would be removed. Incomplete chains retain their full event history for forensics. The data-key-level caveat described in the server purge section applies here as well.

### Client provision fields

#### endpoint

Optional client endpoint identifier (previously registered through `POST /admin/v2/client-endpoint`) that indicates where this provision sends its request. It is not mandatory in the schema.

**Per-state endpoint in a chain**: each link of a chain (each provision sharing the same `id` but with a different `inState`) may declare its own `endpoint`. This means a single chained scenario can mix client endpoints across its states — for example, alternate requests towards two different remote servers within the same automatic flow. The endpoint is re-resolved on every state progression, so the link's own `endpoint` is honored when the chain advances (not only on the first, triggered link).

**Fallback behavior**: because `endpoint` is optional, three cases apply on progression to the next link:

- The next link declares **no** `endpoint`: it inherits the endpoint of the previous link (silent, legitimate — useful for homogeneous chains where the endpoint is stated once).
- The next link declares a **valid** `endpoint`: it is re-resolved and the request is routed there.
- The next link declares an `endpoint` that is **not registered or disabled** (`permit=false`): it inherits the previous link's endpoint and a warning is logged (non-breaking, to preserve existing configurations).

The triggered (`initial`) link, however, always requires a valid endpoint: the trigger resolves it up front and aborts the send if it is missing or disabled.

Example of a heterogeneous chain (states routed to different endpoints):

```json
[
  {
    "id": "multiEndpoint",
    "endpoint": "serverA",
    "requestMethod": "GET",
    "requestUri": "/app/v1/step-a",
    "outState": "second"
  },
  {
    "id": "multiEndpoint",
    "inState": "second",
    "endpoint": "serverB",
    "requestMethod": "POST",
    "requestUri": "/app/v1/step-b"
  }
]
```

Triggering `multiEndpoint` sends the first request to `serverA` and, upon its response, the second request to `serverB`.

> Note on data storage: the `clientEndpointId` recorded in `client-data` events is taken from each link's own provision, so it always reflects the link's declared endpoint. The endpoint resolution described above governs the *physical* destination of the request, aligning it with that declared identifier.

#### requestMethod & requestUri

Both can be omitted in the provision, but they are mandatory to be available (so they should be created on transformations) when preparing the request to be sent. The `requestUri` is normally completed/appended by dynamic sequences in order to configure the final *URI* to be sent.

#### requestSchemaId

We could optionally validate built request (after transformations) against a `json` schema. Schemas are identified by string name and configured through [command line](../../README.md#command-line) or REST API. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### requestHeaders

Header fields for the request. For example:

```json
"requestHeaders":
{
  "content-type": "application/json"
}
```

#### requestBody

Request body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

#### requestDelayMs

Optional request delay simulation in milliseconds.

#### timeoutMs

Optional timeout for response in milliseconds. Defaults to 1000 ms (1 second) when not specified, aligned with the underlying `Http2Client::asyncSend` default. A value of 0 is treated as unset and also defaults to 1000 ms.

#### responseSchemaId

We could optionally validate received responses against a `json` schema. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### expectedResponseStatusCode

Optional expected HTTP status code for the response (integer, 100-599). When configured and the received response status code does not match, the provision flow chain is interrupted (no state progression) and a validation failure counter is incremented. This is useful for automated verdict without requiring event storage:

```json
{
  "id": "myFlow",
  "endpoint": "myServer",
  "requestMethod": "GET",
  "requestUri": "/api/v1/resource",
  "expectedResponseStatusCode": 200
}
```

When either `expectedResponseStatusCode` or `responseSchemaId` validation fails:
- The error is logged
- The `h2agent_traffic_client_unexpected_response_status_code_counter` prometheus metric is incremented
- The event is still stored (if storage is enabled) for debugging
- The state progression chain is interrupted (no purge, no next provision)

### Client transformation differences

As in the server mode, we have transformations to be applied, but this time we can transform the context before sending (`transform` node), and when the response is received (`onResponseTransform` node).

Most items are described in the [transformation pipeline](#transformation-pipeline) section. Here, they work in the same way, but there are a few differences:

New **sources**:

- `{"source": "sendseq"}`: sequence id number increased for every mock sending over specific client endpoint (starts on *1* when the *h2agent* is started).

**Reserved variable `sequence`**: the current iteration number within a triggered range. This is the **fundamental variable for building dynamic client requests** — without it, every request in a range would be identical.

When a provision is triggered with a range (`sequenceBegin`/`sequenceEnd`), `sequence` holds the current counter value on each execution. It is automatically injected as a scoped variable before each transform execution, so it is accessible both as:

- `{"source": "var", "name": "sequence"}` — as a source (e.g. for arithmetic with the `sum` filter)
- `@{sequence}` — inline substitution within `literal` sources, `append`/`prepend` filters, and any field that admits variables substitution

Common use cases:

- **Dynamic URI** — build a unique path per request:
  ```json
  {"from": {"source": "var", "name": "sequence"}, "filter": [{"op": "prepend", "value": "/api/v1/sessions/"}], "to": {"target": "requestUri"}}
  ```

- **Dynamic body field** — inject the counter into the request body:
  ```json
  {"from": {"source": "literal", "value": "@{sequence}"}, "filter": [{"op": "cast", "type": "integer"}], "to": {"target": "requestBody", "path": "/counter"}}
  ```

- **Unique vault keys** — isolate concurrent flows:
  ```json
  {"from": {"source": "var", "name": "sequence"}, "to": {"target": "vault", "key": "flow_@{sequence}_status"}}
  ```

The value starts at `sequenceBegin` and increments by 1 on each tick. When paused (`cps=0`) and resumed (only `cps` provided), `sequence` continues from where it left off. Providing a new range resets it to the new `sequenceBegin`. For synchronous single-shot triggers (`?sequence=N`), the value is set to `N`.

New **targets**:

- `{"target": "requestDelayMs"}` *[unsigned integer]*: simulated delay before sending the request: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- `{"target": "requestTimeoutMs"}` *[unsigned integer]*: timeout to wait for the response: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- `{"target": "requestUri"}` *[string]*: overrides the request URI to be sent. This is useful in combination with `sequence` source to build dynamic URIs (e.g., `/api/v1/resource/@{myId}`).
- `{"target": "requestMethod"}` *[string]*: overrides the HTTP method to be sent (e.g., `POST`, `GET`, `PUT`, `DELETE`, `HEAD`).
- `{"target": "stop"}`: this target is activated with a **true** source (see [boolean criterion](#boolean-criterion)) (for example `{"source": "literal", "value": "1"}`) and interrupts the transformation list. It is used on response context to discard further transformations when, for example, response status code is not valid to continue processing the test scenario. Normally, we should "dirty" the `outState` (for example, setting an unprovisioned "road closed" state, in order to stop the flow) and then stop the transformation procedure (this also dodges a probable purge state configured in next stages, keeping internal data for further analysis). Note that placing `stop` as the **last** item in a transformation list is illogical — there are no further items to interrupt. A warning is logged at provision time when this is detected.

Note that the `clientProvision` target is only available in server mode transformations (not in client mode), as it is the mechanism to trigger client flows from server context.

### Multiple client provisions

Provision of a set of provisions through an array object is allowed:

```json
[
  {
    "id": "test1",
    "endpoint": "myClientEndpoint",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/madrid?loc=123",
    "requestBody": { "engine": "tdi", "model": "audi", "year": 2021 },
    "requestHeaders": { "content-type": "application/json" },
    "requestDelayMs": 20,
    "timeoutMs": 2000
  },
  {
    "id": "test2",
    "endpoint": "myClientEndpoint2",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/malaga?loc=124",
    "requestBody": { "engine": "hdi", "model": "peugeot", "year": 2023 },
    "requestHeaders": { "content-type": "application/json" },
    "requestDelayMs": 20,
    "timeoutMs": 2000
  }
]
```

A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### Unused client provisions

`GET /admin/v2/client-provision/unused` retrieves all the provisions configured that were not used yet. This is useful for troubleshooting (during tests implementation or *SUT* updates) to filter unnecessary provisions configured: when the test is executed, just identify unused items and then remove them from test configuration.
The 'unused' status is initialized at creation time (`POST` operation) or when the provision is overwritten.

### Triggering

To trigger a client provision we use `POST /admin/v2/client-provision/<id>/trigger`, providing its identifier in the *URI* and the trigger options as query parameters (no request body is needed). The former `GET /admin/v2/client-provision/<id>` is retired (`410 Gone` with a hint): triggering is an action, not a read.

Normally we shall trigger only provisions for `inState` = "initial" (so, it is the default value when this query parameter is missing). This is because the traffic flow will evolve activating other provision keys given by the <u>same</u> provision identifier but another `inState`. All those internal triggers are indirectly caused by the primal administrative operation which is the only one externally initiated. Although it is possible to trigger an intermediate state (via the `inState` query parameter), this is useful for debugging and also for [partial chain execution](#partial-chain-execution) scenarios.

There are two triggering modes: **synchronous** (single request) and **asynchronous** (timer-based). Their query parameters are mutually exclusive.

#### Synchronous triggering

A single request is sent immediately and the operation returns status code *200*. This is the default behavior when no query parameters are provided (using `sequence` value of '0').

Optionally, the `sequence` query parameter can be provided to set a specific sequence value before sending:

* `sequence`: specific `sequence` value for the request.

This is useful when the provision uses `sequence` in its transformations to build dynamic content (e.g., a unique URI or body field). Successive calls with different `sequence` values allow sending varied requests synchronously:

```bash
# Send three requests with sequence values 10, 20 and 30:
for seq in 10 20 30; do
  curl --http2-prior-knowledge -XPOST "http://localhost:8074/admin/v2/client-provision/myFlow/trigger?sequence=${seq}"
done
```

#### Asynchronous triggering

Optional query parameters can be specified to perform multiple triggering (status code *202* is used in operation response instead of *200*). This operation creates internal events sequenced in a range of values (`sequence` variable will be available in provision process for each iterated value) and with specific rate (events per second) to perform system/load tests.

Each client provision can evolve the range of values independently of others, and triggering process may be stopped (with `cps` zero-valued) and then resumed again with a positive rate. Also repeat mode is stored as part of provision trigger configuration with these defaults: range `[0, 0]`, rate of '0' and repeat 'false'.

*Query parameters:*

* `sequenceBegin`: initial `sequence` variable.
* `sequenceEnd`: final `sequence` variable.
* `cps`: rate in provisions per second triggered (non-negative value, '0' to stop). Each tick fires one provision execution which may send multiple requests if the flow has several steps.
* `repeat`: range repetition once exhausted (true or false).

Negative values are allowed for `sequenceBegin` and `sequenceEnd`, which is useful when a transform applies an offset (e.g. `sum`) over a central base value:

```bash
# Trigger 11 provision executions with sequence values from -5 to 5 at 100 cps:
curl --http2-prior-knowledge -XPOST \
  "http://localhost:8074/admin/v2/client-provision/myFlow/trigger?sequenceBegin=-5&sequenceEnd=5&cps=100"
```

> **Important**: `sequence` parameter (synchronous) cannot be mixed with `sequenceBegin`, `sequenceEnd`, `cps` or `repeat` (asynchronous). Providing both will result in a *400 Bad Request* error.

So, together with provision information configured, we store dynamic load configuration and state (current `sequence`):

```json
"dynamics": {
  "repeat": false,
  "cps": 1500,
  "sequence": 2994907,
  "sequenceBegin": 0,
  "sequenceEnd": 10000000,
  "state": "running",
  "nextSequence": 2994908
}
```

The `sequence` field is the **raw internal iterator**: it uses a "use-then-increment starting at `sequenceBegin - 1`" scheme, so right after arming a range it holds `sequenceBegin - 1` (which shows up as a confusing value, e.g. `-1` when `sequenceBegin` is `0`). To avoid having to decode that sentinel, two human-friendly read-only fields are provided:

* `state`: lifecycle of the trigger range:
  * `armed`: prepared, waiting for the first tick (no element of the range emitted yet; this is the state after setting `sequenceBegin` with `cps=0`).
  * `running`: ticker active (asynchronous triggering with `cps > 0`).
  * `paused`: positioned within the range but not ticking (e.g. `cps` set back to `0`).
  * `exhausted`: the whole range was consumed without `repeat` (a new administrative operation is needed to trigger again).
* `nextSequence`: the `sequence` value the **next** request will carry (`sequenceBegin` when `armed`, `sequence + 1` while progressing, wrapping to `sequenceBegin` when `repeat` is enabled); `null` when `exhausted` (nothing pending).

> **Note**: `dynamics` fields are a read-only external snapshot accessible via the REST API only (e.g., to poll for completion or inspect the pause/armed state). They are **not** available as transform variables. To use the current sequence value inside a transform, use the `seq` source instead. Dynamics are refreshed every 500ms during active ticking and on-demand when queried via the admin API, so values may lag slightly under high CPS.

*Configuration rules:*

- If no query parameters are provided, single event is triggered for `sequence` value of '0'.
- Omitted parameter(s) keeps previous value.
- Provided parameter(s) updates previous value.
- If both `sequenceBegin` and `sequenceEnd` query parameters are present, a single (when coincide) or multiple list of events are created for each `sequence` value.
- Whenever `cps` rate is provided, tick period for provision triggering is updated (stopped with '0').
- Cycle `repeat` can be updated in any moment, but its effect will be ignored if the range has been completely processed while it was disabled.
- When the range of sequences is completed (`sequenceEnd` reached), trigger configuration is reset and a new administrative operation will be needed.
- Several operations could update load parameters, but `sequence` will evolve if complies with range requirements while rate is positive, so operations could have no effect depending on the information provided.

User may transform sequence value to adapt the test case taking into account that any transformation implemented should be *bijective* towards target set to prevent that values used in the test are repeated or overlapped. For example, we could provide generation range `[0, 99]` to trigger one hundred of *URIs* in the form `/foo/bar/<odd natural numbers>`, just by mean the following transformation item:

```json
{
  "from": {"source": "math", "expression": "2*@{sequence} + 1"},
  "filter": [{"op": "prepend", "value": "/foo/bar/"}],
  "to": {"target": "requestUri"}
}
```

Or for example, trigger all the existing values (also even numbers) from `/foo/bar/555000000` to `/foo/bar/555000099`, by mean adding (so padding "in a row") the base number `555000000` to the sequence iterated within the range provided (`[0, 99]`):

```json
[
  {
    "from": {"source": "var", "name": "sequence"},
    "filter": [{"op": "sum", "value": 555000000}],
    "to": {"target": "var", "name": "phone"}
  },
  {
    "from": {"source": "literal", "value": "/foo/bar/"},
    "filter": [{"op": "append", "value": "@{phone}"}],
    "to": {"target": "requestUri"}
  }
]
```

Note that, in the first transformation item, we are creating a new variable 'phone' because <u>`sequence` variable is reserved and non-writable as target</u> (a warning log is generated when trying to do this).

Also, note that final transformation item uses constant value for source, but it could also use `requestUri` as a source if client provision configures it as `/foo/bar` within provision template.

And finally, note that we could also solve the previous exercise just providing the real range `[555000000, 555000099]` to the operation, processing directly the last single transformation item shown before but appending variable `sequence` instead of `phone`. This is a kind of decision that implies advantages or drawbacks:

* Using ad-hoc ranges saves and simplifies some steps, but you may remember those ranges as part of your testing administrative operations.

* Using standard range `0..N` needs more transformations but shows the real intention within provision programming which are autonomous and ready for use. So testing automation only need to decide the amount of load (`N`) and could mix other provisions already prepared in the same way, which seems easy to coordinate:

  ```bash
  for provision in script1 script2 script3; do # parallel test scripts, 5000 iterations at 200 provisions per second:
    curl -i --http2-prior-knowledge -XPOST "http://localhost:8074/admin/v2/client-provision/${provision}/trigger?sequenceEnd=4999&cps=200"
  done
  ```

#### Partial chain execution

A common pattern is to define a full lifecycle chain (e.g., `initial` → `established` → `updated` → `terminated`) but execute it in stages. This is achieved by combining a vault with a conditional `stop` in the `onResponseTransform` of an intermediate step.

For example, consider a 3-step session flow (create → update → delete). To execute only the establishment phase:

1. Set a vault to control the chain depth:

   ```bash
   curl --http2-prior-knowledge -d '{"STOP_AFTER": "establishment"}' \
     http://localhost:8074/admin/v2/vault
   ```

2. In the `onResponseTransform` of the establishment provision, add a conditional outState override:

   ```json
   {
     "from": {"source": "vault", "key": "STOP_AFTER"},
     "filter": [{"op": "regexCapture", "rgx": "establishment"}],
     "to": {"target": "var", "name": "mustStop"}
   },
   {
     "from": {"source": "literal", "value": "road-closed"},
     "filter": [{"op": "conditionVar", "name": "mustStop"}],
     "to": {"target": "outState"}
   }
   ```

   When `STOP_AFTER` matches `"establishment"`, the `outState` is overridden to an unprovisioned state (`road-closed`), preventing automatic state progression. The chain simply stops because no provision exists for `(mySession, road-closed)`.

3. Later, to complete the remaining steps (update + delete), trigger from the intermediate state:

   ```bash
   # First, clear the stop condition:
   curl --http2-prior-knowledge -d '{"STOP_AFTER": ""}' \
     http://localhost:8074/admin/v2/vault

   # Then trigger from the intermediate state:
   curl --http2-prior-knowledge -XPOST \
     "http://localhost:8074/admin/v2/client-provision/mySession/trigger?inState=established&sequenceBegin=0&sequenceEnd=999&cps=500"
   ```

> **Note**: when triggering from an intermediate state, scoped variables (`var`) captured in earlier steps are only available if the chain was previously executed through those steps (variables propagate along `outState` links). If you trigger an intermediate state directly, ensure the provision does not depend on variables from prior steps — or use `vault` instead.

This pattern is useful for benchmarking scenarios where you want to measure each phase independently, or when you need to pre-populate sessions before running update/delete workloads.

#### Server-triggered client flows

Client provisions can also be triggered from within server provision transformations using the `clientProvision` target (`{"target": "clientProvision", "id": "<clientProvisionId>", "inState": "<inState>"}`) (described in the [transformation pipeline](#transformation-pipeline) section). This allows the server to react to an incoming request by firing one or more outgoing client flows as a side effect. The source acts as a conditional gate: a true value fires the trigger, a false or undefined one skips it (see [boolean criterion](#boolean-criterion)).

```json
{
  "requestMethod": "POST",
  "requestUri": "/webhook/notify",
  "responseCode": 200,
  "transform": [
    {
      "from": {"source": "literal", "value": "1"},
      "to": {"target": "clientProvision", "id": "forwardNotification", "inState": "initial"}
    }
  ]
}
```

The triggers are executed asynchronously after the server response is fully built, so they do not affect server response latency. Multiple `clientProvision` targets can coexist in the same transformation list.

## Client data

### Client dispatch latency

`GET /admin/v2/client/dispatch-latency`

Returns accumulated statistics about the worker pool dispatch latency — the time between posting work to the client worker pool (`boost::asio::post()`) and the worker thread starting to execute the lambda. This measures queue wait time plus thread wakeup overhead.

Both the CPS timer tick (initial request creation) and chain continuations (next step after response) are measured.

```json
{
  "avgUs": 42.5,
  "maxUs": 312,
  "count": 750000
}
```

Interpretation:

| avgUs | Status | Action |
|------:|--------|--------|
| < 50 | Pool idle — workers waiting for work | Ideal. No action needed |
| 100–500 | Pool busy but healthy — no significant queuing | Normal under load |
| 1,000–5,000 | Pool at capacity — work queues up | Consider increasing `--traffic-client-worker-threads` |
| > 10,000 | Pool saturated — workers cannot keep up | Add more threads or reduce per-request transform cost |

Statistics accumulate for the lifetime of the process and are not reset between test runs. To get per-run values, query before and after the test and compute the delta.

### Client storage configuration

Same explanation done for `server-data` equivalent operation, applies here (`PUT /admin/v2/client-data/configuration`). Just to know that history events here have an extended key adding `client endpoint id` to the `method` and `uri` processed. The purge procedure clears all events accumulated during the chain, removing everything registered across all steps (different endpoints, methods and URIs) that participated in the completed flow.

The same agent could manage server and client connections, so you have specific configurations for internal data regarding server or client events, but normally, we shall use only one mode to better separate responsibilities within the testing ecosystem.

`GET /admin/v2/client-data/configuration` retrieves the current configuration:

```json
{
    "executionTrace": false,
    "needsStorage": false,
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### Querying client data

`GET /admin/v2/client-data` retrieves the current client internal data (requests sent, their provision identifiers, states and other useful information like timing or global order). By default, the `h2agent` stores the whole history of events (for example requests sent for the same `clientEndpointId`, `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the final sent `uri` normalized</u> (having for example, a better predictable query parameters order during client data events search), not necessarily the `provisioned uri` within the provision template.

<u>Without query parameters</u> (`GET /admin/v2/client-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *clientEndpointId*, *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v2/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/responseBody`

The `json` document response shall contain three main nodes: `clientEndpointId`, `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `clientEndpointId/method/uri` combination.

Both *clientEndpointId*, *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `clientEndpointId/method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a response schema file to validate incoming responses through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking).

**Important note**: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). Use `./tools/url.sh` helper to encode:

`/admin/v2/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/responseBody`

As for server data, the query accepts the addressing extensions of the `clientEvent` transformation source with the same semantics: *requestUriStartsWith* (literal prefix instead of the exact `requestUri`; *clientEndpointId*, *requestMethod* and *eventNumber* mandatory) and *sendseq* (send sequence within the *clientEndpointId* + *method* + *uri* key; *eventNumber* ignored). Both return the single event object (or its *eventPath* node), with 200, 204 or 400 as for server data. Deletion accepts *sendseq* but not *requestUriStartsWith*. Helpers: `client_data --starts-with <client endpoint id> <method> <uri prefix> <[-]event number> [event path]` and `client_data --sendseq <client endpoint id> <method> <uri> <sendseq> [event path]`.

#### Client event fields

The information collected for a client event item is:

* `clientProvisionId`: provision identifier.
* `sendseq`: current client monotonically increased sequence for every sending (`1..N`).
* `sendingTimestampUs`: event sending *timestamp* (request).
* `receptionTimestampUs`: event reception *timestamp* (response).
* `state`: working/current state for the event (provision `outState` or target state modified by transformation filters).
* `requestHeaders`: object containing the list of request headers.
* `requestBody`: object containing the request body.
* `previousState`: original provision state which managed this request (provision `inState`).
* `responseBody`: response which was received.
* `requestDelayMs`: delay for outgoing request.
* `responseStatusCode`: status code which was received.
* `responseHeaders`: object containing the list of response headers which were received.
* `sequence`: internal provision sequence.
* `timeoutMs`: accepted timeout for request response.

### Client data summary

`GET /admin/v2/client-data/summary` — when a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the client data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of sendings or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of client data summary operation is try to guide the user to narrow and prepare an efficient query.

The summary response fields:

* `displayedKeys`: query parameter *maxKeys* will limit the number (`amount`) of displayed keys. Each key in the `list` is given by the *clientEndpointId*, *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: total number of events.
* `totalKeys`: total different keys (clientEndpointId/method/uri) registered.

Example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/1?name=test" },
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/2?name=test" },
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/3?name=test" }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### Deleting client data

`DELETE /admin/v2/client-data` deletes the client data given by query parameters defined in the same way as the *GET* operation. For example:

`/admin/v2/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *clientEndpointId*, *method* and *URI* are provided together and *eventNumber* restricts optionally them.
