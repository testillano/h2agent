# Gherkin tutorial

This tutorial builds, step by step, the kind of scenarios a test suite needs:
a mock server answering the system under test, values shared among
provisions, and client flows. Every snippet is runnable: the complete
versions are `features/example-server.feature` and
`features/example-client.feature`.

The steps are a thin layer over the h2agent [REST API](../../docs/api/README.md):
each scenario becomes the same provisions you would `POST` by hand. What the
driver adds is the wording, a table for the transformations, and the waiting
needed by asynchronous flows.

## 0. Setup

```bash
$ cd tools/gherkin
$ pip install -r requirements.txt
$ ../../run.sh &        # an h2agent on localhost (admin 8074, traffic 8000)
$ behave features/example-server.feature
```

Point the driver to another agent with `H2AGENT_ADMIN_ENDPOINT` and
`H2AGENT_TRAFFIC_ENDPOINT` (e.g. `http://localhost:28074`).

> Every scenario starts by **wiping the agent** (provisions, data, vault,
> schemas), so it always starts from scratch. Use an agent of your own, not a
> shared one (the target is printed at start). Tag a scenario `@no_clean` to
> keep the previous state.

## 1. A static answer

```gherkin
Feature: Users service mock

  Background:
    Given server matching is "FullMatching"

  Scenario: Static answer
    Given the server answers "GET" "/api/v1/status" with 200
      """
      {"status": "ok"}
      """
    When I send "GET" to "/api/v1/status"
    Then the response code should be 200
      And the response body at "/status" should be "ok"
```

* `Given the server answers <method> <uri> with <code>` describes a server
  provision. The doc string (`"""`) is the answer body: json bodies get
  `content-type: application/json` unless you set another one. For a text
  body: `... with 200 and body "pong"`.
* `When I send ...` plays the system under test: it sends traffic to the
  mock server. `Then ...` checks the last answer.
* `Background` runs before each scenario. Server matching (how a request
  finds its provision) is configured once there.

There is no "commit" step: the provision is sent by the first step which does
not describe it (here, `When I send`).

## 2. Describing the answer

Modifiers start with `And` and apply to the provision just described:

```gherkin
    Given the server answers "POST" "/api/v1/validated" with 200
      And with request schema "user"
      And with response delay 20 ms
      And with response header "x-version" as "2"
```

## 3. Transformations: a table, one rule per row

The answer is usually built from the request. Each row of the table is one
transformation rule: where a value comes **from**, optional **filter**s, and
where it goes **to**:

```gherkin
    Given the server answers "POST" "/api/v1/users" with 201
      """
      {"id": 1}
      """
      And with transformations
        | from                     | filter        | to                        |
        | requestBody /name        |               | responseBody /name        |
        | timestamp ms             | cast unsigned | responseBody /createdMs   |
        | requestHeader x-trace-id |               | responseHeader x-trace-id |
```

A cell is a kind followed by its arguments:

| Write | Means | Example |
|-------|-------|---------|
| `<kind>` | a kind with no argument | `recvseq`, `requestUriPath`, `size` |
| `/...` | the json pointer of kinds taking one (`requestBody`, `responseBody`, `var`, `vault`) | `requestBody /user/name` |
| a bare value | the kind's main field (the second one, if any, next) | `requestHeader authorization`, `cast integer`, `regexReplace '^/id-(.*)' $1` |
| `name=value` | any other field | `outState method=POST uri=/x`, `split sep=, count=2` |
| `not` | negates `equalTo` / `conditionVar` | `equalTo 200 not` |
| `'...'` or `"..."` | a value with spaces; single quotes keep backslashes | `regexCapture '\d+ (\w+)'` |

Several filters go in one cell separated by `;`
(`regexReplace '/users/(.*)' $1 ; cast integer`). The kinds and fields are
exactly those of the [transformation grammar](../../docs/api/README.md); the
agent validates them, so a wrong kind fails the step that sends the
provision.

Values of one rule feed the next through variables, and `@{name}` inserts a
variable in a text:

