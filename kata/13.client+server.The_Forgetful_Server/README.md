# The Forgetful Server

Some servers are stateful: they remember what you stored, but only until you read it. After that, the data is gone. This kata simulates that behavior using `h2agent`'s state machine and the special `purge` state.

## The purge state

When a provision sets `outState` to `purge`, the server data for that URI is deleted after processing. The next request to the same URI will be treated as a fresh (maiden) event — as if it never happened.

```json
{
  "inState": "stored",
  "requestMethod": "GET",
  "requestUri": "/vault/item",
  "responseCode": 200,
  "responseBody": {"secret": "42"},
  "outState": "purge"
}
```

After this GET is processed, the server forgets the item. A second GET returns `404` (a provision with no `inState` matches the fresh state and returns 404).

## Exercise

Complete `server-provision.json` so that:

1. A `POST /items/id-<number>` stores the item (body: `{"value": <number>}`), responds `201`.
2. A `GET /items/id-<number>` returns the stored body with `200`. **After this GET, the item is forgotten.**
3. A second `GET` to the same URI returns `404`.

## Exercise

Create `client-provision.json` with two provisions using endpoint `myServer`:

- `store-item`: `POST /items/id-42` with body `{"value": 42}` and header `content-type: application/json`. Use `expectedResponseStatusCode: 201` to validate the server response inline.
- `read-item`: `GET /items/id-42`. Use `expectedResponseStatusCode: 200`.

Complete `server-provision.json` so that:

1. A `POST /items/id-<number>` stores the item (body: `{"value": <number>}`), responds `201`.
2. A `GET /items/id-<number>` returns the stored body with `200`. **After this GET, the item is forgotten.**
3. A second `GET` to the same URI returns `404`.

The `test.sh` verifies the full flow: `POST` → first `GET` (200) → second `GET` (404).

**Hint**: `POST` and `GET` have **separate state machines** (keyed by method + URI). A plain `outState` on the `POST` only affects future `POST` requests to the same URI — it does not change the `GET` state. To cross the boundary, use a *foreign state* target: `outState.GET.<uri>`. Since the URI contains a dynamic number, capture it first with `RegexCapture` into a variable and use substitution: `outState.GET.@{item-uri}`. To return the stored body in the `GET`, use a `serverEvent` source referencing the previous `POST` event for that same URI.
