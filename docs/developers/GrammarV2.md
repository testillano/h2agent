# Grammar v2: v1 -> v2 mapping

This is the exact syntax mapping the migration tool (`h2agent-migrate`) produces and the v2 parser will accept (phase [0.7]). It is a **syntax-only** step: every v2 rule behaves exactly as its v1 origin. Semantic changes (store unification, `purge`, `stop`, `If`, chainable regex semantics...) come in later phases, each with its own migrator rule.

## Tool

`h2agent-migrate --in <v1 provisions> [--out <file>]` applies this mapping (server and client provisions, single object or array); `--reverse` goes back to v1 for the v2 subset that has a v1 equivalent, and `--scan <dir>` lists v1 leftovers. Unit tests (`GrammarMigrator_test`) check every row of the tables below, the `v1 -> v2 -> v1` identity, and that `model::Transformation` loads the original and the round-tripped v1 rule identically.

Two v1 spellings are normalized (same behaviour): `random.<min>.+<max>` loses the `+` sign, and a vault key never contains dots (v1 already reads `vault.A.B` as key `A` with an invalid path; use `vault.A:B`).

## Rule shape

```json
v1: {"source": "<string>", "target": "<string>", "filter": {<one filter>}, "onFilterFail": [<v1 rules>]}
v2: {"from": "<kind>" | {<kind>: <main argument>, ...}, "to": "<kind>" | {<kind>: <main argument>, ...}, "filter": [<filters>], "onFilterFail": [<v2 rules>]}
```

* `from` / `to`: a kind **without arguments is a plain string** (`"requestHeaders"`); a kind **with arguments is an object with exactly one kind key**, whose value is the main argument, other arguments being sibling fields (`{"vault": "K", "path": "/a"}`). Each kind has exactly one valid form: `{"requestHeaders": true}` or `"vault"` are errors, not alternatives.
* `filter`: always a list (v1 single filter -> one-element list). Its items follow the same rule: a string for filters without arguments (`"Size"`), a one-key object otherwise. Typed v1 targets add a final `{"as": <type>}`. Filters run **in list order**, each one on the output of the previous one; the first failing filter stops the chain (target untouched, `onFilterFail` runs). The migrator only produces `[<v1 filter>]`, `[{"as": ...}]` or `[<v1 filter>, {"as": ...}]`, which is exactly the v1 order (filter first, target typing last).
* `onFilterFail`: same conversion, recursively.
* Paths are JSON pointers (`""` = whole document). Nothing in the grammar is separated by dots any more.
* Everything else in a provision (`requestMethod`, `responseCode`, `requestHeaders`, `inState`, `outState`, ...) is unchanged.

## Sources (`from`)

| v1 `source` | v2 `from` | Context |
|---|---|---|
| `request.uri` | `"requestUri"` | server, client |
| `request.uri.path` | `"requestUriPath"` | server |
| `request.uri.param.<name>` | `{"requestUriParam": "<name>"}` | server |
| `request.body` / `request.body./a/b` | `{"requestBody": ""}` / `{"requestBody": "/a/b"}` | server, client |
| `response.body` / `response.body./a` | `{"responseBody": ""}` / `{"responseBody": "/a"}` | server (body being built), client response |
| `request.header.<name>` | `{"requestHeader": "<name>"}` | server, client |
| `request.headers` | `"requestHeaders"` | server, client |
| `response.header.<name>` | `{"responseHeader": "<name>"}` | client response |
| `response.headers` | `"responseHeaders"` | client response |
| `response.statusCode` | `"responseStatusCode"` | client response |
| `eraser` | `"eraser"` | all (retired in phase [6]) |
| `math.<expression>` | `{"math": "<expression>"}` | all |
| `random.<min>.<max>` | `{"random": [<min>, <max>]}` | all |
| `randomset.<a>\|<b>\|<c>` | `{"randomset": ["<a>", "<b>", "<c>"]}` | all |
| `timestamp.<unit>` | `{"timestamp": "<s\|ms\|us\|ns>"}` | all |
| `strftime.<format>` | `{"strftime": "<format>"}` | all |
| `recvseq` | `"recvseq"` | server |
| `sendseq` | `"sendseq"` | client |
| `var.<name>` | `{"var": "<name>"}` | all |
| `vault.<key>` / `vault.<key>./a` | `{"vault": "<key>"}` / `{"vault": "<key>", "path": "/a"}` | all |
| `value.<text>` | `{"value": "<text>"}` | all |
| `inState` | `"inState"` | all |
| `serverEvent.<query>` | `{"serverEvent": "<query>"}` | all |
| `clientEvent.<query>` | `{"clientEvent": "<query>"}` | all |
| `txtFile.<path>` / `binFile.<path>` | `{"txtFile": "<path>"}` / `{"binFile": "<path>"}` | all |
| `command.<command line>` | `{"command": "<command line>"}` | all |

`<query>` keeps the inspection API format (`requestMethod=POST&requestUri=/x&eventNumber=-1&eventPath=/requestBody`), so it can still be tried with `GET /admin/v2/server-data?<query>`.

## Targets (`to`)