```gherkin
    Given server matching is "RegexMatching"
      And the server answers "GET" "/api/v1/users/[0-9]+" with 200
        """
        {}
        """
      And with transformations
        | from                 | filter                                   | to                 |
        | requestUriPath       | regexReplace '/api/v1/users/([0-9]+)' $1 | var id             |
        | var id               | cast integer                             | responseBody /id   |
        | literal 'user-@{id}' |                                          | responseBody /name |
    When I send "GET" to "/api/v1/users/42"
    Then the response body at "/name" should be "user-42"
```

## 4. Answers which depend on the state

A provision may only apply in a state and move the key (method + uri) to
another one:

```gherkin
    Given the server answers "POST" "/api/v1/toggle" with 200 and body "on"
      And in state "initial" moving to "on"
      And the server answers "POST" "/api/v1/toggle" with 200 and body "off"
      And in state "on" moving to "initial"
    When I send "POST" to "/api/v1/toggle"
    Then the response body should be "on"
    When I send "POST" to "/api/v1/toggle"
    Then the response body should be "off"
```

`in state "x"` and `moving to state "y"` set one of them alone.

## 5. Shared values: vault entries and presets

Values shared by many provisions (regular expressions, common blocks) live
in the vault and are referenced from the provisions: `#{key}` or
`#{key/pointer}` inserts the value when the provision is loaded. A **preset**
is a vault object a body starts from: the body only writes what differs.

```gherkin
  Background:
    Given the vault entry "patterns"
      """
      {"id": "([0-9]+)"}
      """
      And the preset "pizza"
      """
      {"size": "medium", "toppings": {"cheese": "mozzarella", "sauce": "tomato"}}
      """
      And server matching is "RegexMatching"

  Scenario: A pizza with another cheese
    Given the server answers "GET" "/api/v1/orders/#{patterns/id}" with 200 from preset "pizza"
      """
      {"toppings": {"cheese": "gorgonzola"}}
      """
    When I send "GET" to "/api/v1/orders/7"
    Then the response body should be
      """
      {"size": "medium", "toppings": {"cheese": "gorgonzola", "sauce": "tomato"}}
      """
```

* `Given the vault entry "k"` takes a json (or text) doc string;
  `Given the vault entry "k" is "v"` a text.
