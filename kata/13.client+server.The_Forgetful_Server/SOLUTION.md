The `server-provision.json` needs three provisions:

1. `POST /items/id` (inState: initial) → 201, stores the body in a variable, sets outState to `"stored"`.
2. `GET /items/id` (inState: stored) → 200, returns the stored body, sets outState to `"purge"`.
3. `GET /items/id` (inState: initial) → 404 (maiden event after purge).

```json
[
  {
    "requestMethod": "POST",
    "requestUri": "/items/id",
    "responseCode": 201,
    "outState": "stored",
    "transform": [
      {"source": "request.body", "target": "var.stored-body"}
    ]
  },
  {
    "inState": "stored",
    "requestMethod": "GET",
    "requestUri": "/items/id",
    "responseCode": 200,
    "outState": "purge",
    "transform": [
      {"source": "var.stored-body", "target": "response.body.json.object"}
    ]
  },
  {
    "requestMethod": "GET",
    "requestUri": "/items/id",
    "responseCode": 404
  }
]
```

The `client-endpoint.json` and `client-provision.json` are straightforward — two separate provisions, triggered independently. The `store-item` provision fires first, then `read-item` is triggered manually by the test.

Note: variables (`var.*`) are scoped per URI event, so `stored-body` is isolated per item id after the `FullMatchingRegexReplace` normalization.
