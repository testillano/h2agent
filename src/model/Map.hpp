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
#include <unordered_map>

#include <common.hpp>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

template<typename Key, typename Value>
class Map {

    typedef typename std::unordered_map<Key, Value> map_t;
    using IterationCallback = std::function<void(const Key&, const Value&)>;

    mutable mutex_t mutex_{};
    map_t map_{};

protected:
    bool clear_unsafe() noexcept {
        bool result = (map_.size() != 0); // same !
        map_.clear();
        return result;
    }

public:

    Map() {};

    /** copy constructor */
    Map(const Map& other) : map_{}
    {
        read_guard_t guard(other.mutex_);
        this->map_ = other.map_;
    }

    ~Map() = default;

    // getters

    bool exists(const Key& key) const
    {
        read_guard_t guard(mutex_);
        return (map_.find(key) != map_.end());
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
        read_guard_t guard(mutex_);
        auto it = map_.find(key);
        exists = (it != map_.end());
        return (exists ? it->second : Value{}); // return copy
    }

    /**
     * Getter which avoid copy of empty value and is more readable than get()
     */
    bool tryGet(const Key& key, Value& out_value) const {
        read_guard_t guard(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Copia solo si se encuentra. out_value es una referencia.
            out_value = it->second;
            return true;
        }
        return false;
    }

    //// Lvalue
    //bool insert_if_not_exists(const Key& key, const Value& value) {
    //    write_guard_t guard(mutex_);
    //    // try_emplace is the atomic equivalent to insert_if_not_exists
    //    return map_.try_emplace(key, value).second;
    //}

    //// Rvalue (max performance)
    //bool insert_if_not_exists(const Key& key, Value&& value) {
    //    write_guard_t guard(mutex_);
    //    return map_.try_emplace(key, std::move(value)).second;
    //}

    //template <typename... Args>
    //bool emplace(const Key& key, Args&&... args) {
    //    write_guard_t guard(mutex_);
    //    return map_.try_emplace(key, std::forward<Args>(args)...).second;
    //}

    /** map size */
    size_t size() const
    {
        read_guard_t guard(mutex_);
        return map_.size();
    }

    bool empty() const
    {
        read_guard_t guard(mutex_);
        return (map_.size() == 0);
    }

    /**
     * @brief Iterates safely over all elements in the map.
     * * This method provides **read access** to the map's elements in a **thread-safe** manner
     * by applying a user-defined callback function to each key-value pair. The entire
     * iteration is performed atomically and under a read lock.
     *
     * @attention This method acquires a \c std::shared_lock (read lock) for its full duration.
     * To ensure high concurrency, avoid placing long-running operations (such as file I/O or
     * thread sleeping) inside the callback function.
     *
     * @tparam Key The type of the map's key (e.g., std::string).
     * @tparam Value The type of the map's value (e.g., std::shared_ptr<T>).
     *
     * @param callback A function (lambda or functor) that is applied to every key-value pair.
     * The signature must be compatible with:
     * \code
     * void(const Key&, const Value&)
     * \endcode
     *
     * @note This function uses the callback pattern to prevent the exposure of unsafe iterators
     * (\c dangling iterators) to external threads.
     */
    void forEach(const IterationCallback& callback) const {
        read_guard_t guard(mutex_);
        for (const auto& pair : map_) {
            callback(pair.first, pair.second);
        }
    }

    /**
     * @brief Safely converts the internal map content into a JSON object.
     * * This method provides a thread-safe way to serialize the data stored in the
     * map by acquiring a read lock for the entire duration of the conversion.
     * * @details The method acquires a \c std::shared_lock on the map's mutex. While
     * the lock is held, the internal \c std::unordered_map is copied and cast
     * into a \c nlohmann::json object. This ensures that the map cannot be
     * modified (added to or removed from) by writer threads during serialization.
     * * @tparam Key The type of the map's key.
     * @tparam Value The type of the map's value.
     * * @note This implementation relies on \c nlohmann::json's built-in support
     * for converting \c std::unordered_map. If \c Value is a complex type (e.g.,
     * a smart pointer or a custom struct), ensure the appropriate \c to_json()
     * function is implemented for automatic serialization.
     *
     * @return nlohmann::json A new, thread-safe copy of the map's content
     * as a JSON object.
     */
    nlohmann::json getJson() const {
        read_guard_t guard(mutex_);
        nlohmann::json j(map_);
        return j;  // return copy
    }

    // setters

    /**
     * Adds a new value to the map
     * Lvalue variant
     *
     * @param key key to add
     * @param value stored
     */
    void add(const Key& key, const Value &value) {
        write_guard_t guard(mutex_);
        map_.insert_or_assign(key, value);
    }

    // Rvalue variant (std::move)
    void add(const Key& key, Value&& value) {
        write_guard_t guard(mutex_);
        map_.insert_or_assign(key, std::move(value));
    }

    /**
     * Adds another map of same kind to the map
     *
     * @param map map to add
     */
    void add(const map_t& m)
    {
        write_guard_t guard(mutex_);
        for (const auto& kv : m)
            map_.insert_or_assign(kv.first, kv.second);
    }

    /**
     * Removes key
     *
     * @param key key to remove
     */
    void remove(const Key& key, bool &exists)
    {
        write_guard_t guard(mutex_);
        exists = (map_.erase(key) > 0);
    }

    /** Clear map */
    // return if something was deleted
    bool clear()
    {
        write_guard_t guard(mutex_);
        return clear_unsafe(); // don't call Map::size() to avoid mutex deadlocks (this one uses map_.size())
    }
};

}
}

