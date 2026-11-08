# Gherkin driver

Gherkin (BDD) driver for [h2agent](https://github.com/testillano/h2agent)
(`tools/gherkin`):
write mock scenarios with Given/When/Then steps; the driver turns them into
h2agent REST API calls (HTTP/2), or into json files (dump mode).

```gherkin
  Scenario: Echo the user name
    Given the server answers "POST" "/api/v1/users" with 201
      """
      {"id": 1}
      """
      And with transformations
        | from              | filter | to                 |
        | requestBody /name |        | responseBody /name |
    When I send "POST" to "/api/v1/users" with body
      """
      {"name": "Alice"}
      """
    Then the response body at "/name" should be "Alice"
```

Start with the **[tutorial](TUTORIAL.md)**; this page is the reference.

## Quick start

```bash
$ pip install -r requirements.txt
$ ../../run.sh &                       # h2agent on localhost:8074 / 8000
$ behave features/                     # server, client and the complete library example
$ python3 -m unittest discover -s tests   # driver unit tests (no agent)
```

| Variable | Default | Description |
|---|---|---|
| `H2AGENT_ADMIN_ENDPOINT` | `http://localhost:8074` | Admin API base URL |
| `H2AGENT_TRAFFIC_ENDPOINT` | `http://localhost:8000` | Traffic port base URL |

| User data (`-D`) | Default | Description |
|---|---|---|
| `dump=<dir>` | | Record the API calls as json files instead (no agent) |
| `timeout=<seconds>` | `3` | How long data and vault assertions wait |

Each scenario wipes the agent first (provisions, data, vault, schemas): use
an agent of your own. Tag `@no_clean` to keep the previous state.

## How steps work

* A provision is a starting step plus `And` modifiers; it is sent by the
  first step which does not modify it (no commit step).
* Vault entries and presets are sent at once: declare them before the
  provisions referencing them (`#{key}`, `from preset`), e.g. in the
  `Background`. An unresolved reference fails.
* Json bodies are sent and answered with `content-type: application/json`
  unless one is given.
* A value in a step matches as json or as text (`"1"` matches 1 and "1"); a
  doc string compares exact json.
* Data and vault assertions retry until they pass or the timeout expires
  (client flows and traffic processing are asynchronous).

## Transformation tables

`with transformations` / `with response transformations` take a table with
the columns `from`, `filter` (optional) and `to`; each row is one rule of the
[transformation grammar](../../docs/api/README.md). A cell is
`<kind> [args]`:

| Write | Means | Example |
|---|---|---|
| `/...` | json pointer (`requestBody`, `responseBody`, `var`, `vault`) | `requestBody /user/name` |
| bare value(s) | the kind's main field(s), in order | `requestHeader authorization`, `regexReplace '^/id-(.*)' $1` |
| `name=value` | any field | `outState method=POST uri=/x` |
| `not` | negated `equalTo` / `conditionVar` | `equalTo 200 not` |
| `'...'` / `"..."` | value with spaces (single quotes keep `\`) | `regexCapture '\d+ (\w+)'` |
| `a ; b` | several filters | `regexReplace '/u/(.*)' $1 ; cast integer` |

Main fields: `requestHeader`, `requestUriParam`, `var`, `responseHeader`,
`conditionVar`: name; `vault`: key; `literal`, `append`, `prepend`, `sum`,
`multiply`, `equalTo`, `jsonConstraint`: value; `timestamp`: unit;
`strftime`, `strptime`: fmt; `math`: expression; `random`: min max;
`randomset`: values (all of them); `regexCapture`, `regexKey`: rgx;
`regexReplace`: rgx fmt; `cast`: type; `baseConvert`: in out; `schemaId`: id;
`clientProvision`: id inState; `txtFile`, `binFile`: file; `command`:
command; `udpSocket`: socket.

## Step reference

### Pre-provision (vault, presets)

```gherkin
Given the vault entry "patterns"
  """
  {"isbn": "([0-9]{13})"}
  """
Given the vault entry "counter" is "0"
Given the preset "pizza"
  """
  {"size": "medium", "toppings": {"cheese": "mozzarella"}}
  """
```

### Server

```gherkin
Given server matching is "FullMatching"
Given server matching is "FullMatchingRegexReplace" with rgx "..." and fmt "..."
Given server matching is "FullMatching" with query parameters filter "Sort"
Given server matching is "FullMatching" with query parameters filter "Ignore" separator "Semicolon"

Given the server answers "GET" "/path" with 200          # doc string: body (optional)
Given the server answers "GET" "/path" with 200 and body "text"
Given the server answers "GET" "/path" with 200 from preset "a"   # or: from presets "a", "b"
  And in state "initial" moving to "next"                # or: in state "x" / moving to state "y"
  And with response header "x-a" as "b"
  And with response delay 50 ms
  And with request schema "s"
  And with response schema "s"
  And with transformations                               # table: | from | filter | to |

Given the schema "s"                                     # doc string: json schema
```

### Client

```gherkin
Given the client endpoint "ep" at "localhost" port 8000
Given the secure client endpoint "ep" at "localhost" port 8443

Given the client "id" sends "POST" "/path" to "ep"       # doc string: body (optional)
Given the client "id" sends "POST" "/path" to "ep" from preset "a"
  And in state "initial" moving to "next"
  And with request header "x-a" as "b"
  And with request delay 100 ms
  And with timeout 5000 ms
  And expecting status code 201
  And with transformations                               # build the request
  And with response transformations                      # process the answer

When the client "id" is triggered
When client provision "id" is triggered with sequence 5
When client provision "id" is triggered from 1 to 100 at 10 cps
When client provision "id" cps is set to 20
```

### Traffic towards the mock server

```gherkin
When I send "GET" to "/path"
When I send "POST" to "/path" with body                   # doc string
When I send "POST" to "/path" with body "text"
When I send "GET" to "/path" with header "x-a" as "b"
```

### Assertions

```gherkin
Then the response code should be 200
Then the response body should be                          # doc string: exact json
Then the response body should be "text"
Then the response body at "/a/b" should be "v"
Then the response header "content-type" should be "application/json"

Then the server should have received 3 "POST" "/path"
Then server data for "POST" "/path" event -1 at "/requestBody/a" should be "v"
Then server data for "POST" "/path" event 1 at "/requestBody" should be   # doc string
Then there should be 0 unused server provisions

Then the client "id" should receive 201
Then the client "id" response body at "/a" should be "v"
Then client data for "ep" "POST" "/path" should have 5 events

Then the vault entry "k" should be "v"
Then the vault entry "k" should be                        # doc string
Then h2agent should be healthy
Then logging level should be "Error"
```

### Maintenance and configuration

```gherkin
When server data is cleared
When server data for "POST" "/path" is cleared
When client data is cleared
When vault is cleared
When vault "k" is cleared
When I wait for vault "k" with value "v" timeout 5000 ms
When I wait for vault "k" to change timeout 3000 ms
Given logging level is "Error"
Given server configuration receiveRequestBody is true
Given server data storage discard is false and discardKeyHistory is true
Given client data storage discard is false and discardKeyHistory is false
```

### Escape hatches (REST API json)

```gherkin
Given server provision                                    # doc string: provision
Given server provision from file "provision.json"
Given client provision                                    # doc string: provision
  And with transform                                      # doc string: one rule or a list
  And with on-response transform                          # doc string: one rule or a list
When I POST to admin "server-matching"                    # doc string; also GET, PUT, DELETE
```

The former builder steps (`a server provision for "M" on "U"`,
`with response code`, `with inState`, `the server provision is committed`...)
are still accepted.

## Dump mode

`behave -D dump=<dir> features/x.feature` writes one numbered json file per
API call (`method`, `path`, `body`, `queryParams`, `headers`, and
`type: traffic` for traffic), no agent needed; `Then` steps are skipped. The
directory must not exist.

## Tags

| Tag | Effect |
|---|---|
| `@no_clean` | Skip the cleanup before the scenario |
