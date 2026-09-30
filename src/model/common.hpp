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

#include <memory>
#include <mutex>
#include <shared_mutex>

#include <ert/metrics/Metrics.hpp>

#define DEFAULT_ADMIN_PROVISION_STATE "initial"
#define DEFAULT_ADMIN_PROVISION_CLIENT_OUT_STATE "road-closed"


namespace h2agent
{
namespace model
{

class AdminData;
class Configuration;
class Vault;
class FileManager;
class SocketManager;
class MockServerData;
class MockClientData;

typedef struct {
    AdminData *AdminDataPtr;
    Configuration *ConfigurationPtr;
    Vault *VaultPtr;
    FileManager *FileManagerPtr;
    SocketManager *SocketManagerPtr;
    MockServerData *MockServerDataPtr;
    MockClientData *MockClientDataPtr;
    ert::metrics::Metrics *MetricsPtr;
    ert::metrics::bucket_boundaries_t ResponseDelaySecondsHistogramBucketBoundaries;
    ert::metrics::bucket_boundaries_t MessageSizeBytesHistogramBucketBoundaries;
    std::string ApplicationName;

} common_resources_t;

using mutex_t = std::shared_mutex;
using read_guard_t = std::shared_lock<mutex_t>;
using write_guard_t = std::unique_lock<mutex_t>;

// Mutex sharding factor for the hot-traffic Map-based stores (Vault, MockData).
//
// Why 16 (not 1, not 64):
//  - Power of two: makes the hash%N routing cheap and spreads keys evenly.
//  - Of the order of the concurrent writers/cores on a load-test host (commonly
//    8-32), so distinct keys from different threads rarely collide on the same
//    shard -- that is where the single-global-mutex contention is removed.
//  - Negligible footprint: 16 shared_mutex per store instance (a few hundred
//    bytes); the per-key hot path still takes exactly one lock.
//  - More shards would only add cost to the WHOLE-MAP admin ops (size/getJson/
//    forEach/clear lock ALL shards), with no extra hot-path benefit.
// Validated empirically: ~3-6x write/mixed throughput under concurrent,
// many-distinct-key load, ~1x (no regression) with a single thread.
//
// Sharding pays off ONLY with many DISTINCT keys written/read concurrently
// (e.g. Vault: many vault.X per provision; MockData: millions of subscribers
// each with its own URI plus FSM state updates). It does NOT help when traffic
// concentrates on one key (that lands on a single shard).
constexpr std::size_t VAULT_MUTEX_SHARDS = 16;
constexpr std::size_t MOCK_DATA_MUTEX_SHARDS = 16;

}
}

