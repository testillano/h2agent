Timer-based 2-step client flow triggered 3 times at 1 request per second.

Demonstrates rps/range triggering combined with state progression and
dynamic URI construction using the 'sequence' variable.

Each tick of the timer fires the full chain (initial->step2->road-closed),
so 3 ticks produce 6 total requests (3 GETs + 3 POSTs).

The 'sequence' variable (0, 1, 2) is used in transforms to build unique
URIs: /api/v1/hello/0, /api/v1/hello/1, /api/v1/hello/2 (and likewise
for /api/v1/goodbye/*). Server uses RegexMatching to handle all paths.

Flow (repeated 3 times, once per second):
  1. GET /api/v1/hello/<seq> -> captures response body
  2. POST /api/v1/goodbye/<seq> -> sends captured body
  3. road-closed -> chain stops, timer schedules next tick

After execution, check client-data for 6 events (3 pairs with unique URIs).
