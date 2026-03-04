The `client-provision.json` needs two provisions with `onResponseTransform` to extract the timestamp from the response body:

```json
[
  {
    "id": "first-tick",
    "endpoint": "myServer",
    "requestMethod": "GET",
    "requestUri": "/time",
    "onResponseTransform": [
      {
        "source": "clientEvent.responseBody./ts",
        "target": "globalVar.t1"
      }
    ]
  },
  {
    "id": "second-tick",
    "endpoint": "myServer",
    "requestMethod": "GET",
    "requestUri": "/time",
    "onResponseTransform": [
      {
        "source": "clientEvent.responseBody./ts",
        "target": "globalVar.t2"
      },
      {
        "source": "math.@{globalVar.t2} - @{globalVar.t1}",
        "target": "globalVar.elapsed"
      }
    ]
  }
]
```

Key points:
- `clientEvent.responseBody./ts` reads the `/ts` field from the response body JSON.
- `globalVar.*` persists across provision executions (unlike `var.*` which is local).
- The `math` expression uses global variable substitution to compute the difference.
- The elapsed value will be in milliseconds, matching the `timestamp.ms` source used by the server.
