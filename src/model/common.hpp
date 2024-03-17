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
class GlobalVariable;
class FileManager;
class SocketManager;
class MockServerData;
class MockClientData;

typedef struct {
    AdminData *AdminDataPtr;
    Configuration *ConfigurationPtr;
    GlobalVariable *GlobalVariablePtr;
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

}
}

