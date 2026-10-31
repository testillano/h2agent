# Migration guide: h2agent 4.x (v1) to 5.0.0 (v2)

This guide walks through **every** change between h2agent 4.x (admin API v1, transformation grammar v1) and
5.0.0 (admin API v2, grammar v2), with before/after examples. It is meant to be read once while migrating a
test suite, and it will be kept here during the transition.

* The exact, table-driven syntax mapping is in [GrammarV2.md](developers/GrammarV2.md).
* The full reference is the [API user guide](api/README.md) and [openapi.yaml](api/openapi.yaml).
* Most of the syntax work is automatic: see [1. The migration tool](#1-the-migration-tool).

Changes are tagged by how much attention they need:

* **[auto]** the migration tool rewrites it, behaviour is identical.
* **[behaviour]** same syntax (or migrated syntax), but the result may differ: review the provisions that hit it.
* **[new]** new capability, nothing to migrate.
* **[removed]** something that is not accepted any more.

---

## Contents

1. [The migration tool](#1-the-migration-tool)
2. [Admin API](#2-admin-api)
3. [Rule shape](#3-rule-shape)
4. [Sources](#4-sources)
5. [Targets](#5-targets)
6. [Filters](#6-filters)
7. [Typing: the cast filter](#7-typing-the-cast-filter)
8. [Filter chains](#8-filter-chains)
9. [Variables and vaults hold native json](#9-variables-and-vaults-hold-native-json)
10. [Variable references `@{}`](#10-variable-references-)
11. [Provision references `#{}`](#11-provision-references-)
12. [Boolean criterion](#12-boolean-criterion)
13. [Eraser](#13-eraser)
14. [Stop (former break)](#14-stop-former-break)
15. [Events addressing](#15-events-addressing)
16. [Execution trace](#16-execution-trace)
17. [Stricter validation](#17-stricter-validation)
18. [Math source fixes](#18-math-source-fixes)
19. [Startup order](#19-startup-order)
20. [Checklist](#20-checklist)

---

## 1. The migration tool

`h2agent-migrate` is shipped in the image (`/opt/h2agent-migrate`) and converts v1 provisions (server or
client, single object or array) into v2. Only `transform` and `onResponseTransform` are rewritten; any other
field is kept untouched and in order.

```bash
# Convert a provisions file (in place is allowed):
docker run --rm -v $PWD:$PWD -w $PWD --entrypoint /opt/h2agent-migrate ghcr.io/testillano/h2agent:5.0.0 \
       --in server-provision.json --out server-provision.json

# From standard input:
cat provision.json | h2agent-migrate --in -

# Find v1 leftovers in a whole test suite: /admin/v1 references, v1 provision files,
# v1 rule literals embedded in scripts, python or C++ tests (exit code 1 when found):
h2agent-migrate --scan ./ct

# Back to v1 (diagnostics only; the v2 subset with a v1 equivalent):
h2agent-migrate --reverse --in provision.v2.json
```

The conversion is syntax-only: each v2 rule behaves as its v1 origin, except for the changes tagged
**[behaviour]** below, which depend on runtime values and cannot be compensated by the tool. When a v1 rule
did silently nothing (v1 skipped it), the tool fails on purpose, telling you to remove or rewrite the rule
(see [17. Stricter validation](#17-stricter-validation)).

---

## 2. Admin API

### 2.1 Base path **[removed]**

Every operation moves from `/admin/v1/` to `/admin/v2/` (same resource names). Any request to `/admin/v1/...`
is answered `410 Gone`, with a `cause` pointing to the v2 path:

```bash
# Before
curl --http2-prior-knowledge http://localhost:8074/admin/v1/server-provision
# After
curl --http2-prior-knowledge http://localhost:8074/admin/v2/server-provision
```

The helpers (`tools/helpers.bash`), the gherkin driver, the playground, the tester GUI and the helm chart
utilities already use `/admin/v2`.

### 2.2 Admin schemas **[removed]** / **[new]**

The json schemas the agent validates admin requests against moved to their own resource:

| v1 | v2 |
|---|---|
| `GET /admin/v1/server-matching/schema` | `GET /admin/v2/api-schema/server-matching` |
| `GET /admin/v1/server-provision/schema` | `GET /admin/v2/api-schema/server-provision` |
| `GET /admin/v1/client-endpoint/schema` | `GET /admin/v2/api-schema/client-endpoint` |
| `GET /admin/v1/client-provision/schema` | `GET /admin/v2/api-schema/client-provision` |
| `GET /admin/v1/schema/schema` | `GET /admin/v2/api-schema/schema` |
| `GET /admin/v1/vault/schema` | `GET /admin/v2/api-schema/vault` |
| - | `GET /admin/v2/api-schema` (list of resources) |

`GET /admin/v2/<resource>/schema` answers `410 Gone` with a hint. Do not confuse them with the user schemas
(`/admin/v2/schema`), which validate traffic.

Why: the old path shared the id space of the resources, so a client provision called `schema` could not be
triggered.

### 2.3 Client provision trigger **[removed]**

Triggering is an action, so it is a `POST` on a dedicated sub-resource. Query parameters are unchanged
(`inState`, `sequence`, `sequenceBegin`, `sequenceEnd`, `cps`, `repeat`...), and no body is needed:

```bash
# Before
curl --http2-prior-knowledge "http://localhost:8074/admin/v1/client-provision/myFlow?sequenceEnd=999&cps=100"
# After
curl --http2-prior-knowledge -XPOST "http://localhost:8074/admin/v2/client-provision/myFlow/trigger?sequenceEnd=999&cps=100"
```

`GET /admin/v2/client-provision/<id>` answers `410 Gone` with a hint. As with the schemas, provision ids
`schema` and `unused` are not shadowed any more.

### 2.4 Capabilities **[new]**

```bash
curl --http2-prior-knowledge http://localhost:8074/admin/v2/capabilities
{ "apiVersion": "v2", "programVersion": "v5.0.0", "grammar": "v2" }
```

Informative only: a runner can detect which grammar the agent accepts.

### 2.5 Data configuration: `executionTrace` **[new]**

`GET /admin/v2/server-data/configuration` and `GET /admin/v2/client-data/configuration` include a new field:

```json
{"executionTrace": false, "needsStorage": false, "purgeExecution": true, "storeEvents": true, "storeEventsKeyHistory": true}
```

Tests comparing the whole json must add it. See [16. Execution trace](#16-execution-trace).

### 2.6 Vault listing **[behaviour]**

`GET /admin/v2/vault` lists numbers and booleans written by transformations with their json type (`2`,
`true`), not as texts (`"2"`, `"true"`). See [9](#9-variables-and-vaults-hold-native-json).

### 2.7 Unchanged

Everything else keeps its form: matching, provisions (apart from the transformation items), schemas, vault
(including `wait` and SSE `events`), server/client data inspection (`summary`, `sequence`, filtered `GET`,
`DELETE`), files, sockets, logging, configuration, health, malloc-trim, dispatch latency.

---

## 3. Rule shape

**[auto]** A v1 rule is three strings (`source`, `target`, optional `filter`) with the arguments glued into
them with dots. A v2 rule is three objects: `from`, an optional `filter` **list**, and `to`. The kind of each
stage is a discriminator field (`source`, `target`, `op`), and every argument is a named field:

```json
v1:
{"source": "request.body./user/id", "target": "response.body.json.integer./id"}

v2:
{"from": {"source": "requestBody", "path": "/user/id"},
 "filter": [{"op": "cast", "type": "integer"}],
 "to": {"target": "responseBody", "path": "/id"}}
```

General rules of v2:

* Every kind and field is lowerCamelCase, filters included (`RegexReplace` -> `regexReplace`).
* One form per kind: no string shortcuts. A kind without arguments is `{"source": "recvseq"}`.
* Field names mean the same everywhere: `path` (json pointer; **absent = whole document**), `name`
  (variable, header, query parameter), `key` (vault entry), `id`, `file`, `socket`, `value` (literal
  operand), `rgx` / `fmt` (regular expression / format).
* Paths are json pointers. Nothing is separated by dots and nothing is url-encoded.
* `onFilterFail` keeps its meaning (rules executed when a filter fails), with v2 rules inside.

The provision fields around the rules (`requestMethod`, `requestUri`, `responseCode`, `responseBody`,
`responseHeaders`, `responseDelayMs`, `inState`, `outState`, schemas, `endpoint`, `expectedResponseStatusCode`...)
are unchanged.

---

## 4. Sources

All **[auto]**:

| v1 | v2 |
|---|---|
| `"source": "request.uri"` | `"from": {"source": "requestUri"}` |
| `"source": "request.uri.path"` | `"from": {"source": "requestUriPath"}` |
| `"source": "request.uri.param.id"` | `"from": {"source": "requestUriParam", "name": "id"}` |
| `"source": "request.body"` | `"from": {"source": "requestBody"}` |
| `"source": "request.body./a/b"` | `"from": {"source": "requestBody", "path": "/a/b"}` |
| `"source": "response.body./a"` | `"from": {"source": "responseBody", "path": "/a"}` |
| `"source": "request.header.x-id"` | `"from": {"source": "requestHeader", "name": "x-id"}` |
| `"source": "request.headers"` | `"from": {"source": "requestHeaders"}` |
| `"source": "response.header.location"` | `"from": {"source": "responseHeader", "name": "location"}` |
| `"source": "response.headers"` | `"from": {"source": "responseHeaders"}` |
| `"source": "response.statusCode"` | `"from": {"source": "responseStatusCode"}` |
| `"source": "eraser"` | `"from": {"source": "eraser"}` |
| `"source": "math.@{a}*2"` | `"from": {"source": "math", "expression": "@{a}*2"}` |
| `"source": "random.10.30"` | `"from": {"source": "random", "min": 10, "max": 30}` |
| `"source": "randomset.rock\|paper\|scissors"` | `"from": {"source": "randomset", "values": ["rock", "paper", "scissors"]}` |
| `"source": "timestamp.ms"` | `"from": {"source": "timestamp", "unit": "ms"}` |
| `"source": "strftime.%Y-%m-%d"` | `"from": {"source": "strftime", "fmt": "%Y-%m-%d"}` |
| `"source": "recvseq"` / `"sendseq"` | `"from": {"source": "recvseq"}` / `{"source": "sendseq"}` |
| `"source": "var.id"` | `"from": {"source": "var", "name": "id"}` |
| `"source": "vault.TPL"` | `"from": {"source": "vault", "key": "TPL"}` |
| `"source": "vault.TPL./auth"` | `"from": {"source": "vault", "key": "TPL", "path": "/auth"}` |
| `"source": "value.hello"` | `"from": {"source": "literal", "value": "hello"}` |
| `"source": "inState"` | `"from": {"source": "inState"}` |
| `"source": "serverEvent.requestMethod=POST&requestUri=/x&eventNumber=-1&eventPath=/requestBody"` | `"from": {"source": "serverEvent", "requestMethod": "POST", "requestUri": "/x", "eventNumber": -1, "eventPath": "/requestBody"}` |
| `"source": "txtFile./tmp/in.txt"` | `"from": {"source": "txtFile", "file": "/tmp/in.txt"}` |
| `"source": "command.date +%s"` | `"from": {"source": "command", "command": "date +%s"}` |

Notes:

* The v1 `value.` source is now `literal`: `value` is kept as the field of every literal operand. An empty
  literal is `{"source": "literal", "value": ""}` (v1: `"value."`).
* `random.<min>.+<max>` (explicit plus sign) is normalized to `"max": <n>`.
* **[new]** `var` admits `path`, like `vault` (see [9](#9-variables-and-vaults-hold-native-json)).
* **[removed]** a `randomset` value holding `|` is rejected (`... is not supported yet`): in v1 it could not
  be written anyway.

---

## 5. Targets

All **[auto]**:

| v1 | v2 |
|---|---|
| `"target": "response.body.string"` | `"to": {"target": "responseBodyText"}` |
| `"target": "response.body.hexstring"` | `"to": {"target": "responseBodyHex"}` |
| `"target": "response.body.json.string./a"` | `"to": {"target": "responseBody", "path": "/a"}` (+ `cast string` when the source is not always text) |
| `"target": "response.body.json.integer./a"` | `"to": {"target": "responseBody", "path": "/a"}` + `{"op": "cast", "type": "integer"}` |
| `"target": "response.body.json.object"` | `"to": {"target": "responseBody"}` + `{"op": "cast", "type": "object"}` |
| `"target": "response.body.json.jsonstring./a"` | `"to": {"target": "responseBody", "path": "/a"}` + `{"op": "cast", "type": "json"}` |
| `"target": "request.body.string"` / `hexstring` (client) | `"to": {"target": "requestBodyText"}` / `{"target": "requestBodyHex"}` |
| `"target": "request.body.json.<type>./a"` (client) | `"to": {"target": "requestBody", "path": "/a"}` + cast |
| `"target": "response.header.x-id"` | `"to": {"target": "responseHeader", "name": "x-id"}` |
| `"target": "response.statusCode"` | `"to": {"target": "responseStatusCode"}` |
| `"target": "response.delayMs"` | `"to": {"target": "responseDelayMs"}` |
| `"target": "request.header.x-id"` (client) | `"to": {"target": "requestHeader", "name": "x-id"}` |
| `"target": "request.uri"` / `"request.method"` (client) | `"to": {"target": "requestUri"}` / `{"target": "requestMethod"}` |
| `"target": "request.delayMs"` / `"request.timeoutMs"` (client) | `"to": {"target": "requestDelayMs"}` / `{"target": "requestTimeoutMs"}` |
| `"target": "var.id"` | `"to": {"target": "var", "name": "id"}` |
| `"target": "vault.K"` / `"vault.K./a"` | `"to": {"target": "vault", "key": "K"}` / `{..., "path": "/a"}` |
| `"target": "vault.K.json.object"` / `"vault.K.json.jsonstring"` | `"to": {"target": "vault", "key": "K"}` + `cast object` / `cast json` |
| `"target": "outState"` | `"to": {"target": "outState"}` |
| `"target": "outState.GET"` | `"to": {"target": "outState", "method": "GET"}` |
| `"target": "outState.GET./app/v1/items/1"` | `"to": {"target": "outState", "method": "GET", "uri": "/app/v1/items/1"}` |
| `"target": "txtFile./tmp/out.txt"` | `"to": {"target": "txtFile", "file": "/tmp/out.txt"}` |
| `"target": "udpSocket./tmp/udp.sock"` | `"to": {"target": "udpSocket", "socket": "/tmp/udp.sock"}` |
| `"target": "udpSocket./tmp/udp.sock\|200"` | `"to": {"target": "udpSocket", "socket": "/tmp/udp.sock", "delayMs": 200}` |
| `"target": "serverEvent.requestMethod=GET&requestUri=/x"` (with eraser) | `"to": {"target": "serverEvent", "requestMethod": "GET", "requestUri": "/x"}` |
| `"target": "clientProvision.myFlow.initial"` | `"to": {"target": "clientProvision", "id": "myFlow", "inState": "initial"}` |
| `"target": "break"` | `"to": {"target": "stop"}` |

Foreign out-state example (the state is still the value of `from`):

```json
v1: {"source": "value.stored", "target": "outState.GET.@{item-uri}"}
v2: {"from": {"source": "literal", "value": "stored"}, "to": {"target": "outState", "method": "GET", "uri": "@{item-uri}"}}
```

---

## 6. Filters

All **[auto]**, except `differentFrom` which no longer exists in v2 (the tool converts the v1 one):

| v1 | v2 |
|---|---|
| `"filter": {"RegexCapture": "id-([0-9]+)"}` | `"filter": [{"op": "regexCapture", "rgx": "id-([0-9]+)"}]` |
| `"filter": {"RegexReplace": {"rgx": "id-([0-9]+)", "fmt": "$1"}}` | `"filter": [{"op": "regexReplace", "rgx": "id-([0-9]+)", "fmt": "$1"}]` |
| `"filter": {"RegexKey": "^item"}` | `"filter": [{"op": "regexKey", "rgx": "^item"}]` |
| `"filter": {"Append": ".com"}` | `"filter": [{"op": "append", "value": ".com"}]` |
| `"filter": {"Prepend": "www."}` | `"filter": [{"op": "prepend", "value": "www."}]` |
| `"filter": {"Sum": 5}` / `{"Multiply": 2}` | `"filter": [{"op": "sum", "value": 5}]` / `[{"op": "multiply", "value": 2}]` |
| `"filter": {"ConditionVar": "ok"}` | `"filter": [{"op": "conditionVar", "name": "ok"}]` |
| `"filter": {"ConditionVar": "!ok"}` | `"filter": [{"op": "conditionVar", "name": "ok", "not": true}]` |
| `"filter": {"EqualTo": "200"}` | `"filter": [{"op": "equalTo", "value": "200"}]` |
| `"filter": {"DifferentFrom": "200"}` | `"filter": [{"op": "equalTo", "value": "200", "not": true}]` |
| `"filter": {"JsonConstraint": {"a": 1}}` | `"filter": [{"op": "jsonConstraint", "value": {"a": 1}}]` |
| `"filter": {"SchemaId": "mySchema"}` | `"filter": [{"op": "schemaId", "id": "mySchema"}]` |
| `"filter": {"Split": {"size": 2, "sep": "."}}` | `"filter": [{"op": "split", "size": 2, "sep": "."}]` |
| `"filter": {"BaseConvert": {"in": 10, "out": 16}}` | `"filter": [{"op": "baseConvert", "in": 10, "out": 16}]` |
| `"filter": {"Strptime": {"fmt": "%Y-%m-%d", "unit": "s"}}` | `"filter": [{"op": "strptime", "fmt": "%Y-%m-%d", "unit": "s"}]` |
| `"filter": {"Strftime": {"fmt": "%H:%M", "unit": "ms"}}` | `"filter": [{"op": "strftime", "fmt": "%H:%M", "unit": "ms"}]` |
| `"filter": "Size"` | `"filter": [{"op": "size"}]` |

**[removed]** `differentFrom`: there is one comparison filter, `equalTo`, negated with `"not": true` exactly as
`conditionVar`.

**[new]** `conditionVar` accepts a pointer into the variable, `"name": "doc/enabled"` (see [10](#10-variable-references-)).

---

## 7. Typing: the cast filter

### 7.1 Typed targets become a cast **[auto]**

In v1 the type was part of the target name (`response.body.json.integer`, `vault.K.json.object`...). In v2
the target only says where, and the type is the `cast` filter:

```json
v1: {"source": "request.uri.path", "target": "response.body.json.unsigned./uriId",
     "filter": {"RegexReplace": {"rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"}}}

v2: {"from": {"source": "requestUriPath"},
     "filter": [{"op": "regexReplace", "rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"},
                {"op": "cast", "type": "unsigned"}],
     "to": {"target": "responseBody", "path": "/uriId"}}
```

Types: `string`, `integer`, `unsigned`, `float`, `boolean`, `object` (an object or array is required; v1
best-effort typing of `object` targets is kept for migrated rules), `json` (v1 `jsonstring`: the value text is
parsed as json).

### 7.2 Body targets without cast **[new]**

A json body target without cast writes the value **with the type the source yields**: a text source gives a
string, a json source its json (object, number...). v1 always needed a typed target. That is why the tool does
not produce `{"op": "cast", "type": "string"}` for sources that always yield text (`literal`,
`requestHeader`, `requestUriParam`, `requestUri`, `requestUriPath`, `strftime`, `inState`):

```json
v1: {"source": "request.header.app-version", "target": "response.body.json.string./appVersion"}
v2: {"from": {"source": "requestHeader", "name": "app-version"}, "to": {"target": "responseBody", "path": "/appVersion"}}

v2, keeping a json node as it is (no cast needed):
{"from": {"source": "requestBody", "path": "/address"}, "to": {"target": "responseBody", "path": "/address"}}
```

### 7.3 Cast anywhere, on any target **[new]**

The cast is an ordinary filter: it can appear at any position of the chain and before any target (v1 could
only type body and vault targets). A value which cannot be converted fails the filter (target untouched,
`onFilterFail` runs):

```json
{"from": {"source": "requestUriParam", "name": "n"},
 "filter": [{"op": "cast", "type": "integer"}, {"op": "sum", "value": 1000}],
 "to": {"target": "var", "name": "n"}}

{"from": {"source": "requestBody", "path": "/count"},
 "filter": [{"op": "cast", "type": "unsigned"}],
 "to": {"target": "responseStatusCode"},
 "onFilterFail": [{"from": {"source": "literal", "value": "400"}, "to": {"target": "responseStatusCode"}}]}
```

---

## 8. Filter chains

**[new]** v1 allowed exactly one filter per rule, so multi-step processing needed intermediate variables. v2
`filter` is a list, run in order, each filter over the output of the previous one; the first failing filter
stops the chain (target untouched, `onFilterFail` runs):

```json
v1 (three rules and two auxiliary variables):
{"source": "request.uri.path", "target": "var.id", "filter": {"RegexReplace": {"rgx": "/id-([0-9]+)", "fmt": "$1"}}},
{"source": "var.id", "target": "var.id2", "filter": {"Sum": 1000}},
{"source": "var.id2", "target": "response.body.json.integer./id"}

v2 (one rule):
{"from": {"source": "requestUriPath"},
 "filter": [{"op": "regexReplace", "rgx": "/id-([0-9]+)", "fmt": "$1"},
            {"op": "cast", "type": "integer"},
            {"op": "sum", "value": 1000}],
 "to": {"target": "responseBody", "path": "/id"}}
```

Capture groups of `regexCapture` / `regexKey` (the `<name>.<n>` variables, or the vault group object) are only
published when that filter is the **last** one of the chain; otherwise the whole match (or the matched key
value) goes on to the next filter.

---

## 9. Variables and vaults hold native json

**[behaviour]** In v1 a `var` held strings only (objects were serialized to their json text), and a vault
stored objects and arrays natively but scalars as texts. In v2 both hold native json values with the type the
source (or a cast) yields; the only difference between them is the scope.

| Value written | v1 `var` | v1 `vault` | v2 `var` and `vault` |
|---|---|---|---|
| `{"source": "recvseq"}` | `"7"` | `"7"` | `7` |
| `{"source": "math", "expression": "1+1"}` | `"2"` | `"2"` | `2` |
| `{"source": "literal", "value": "2"}` | `"2"` | `"2"` | `"2"` (a literal is text) |
| `{"source": "requestBody", "path": "/flag"}` (`false`) | `"false"` | `"false"` | `false` |
| `{"source": "requestBody"}` (`{"a":1}`) | `"{\"a\":1}"` | `{"a":1}` | `{"a":1}` |

What it changes in practice:

1. **An object stored in a `var` reaches a body as an object.** v1 wrote its json text; the README told to use
   `cast json` (v1 `jsonstring`) to parse it back. That workaround still works (it parses a string), but on an
   object the `object` cast is now the natural one:

   ```json
   {"from": {"source": "requestBody"}, "to": {"target": "var", "name": "stored"}},
   {"from": {"source": "var", "name": "stored"}, "filter": [{"op": "cast", "type": "object"}], "to": {"target": "responseBody"}}
   ```

   v1 result: `"{\"value\":42}"` (a string). v2 result: `{"value": 42}`.

2. **Vault values are listed with their type** by `GET /admin/v2/vault`.

3. **`conditionVar` over a numeric `0` or a boolean `false` is false** (see [12](#12-boolean-criterion)).

4. **`@{}` substitution is unchanged**: a non-string value is inserted as its compact json text, which is
   what v1 stored (`@{seq}` -> `7`, `@{doc}` -> `{"a":1}`).

To keep v1 typing for a value, add a string cast before the store target:

```json
{"from": {"source": "recvseq"}, "filter": [{"op": "cast", "type": "string"}], "to": {"target": "var", "name": "seq"}}
```

**[new]** `var` admits a json pointer `path`, as source and as target, like `vault`:

```json
{"from": {"source": "requestBody"}, "to": {"target": "var", "name": "doc"}},
{"from": {"source": "var", "name": "doc", "path": "/user/id"}, "to": {"target": "responseBody", "path": "/id"}},
{"from": {"source": "literal", "value": "checked"}, "to": {"target": "var", "name": "doc", "path": "/status"}}
```

Unchanged: `regexCapture` into a `var` still creates the flat variables `<name>` (whole match), `<name>.1`,
`<name>.2`...; into a vault, the object `{"0": ..., "1": ..., ...}`. Chain variables (scoped to the `outState`
chain) still propagate across the chain links, now as json.

---

## 10. Variable references `@{}`

### 10.1 Pointer navigation **[new]**

`@{name/<json pointer>}` reads a node of a stored value (the first `/` starts the pointer). It works in every
field admitting `@{}`, in `math` expressions and in `conditionVar` names. Plain names (no `/`) behave as in
v1, with the same lookup order (variables before vaults):

```json
{"from": {"source": "requestBody"}, "to": {"target": "vault", "key": "user"}},
{"from": {"source": "literal", "value": "Hello @{user/name}, you are @{user/age}"}, "to": {"target": "responseBodyText"}},
{"from": {"source": "math", "expression": "@{user/age} + 1"}, "to": {"target": "responseBody", "path": "/nextAge"}},
{"from": {"source": "literal", "value": "adult"}, "filter": [{"op": "conditionVar", "name": "user/adult"}], "to": {"target": "responseBody", "path": "/kind"}}
```

It also removes the v1 "dotted key" gotcha: a vault key cannot contain dots, so `"key": "RCVD_@{uri_parts.1}"`
was rejected and needed an auxiliary variable. Capturing into a vault, the pointer form needs no dot:

```json
v1 (workaround):
{"source": "request.uri.path", "target": "var.uri_parts", "filter": {"RegexCapture": "/app/v1/(foo)/(bar)/([0-9]+)"}},
{"source": "var.uri_parts.1", "target": "var.group1"},
{"source": "value.0", "target": "vault.RCVD_@{group1}"}

v2:
{"from": {"source": "requestUriPath"}, "filter": [{"op": "regexCapture", "rgx": "/app/v1/(foo)/(bar)/([0-9]+)"}], "to": {"target": "vault", "key": "uri_parts"}},
{"from": {"source": "literal", "value": "0"}, "to": {"target": "vault", "key": "RCVD_@{uri_parts/1}"}}
```

### 10.2 Unknown references are kept literally **[behaviour]** (only for pre-release 5.0 builds)

An unknown variable (or a pointer which does not resolve) is kept literally: `"a@{missing}b"` stays
`"a@{missing}b"`. This is the v1 rule (v1 only replaced existing variables), so 4.x provisions are not
affected; it is listed because early refactor builds replaced them by an empty string.

Consequence worth knowing (same as v1): a `math` expression with an unknown variable does not compile, so the
rule is skipped.

---

## 11. Provision references `#{}`

**[new]** `#{key}` / `#{key/<json pointer>}` is resolved **once, at provision load**, against the vault, in
every string value of the provision (`requestUri`, `responseBody`, headers, any transformation field...). The
stored provision (and its `GET`) holds the resolved values. Strings are inserted as they are, other values as
their compact json text.

The main use: share regular expressions among many provisions. Regex filters are compiled at load, so they
cannot hold runtime `@{}` variables; `#{}` is resolved before that compilation:

```bash
curl --http2-prior-knowledge -XPOST http://localhost:8074/admin/v2/vault -H'content-type:application/json' \
     -d'{"patterns": {"imsi": "([0-9]{15})", "msisdn": "(34[0-9]{9})"}}'
```

```json
{"from": {"source": "requestUriPath"},
 "filter": [{"op": "regexCapture", "rgx": "/subscribers/imsi-#{patterns/imsi}"}],
 "to": {"target": "var", "name": "imsi"}}
```

Rules:

* `@{}` is runtime (variables and vaults, every event, the provision keeps the template); `#{}` is load time
  (vaults only, once). Both can coexist.
* A reference which does not exist is kept literally and reported as a warning (logs and the `warning` field
  of the `POST` response); the provision is loaded anyway.
* References are never dynamic: changing the vault entry later is not observed until the provision is loaded
  again.
* Object keys are not scanned; resolution is a single pass (a resolved text is not scanned again).

---

## 12. Boolean criterion

**[behaviour]** Every decision on a value (`conditionVar`, `stop`, `clientProvision`, boolean cast) uses one
criterion:

| Value | Result |
|---|---|
| `null`, `false`, `0` (any numeric zero), `""` | false |
| anything else: any non-empty string (`"0"`, `"false"` included), non-zero numbers, `true`, objects and arrays | true |

v1 tested the text of the value (non-empty is true). Text values behave as before; what changes is that a
numeric `0` or a boolean `false` (which v1 stored as the non-empty texts `"0"` / `"false"`) is now false:

```json
{"from": {"source": "math", "expression": "@{recvseq} > 10"}, "to": {"target": "var", "name": "greater"}},
{"from": {"source": "literal", "value": "big"}, "filter": [{"op": "conditionVar", "name": "greater"}], "to": {"target": "responseBodyText"}}
```

* v1: `greater` held `"0"` or `"1"`, both non-empty, so the condition was **always true**. The documented
  pattern added `{"EqualTo": "1"}` to the math rule so that `greater` was undefined when false.
* v2: `greater` holds `0` or `1`, so the condition works directly. The v1 pattern with `equalTo` still works.

Per context:

| Context | true | false | absent |
|---|---|---|---|
| `conditionVar` | passes | fails (`onFilterFail` runs) | fails |
| `conditionVar` + `"not": true` | fails | passes | passes |
| `stop` | interrupts the list | ignored | rule skipped |
| `clientProvision` | triggers | not triggered | rule skipped |
| `cast` boolean | `true` | `false` | rule skipped |

So `stop` and `clientProvision` fed by a numeric `0` or a boolean `false` no longer stop / trigger (v1: the
texts `"0"` / `"false"` did).

---

## 13. Eraser

### 13.1 One meaning: remove what lives at the target **[behaviour]**

| Target | v1 | v2 |
|---|---|---|
| `var` / `vault` | removes the entry (vault with path: whole entry, path ignored) | removes the entry; **with `path`, only that node** |
| body + `object` cast (v1 `json.object`) | removes the object key at path / clears the body | same, and **array items** too (`"path": "/items/0"`) |
| body without cast | (not expressible) | removes the node / clears the body |
| body + `string` cast (v1 `json.string`) | empty string at path | same |
| `responseBodyText` / `outState` | empty string | same |
| `txtFile` / `binFile` | empties the file | same |
| `serverEvent` / `clientEvent` | purges the events at the address | same |
| missing node | created null parents | **no-op** |

```json
{"from": {"source": "eraser"}, "to": {"target": "vault", "key": "doc", "path": "/tmp"}}
{"from": {"source": "eraser"}, "to": {"target": "responseBody", "path": "/items/0"}}
{"from": {"source": "eraser"}, "to": {"target": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/item/1"}}
```

Migrated rules keep their v1 flavour: the tool always adds the cast of v1 typed body targets.

### 13.2 Conditional erase **[new]**

v1 silently skipped any `eraser` rule with a filter. In v2 `eraser` accepts `conditionVar` filters (one or
several, optionally negated): the target is erased only when every condition holds; otherwise it is kept and
`onFilterFail` runs. This also gives conditional event purges:

```json
{"from": {"source": "eraser"}, "filter": [{"op": "conditionVar", "name": "done"}],
 "to": {"target": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/item/@{id}"}}
```

### 13.3 What is rejected **[removed]**

* `eraser` with any filter other than `conditionVar` (and the cast): v1 skipped the rule, v2 rejects it.
* `eraser` feeding `stop` or `clientProvision`: v1 documented it as a way to "disable" a trigger, but such a
  rule does nothing. Remove the rule (or the trigger).
* An event purge target (`serverEvent` / `clientEvent` in `to`) fed by a source other than `eraser`: v1
  skipped it.

---

## 14. Stop (former break)

**[auto]** `"target": "break"` is `{"target": "stop"}`. **[behaviour]** It now acts on a true value (see
[12](#12-boolean-criterion)) instead of a non-empty text:

```json
v1: {"source": "response.statusCode", "target": "break", "filter": {"DifferentFrom": "200"}}
v2: {"from": {"source": "responseStatusCode"}, "filter": [{"op": "equalTo", "value": "200", "not": true}], "to": {"target": "stop"}}
```

---

## 15. Events addressing

**[auto]** The v1 query string of `serverEvent` / `clientEvent` becomes plain fields with the same names as the
inspection API (`GET /admin/v2/server-data?...`). Values are plain json, **not url-encoded** (the tool decodes
them as the v1 runtime did); numeric fields accept numbers or a text holding a variable:

```json
v1: {"source": "serverEvent.requestMethod=POST&requestUri=/app/v1/items%3Fid%3D5&eventNumber=-1&eventPath=/requestBody",
     "target": "response.body.json.object"}

v2: {"from": {"source": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/items?id=5",
              "eventNumber": -1, "eventPath": "/requestBody"},
     "filter": [{"op": "cast", "type": "object"}],
     "to": {"target": "responseBody"}}
```

Fields: `clientEndpointId` (client), `requestMethod`, `requestUri` or `requestUriStartsWith` (literal prefix,
sources only), `eventNumber`, `eventPath` (sources only), `recvseq` (server) / `sendseq` (client).

**[removed]** A v1 query with an unknown or repeated key is rejected by the tool (v1 ignored it or produced an
empty address).

---

## 16. Execution trace

**[new]** Each stored event can include the trace of its transformation pipeline: which rule was applied and
why a rule was skipped (a filter failed, a source or path did not resolve, a cast failed...). Disabled by
default:

```bash
curl --http2-prior-knowledge -XPUT "http://localhost:8074/admin/v2/server-data/configuration?executionTrace=true"
# helpers: server_data_configuration --enable-trace   /   client_data_configuration --enable-trace
```

```bash
curl --http2-prior-knowledge "http://localhost:8074/admin/v2/server-data?requestMethod=GET&requestUri=/app/v1/trace/1&eventNumber=-1"
```

```json
"executionTrace": [
  {"rule": "/transform/0", "applied": true},
  {"rule": "/transform/1", "applied": false, "note": "EqualTo is false; rule skipped"},
  {"rule": "/transform/1/onFilterFail/0", "applied": true}
]
```

`rule` is a json pointer into the provision (`/transform/<n>`, `/onResponseTransform/<n>` for client response
transformations, `.../onFilterFail/<m>`). It needs the event storage; meant for test development, not for load
testing.

---

## 17. Stricter validation

**[removed]** v1 accepted, and silently skipped at runtime, some rules that could never do anything. v2 rejects
them at load (`400`), and the tool reports them so they can be removed:

* `eraser` with a filter other than `conditionVar` / cast.
* `eraser` feeding `stop` / `clientProvision`.
* Event purge target with a source other than `eraser`.
* Unknown fields in any stage (the v2 schema is closed: a typo such as `"delay"` instead of `"delayMs"` is an
  error, not an ignored field).
* Event address with unknown or repeated fields.

Provisions loaded with a v1 grammar rule get a `400` whose message hints at `h2agent-migrate`.

---

## 18. Math source fixes

**[behaviour]** `math` expressions are compiled once at load (variables bound), which made two v1 bugs visible;
they are fixed:

* An expression that does not compile (e.g. `2*/`): v1 evaluated it anyway and wrote a meaningless result;
  v2 skips the rule (target untouched), as any other source that cannot be resolved.
* A whole result beyond the `int` range (e.g. `3000000000`): v1 stored it as a float; v2 stores an integer.

---

## 19. Startup order

**[behaviour]** With `--vault` and provision files given at startup, the vault is now loaded **before** the
provision files (`--traffic-server-provision`, `--traffic-client-provision`), so that they can hold `#{}`
references. Nothing else depends on that order.

---

## 20. Checklist

1. Replace `/admin/v1/` by `/admin/v2/` in scripts, runners and CI (`h2agent-migrate --scan` lists them).
2. Change client provision triggers to `POST /admin/v2/client-provision/<id>/trigger`.
3. Change admin schema reads to `GET /admin/v2/api-schema/<resource>`.
4. Migrate provision files with `h2agent-migrate --in ... --out ...`. For rules embedded in scripts or tests,
   use `--scan` to find them, and convert each one wrapped as a provision fragment:
   `echo '{"transform": [<v1 rule>]}' | h2agent-migrate --in -`.
5. Remove the rules the tool rejects as "v1 never applies ..." (they did nothing).
6. Review the **[behaviour]** items against your provisions:
   * objects stored in a `var` and written to a body ([9](#9-variables-and-vaults-hold-native-json));
   * `conditionVar` / `stop` / `clientProvision` fed by numbers or booleans ([12](#12-boolean-criterion));
   * vault values compared as texts in tests (`GET /admin/v2/vault`);
   * eraser on paths that may not exist, or on vault paths ([13](#13-eraser));
   * `math` expressions that did not compile ([18](#18-math-source-fixes)).
7. Tests comparing the whole data configuration json: add `"executionTrace": false`.
8. Optionally simplify: filter chains instead of auxiliary variables ([8](#8-filter-chains)), `@{name/pointer}`
   ([10](#10-variable-references-)), `#{}` for shared regular expressions ([11](#11-provision-references-)),
   conditional erase ([13.2](#132-conditional-erase-new)), and `executionTrace` while developing.
