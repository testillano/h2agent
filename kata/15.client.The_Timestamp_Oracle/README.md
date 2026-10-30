# The Timestamp Oracle

Time is relative — but measurable. In this kata, the server acts as a timestamp oracle: every request returns the current time in milliseconds. Your job is to configure the client to make two consecutive requests and calculate the time elapsed between them.

## Timestamp source

`h2agent` can generate timestamps as a data source:

```json
{"from": {"source": "timestamp", "unit": "ms"}, "to": {"target": "var", "name": "now"}}
```

This stores the current time in milliseconds into a local variable. You can also use the units `s` (seconds), `us` (microseconds) or `ns` (nanoseconds).

## Vault

Local variables (`var`) live along a provision chain (`outState`). To share data between two independent provisions, use **vault** entries (`vault`):

```json
{"from": {"source": "literal", "value": "hello"}, "to": {"target": "vault", "key": "greeting"}}
```

Vault persist across provision executions and can be read back:

```json
{"from": {"source": "vault", "key": "greeting"}, "filter": [{"op": "cast", "type": "string"}], "to": {"target": "responseBody"}}
```

## Math expressions

You can compute expressions using the `math` source:

```json
{"from": {"source": "math", "expression": "@{t2} - @{t1}"}, "to": {"target": "vault", "key": "elapsed"}}
```

## The server

The server provision is provided (`server-provision.json`). It responds to `GET /time` with the current timestamp in milliseconds in the response body.

The client endpoint is also provided (`client-endpoint.json`).

## Exercise

Complete `client-provision.json` with **two provisions**:

1. `first-tick`: sends `GET /time`, stores the response timestamp in the vault entry `t1`.
2. `second-tick`: sends `GET /time`, stores the response timestamp in the vault entry `t2`, then computes the vault entry `elapsed = t2 - t1`.

The `test.sh` will trigger both provisions in sequence, then read the vault entry `elapsed` and verify it is a positive number.

**Hint**: use `onResponseTransform` to extract the timestamp from the response body into a vault entry.