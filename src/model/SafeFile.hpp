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

#include <fstream>
#include <atomic>
#include <mutex>
#include <string>
#include <unistd.h> // sysconf

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{
class FileManager;

/**
 * This class allows safe writting of text/binary files.
 * Close procedure could be planned with a certain delay to optimize overhead
 * during i/o operations.
 * Maximum opened files within the system is also controlled by condition variable.
 */
class SafeFile {

    std::string path_;
    //std::ofstream file_; // we add read capabilities for UT:
    std::fstream file_;
    int max_open_files_;
    std::mutex mutex_; // write file mutex
    bool opened_;
    boost::asio::deadline_timer *timer_{};
    boost::asio::io_service *io_service_{};

    std::string data_; // used for read cache, but never shown in json string representation (just in case it is huge)
    bool read_cached_;

    FileManager *file_manager_{};

    void delayedClose(unsigned int closeDelayUs);

public:

    /**
    * Constructor
    *
    * @param fileManager parent reference to file manager.
    * @param path file path to write. It could be relative (to execution path) or absolute.
    * @param timersIoService asio io service which will be used to delay close
    * operations with the intention to reduce overhead in some scenarios. By default
    * it is not used (if not provided in constructor), so delay is not performed
    * regardless the close delay configured.
    * about I/O operations. It may be 'nullptr' if no metrics are enabled.
    * @param mode open mode. By default, text files and append is selected. You
    * could anyway add other flags, for example for binary dumps: std::ios::binary
    */
    SafeFile (FileManager *fileManager,
              const std::string& path,
              boost::asio::io_service *timersIoService = nullptr,
              std::ios_base::openmode mode = std::ofstream::out | std::ios_base::app);

    ~SafeFile();

    // Opened files control:
    static std::atomic<int> CurrentOpenedFiles;
    static std::mutex MutexOpenedFiles;
    static std::condition_variable OpenedFilesCV;

    /**
    * Open the file for writting
    *
    * @param mode open mode. By default, text files and append is selected. You
    * could anyway add other flags, for example for binary dumps: std::ios::binary
    *
    * @return Boolean about success operation.
    */
    bool open(std::ios_base::openmode mode = std::ofstream::out | std::ios_base::app);

    /**
    * Close the file
    */
    void close();

    /**
    * Empty the file
    */
    void empty();

    /**
    * Read the file content.
    *
    * @param success success of the read operation.
    * @param mode open mode. By default, text files are assumed, but you could
    * pass binary flag: std::ios::binary
    * @param cached enable cache mode to retrieve data
    *
    * @return Content read. Empty if failed to read. with cache enabled, empty data could be returned with success (no way no know if the file was empty or failed to read).
    */
    std::string read(bool &success, std::ios_base::openmode mode = std::ifstream::in, bool cached = false);

    /**
    * Json representation of the class instance
    */
    nlohmann::json getJson() const;

    /**
    * Write data to the file.
    * Close could be delayed.
    *
    * @param data data to write
    * @param closeDelayUs delay, after write operation, to close the file. By default
    * it is configured to 1 second, something appropiate to log over long term files.
    * Zero value means that no planned close is scheduled, so the file is opened,
    * written and closed in the same moment. This could be interesting for write
    * many different files when they are not rewritten. If they need to append
    * data later, you could provide a smaller value to indicate that it is a
    * short term file, but you may take into account the maximum number of files
    * that your system could open simultaneously. This class will blocks new open
    * operations when that limit is reached, to prevent file system errors, and this
    * will be exposed as performance drop. So if your application is writting many
    * different files, to optimize for example a load traffic rate of 200k req/s
    * with a limit of 1024 concurrent files, we need a maximum delay of 1024 divided
    * by 200000, that is to say, 0,00512 seconds = 5000 microseconds.
    */
    void write(const std::string& data, unsigned int closeDelayUs = 1000000);
};

}
}

