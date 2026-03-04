# The Timestamp Oracle

Time is relative — but measurable. In this kata, the server acts as a timestamp oracle: every request returns the current time in milliseconds. Your job is to configure the client to make two consecutive requests and calculate the time elapsed between them.

## Timestamp source

`h2agent` can generate timestamps as a data source:

```json
{"source": "timestamp.ms", "target": "var.now"}
```

This stores the current time in milliseconds into a local variable. You can also use `timestamp.us` (microseconds) or `timestamp.ns` (nanoseconds).

## Global variables

Local variables (`var.*`) are scoped to a single provision execution. To share data between two separate provision executions, use **global variables** (`globalVar.*`):

```json
{"source": "value.hello", "target": "globalVar.greeting"}
```

Global variables persist across provision executions and can be read back:

```json
{"source": "globalVar.greeting", "target": "response.body.json.string"}
```

## Math expressions

You can compute expressions using the `math` source:

```json
{"source": "math.@{globalVar.t2} - @{globalVar.t1}", "target": "globalVar.elapsed"}
```

## The server

The server provision is provided (`server-provision.json`). It responds to `GET /time` with the current timestamp in milliseconds in the response body.

The client endpoint is also provided (`client-endpoint.json`).

## Exercise

Complete `client-provision.json` with **two provisions**:

1. `first-tick`: sends `GET /time`, stores the response timestamp in `globalVar.t1`.
2. `second-tick`: sends `GET /time`, stores the response timestamp in `globalVar.t2`, then computes `globalVar.elapsed = t2 - t1`.

The `test.sh` will trigger both provisions in sequence, then read `globalVar.elapsed` and verify it is a positive number.

**Hint**: use `onResponseTransform` to extract the timestamp from the response body into a global variable.