| v1 `target` | v2 `to` (+ added filter) | Context |
|---|---|---|
| `response.body.string` | `"responseBodyText"` | server |
| `response.body.hexstring` | `"responseBodyHex"` | server |
| `response.body.json.<type>` / `response.body.json.<type>./a` | `{"responseBody": ""}` / `{"responseBody": "/a"}` + `{"as": <type'>}` | server |
| `request.body.string` / `request.body.hexstring` | `"requestBodyText"` / `"requestBodyHex"` | client request |
| `request.body.json.<type>[./a]` | `{"requestBody": "" \| "/a"}` + `{"as": <type'>}` | client request |
| `response.header.<name>` | `{"responseHeader": "<name>"}` | server |
| `response.statusCode` | `"responseStatusCode"` | server |
| `response.delayMs` | `"responseDelayMs"` | server |
| `request.header.<name>` | `{"requestHeader": "<name>"}` | client request |
| `request.uri` / `request.method` | `"requestUri"` / `"requestMethod"` | client request |
| `request.delayMs` / `request.timeoutMs` | `"requestDelayMs"` / `"requestTimeoutMs"` | client request |
| `var.<name>` | `{"var": "<name>"}` | all |
| `vault.<key>[./a]` | `{"vault": "<key>"[, "path": "/a"]}` | all |
| `vault.<key>.json.object[./a]` | `{"vault": "<key>"[, "path": "/a"]}` + `{"as": "object"}` | all |
| `vault.<key>.json.jsonstring[./a]` | `{"vault": "<key>"[, "path": "/a"]}` + `{"as": "json"}` | all |
| `outState` | `"outState"` | all |
| `outState.<METHOD>` / `outState.<METHOD>.<uri>` | `{"foreignOutState": "<METHOD>"[, "uri": "<uri>"]}` | server |
| `txtFile.<path>` / `binFile.<path>` | `{"txtFile": "<path>"}` / `{"binFile": "<path>"}` | all |
| `udpSocket.<path>` / `udpSocket.<path>\|<ms>` | `{"udpSocket": "<path>"[, "delayMs": <ms>]}` | all |
| `serverEvent.<query>` / `clientEvent.<query>` (with `eraser`) | `{"serverEvent": "<query>"}` / `{"clientEvent": "<query>"}` | all (become `purge` in phase [6]) |
| `clientProvision.<id>.<inState>` | `{"clientProvision": "<id>", "inState": "<inState>"}` | server |
| `break` | `"break"` | all (becomes `stop` in phase [5]) |

`<type'>` for the v1 `json.<type>` targets: `string`, `integer`, `unsigned`, `float`, `boolean`, `object` (v1 `object` best-effort typing, kept as is), `json` (v1 `jsonstring`: parse the text as JSON).

The state written by `outState` is still the value of `from` (as in v1), e.g. `{"from": {"value": "done"}, "to": "outState"}` or `{"from": {"value": "done"}, "to": {"foreignOutState": "POST", "uri": "/x"}}` (a separate kind, so that `outState` keeps a single form).

## Filters

Same names and arguments as v1, in a list:

```json
v1: "filter": {"RegexReplace": {"rgx": "...", "fmt": "$1"}}      v2: "filter": [{"RegexReplace": {"rgx": "...", "fmt": "$1"}}]
v1: "filter": "Size"                                              v2: "filter": ["Size"]
```

`Append`, `Prepend`, `Sum`, `Multiply`, `ConditionVar`, `EqualTo`, `DifferentFrom`, `JsonConstraint`, `SchemaId`, `RegexCapture`, `RegexReplace`, `RegexKey`, `Split`, `BaseConvert`, `Strptime`, `Strftime`, `Size`: unchanged contents. New: `{"as": "string|integer|unsigned|float|boolean|object|json"}` (only produced by the migrator for typed v1 targets at this step).

## Full example

```json
v1:
{"source": "request.uri.path", "target": "response.body.json.unsigned./uriId",
 "filter": {"RegexReplace": {"rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"}}}

v2:
{"from": "requestUriPath", "to": {"responseBody": "/uriId"},
 "filter": [{"RegexReplace": {"rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"}}, {"as": "unsigned"}]}
```

## Decisions (review 2026-10-30)

1. Kinds and filters without arguments are plain strings (`"requestHeaders"`, `"Size"`), never `{"x": true}`. Kinds that may grow an argument already take a mandatory one (`requestBody: ""` for the whole document), so the string form is reserved for atomic kinds.
2. Raw body targets are separate kinds: `"responseBodyText"` / `"responseBodyHex"` (and `"requestBodyText"` / `"requestBodyHex"` on client).
3. `{"as": "object"}` is kept: it reproduces the v1 `json.object` best-effort typing (a non-object value is converted from its text to integer, unsigned, float or boolean, in that order). It is **not** a no-op once stores keep native JSON types: for text sources (`value`, headers, uri params...) it still turns `"30"` into `30`. The phase that introduces native store types must replace it with the concrete `as` (or drop it) per source, in its own migrator rule.
4. Filter lists are chains executed in order. The v2 parser accepts chains of any length from phase [0.7] (no separate "chainable filters" phase); the migrator still produces at most `[<v1 filter>, {"as": ...}]`.
