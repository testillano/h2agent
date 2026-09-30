/*
 ___________________________________________
|    _     ___                        _     |
|   | |   |__ \                      | |    |
|   | |__    ) |__ _  __ _  ___ _ __ | |_   |
|   | '_ \  / // _` |/ _` |/ _ \ '_ \| __|  |  HTTP/2 AGENT FOR MOCK TESTING
|   | | | |/ /| (_| | (_| |  __/ | | | |_   |  Version 0.0.z
|   |_| |_|____\__,_|\__, |\___|_| |_|\__|  |  https://github.com/testillano/h2agent
|                     __/ |                 |
|                    |___/                  |
|___________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

// Better unordered_map than map:
// Slighly more memory consumption (not significative in load tests) due to the hash map.
// But order is not important for us, and the size is not very big (prune is normally
//  applied in load test provisions), so the cache is not used.
// As insertion and deletion are equally fast for both containers, we focus on search
//  (O(log2(n)) for map as binary tree, O(1) constant as average (O(n) in worst case)
//  for unordered map as hash table), so for our case, unordered_map seems to be the best choice.
#include <array>
#include <functional>
#include <unordered_map>

#include <common.hpp>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

/**
 * Thread-safe associative map with OPT-IN mutex sharding.
 *
 * @tparam Key    key type
 * @tparam Value  value type
 * @tparam Shards number of independent lock shards (default 1).
 *
 * With \c Shards == 1 (the default) this is byte-for-byte the historical
 * single-\c shared_mutex map: every existing user (event stores, etc.) keeps
 * exactly the old behaviour with zero extra overhead.
 *
 * With \c Shards > 1 the keyspace is partitioned across N independent
 * (\c mutex, \c unordered_map) shards, routed by \c std::hash(key) % Shards:
 * - PER-KEY operations (get/tryGet/exists/add/remove/modifyOrInsert) lock ONLY
 *   the shard owning the key, so writes to independent keys from different
 *   threads do not serialize against each other. This is the hot-traffic win
 *   (e.g. a provision touching many vault keys under high concurrency).
 * - WHOLE-MAP operations (size/empty/forEach/getJson/clear/copy) must observe
 *   every shard, so they lock ALL shards in a FIXED ascending order (which is
 *   deadlock-free). These are administrative/report paths (e.g.
 *   GET /admin/v1/vault), NOT the traffic hot path, so global locking there is
 *   acceptable by design.
 *
 * Per-key atomicity of \c modifyOrInsert (read-modify-write under one lock) is
 * preserved in both modes.
 */
template<typename Key, typename Value, std::size_t Shards = 1>
class Map {

    static_assert(Shards >= 1, "Map requires at least one shard");

    typedef typename std::unordered_map<Key, Value> map_t;
    using IterationCallback = std::function<void(const Key&, const Value&)>;

    struct Shard {
        mutable mutex_t mutex_{};
        map_t map_{};
    };

    std::array<Shard, Shards> shards_{};

    // Route a key to its shard. For Shards==1 this is a no-op (index 0), so the
    // hash is not even computed on the single-shard hot path.
    Shard& shardFor(const Key& key) {
        if constexpr (Shards == 1) return shards_[0];
        else return shards_[std::hash<Key>{}(key) % Shards];
    }
    const Shard& shardFor(const Key& key) const {
        if constexpr (Shards == 1) return shards_[0];
        else return shards_[std::hash<Key>{}(key) % Shards];
    }

public:

    Map() {};

    /** copy constructor: snapshot every shard of 'other' under read locks
     * (fixed ascending order), then copy into our matching shards. */
    Map(const Map& other) {
        for (std::size_t i = 0; i < Shards; ++i) {
            read_guard_t guard(other.shards_[i].mutex_);
            shards_[i].map_ = other.shards_[i].map_;
        }
    }

    ~Map() = default;

    // getters

    bool exists(const Key& key) const
    {
        const Shard& s = shardFor(key);
        read_guard_t guard(s.mutex_);
        return (s.map_.find(key) != s.map_.end());
    }

    /**
     * Searchs map key
     *
     * @param key key to find
     * @param exits written by reference
     * @return Value for provided key, or initizalized Value when key is missing
     */
    Value get(const Key& key, bool &exists) const
    {
        const Shard& s = shardFor(key);
        read_guard_t guard(s.mutex_);
        auto it = s.map_.find(key);
        exists = (it != s.map_.end());
        return (exists ? it->second : Value{}); // return copy
    }

    /**
     * Getter which avoid copy of empty value and is more readable than get()
     */
    bool tryGet(const Key& key, Value& out_value) const {
        const Shard& s = shardFor(key);
        read_guard_t guard(s.mutex_);
        auto it = s.map_.find(key);
        if (it != s.map_.end()) {
            out_value = it->second;
            return true;
        }
        return false;
    }

    /** map size (sum across shards; each shard read-locked in turn) */
    size_t size() const
    {
        size_t total = 0;
        for (const auto& s : shards_) {
            read_guard_t guard(s.mutex_);
            total += s.map_.size();
        }
        return total;
    }

