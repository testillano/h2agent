// =============================================================================
// MockData sharding micro-benchmark
// =============================================================================
// Measures whether sharding the model Map helps the MockData (server/client
// event store) access pattern, which is DIFFERENT from the Vault one:
//
//   - WRITE per request:  loadEvent() -> getEvents() [Map::get] + Map::add()
//                         (a new key on first sight, then get on subsequent).
//   - READ from transforms: getEvent(ekey) -> Map::get() per key. Many
//                         provisions read PAST events to feed transformations,
//                         so the real traffic mix is READ-HEAVY, not write-only.
//
// Key facts that make this NOT the same as Vault:
//   * shared_mutex READS already run concurrently among themselves (no
//     serialization), so a read-heavy pattern has little single-mutex
//     contention to remove -- sharding may show little/no gain.
//   * Writes still serialize on the single mutex; sharding disperses those.
//   * A popular "hot key" (e.g. everyone reads the last event of one resource)
//     lands on ONE shard, so sharding cannot help that fraction.
//
// This bench therefore sweeps THREADS x KEYS with a configurable READ ratio and
// a hot-key fraction, so we can see under which realistic mix (if any) sharding
// MockData pays off before deciding to shard it.
//
// Build/run: use ./run.sh (compiles inside the h2agent builder image).
//   THREADS, KEYS, ROUNDS, REPEATS, READ_PCT, HOT_PCT, SWEEP are env knobs.
// =============================================================================

#include <Map.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using Clock = std::chrono::steady_clock;

std::size_t envSize(const char* name, std::size_t def) {
    const char* v = std::getenv(name);
    if (!v || !*v) return def;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(v, &end, 10);
    return (end && *end == '\0') ? static_cast<std::size_t>(parsed) : def;
}

// A value shaped like a MockData entry: a small JSON object (stand-in for the
// per-key MockEventsHistory handle; here we store a counter + payload so the
// read actually touches the value like a transformation would).
nlohmann::json makeEntry(long long seq) {
    return nlohmann::json{{"lastSeq", seq}, {"state", "ok"}};
}