* `from preset "a"` (or `from presets "a", "b"`: later ones win) starts the
  body from those objects; the body's own keys always win (merge patch rules,
  see [presets](../../docs/api/README.md#provision-presets-preset)).
* References are resolved **once, when the provision is loaded**, so vault
  entries and presets go first: in the `Background`, or anywhere before the
  provisions using them. A reference which does not exist at that moment
  fails the step, telling which provision it was.

## 6. Client flows

The h2agent client sends requests to a server (normally the system under
test). Here it targets its own mock server, so the examples run alone:

```gherkin
  Background:
    Given server matching is "FullMatching"
      And the client endpoint "self" at "localhost" port 8000

  Scenario: Create a user and keep its name
    Given the server answers "POST" "/api/v1/users" with 201
      """
      {"id": 1}
      """
      And with transformations
        | from              | to                 |
        | requestBody /name | responseBody /name |
      And the client "createUser" sends "POST" "/api/v1/users" to "self"
      """
      {"name": "Bob"}
      """
      And with response transformations
        | from               | to                    |
        | responseBody /name | vault lastCreatedUser |
    When the client "createUser" is triggered
    Then the client "createUser" should receive 201
      And the client "createUser" response body at "/name" should be "Bob"
      And the vault entry "lastCreatedUser" should be "Bob"
```

* `Given the client "<id>" sends <method> <uri> to "<endpoint>"` describes a
  client provision; the doc string is the request body (also
  `from preset ...`). Modifiers: `with request header`, `with timeout N ms`,
  `with request delay N ms`, `expecting status code N`, the states, and two
  tables: `with transformations` build the request, `with response
  transformations` process the answer.
* A trigger starts the flow and returns at once: the flow runs
  asynchronously. So `Then` steps on data and vault **wait** for the expected
  value, up to 3 seconds (`behave -D timeout=10` to change it), and fail with
  the last value seen.

Sequences: `When client provision "batch" is triggered from 1 to 5 at 50 cps`
sends 5 requests, `@{sequence}` being 1..5 in each one:

```gherkin
      And the client "batch" sends "POST" "/api/v1/items" to "self"
      """
      {"item": ""}
      """
      And with transformations
        | from                       | to                |
        | literal 'item-@{sequence}' | requestBody /item |
    When client provision "batch" is triggered from 1 to 5 at 50 cps
    Then the server should have received 5 "POST" "/api/v1/items"
```

Chains: provisions with the same id and different states form a chain; each
answer moves it to the next state, and variables travel along:

```gherkin
      And the client "login" sends "POST" "/api/v1/auth" to "self"
      And in state "initial" moving to "authenticated"
      And with response transformations
        | from                | to        |
        | responseBody /token | var token |
      And the client "login" sends "GET" "/api/v1/data" to "self"
      And in state "authenticated"
      And with transformations
        | from      | to                          |
        | var token | requestHeader authorization |
    When the client "login" is triggered
```

## 7. What did the server receive?

```gherkin
    Then the server should have received 2 "POST" "/api/v1/items"
      And server data for "POST" "/api/v1/items" event -1 at "/requestBody/item" should be "second"
      And there should be 0 unused server provisions
```

Event numbers start at 1; negative ones count from the last (`-1`). A value
written in a step matches the stored one as json or as text: `"1"` matches
both the number 1 and the string "1"; a doc string compares exact json.

## 8. From scenarios to provision files (dump mode)

The same features can generate the REST calls instead of running them, e.g.
to keep json provisions in a component test:

```bash
$ behave -D dump=./output features/example-server.feature
```

```
output/
├── 01.server-matching.json
├── 02.server-provision.json
├── 03.traffic-GET.json
...
```

Each file holds `method`, `path`, and the `body` (plus `queryParams`,
`headers`, and `type: traffic` for traffic). No agent is needed; `Then` steps
are skipped. The output directory must not exist.

## 9. A complete example

`features/example-library.feature` puts everything together: a library
loans mock whose `Background` is the whole mock, so each scenario is just a
business rule played as traffic:

* shared regular expressions in the vault, referenced from uris, regex
  cells and other provisions (`#{patterns/loan}`);
* answers built from presets (`loan`), with a vault object written into the
  answer only for one membership (`equalTo` + `conditionVar`);
* one state per loan uri: lending a book moves `GET /loans/<isbn>` to
  `active` (`outState method=GET uri=/loans/@{id}`), returning it moves it
  back, so a loan is found only while it lives;
* returning a book triggers the h2agent client, which notifies a receiver
  (`clientProvision notifyReturn initial`) with a value the server left in
  the vault;
* a `Scenario Outline` running the same rule over an `Examples` table.

```gherkin
  Scenario Outline: The loan terms depend on the membership
    When I send "POST" to "/loans" with body
      """
      {"book": "isbn-<isbn>", "membership": "<membership>"}
      """
    Then the response code should be 201
      And the response header "location" should be "/loans/<isbn>"
      And the response body at "/terms/days" should be "<days>"

    Examples:
      | isbn          | membership | days |
      | 9780000000001 | basic      | 14   |
      | 9780000000002 | premium    | 30   |
```

## 10. Escape hatches

When a provision needs something the steps do not cover, write it as in the
REST API:

```gherkin
    Given server provision
      """
      {"requestMethod": "GET", "requestUri": "/x", "responseCode": 200}
      """
    Given server provision from file "provision.json"
    Given client provision
      """
      {"id": "x", "endpoint": "self", "requestMethod": "GET", "requestUri": "/x"}
      """
      And with transform
      """
      {"from": {"source": "recvseq"}, "to": {"target": "responseBody", "path": "/seq"}}
      """
    When I POST to admin "server-matching"
      """
      {"algorithm": "RegexMatching"}
      """
```

The full list of steps is in the [README](README.md#step-reference).
