# Server-Sent Events (SSE) for Vault Streaming

## What is SSE?

Server-Sent Events is a W3C standard for unidirectional server-to-client streaming over HTTP. The client opens a single connection and the server pushes text events whenever it wants — no polling, no bidirectional overhead.

Protocol format (plain text over HTTP):

```
event: vault-set
data: {"key": "my_barrier", "value": "done"}

event: vault-set
data: {"key": "another_key", "value": "42"}

```

Each event block is terminated by a blank line (`\n\n`). The client reads from the stream, blocking until data arrives.

## Why SSE (vs alternatives)

| Approach | Direction | Complexity | Fit |
|----------|-----------|------------|-----|
| Polling | Client→Server (repeated) | Low | Poor at scale |
| Long-poll | Client→Server (blocking) | Low | OK for single key |
| SSE | Server→Client (push) | Low | Ideal for event streams |
| WebSocket | Bidirectional | Medium | Overkill (no client→server needed) |
| gRPC stream | Bidirectional | High | Overkill (adds protobuf dep) |

## h2agent Implementation

### Endpoint

```
GET /admin/v1/vault/events[?key=<k1>&key=<k2>...]
```

- Response: `200 OK`, `Content-Type: text/event-stream`
- Stream stays open until client disconnects
- If `key=` params provided: only matching vault mutations are pushed
- If no `key=` params: all vault mutations are pushed

### Architecture

```
┌─────────────┐       notify(key, value)        ┌────────────┐
│    Vault    │ ──────────────────────────────► │ SseManager │
│  (storage)  │                                 │            │
└─────────────┘                                 └─────┬──────┘
                                                      │
                                          for each matching connection:
                                                      │
                                                      ▼
                                               ┌─────────────┐
                                               │  SSE Stream │ ──► client (DATA frame)
                                               │  (nghttp2)  │
                                               └─────────────┘
```

1. Client sends `GET /admin/v1/vault/events?key=X`
2. Handler sends `200` + SSE headers, keeps stream open via nghttp2 generator callback
3. Generator returns `NGHTTP2_ERR_DEFERRED` (no data yet → pause stream)
4. When Vault mutates key X → `SseManager::notify()` buffers the SSE event and calls `res.resume()`
5. nghttp2 re-invokes the generator, which now returns the buffered data
6. Client receives the DATA frame with the SSE event text

### HTTP/2 Streaming Mechanics

In HTTP/2, SSE works via a single stream where the server sends DATA frames without the END_STREAM flag. The connection remains open. This is achieved using nghttp2's deferred data provider pattern:

- `res.write_head(200, headers)` — sends HEADERS frame
- `res.end(generator_cb)` — installs a data provider that nghttp2 calls when ready to send
- Generator returns `NGHTTP2_ERR_DEFERRED` when idle (pauses the stream)
- `res.resume()` wakes the stream, nghttp2 calls the generator again

### Key Classes

- **`SseManager`** (`src/model/SseManager.hpp`): Registry of active SSE connections. Thread-safe. Each connection has a key filter set and a write callback.
- **`Vault`** (`src/model/Vault.hpp`): Calls `sse_manager_->notify(key, value)` on every mutation.
- **`MyAdminHttp2Server::setupSseEndpoint()`**: Registers the nghttp2 handler before `serve()`.

### Connection Lifecycle

1. **Open**: Handler parses `?key=` params, registers connection in SseManager
2. **Active**: Vault mutations trigger buffered writes + `resume()`
3. **Close**: `res.on_close()` callback removes connection from SseManager

### Filtering

- Empty key set = receive all vault mutations (useful for debugging)
- Non-empty key set = O(1) lookup per vault write (unordered_set)
