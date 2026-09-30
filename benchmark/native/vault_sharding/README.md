# Native benchmarks

Compiled, in-process benchmarks that isolate a specific code path. Unlike the
network load profiles under `benchmark/tests/{client,server}/` (declarative
`test.json` + provisions, driven by `start.sh` with h2load, no compilation),
these are **native C++ programs** that exercise the model directly, so the
measured effect is not masked by network or SUT limits. They are therefore kept
out of `tests/` (which is reserved for the `start.sh` network profiles).

## vault_sharding

Quantifies the effect of sharding the vault mutex (`src/model/Map.hpp`).

The vault is written on the **traffic hot path**: every `vault.X` transformation
target (from both the client and server roles) is a write under the map mutex.
A provision that touches **many** vault keys, under **many** concurrent
requests, serializes on that mutex. `Map` supports opt-in N-way sharding (the
`Shards` template parameter; `Vault` uses 16), so per-key writes lock only their
shard and independent keys no longer contend.

This benchmark reproduces that pattern: `THREADS` concurrent "provisions", each
writing `KEYS` distinct vault keys per round, over `ROUNDS` rounds, and compares
`Map<...,1>` (today's single global mutex) vs `Map<...,16>` (sharded) on the same
workload. It also probes the admin `getJson()` whole-map path under concurrent
writers (must not deadlock).

### Run

```bash
./run.sh                                   # defaults: THREADS=8 KEYS=16 ROUNDS=10000
THREADS=16 KEYS=32 ROUNDS=20000 ./run.sh   # heavier, vault-dense workload
SWEEP=1 ./run.sh                           # matrix of THREADS x KEYS with a speedup table
```

`SWEEP=1` runs a `THREADS x KEYS` matrix (1,2,4,8,16 threads x 1,4,8,16,32,64
keys) and prints, per cell, the 1-shard vs 16-shard throughput and the speedup.
It shows the win **growing** with both concurrency and the number of distinct
vault keys per provision (and staying ~1x at `THREADS=1` or `KEYS=1`, where there
is no contention to remove).

### Correctness (data races) is NOT checked here

This benchmark measures **performance only**; it exercises the sharded `Map` in
isolation, not the real `Vault` usage (client+server roles, transformations,
managers). To check for data races, use h2agent's **native** sanitizer support
on the actual code, which is the real gate:

```bash
SANITIZER=tsan build_type=Debug ./build.sh --image   # instrumented h2agent; then run under load
```

(or run the unit-test / CT targets built with `SANITIZER=tsan`). Do not treat a
clean run of this micro-benchmark as a race-freedom guarantee for h2agent.

`run.sh` compiles and runs inside the `h2agent_builder` image (needs
`./build.sh --builder` once, so the deps are available). Override the image with
`BUILDER_IMAGE=<image>`.

### Interpreting results

- The sharded run should show **higher ops/s and lower wall time** for the
  distinct-key workload, and the gap **widens** as `THREADS` and `KEYS` grow
  (more concurrency and more vault keys per provision = more contention on the
  single mutex).
- Sharding reduces contention by roughly `1/N` **only for distinct keys**. A
  single hot key shared across threads still serializes on its one shard (by
  design) -- and note that a shared counter incremented via
  `math.@{K}+1 -> vault.K` also has a read-modify-write race that sharding does
  not fix (see `docs/api` on per-subscriber vs global counters).
- The admin `getJson()` probe is a correctness/no-deadlock check, not a speed
  target: whole-map operations lock all shards on purpose (admin path).
