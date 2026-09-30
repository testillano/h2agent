/*
Vault sharding micro-benchmark (standalone, NOT part of the unit-test suite).

Purpose: isolate and quantify the lock-contention effect of sharding the vault
mutex (src/model/Map.hpp), reproducing the traffic pattern that matters:

    K concurrent threads, each simulating a "provision" that writes M distinct
    vault keys per request (modifyOrInsert, the 'vault.X' transformation path),
    repeated over R rounds.

It compares Map<..., 1> (today's single global mutex) vs Map<..., SHARDS>
(the sharded vault) on the SAME workload, and also measures the admin
whole-map getJson() path under concurrent writers.

This is a lock-contention probe, not an end-to-end HTTP benchmark (the
benchmark/tests/ directory holds the network-level load harness). It includes
the header-only Map (which pulls in nlohmann/json and ert/metrics via
common.hpp), so build it where those deps are available -- easiest inside the
builder image via the provided run.sh:

  ./benchmark/native/vault_sharding/run.sh
  THREADS=16 KEYS=32 ROUNDS=20000 ./benchmark/native/vault_sharding/run.sh

Or manually, if the deps are installed under /usr/local:

  g++ -O2 -std=c++17 -pthread -I src/model -I /usr/local/include \
      benchmark/native/vault_sharding/vault_sharding_bench.cpp -o /tmp/vault_bench
  /tmp/vault_bench

Env knobs: THREADS (default 8), KEYS per round/provision (default 16),
ROUNDS per thread (default 10000), SHARDS for the sharded case (default 16).
*/

#include <Map.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

std::size_t envSize(const char* name, std::size_t dflt) {
    const char* v = std::getenv(name);
    if (!v) return dflt;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    return (end && *end == '\0' && n > 0) ? static_cast<std::size_t>(n) : dflt;
}

using Clock = std::chrono::steady_clock;

// One benchmark run over a given Map instance. Each thread writes KEYS distinct
// keys per round (namespaced by thread so keys are independent, the common
// per-subscriber case), for ROUNDS rounds. Returns elapsed seconds.
template <typename MapT>
double runWriteWorkload(MapT& map, std::size_t threads, std::size_t keys, std::size_t rounds) {
    std::atomic<bool> go{false};
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (std::size_t t = 0; t < threads; ++t) {
        workers.emplace_back([&, t]() {
            while (!go.load(std::memory_order_acquire)) { /* spin to start together */ }
            for (std::size_t r = 0; r < rounds; ++r) {
                for (std::size_t k = 0; k < keys; ++k) {
                    // Distinct key per (thread,key): the per-subscriber pattern.
                    std::string key = "t" + std::to_string(t) + "_k" + std::to_string(k);
                    map.modifyOrInsert(key, [](nlohmann::json& v) {
                        if (!v.is_number_integer()) v = 0;
                        v = v.get<long long>() + 1;
                    });
                }
            }
        });
    }

    auto start = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& w : workers) w.join();
    auto end = Clock::now();
    return std::chrono::duration<double>(end - start).count();
}

// Return ops/s for a fresh map of the given shard count, taking the MEDIAN of
// 'repeats' measurements after one discarded warm-up run (fills caches and the
// allocator so scheduling/first-touch noise does not skew the result). A fresh
// map is used for every run so results are independent.
template <std::size_t SHARDS>
double medianOpsPerSec(std::size_t threads, std::size_t keys, std::size_t rounds, std::size_t repeats) {
    const double totalOps = static_cast<double>(threads * keys * rounds);

    // Warm-up (discarded).
    {
        h2agent::model::Map<std::string, nlohmann::json, SHARDS> warm;
        runWriteWorkload(warm, threads, keys, rounds);
    }

    std::vector<double> samples;
    samples.reserve(repeats);
    for (std::size_t i = 0; i < repeats; ++i) {
        h2agent::model::Map<std::string, nlohmann::json, SHARDS> map;
        double secs = runWriteWorkload(map, threads, keys, rounds);
        samples.push_back(totalOps / secs);
    }
    std::sort(samples.begin(), samples.end());
    std::size_t mid = samples.size() / 2;
    return (samples.size() % 2) ? samples[mid] : 0.5 * (samples[mid - 1] + samples[mid]);
}

