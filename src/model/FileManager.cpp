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

#include <memory>

#include <ert/tracing/Logger.hpp>

#include <FileManager.hpp>

namespace h2agent
{
namespace model
{


void FileManager::enableMetrics(ert::metrics::Metrics *metrics, const std::string &source) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::labels_t familyLabels = {{"source", source}};

        ert::metrics::counter_family_t& cf = metrics->addCounterFamily("h2agent_file_manager_operations_counter", "File system operations counter in h2agent_file_manager", familyLabels);
        observed_open_operation_counter_ = &(cf.Add({{"operation", "open"}}));
        observed_close_operation_counter_ = &(cf.Add({{"operation", "close"}}));
        observed_write_operation_counter_ = &(cf.Add({{"operation", "write"}}));
        observed_empty_operation_counter_ = &(cf.Add({{"operation", "empty"}}));
        observed_delayed_close_operation_counter_ = &(cf.Add({{"operation", "delayedClose"}}));
        observed_instant_close_operation_counter_ = &(cf.Add({{"operation", "instantClose"}}));
        observed_error_open_operation_counter_ = &(cf.Add({{"result", "failed"}, {"operation", "open"}}));
    }
}

void FileManager::incrementObservedOpenOperationCounter() {
    if (metrics_) observed_open_operation_counter_->Increment();
}

void FileManager::incrementObservedCloseOperationCounter() {
    if (metrics_) observed_close_operation_counter_->Increment();
}

void FileManager::incrementObservedWriteOperationCounter() {
    if (metrics_) observed_write_operation_counter_->Increment();
}

void FileManager::incrementObservedEmptyOperationCounter() {
    if (metrics_) observed_empty_operation_counter_->Increment();
}

void FileManager::incrementObservedDelayedCloseOperationCounter() {
    if (metrics_) observed_delayed_close_operation_counter_->Increment();
}

void FileManager::incrementObservedInstantCloseOperationCounter() {
    if (metrics_) observed_instant_close_operation_counter_->Increment();
}

void FileManager::incrementObservedErrorOpenOperationCounter() {
    if (metrics_) observed_error_open_operation_counter_->Increment();
}

void FileManager::write(const std::string &path, const std::string &data, bool textOrBinary, unsigned int closeDelayUs) {

    std::shared_ptr<SafeFile> safeFile;

    auto it = get(path);
    if (it != end()) {
        safeFile = it->second;
    }
    else {
        std::ios_base::openmode mode = std::ofstream::out | std::ios_base::app; // for text files
        if (!textOrBinary) mode |= std::ios::binary;

        safeFile = std::make_shared<SafeFile>(this, path, io_context_, mode);
        add(path, safeFile);
    }

    safeFile->write(data, closeDelayUs);
}

bool FileManager::read(const std::string &path, std::string &data, bool textOrBinary) {

    bool result;

    std::shared_ptr<SafeFile> safeFile;
    std::ios_base::openmode mode = std::ifstream::in; // for text files

    auto it = get(path);
    if (it != end()) {
        safeFile = it->second;
    }
    else {
        if (!textOrBinary) mode |= std::ios::binary;

        safeFile = std::make_shared<SafeFile>(this, path, io_context_, mode);
        add(path, safeFile);
    }

    data = safeFile->read(result, mode, read_cache_);

    return result;
}

void FileManager::empty(const std::string &path) {

    std::shared_ptr<SafeFile> safeFile;

    auto it = get(path);
    if (it != end()) {
        safeFile = it->second;
    }
    else {
        safeFile = std::make_shared<SafeFile>(this, path, io_context_);
        add(path, safeFile);
    }

    safeFile->empty();
}

bool FileManager::clear()
{
    bool result = (map_.size() > 0); // something deleted

    map_.clear(); // shared_ptr dereferenced too

    return result;
}

nlohmann::json FileManager::getConfigurationJson() const {

    nlohmann::json result;
    result["readCache"] = read_cache_ ? "enabled":"disabled";

    return result;
} // LCOV_EXCL_LINE

std::string FileManager::configurationAsJsonString() const {

    return (getConfigurationJson().dump());
}

nlohmann::json FileManager::getJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->getJson());
    };

    return result;
}

std::string FileManager::asJsonString() const {

    return ((size() != 0) ? getJson().dump() : "[]");
}


}
}

