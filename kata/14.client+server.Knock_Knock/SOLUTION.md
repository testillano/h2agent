The `server-provision.json` needs three provisions:

1. `GET /enter` (inState: initial) → 403. The door is closed by default.
2. `POST /knock` (inState: initial) → 200, outState: `"open"`. Knocking opens the door.
3. `GET /enter` (inState: open) → 200, outState: initial. Entering closes the door again.

```json
[
  {
    "requestMethod": "GET",
    "requestUri": "/enter",
    "responseCode": 403,
    "responseBody": {"error": "knock first"}
  },
  {
    "requestMethod": "POST",
    "requestUri": "/knock",
    "responseCode": 200,
    "outState": "open",
    "transform": [
      {"source": "value.open", "target": "outState.GET./enter"}
    ]
  },
  {
    "inState": "open",
    "requestMethod": "GET",
    "requestUri": "/enter",
    "responseCode": 200,
    "responseBody": {"message": "welcome"},
    "outState": "initial"
  }
]
```

Note: the `POST /knock` uses a foreign state (`outState.GET./enter`) to set the state for the `GET /enter` event, since they are different URIs/methods. This is the key mechanism — without it, the `outState` on the POST would only affect future POSTs to `/knock`.

The client provisions are trivial: `try-enter` (GET /enter) and `knock` (POST /knock), triggered independently by the test.