// SWEEP mode: run a THREADS x KEYS matrix (fixed ROUNDS) and print, for each
// cell, the 1-shard vs N-shard throughput (median of REPEATS runs) and the
// resulting speedup. Shows how the sharding win GROWS with concurrency and with
// the number of distinct vault keys a provision touches.
void sweep(std::size_t rounds, std::size_t repeats) {
    const std::vector<std::size_t> threadsList = {1, 2, 4, 8, 16};
    const std::vector<std::size_t> keysList = {1, 4, 8, 16, 32, 64};
    constexpr std::size_t SHARDS = 16;

    std::cout << "SWEEP (ROUNDS/thread=" << rounds << ", SHARDS=" << SHARDS
              << ", REPEATS=" << repeats << " median + warm-up)\n"
              << "distinct per-subscriber keys; ops/s in millions; speedup = Nshard / 1shard\n\n";
    std::cout << "  THREADS  KEYS   1shard(M/s)   " << SHARDS << "shard(M/s)   speedup\n";
    std::cout << "  -------  ----   -----------   -----------   -------\n";
    for (std::size_t th : threadsList) {
        for (std::size_t k : keysList) {
            double one = medianOpsPerSec<1>(th, k, rounds, repeats);
            double many = medianOpsPerSec<SHARDS>(th, k, rounds, repeats);
            double speedup = (one > 0.0) ? (many / one) : 0.0;
            char line[160];
            std::snprintf(line, sizeof(line),
                          "  %7zu  %4zu   %11.2f   %11.2f   %6.2fx\n",
                          th, k, one / 1e6, many / 1e6, speedup);
            std::cout << line;
        }
    }
    std::cout << "\nExpected shape: speedup ~1x at THREADS=1 or KEYS=1 (no contention to remove),\n"
                 "growing with both THREADS and KEYS (more concurrent writers hitting more\n"
                 "distinct keys = more contention on the single mutex that sharding disperses).\n";
}

}  // namespace

int main() {
    const std::size_t THREADS = envSize("THREADS", 8);
    const std::size_t KEYS = envSize("KEYS", 16);
    const std::size_t ROUNDS = envSize("ROUNDS", 10000);
    const std::size_t REPEATS = envSize("REPEATS", 5);
    constexpr std::size_t SHARDS = 16;

    // SWEEP=1 -> matrix mode (THREADS/KEYS env are ignored; ROUNDS/REPEATS honored).
    if (std::getenv("SWEEP")) {
        sweep(ROUNDS, REPEATS);
        return 0;
    }

    std::cout << "Vault sharding micro-benchmark\n"
              << "  THREADS=" << THREADS << " KEYS/round=" << KEYS
              << " ROUNDS/thread=" << ROUNDS << " SHARDS=" << SHARDS << "\n"
              << "  total writes = " << (THREADS * KEYS * ROUNDS) << "\n\n";

    std::cout << "Write workload (distinct per-subscriber keys, median of " << REPEATS
              << " runs + warm-up):\n";
    {
        double one = medianOpsPerSec<1>(THREADS, KEYS, ROUNDS, REPEATS);
        double many = medianOpsPerSec<SHARDS>(THREADS, KEYS, ROUNDS, REPEATS);
        std::cout << "  1 shard (today) : " << static_cast<long long>(one) << " ops/s\n";
        std::cout << "  " << SHARDS << " shards        : " << static_cast<long long>(many) << " ops/s\n";
        std::cout << "  speedup         : " << (one > 0.0 ? many / one : 0.0) << "x\n";
    }

    // Admin path under concurrent writers: a reader hammering getJson() while
    // writers run. This must not deadlock and shows the whole-map cost.
    std::cout << "\nAdmin getJson() under concurrent writers (correctness/no-deadlock probe):\n";
    {
        h2agent::model::Map<std::string, nlohmann::json, SHARDS> sharded;
        std::atomic<bool> stop{false};
        std::atomic<long long> snapshots{0};
        std::thread reader([&]() {
            while (!stop.load()) {
                nlohmann::json j = sharded.getJson();
                snapshots.fetch_add(1, std::memory_order_relaxed);
            }
        });
        double secs = runWriteWorkload(sharded, THREADS, KEYS, ROUNDS);
        stop.store(true);
        reader.join();
        std::cout << "  writers done in " << secs << " s; getJson snapshots taken="
                  << snapshots.load() << ", final size=" << sharded.size() << "\n";
    }

    std::cout << "\nNote: sharding reduces contention ~1/N for DISTINCT keys; a shared\n"
                 "hot key still serializes on its single shard (by design).\n";
    return 0;
}
