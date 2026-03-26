# h2agent-gherkin

Gherkin (BDD) driver for [h2agent](https://github.com/testillano/h2agent).
Write mock scenarios in plain English using Given/When/Then syntax — the driver
translates each step into REST API calls against a running h2agent instance.

## Quick start

```bash
pip install -r requirements.txt
```

### Run mode — execute against a live h2agent

```bash
<project root>/run.sh  # start h2agent in another terminal
behave features/example.feature
```

Each scenario cleans h2agent state before running (provisions, data, vault, schemas).
Use `@no_clean` tag to opt out.

### Dump mode — generate JSON files without h2agent

```bash
behave -D dump=./output features/example.feature
```

Generates numbered JSON files representing each API call in order:

```
output/
├── 01.server-matching.json       ← POST /admin/v1/server-matching
├── 02.server-provision.json      ← POST /admin/v1/server-provision
├── 03.traffic-POST.json          ← POST /api/v1/users (type: traffic)
├── 04.client-endpoint.json       ← POST /admin/v1/client-endpoint
├── 05.client-provision.json      ← POST /admin/v1/client-provision
└── 06.trigger-fetchData.json     ← GET /admin/v1/client-provision/fetchData
```

Each file is a JSON object with the following fields:

| Field | Always | Description |
|---|---|---|
| `method` | yes | HTTP method (`POST`, `GET`, `PUT`, `DELETE`) |
| `path` | yes | Full path (e.g. `/admin/v1/server-provision`) |
| `body` | no | JSON body for `POST`/`PUT` calls |
| `queryParams` | no | Query parameters (e.g. `{"sequence": 5}`) |
| `type` | no | `"traffic"` for traffic-port calls; absent for admin calls |

Admin calls can be replayed directly with curl:

```bash
# From a dumped file:
# {"method": "POST", "path": "/admin/v1/server-provision", "body": {...}}
curl --http2-prior-knowledge -X POST \
  -H "content-type: application/json" \
  -d @02.server-provision.json \  # or extract the body field
  http://localhost:8074/admin/v1/server-provision
```

The output directory must not exist — behave will abort if it does.
Assertions (Then steps) are skipped — only actions are recorded.

### Environment variables

| Variable | Default | Description |
|---|---|---|
| `H2AGENT_ADMIN_ENDPOINT` | `http://localhost:8074` | Admin API base URL |
| `H2AGENT_TRAFFIC_ENDPOINT` | `http://localhost:8000` | Traffic port base URL |

## Step reference

### Server matching

```gherkin
Given server matching is "FullMatching"
Given server matching is "FullMatchingRegexReplace" with rgx "..." and fmt "..."
Given server matching is "FullMatching" with query parameters filter "Sort"
Given server matching is "FullMatching" with query parameters filter "Ignore" separator "Semicolon"
```

### Server provision (builder)

```gherkin
Given a server provision for "POST" on "/api/v1/users"
  And with inState "initial"
  And with outState "created"
  And with response code 201
  And with response header "content-type" as "application/json"
  And with response body
    """
    {"id": 1}
    """
  And with response delay 50 ms
  And with request schema "my-schema"
  And with response schema "my-schema"
  And with transform from "request.body./name" to "response.body.json.string./name"
  And with transform from "value.42" to "response.body.json.integer./count" and filter
    """
    {"Sum": 1}
    """
  And with transforms
    """
    [{"source": "timestamp.ms", "target": "response.body.json.unsigned./ts"}]
    """
When the server provision is committed
```

### Server provision (JSON / file)

```gherkin
Given server provision
  """
  {"requestMethod": "GET", "requestUri": "/path", "responseCode": 200}
  """

Given server provision from file "provision.json"
```

### Client endpoint

```gherkin
Given a client endpoint "myEp" at "localhost" port 8000
Given a secure client endpoint "myEp" at "localhost" port 8443
```

### Client provision (builder)

```gherkin
Given a client provision "myProv"
  And with endpoint "myEp"
  And with request method "POST"
  And with request uri "/target/path"
  And with request header "x-trace" as "abc"
  And with request body
    """
    {"key": "value"}
    """
  And with request delay 100 ms
  And with timeout 5000 ms
  And with transform from "value.hello" to "request.body.json.string./greeting"
  And with on-response transform from "response.body./id" to "vault.lastId"
When the client provision is committed
```

### Client provision trigger

```gherkin
When client provision "myProv" is triggered
When client provision "myProv" is triggered with sequence 5
When client provision "myProv" is triggered from 1 to 100 at 10 cps
```

### Traffic

```gherkin
When I send "GET" to "/api/v1/status"
When I send "POST" to "/api/v1/items" with body
  """
  {"name": "test"}
  """
When I send "POST" to "/api/v1/items" with body "plain text"
When I send "GET" to "/path" with header "x-version" as "2.0"
```

### Response assertions

```gherkin
Then the response code should be 200
Then the response body should be
  """
  {"status": "ok"}
  """
Then the response body should be "plain text"
Then the response body at "/data/name" should be "Alice"
Then the response header "content-type" should be "application/json"
Then the response result should be true
```

### Server data

```gherkin
Then server data for "POST" "/api/v1/items" should have 3 event(s)
Then server data for "POST" "/api/v1/items" event 1 at "/requestBody/name" should be "test"
When server data is cleared
When server data for "POST" "/api/v1/items" is cleared
```

### Client data

```gherkin
Then client data for "myEp" "POST" "/target" should have 5 event(s)
When client data is cleared
```

### Schema

```gherkin
Given a schema "user-schema"
  """
  {"type": "object", "required": ["name"]}
  """
```

### Vault

```gherkin
Given vault "counter" with value "0"
Then vault "counter" should be "0"
When vault is cleared
When vault "counter" is cleared
```

### Wait (long-poll)

```gherkin
When I wait for vault "counter" with value "10" timeout 5000 ms
When I wait for vault "counter" to change timeout 3000 ms
```

### Logging & configuration

```gherkin
Given logging level is "Error"
Then logging level should be "Error"
Given server configuration receiveRequestBody is true
Given server data storage discard is false and discardKeyHistory is true
Given client data storage discard is false and discardKeyHistory is false
```

### Health & unused provisions

```gherkin
Then h2agent should be healthy
Then there should be 0 unused server provision(s)
```

### Generic admin (escape hatch)

```gherkin
When I POST to admin "server-provision"
  """
  {"requestMethod": "GET", "requestUri": "/any", "responseCode": 200}
  """
When I GET admin "server-provision"
When I DELETE admin "server-provision"
When I PUT admin "logging?level=Error"
```

## Tags

| Tag | Effect |
|---|---|
| `@no_clean` | Skip automatic cleanup before the scenario |
