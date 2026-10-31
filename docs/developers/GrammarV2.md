# Grammar v2: v1 -> v2 mapping

This is the exact syntax mapping the migration tool (`h2agent-migrate`) produces and the v2 parser accepts. It is a **syntax-only** step: every v2 rule behaves as its v1 origin, except for the value typing of the stores (see [Store values](#store-values)). Further semantic changes (`If`, chainable regex semantics...) come in later phases, each with its own migrator rule.

## Tool

`h2agent-migrate --in <v1 provisions> [--out <file>]` applies this mapping (server and client provisions, single object or array); `--reverse` goes back to v1 for the v2 subset that has a v1 equivalent, and `--scan <dir>` lists v1 leftovers. Unit tests (`GrammarMigrator_test`) check every row of the tables below and the `v1 -> v2 -> v1` identity; the unit and component tests run the migrated provisions.

## Rule shape

Every rule has the same three stages, executed in this order:

```json
{
  "from":   {"source": "<kind>", <fields>},             // 1. capture a value
  "filter": [{"op": "<filter>", <fields>}, ...],         // 2. optional chain, in list order
  "to":     {"target": "<kind>", <fields>},              // 3. write (or act on) the value
  "onFilterFail": [<rules>]                              // optional, when a filter fails
}
```

* **Naming:** every kind (`source`, `target` and `op` values) and every field is lowerCamelCase; v1 PascalCase filter names are lowercased (`RegexReplace` -> `regexReplace`).
* **One form per kind.** `source`, `target` and `op` are the discriminators; every other argument is a named field. A kind without arguments is just `{"source": "recvseq"}`. There are no string shortcuts.
* **Fields are named after their role**, and the same name means the same thing in every kind:

  | Field | Meaning | Kinds |
  |---|---|---|
  | `path` | json pointer inside the document; **absent = whole document** (json or text) | `requestBody`, `responseBody`, `vault` |
  | `name` | variable, header or query parameter name | `var`, `requestHeader`, `responseHeader`, `requestUriParam`, `conditionVar` |
  | `key` | vault entry key (no dots) | `vault` |
  | `id` | identifier | `clientProvision`, `schemaId` |
  | `file` / `socket` | file or unix socket path | `txtFile`, `binFile` / `udpSocket` |
  | `value` | literal (text, number or json depending on the kind) | `literal`, `append`, `prepend`, `equalTo`, `sum`, `multiply`, `jsonConstraint` |
  | `rgx` / `fmt` | regular expression / format | regex filters / `strftime` (source), `regexReplace`, `strptime`, `strftime` (filters) |

* **Filters run in list order**, each one on the output of the previous one; the first failing filter stops the chain (target untouched, `onFilterFail` runs). The cast is a filter (it can only appear in a filter list): `{"op": "cast", "type": "string|integer|unsigned|float|boolean|object|json"}`.
* **Paths are json pointers.** Nothing in the grammar is separated by dots, and nothing is url-encoded any more.
* Variables (`@{name}`) are substituted in the same fields as in v1. New in v2: `@{name/<json pointer>}` navigates into the stored value (`@{doc/a/b}`, `@{caps/1}`); `conditionVar` accepts the same reference.
* Everything else in a provision (`requestMethod`, `responseCode`, `inState`, `outState`, ...) is unchanged.

## Sources (`from`)

| v1 `source` | v2 `from` | Context |
|---|---|---|
| `request.uri` | `{"source": "requestUri"}` | server, client |
| `request.uri.path` | `{"source": "requestUriPath"}` | server |
| `request.uri.param.<name>` | `{"source": "requestUriParam", "name": "<name>"}` | server |
| `request.body` / `request.body./a/b` | `{"source": "requestBody"}` / `{"source": "requestBody", "path": "/a/b"}` | server, client |
| `response.body` / `response.body./a` | `{"source": "responseBody"}` / `{"source": "responseBody", "path": "/a"}` | server (body being built), client response |
| `request.header.<name>` / `request.headers` | `{"source": "requestHeader", "name": "<name>"}` / `{"source": "requestHeaders"}` | server, client |
| `response.header.<name>` / `response.headers` | `{"source": "responseHeader", "name": "<name>"}` / `{"source": "responseHeaders"}` | client response |
| `response.statusCode` | `{"source": "responseStatusCode"}` | client response |
| `eraser` | `{"source": "eraser"}` (see [Eraser](#eraser)) | all |
| `math.<expression>` | `{"source": "math", "expression": "<expression>"}` | all |
| `random.<min>.<max>` | `{"source": "random", "min": <min>, "max": <max>}` | all |
| `randomset.<a>\|<b>\|<c>` | `{"source": "randomset", "values": ["<a>", "<b>", "<c>"]}` | all |
| `timestamp.<unit>` | `{"source": "timestamp", "unit": "s\|ms\|us\|ns"}` | all |
| `strftime.<format>` | `{"source": "strftime", "fmt": "<format>"}` | all |
| `recvseq` / `sendseq` | `{"source": "recvseq"}` / `{"source": "sendseq"}` | server / client |
| `var.<name>` | `{"source": "var", "name": "<name>"}` (v2 also admits `"path"`) | all |
| `vault.<key>` / `vault.<key>./a` | `{"source": "vault", "key": "<key>"}` / `{..., "path": "/a"}` | all |
| `value.<text>` | `{"source": "literal", "value": "<text>"}` | all |
| `inState` | `{"source": "inState"}` | all |
| `serverEvent.<query>` / `clientEvent.<query>` | `{"source": "serverEvent", <event fields>}` / `{"source": "clientEvent", <event fields>}` | all |
| `txtFile.<path>` / `binFile.<path>` | `{"source": "txtFile", "file": "<path>"}` / `{"source": "binFile", "file": "<path>"}` | all |
| `command.<command line>` | `{"source": "command", "command": "<command line>"}` | all |

### Event fields

The v1 query string becomes fields with the same names as the inspection API (`GET /admin/v2/server-data?...`); values are plain json, **not url-encoded** (the migrator url-decodes them exactly as the v1 runtime did):

| Field | Type | `serverEvent` | `clientEvent` |
|---|---|---|---|
| `clientEndpointId` | string | - | source, purge |
| `requestMethod` | string | source, purge | source, purge |
| `requestUri` / `requestUriStartsWith` | string (raw uri, e.g. `/app/v1/foo?id=5&name=test`) | source, purge / source | source, purge / source |
| `eventNumber` | integer (negative from the end) | source, purge | source, purge |
| `eventPath` | json pointer | source | source |
| `recvseq` / `sendseq` | unsigned | source, purge / - | - / source, purge |

Numeric fields also accept a string holding a variable (`"@{seq}"`). A v1 query with a repeated or unknown key is rejected by the migrator (v1 silently ignored it or produced an empty address).

## Targets (`to`)

| v1 `target` | v2 `to` (+ added filter) | Context |
|---|---|---|
| `response.body.string` / `response.body.hexstring` | `{"target": "responseBodyText"}` / `{"target": "responseBodyHex"}` | server |
| `response.body.json.<type>` / `response.body.json.<type>./a` | `{"target": "responseBody"}` / `{"target": "responseBody", "path": "/a"}` + `{"op": "cast", "type": <type'>}` (no cast for `string` from a text source) | server |
| `request.body.string` / `request.body.hexstring` | `{"target": "requestBodyText"}` / `{"target": "requestBodyHex"}` | client request |
| `request.body.json.<type>[./a]` | `{"target": "requestBody"[, "path": "/a"]}` + `{"op": "cast", "type": <type'>}` | client request |
| `response.header.<name>` | `{"target": "responseHeader", "name": "<name>"}` | server |
| `response.statusCode` / `response.delayMs` | `{"target": "responseStatusCode"}` / `{"target": "responseDelayMs"}` | server |
| `request.header.<name>` | `{"target": "requestHeader", "name": "<name>"}` | client request |
| `request.uri` / `request.method` | `{"target": "requestUri"}` / `{"target": "requestMethod"}` | client request |
| `request.delayMs` / `request.timeoutMs` | `{"target": "requestDelayMs"}` / `{"target": "requestTimeoutMs"}` | client request |
| `var.<name>` | `{"target": "var", "name": "<name>"}` | all |
| `vault.<key>[./a]` | `{"target": "vault", "key": "<key>"[, "path": "/a"]}` | all |
| `vault.<key>.json.object[./a]` / `vault.<key>.json.jsonstring[./a]` | same + `{"op": "cast", "type": "object"}` / `{"op": "cast", "type": "json"}` | all |
| `outState` | `{"target": "outState"}` | all |
| `outState.<METHOD>` / `outState.<METHOD>.<uri>` | `{"target": "outState", "method": "<METHOD>"[, "uri": "<uri>"]}` | server |
| `txtFile.<path>` / `binFile.<path>` | `{"target": "txtFile", "file": "<path>"}` / `{"target": "binFile", "file": "<path>"}` | all |
| `udpSocket.<path>` / `udpSocket.<path>\|<ms>` | `{"target": "udpSocket", "socket": "<path>"[, "delayMs": <ms>]}` | all |
| `serverEvent.<query>` / `clientEvent.<query>` (with `eraser`) | `{"target": "serverEvent", <event fields>}` / `{"target": "clientEvent", <event fields>}` | server / client |
| `clientProvision.<id>.<inState>` | `{"target": "clientProvision", "id": "<id>", "inState": "<inState>"}` | server |
| `break` | `{"target": "stop"}` (see [Stop](#stop)) | server, client response |

`<type'>`: `string`, `integer`, `unsigned`, `float`, `boolean`, `object` (v1 `object` best-effort typing, kept as is: a non-object is converted from its text to integer, unsigned, float or boolean, in that order), `json` (v1 `jsonstring`: parse the text as json).

The state written by `outState` is still the value of `from` (as in v1), e.g. `{"from": {"source": "literal", "value": "done"}, "to": {"target": "outState", "method": "POST", "uri": "/x"}}`. `uri` requires `method`.

## Filters (`filter`)

| v1 `filter` | v2 item |
|---|---|
| `{"RegexCapture": "<rgx>"}` | `{"op": "regexCapture", "rgx": "<rgx>"}` |
| `{"RegexReplace": {"rgx": "<rgx>", "fmt": "<fmt>"}}` | `{"op": "regexReplace", "rgx": "<rgx>", "fmt": "<fmt>"}` |
| `{"RegexKey": "<rgx>"}` | `{"op": "regexKey", "rgx": "<rgx>"}` |
| `{"Append": "<text>"}` / `{"Prepend": "<text>"}` | `{"op": "append", "value": "<text>"}` / `{"op": "prepend", "value": "<text>"}` |
| `{"Sum": <n>}` / `{"Multiply": <n>}` | `{"op": "sum", "value": <n>}` / `{"op": "multiply", "value": <n>}` |
| `{"ConditionVar": "<name>"}` / `{"ConditionVar": "!<name>"}` | `{"op": "conditionVar", "name": "<name>"}` / `{..., "not": true}` |
| `{"EqualTo": "<text>"}` / `{"DifferentFrom": "<text>"}` | `{"op": "equalTo", "value": "<text>"}` / `{"op": "equalTo", "value": "<text>", "not": true}` (one comparison filter, negated like `conditionVar`) |
| `{"JsonConstraint": <object\|array>}` | `{"op": "jsonConstraint", "value": <object\|array>}` |
| `{"SchemaId": "<id>"}` | `{"op": "schemaId", "id": "<id>"}` |
| `{"Split": {<size, count, sep, filler, numeric>}}` | `{"op": "split", <size, count, sep, filler, numeric>}` |
| `{"BaseConvert": {"in": <b>, "out": <b>[, "capital": <bool>]}}` | `{"op": "baseConvert", "in": <b>, "out": <b>[, "capital": <bool>]}` |
| `{"Strptime": {"fmt": ..., "unit": ...}}` / `{"Strftime": {...}}` | `{"op": "strptime", "fmt": ..., "unit": ...}` / `{"op": "strftime", ...}` |
| `"Size"` | `{"op": "size"}` |
| (typed v1 target) | `{"op": "cast", "type": <type'>}`, appended last |

The migrator produces at most `[<v1 filter>, {"op": "cast", ...}]`, which is exactly the v1 order (filter first, target typing last). Longer chains are accepted by the grammar and run in list order.

## Store values

`var` and `vault` hold native json values in 5.0.0 (v1: `var` held strings only, the vault stored objects and arrays natively but scalars as strings), and `var` also admits a json pointer `path` (source and target). The migrator does not compensate it, because the value type is only known at runtime. Visible differences:

* a `var` object written to a json body with the `object` cast is the object (v1: its json text);
* numbers and booleans are listed by `GET /admin/v2/vault` with their type (`2`, not `"2"`);
* `conditionVar` over a numeric `0` or a boolean `false` is false (v1: a non-empty string, so true).

`@{name}` substitutions are unchanged (non-string values are inserted as their compact json text, as v1 stored them). To keep a string, add `{"op": "cast", "type": "string"}` before the store target.

## Engine support

The grammar accepts more than the engine runs yet. A provision using any of these is rejected at load time (400, "... is not supported yet"):

* a `randomset` value holding `|`.

A final `cast` on a json body target (any type) or on a `vault` target (`object`/`json`) is run as the typed target of the engine (v1 typed write semantics: merge at root, erase flavours); any other `cast` runs as a chain filter.

## Eraser

`{"source": "eraser"}` provides no value: it removes what lives at the location given by the target (one meaning for every target):

| Target | Effect |
|---|---|
| `var` / `vault` | removes the entry, or only the node at `path` (new in v2: v1 removed the whole vault entry whatever the path) |
| `responseBody` / `requestBody` without cast or with the `object` cast | removes the node at `path` (object key or array item; v1: object keys only), or clears the whole body |
| same with the `string` cast | empty string at `path` (v1 erase flavour of string targets) |
| `responseBodyText`, `outState` | empty string |
| `txtFile` / `binFile` | empties the file |
| `serverEvent` / `clientEvent` | purges the events at that address |

A missing node is a no-op (v1 created null parents when the path did not exist). Any other target ignores it. The migrator produces the cast of v1 typed body targets (`response.body.json.<type>`), so a migrated rule keeps its v1 flavour.

The only filters it accepts are `conditionVar` (one or several, optionally negated), so that the target is erased only when every condition holds, and the `cast`. v1 skipped silently any `eraser` rule carrying a filter, so conditional erase is a **new capability** (no v1 equivalent): the schema rejects any other filter with `eraser` instead of skipping the rule. When a condition does not hold, the target is kept and `onFilterFail` runs.

```json
{"from": {"source": "eraser"}, "to": {"target": "vault", "key": "tmp"}}
{"from": {"source": "eraser"}, "to": {"target": "var", "name": "doc", "path": "/a/b"}}
{"from": {"source": "eraser"}, "filter": [{"op": "conditionVar", "name": "done"}], "to": {"target": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/item/1"}}
```

Event storage is read-only for transformations: erasing is the only write `serverEvent` / `clientEvent` accept as targets, and `eraser` is the only source they accept (v1 skipped any other source silently, the schema rejects it). A dedicated purge target was considered (phase [6]) and discarded: it would only be a longer spelling of this rule.

An `eraser` cannot feed `stop` nor `clientProvision`: they act on a true value, so v1 did nothing with such a rule (the documented way to "disable" a trigger). The schema rejects it, and the migrator reports it to be removed.

## Boolean criterion

`conditionVar`, `stop`, `clientProvision` and the boolean `cast` share one criterion (per-context table in [docs/api/README.md](../api/README.md#boolean-criterion)): `null`, `false`, numeric zero and the empty string are false; anything else (any non-empty string, `"0"` and `"false"` included, non-zero numbers, `true`, any object or array) is true. An absent value (unknown variable, unresolved source or pointer) is false for `conditionVar` and skips the rule for the targets.

v1 tested the text of the value (non-empty is true). With the native store values, a numeric `0` or a boolean `false` is now false (they were the non-empty texts `"0"` / `"false"`); text values behave as in v1.

## Stop

`{"target": "stop"}` is v1 `break` renamed: when the value reaching it (after the filters) is not empty, the remaining items of the list are not executed.

```json
{"from": {"source": "responseStatusCode"}, "filter": [{"op": "equalTo", "value": "200", "not": true}], "to": {"target": "stop"}}
```

## Full example

```json
v1:
{"source": "request.uri.path", "target": "response.body.json.unsigned./uriId",
 "filter": {"RegexReplace": {"rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"}}}

v2:
{"from": {"source": "requestUriPath"},
 "filter": [{"op": "regexReplace", "rgx": "/app/v1/load-test/id-([0-9]+)", "fmt": "$1"}, {"op": "cast", "type": "unsigned"}],
 "to": {"target": "responseBody", "path": "/uriId"}}
```

## Normalizations

Some v1 spellings have no direct equivalent and are normalized (same behaviour):

* `random.<min>.+<max>`: the `+` sign is dropped (`"max": 2`).
* A vault key never contains dots: v1 already read `vault.A.B` as key `A` with an invalid path (use `vault.A:B`).
* Event query values are url-decoded once by the migrator (`%XX` and `+`), as the v1 runtime did.

## Decisions (review 2026-10-30)

1. Discriminators `source` / `target` / `op`; one form per kind; fields named after their role; `path` absent = whole document.
2. Every kind and field in lowerCamelCase, filters included; the cast is the filter `{"op": "cast", "type": ...}`.
3. `fmt` for every format field (`strftime` source included); `expression`, `values`, `command`, `socket` as listed.
4. Event addressing with plain fields (no url-encoding); `outState` with optional `method`/`uri`.
5. `eraser` (a noun, like every source; decided as `erase` on 2026-10-30, renamed on 2026-10-31) is a source accepting only `conditionVar` filters (conditional erase is new) and the cast, the only source of an event purge, and it cannot feed `stop` nor `clientProvision`; `stop` replaces `break`.
6. The v1 `value.<text>` source is the `literal` kind (`{"source": "literal", "value": ...}`): `value` stays the field of every literal operand.
7. A json body target without `cast` writes the value with the type the source yields (text sources give a string, json sources their json). The migrator does not produce `{"op": "cast", "type": "string"}` when the source always yields text (`literal`, `requestHeader`, `requestUriParam`, `requestUri`, `requestUriPath`, `strftime`, `inState`); every other cast is kept.