    bool empty() const
    {
        for (const auto& s : shards_) {
            read_guard_t guard(s.mutex_);
            if (!s.map_.empty()) return false;
        }
        return true;
    }

    /**
     * @brief Iterates safely over all elements in the map.
     * * This method provides **read access** to the map's elements in a **thread-safe** manner
     * by applying a user-defined callback function to each key-value pair. With sharding, ALL
     * shards are read-locked in fixed ascending order for the full iteration (a consistent
     * snapshot). Admin/report path -- keep callbacks short.
     *
     * @param callback A function applied to every key-value pair, signature void(const Key&, const Value&).
     *
     * @note This function uses the callback pattern to prevent the exposure of unsafe iterators
     * (\c dangling iterators) to external threads.
     */
    void forEach(const IterationCallback& callback) const {
        // Lock all shards (ascending order) for a consistent whole-map view.
        std::array<read_guard_t, Shards> guards = lockAllShared();
        for (const auto& s : shards_) {
            for (const auto& pair : s.map_) {
                callback(pair.first, pair.second);
            }
        }
    }

    /**
     * @brief Safely converts the internal map content into a JSON object.
     *
     * Locks ALL shards (read, fixed ascending order) for the duration so the
     * serialized object is a consistent snapshot across the whole keyspace.
     * Administrative path (e.g. GET /admin/v1/vault), not the traffic hot path.
     *
     * @return nlohmann::json A new copy of the map's content as a JSON object.
     */
    nlohmann::json getJson() const {
        std::array<read_guard_t, Shards> guards = lockAllShared();
        if constexpr (Shards == 1) {
            return nlohmann::json(shards_[0].map_);  // return copy
        } else {
            nlohmann::json j = nlohmann::json::object();
            for (const auto& s : shards_) {
                for (const auto& pair : s.map_) {
                    j[pair.first] = pair.second;
                }
            }
            return j;
        }
    }

    // setters

    /**
     * Adds a new value to the map (Lvalue variant). Locks only the key's shard.
     */
    void add(const Key& key, const Value &value) {
        Shard& s = shardFor(key);
        write_guard_t guard(s.mutex_);
        s.map_.insert_or_assign(key, value);
    }

    // Rvalue variant (std::move)
    void add(const Key& key, Value&& value) {
        Shard& s = shardFor(key);
        write_guard_t guard(s.mutex_);
        s.map_.insert_or_assign(key, std::move(value));
    }

    /**
     * Atomically reads, modifies and writes back a value under a single shard
     * write lock. If the key doesn't exist, a default-constructed Value is
     * passed to the modifier.
     *
     * @param key key to modify
     * @param modifier function that receives a reference to the value and modifies it in place
     */
    template<typename Modifier>
    void modifyOrInsert(const Key& key, Modifier&& modifier) {
        Shard& s = shardFor(key);
        write_guard_t guard(s.mutex_);
        modifier(s.map_[key]); // operator[] inserts default if missing
    }

    /**
     * Adds another map of same kind to the map. Each entry is routed to its own
     * shard and locked individually.
     *
     * @param m map to add
     */
    void add(const map_t& m)
    {
        for (const auto& kv : m) {
            Shard& s = shardFor(kv.first);
            write_guard_t guard(s.mutex_);
            s.map_.insert_or_assign(kv.first, kv.second);
        }
    }

    /**
     * Removes key (locks only the key's shard)
     *
     * @param key key to remove
     */
    void remove(const Key& key, bool &exists)
    {
        Shard& s = shardFor(key);
        write_guard_t guard(s.mutex_);
        exists = (s.map_.erase(key) > 0);
    }

    /** Clear map (locks ALL shards for write, fixed ascending order).
     *  @return true if something was deleted */
    bool clear()
    {
        std::array<write_guard_t, Shards> guards = lockAllExclusive();
        bool result = false;
        for (auto& s : shards_) {
            if (!s.map_.empty()) result = true;
            s.map_.clear();
        }
        return result;
    }

private:
    // Acquire a read lock on every shard in fixed ascending order (deadlock-free).
    std::array<read_guard_t, Shards> lockAllShared() const {
        return lockAllSharedImpl(std::make_index_sequence<Shards>{});
    }
    template<std::size_t... I>
    std::array<read_guard_t, Shards> lockAllSharedImpl(std::index_sequence<I...>) const {
        return { read_guard_t(shards_[I].mutex_)... };
    }

    // Acquire a write lock on every shard in fixed ascending order (deadlock-free).
    std::array<write_guard_t, Shards> lockAllExclusive() {
        return lockAllExclusiveImpl(std::make_index_sequence<Shards>{});
    }
    template<std::size_t... I>
    std::array<write_guard_t, Shards> lockAllExclusiveImpl(std::index_sequence<I...>) {
        return { write_guard_t(shards_[I].mutex_)... };
    }
};

}
}