// One mixed read/write run over a given Map. Each thread, per round, picks a key
// (hot-key with HOT_PCT probability, else a distinct per-(thread,key) key) and
// either READS it (READ_PCT probability) or WRITES it (loadEvent-like: get then
// add/modify). Returns elapsed seconds.
template <typename MapT>
double runMixedWorkload(MapT& map, std::size_t threads, std::size_t keys,
                        std::size_t rounds, unsigned readPct, unsigned hotPct,
                        std::size_t globalKeys) {
    std::atomic<bool> go{false};
    std::vector<std::thread> workers;
    workers.reserve(threads);

    // Key model:
    //  - globalKeys == 0 : per-thread distinct keys (many resources, one set per
    //                      thread) -- the optimistic "lots of distinct method/uri".
    //  - globalKeys  > 0 : a SINGLE shared pool of 'globalKeys' keys used by ALL
    //                      threads -- the realistic "a scenario has only a few
    //                      distinct method/uri resources, shared by all traffic".
    const bool global = (globalKeys > 0);

    // Pre-seed the map so reads hit existing keys (transformations read PAST events).
    map.add("hot_key", makeEntry(0));
    if (global) {
        for (std::size_t g = 0; g < globalKeys; ++g)
            map.add("g_k" + std::to_string(g), makeEntry(0));
    } else {
        for (std::size_t t = 0; t < threads; ++t)
            for (std::size_t k = 0; k < keys; ++k)
                map.add("t" + std::to_string(t) + "_k" + std::to_string(k), makeEntry(0));
    }

    for (std::size_t t = 0; t < threads; ++t) {
        workers.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 2654435761u + 1));
            std::uniform_int_distribution<unsigned> pct(0, 99);
            const std::size_t span = global ? globalKeys : (keys ? keys : 1);
            std::uniform_int_distribution<std::size_t> keyPick(0, span - 1);
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            for (std::size_t r = 0; r < rounds; ++r) {
                bool hot = (pct(rng) < hotPct);
                std::string key;
                if (hot) {
                    key = "hot_key";
                } else if (global) {
                    key = "g_k" + std::to_string(keyPick(rng));       // shared pool
                } else {
                    key = "t" + std::to_string(t) + "_k" + std::to_string(keyPick(rng));
                }

                if (pct(rng) < readPct) {
                    // READ path (transformation reading a past event).
                    nlohmann::json out;
                    map.tryGet(key, out);
                } else {
                    // WRITE path (loadEvent: read-modify-write of the entry).
                    map.modifyOrInsert(key, [&](nlohmann::json& v) {
                        if (!v.is_object()) v = makeEntry(0);
                        v["lastSeq"] = v["lastSeq"].get<long long>() + 1;
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

// Median of 'repeats' measurements (fresh map each run) after one warm-up.
template <std::size_t SHARDS>
double medianOpsPerSec(std::size_t threads, std::size_t keys, std::size_t rounds,
                       std::size_t repeats, unsigned readPct, unsigned hotPct,
                       std::size_t globalKeys) {
    const double totalOps = static_cast<double>(threads * rounds);
    {
        h2agent::model::Map<std::string, nlohmann::json, SHARDS> warm;
        runMixedWorkload(warm, threads, keys, rounds, readPct, hotPct, globalKeys);
    }
    std::vector<double> samples;
    samples.reserve(repeats);
    for (std::size_t i = 0; i < repeats; ++i) {
        h2agent::model::Map<std::string, nlohmann::json, SHARDS> map;
        double secs = runMixedWorkload(map, threads, keys, rounds, readPct, hotPct, globalKeys);
        samples.push_back(totalOps / secs);
    }
    std::sort(samples.begin(), samples.end());
    std::size_t mid = samples.size() / 2;
    return (samples.size() % 2) ? samples[mid] : 0.5 * (samples[mid - 1] + samples[mid]);
}

void sweep(std::size_t rounds, std::size_t repeats, unsigned readPct, unsigned hotPct,
           std::size_t globalKeys) {
    const std::vector<std::size_t> threadsList = {1, 2, 4, 8, 16};
    const std::vector<std::size_t> keysList = {1, 4, 8, 16, 32, 64};
    constexpr std::size_t SHARDS = 16;

    std::cout << "SWEEP (ROUNDS/thread=" << rounds << ", SHARDS=" << SHARDS
              << ", REPEATS=" << repeats << " median + warm-up)\n"
              << "MockData mix: READ_PCT=" << readPct << "% (rest writes), HOT_PCT="
              << hotPct << "% (single popular key)\n";
    if (globalKeys > 0) {
        std::cout << "GLOBAL_KEYS=" << globalKeys << " (all threads share this many distinct "
                  << "method/uri resources; KEYS column ignored)\n";
    } else {
        std::cout << "per-thread distinct keys (KEYS column = keys per thread)\n";
    }
    std::cout << "ops/s in millions; speedup = Nshard / 1shard\n\n";
    std::cout << "  THREADS  KEYS   1shard(M/s)   " << SHARDS << "shard(M/s)   speedup\n";
    std::cout << "  -------  ----   -----------   -----------   -------\n";
    for (std::size_t th : threadsList) {
        for (std::size_t k : keysList) {
            double one = medianOpsPerSec<1>(th, k, rounds, repeats, readPct, hotPct, globalKeys);
            double many = medianOpsPerSec<SHARDS>(th, k, rounds, repeats, readPct, hotPct, globalKeys);
            double speedup = (one > 0.0) ? (many / one) : 0.0;
            char line[160];
            std::snprintf(line, sizeof(line),
                          "  %7zu  %4zu   %11.2f   %11.2f   %6.2fx\n",
                          th, k, one / 1e6, many / 1e6, speedup);
            std::cout << line;
        }
    }
    std::cout << "\nInterpretation: MockData traffic is READ-HEAVY (transformations read past\n"
                 "events). shared_mutex reads already run concurrently, so a high READ_PCT\n"
                 "leaves little single-mutex contention for sharding to remove -- expect a\n"
                 "SMALLER win than Vault (which is write-per-key). A high HOT_PCT pins load on\n"
                 "one shard, further limiting the gain. Sharding MockData is only worth it if a\n"
                 "realistic mix here shows a clear, consistent speedup.\n";
}

}  // namespace

int main() {
    const std::size_t THREADS = envSize("THREADS", 8);
    const std::size_t KEYS = envSize("KEYS", 16);
    const std::size_t ROUNDS = envSize("ROUNDS", 20000);
    const std::size_t REPEATS = envSize("REPEATS", 5);
    const unsigned READ_PCT = static_cast<unsigned>(envSize("READ_PCT", 80));  // read-heavy default
    const unsigned HOT_PCT = static_cast<unsigned>(envSize("HOT_PCT", 10));
    const std::size_t GLOBAL_KEYS = envSize("GLOBAL_KEYS", 0);  // 0 = per-thread keys; >0 = shared pool
    constexpr std::size_t SHARDS = 16;

    if (std::getenv("SWEEP")) {
        sweep(ROUNDS, REPEATS, READ_PCT, HOT_PCT, GLOBAL_KEYS);
        return 0;
    }

    std::cout << "MockData sharding micro-benchmark\n"
              << "  THREADS=" << THREADS << " KEYS=" << KEYS << " ROUNDS/thread=" << ROUNDS
              << " SHARDS=" << SHARDS << " READ_PCT=" << READ_PCT << "% HOT_PCT=" << HOT_PCT << "%"
              << (GLOBAL_KEYS ? (" GLOBAL_KEYS=" + std::to_string(GLOBAL_KEYS)) : std::string(" (per-thread keys)"))
              << "\n  total ops = " << (THREADS * ROUNDS) << "\n\n";

    std::cout << "Mixed read/write workload (median of " << REPEATS << " runs + warm-up):\n";
    double one = medianOpsPerSec<1>(THREADS, KEYS, ROUNDS, REPEATS, READ_PCT, HOT_PCT, GLOBAL_KEYS);
    double many = medianOpsPerSec<SHARDS>(THREADS, KEYS, ROUNDS, REPEATS, READ_PCT, HOT_PCT, GLOBAL_KEYS);
    std::cout << "  1 shard (today) : " << static_cast<long long>(one) << " ops/s\n";
    std::cout << "  " << SHARDS << " shards        : " << static_cast<long long>(many) << " ops/s\n";
    std::cout << "  speedup         : " << (one > 0.0 ? many / one : 0.0) << "x\n";

    return 0;
}
