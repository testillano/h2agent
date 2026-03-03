Timer-based 2-step client flow triggered 3 times at 1 request per second.

Demonstrates rps/range triggering combined with state progression.
Each tick of the timer fires the full chain (initial->step2->road-closed),
so 3 ticks produce 6 total requests (3 GETs + 3 POSTs).

The 'sequence' variable (0, 1, 2) is available in transforms for each
tick, though this example uses fixed URIs for simplicity.

Flow (repeated 3 times, once per second):
  1. GET /api/v1/hello -> captures response body via onResponseTransform
  2. POST /api/v1/goodbye -> sends captured body
  3. road-closed -> chain stops, timer schedules next tick

After execution, check client-data for 6 events (3 pairs).
