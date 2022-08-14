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

#include <string>

#include <ert/tracing/Logger.hpp>

#include <SafeFile.hpp>
#include <FileManager.hpp>
#include <functions.hpp>


namespace h2agent
{
namespace model
{

std::atomic<int> SafeFile::CurrentOpenedFiles(0);
std::mutex SafeFile::MutexOpenedFiles;
std::condition_variable SafeFile::OpenedFilesCV;

SafeFile::SafeFile (FileManager *fileManager, const std::string& path, boost::asio::io_service *timersIoService, std::ios_base::openmode mode):
    path_(path),
    io_service_(timersIoService),
    file_manager_(fileManager),
    opened_(false),
    read_cached_(false),
    timer_(nullptr)
{
    max_open_files_ = sysconf(_SC_OPEN_MAX /* 1024 probably */) - 10 /* margin just in case the process open other files */;
    open(mode);
}

SafeFile::~SafeFile() {
    close();
    delete timer_;
}

void SafeFile::delayedClose(unsigned int closeDelayUs) {
    // metrics
    file_manager_->incrementObservedDelayedCloseOperationCounter();

    //if (!io_service_) return; // protection
    if (!timer_) timer_ = new boost::asio::deadline_timer(*io_service_, boost::posix_time::microseconds(closeDelayUs));
    timer_->cancel();
    timer_->expires_from_now(boost::posix_time::microseconds(closeDelayUs));
    timer_->async_wait([this] (const boost::system::error_code& e) {
        if( e ) return; // probably, we were cancelled (boost::asio::error::operation_aborted)
        close();
    });
}

bool SafeFile::open(std::ios_base::openmode mode) {
    std::unique_lock<std::mutex> lock(MutexOpenedFiles);
    if (opened_) return true;

    //Wait until we have data or a quit signal
    OpenedFilesCV.wait(lock, [this]
    {
        return (CurrentOpenedFiles.load() < max_open_files_);
    });

    // After wait, we own the lock
    file_.open(path_, mode);
    if (file_.is_open()) {
        opened_ = true;
        CurrentOpenedFiles++;
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("'%s' opened (currently opened: %d)", path_.c_str(), CurrentOpenedFiles.load()), ERT_FILE_LOCATION));
        // metrics
        file_manager_->incrementObservedOpenOperationCounter();
    }
    else {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Failed to open '%s'", path_.c_str()), ERT_FILE_LOCATION));
        // metrics
        file_manager_->incrementObservedErrorOpenOperationCounter();
        return false;
    }
    //lock.unlock();

    return true;
}

void SafeFile::close() {
    std::unique_lock<std::mutex> lock(MutexOpenedFiles);
    if (!opened_) return;

    file_.close();
    opened_ = false;
    CurrentOpenedFiles--;
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("'%s' closed (currently opened: %d)", path_.c_str(), CurrentOpenedFiles.load()), ERT_FILE_LOCATION));
    // metrics
    file_manager_->incrementObservedCloseOperationCounter();

    lock.unlock();
    OpenedFilesCV.notify_one();
}

void SafeFile::empty() {
    close();
    open(std::ofstream::out | std::ofstream::trunc);
    close();
    // metrics
    file_manager_->incrementObservedEmptyOperationCounter();
}

std::string SafeFile::read(bool &success, std::ios_base::openmode mode, bool cached) {

    // Check cached data:
    if (cached && read_cached_) {
        success = true;
        return data_;
    }

    std::string result;
    success = false;

    if (open(mode)) {
        success = true;
        std::string chunk;
        while (std::getline (file_, chunk))
        {
            result += chunk;
        }
        LOGDEBUG(
            std::string output;
            h2agent::model::asAsciiString(result, output);
            ert::tracing::Logger::debug(ert::tracing::Logger::asString("Read '%s': %s", path_.c_str(), output.c_str()), ERT_FILE_LOCATION);
        );
    }

    // close the file after reading it:
    close();

    if (cached) {
        read_cached_ = success;
        data_ = std::move(result);
        return data_;
    }

    return result;
}

nlohmann::json SafeFile::getJson() const {
    nlohmann::json result;

    result["path"] = path_;

    std::ifstream file( path_, std::ofstream::in | std::ios::ate | std::ios::binary); // valid also for text files
    std::string::size_type size = file.tellg();
    if (size != std::string::npos /* -1 */) {
        result["bytes"] = (unsigned int)size;
        result["state"] = (opened_ ? "opened":"closed");
    }
    else {
        result["state"] = "missing";
    }
    file.close();

    if (read_cached_) result["readCache"] = "true";

    return result;
}

void SafeFile::write (const std::string& data, unsigned int closeDelayUs) {
    // Open file (lazy):
    if (!open()) return;

    // trace delay
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Close delay for write operation is: %lu", closeDelayUs), ERT_FILE_LOCATION));

    // Write file:
    std::lock_guard<std::mutex> lock(mutex_);
    file_.write(data.c_str(), data.size());
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Data written into '%s'", path_.c_str()), ERT_FILE_LOCATION));

    // metrics
    file_manager_->incrementObservedWriteOperationCounter();

    // Close file:
    if (io_service_ && closeDelayUs != 0) {
        delayedClose(closeDelayUs);
    }
    else {
        close();

        // metrics
        file_manager_->incrementObservedInstantCloseOperationCounter();
    }
}

}
}

