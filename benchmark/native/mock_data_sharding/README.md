# MockData sharding micro-benchmark

Decides, with data, whether the model `Map` sharding that helped the `Vault`
(see `../vault_sharding/`) is also worth applying to **`MockData`** (the
server/client event store), before touching any code.

## Why MockData is NOT the same case as Vault

The Vault hot path is **write-per-key**: a provision loads many distinct
`vault.X` keys, and those writes serialize on a single mutex. Sharding disperses
them -> big win.

`MockData` has a different, **read-heavy** mix:

- **Write per request**: `loadEvent()` -> `getEvents()` (`Map::get`) + `Map::add`
  (a new key on first sight of a (method, uri, ...) resource, then updates).
- **Read from transformations**: many provisions read **past** events via
  `getEvent(ekey)` -> `Map::get` to feed transformations. This is the dominant
  traffic, so the real mix is **read-heavy**.

Two reasons sharding may help **less** here than for Vault:

1. `shared_mutex` **reads already run concurrently** among themselves (they take
   a shared lock, not exclusive). A read-heavy pattern therefore has little
   single-mutex contention to remove; sharding mainly disperses the *writes*.
2. A **hot key** (e.g. every transformation reading the last event of one
   popular resource) lands on a single shard, so that fraction cannot benefit.

## What it measures

A mixed read/write workload over a pre-seeded map, per thread per round:

- pick a key: with `HOT_PCT` probability the single `hot_key`, else a distinct
  per-(thread,key) key;
- with `READ_PCT` probability do a `tryGet` (transformation read), else a
  `modifyOrInsert` (loadEvent read-modify-write).

It reports 1-shard vs 16-shard throughput (median of `REPEATS` runs + warm-up),
single mode and a `THREADS x KEYS` sweep.

## Run

```bash
./run.sh                              # defaults: READ_PCT=80, HOT_PCT=10
READ_PCT=50 HOT_PCT=0 ./run.sh        # write-heavier, no hot key (upper bound for sharding)
READ_PCT=95 HOT_PCT=30 ./run.sh       # very read-heavy with a popular key (pessimistic)
SWEEP=1 ./run.sh                      # THREADS x KEYS matrix + speedup table
```

Env knobs: `THREADS`, `KEYS`, `ROUNDS`, `REPEATS`, `READ_PCT`, `HOT_PCT`,
`SWEEP`.

## How to read the result

- **Clear, consistent speedup across realistic mixes** (say `READ_PCT` 70-90,
  `HOT_PCT` 5-20) -> sharding `MockData` is justified; apply it the same opt-in
  way as `Vault` (`Map<Key, Value, N>`).
- **~1x (or noise) at realistic read ratios**, with a win only appearing at low
  `READ_PCT` / `HOT_PCT=0` -> the real traffic will not benefit; **do not shard**
  MockData (sharding still adds cost to whole-map admin ops like `summary()` /
  `getJson()` / `getSequence()` that lock all shards).

## Correctness

Performance probe only. To check the sharded `Map` for data races, use
h2agent's native ThreadSanitizer build on the real code:

```bash
SANITIZER=tsan build_type=Debug ./build.sh --image   # then run under load
```
