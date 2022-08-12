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

void FileManager::write(const std::string &path, const std::string &data, bool textOrBinary, unsigned int closeDelayUs) {

    std::shared_ptr<SafeFile> safeFile;

    auto it = get(path);
    if (it != end()) {
        safeFile = it->second;
    }
    else {
        std::ios_base::openmode mode = std::ofstream::out | std::ios_base::app; // for text files
        if (!textOrBinary) mode |= std::ios::binary;

        safeFile = std::make_shared<SafeFile>(path, io_service_, metrics_, closeDelayUs, mode);
        add(path, safeFile);
    }

    safeFile->write(data);
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

        safeFile = std::make_shared<SafeFile>(path, io_service_, metrics_, 0, mode);
        add(path, safeFile);
    }

    data = safeFile->read(result, mode);

    return result;
}

void FileManager::empty(const std::string &path) {

    std::shared_ptr<SafeFile> safeFile;

    auto it = get(path);
    if (it != end()) {
        safeFile = it->second;
    }
    else {
        safeFile = std::make_shared<SafeFile>(path, io_service_, metrics_);
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

std::string FileManager::asJsonString() const {

    return ((size() != 0) ? getJson().dump() : "[]"); // server data is shown as an array
}

nlohmann::json FileManager::getJson() const {

    nlohmann::json result;

    read_guard_t guard(rw_mutex_);

    for (auto it = begin(); it != end(); it++) {
        result.push_back(it->second->getJson());
    };

    return result;
}

}
}

