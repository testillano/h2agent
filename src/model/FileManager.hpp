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

#include <nlohmann/json.hpp>

#include <Map.hpp>
#include <SafeFile.hpp>


namespace ert
{
namespace metrics
{
class Metrics;
}
}

namespace h2agent
{
namespace model
{

/**
 * This class stores a list of safe files.
 */
class FileManager : public Map<std::string, std::shared_ptr<SafeFile>>
{
    mutable mutex_t rw_mutex_{};
    boost::asio::io_service *io_service_{};

    // metrics (will be passed to SafeFile):
    ert::metrics::Metrics *metrics_{};

public:
    /**
    * File manager class
    *
    * @param timersIoService timers IO service needed to schedule delayed close operations.
    * If you never schedule close operations (@see write) it may be 'nullptr'.
    * @param metrics underlaying reference for SafeFile in order to compute prometheus metrics
    * about I/O operations. It may be 'nullptr' if no metrics are enabled.
    *
    * @see SafeFile
    */
    FileManager(boost::asio::io_service *timersIoService = nullptr, ert::metrics::Metrics *metrics = nullptr) : io_service_(timersIoService), metrics_(metrics) {;}
    ~FileManager() = default;

    /**
    * Set metrics reference
    *
    * @param metrics Optional metrics object to compute counters
    */
    void enableMetrics(ert::metrics::Metrics *metrics) {
        metrics_ = metrics;
    }

    /**
     * Write file
     *
     * @param path path file to write. Can be relative (to execution directory) or absolute.
     * @param data data string to write.
     * @param textOrBinary open file to write text (true) or binary (false) data.
     * @param closeDelayUs delay after last write operation, to close the file.
     * Zero value means that no planned close is scheduled, so the file is opened,
     * written and closed in the same moment.
     */
    void write(const std::string &path, const std::string &data, bool textOrBinary, unsigned int closeDelayUs);

    /**
    * Read the file content.
    *
    * @param success success of the read operation.
    * @param path path file to read. Can be relative (to execution directory) or absolute.
    * @param textOrBinary open file to read text (true) or binary (false) data.
    *
    * @return Content read. Empty if failed to read.
    */
    std::string read(bool &success, const std::string &path, bool textOrBinary);

    /**
     * Empty file
     *
     * @param path path file to empty. Can be relative (to execution directory) or absolute.
     */
    void empty(const std::string &path);

    /** Clears list
     *
     * @return Boolean about success of operation (something removed, nothing removed: already empty)
     */
    bool clear();

    /**
     * Json string representation for class information (json object)
     *
     * @return Json string representation ('[]' for empty object).
     */
    std::string asJsonString() const;

    /**
     * Builds json document for class information
     *
     * @return Json object
     */
    nlohmann::json getJson() const;
};

}
}

