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

#include <unordered_map>

#include <common.hpp>


namespace h2agent
{
namespace model
{

template<typename Key, typename Value>
class Map {
public:

    typedef typename std::unordered_map<Key, Value> map_t;
    typedef typename std::unordered_map<Key, Value>::const_iterator map_it;

    Map() {};

    /** copy constructor */
    explicit Map(const map_t& m): map_(m) {};

    ~Map() = default;

    // getters

    /** Returns a copy of the entire map */
    map_t get() const
    {
        read_guard_t guard(rw_mutex_);
        return map_;
    }

    /** copy operator */
    Map& operator= (const map_t& other)
    {
        write_guard_t guard(rw_mutex_);
        map_ = other;
        return *this;
    }

    /**
     * Searchs map key
     *
     * @param key key to find
     * @return no const iterator for provided key
     */
    map_it find(const Key& key) const
    {
        read_guard_t guard(rw_mutex_);
        return map_.find(key);
    }

    /** begin iterator */
    map_it begin() const
    {
        read_guard_t guard(rw_mutex_);
        return map_.begin();
    }

    /** end iterator */
    map_it end() const
    {
        read_guard_t guard(rw_mutex_);
        return map_.end();
    }

    /** map size */
    size_t size()
    {
        read_guard_t guard(rw_mutex_);
        return map_.size();
    }

    // setters

    /**
     * Adds a new value to the map
     *
     * @param key key to add
     * @param value stored
     */
    void add(const Key& key, const Value &value)
    {
        write_guard_t wr_lock(rw_mutex_);
        map_[key] = value;
    }

    /**
     * Removes key
     *
     * @param it key iterator to remove
     */
    void remove(map_it it)
    {
        write_guard_t guard(rw_mutex_);
        map_.erase(it);
    }

    /**
     * Removes key
     *
     * @param key key to remove
     */
    void remove(const Key& key)
    {
        write_guard_t guard(rw_mutex_);
        map_.erase(key);
    }

    /** Clear map */
    void clear()
    {
        write_guard_t guard(rw_mutex_);
        map_.clear();
    }

protected:
    mutable mutex_t rw_mutex_;
    map_t map_;
};

}
}

