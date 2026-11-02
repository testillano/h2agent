2-step client flow that ends in purge (loopback: h2agent as both client and server).

Demonstrates purge in a client provision chain: when a step reaches the reserved
outState "purge", ALL client events accumulated during the whole chain are removed
once the chain terminates. Unlike server-mode purge (which clears the current data
key: method + URI), client-mode purge clears every step of the chain, each typically
targeting a different method+URI.

Flow:
  1. Trigger fires provision (initial, purgeFlow) -> POST /api/v1/order
  2. Server responds 201 {"orderId":"A-100"}
  3. State progression: initial -> check
  4. Provision (check, purgeFlow) fires -> GET /api/v1/order/A-100
  5. Server responds 200 {"orderId":"A-100","status":"ready"}
  6. outState="purge" -> the whole client-data chain (both events) is removed

After execution, verify:
  - Client-data: EMPTY (HTTP 204) -- both the POST and GET client events were purged.
  - Server-data: the two requests the loopback server received are still present
    (server-mode purge is independent; this chain only purges the CLIENT side).

Both provisions use expectedResponseStatusCode to validate the responses inline; if a
status code differs, the chain breaks before reaching "purge" and the events are kept
(useful to inspect what happened).

Prerequisite handled by the example itself:

  Purge must be ENABLED for client-data. It is enabled by default, but it can be turned
  off at process start (--disable-purge, which now also applies to the client side) or
  at runtime. So that the result does not depend on how the agent was launched, this
  example's trigger.sh makes itself self-contained by issuing the REST PUT first:

    PUT /admin/v2/client-data/configuration?disablePurge=false

  If purge were DISABLED instead, the chain would still run but the two client events
  would REMAIN (that is exactly what --disable-purge is for: keeping events for
  forensics). The trigger also waits for the asynchronous chain to finish before
  checking client-data, so it never reports an intermediate (not-yet-purged) state.
