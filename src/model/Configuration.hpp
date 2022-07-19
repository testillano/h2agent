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

#include <Configuration.hpp>


namespace h2agent
{
namespace model
{

/**
 * This class stores general process static configuration
 */
class Configuration
{
    unsigned int long_term_files_close_delay_us_{};
    unsigned int short_term_files_close_delay_us_{};

public:
    /**
    * Constructor
    */
    Configuration() {
        long_term_files_close_delay_us_ = 1000000; // 1 second
        short_term_files_close_delay_us_ = 0; // instant close
    }

    /**
    * Destructor
    */
    ~Configuration() {;}

    /**
     * Set long-term files category close delay (microseconds)
     *
     * @param usecs close delay in microseconds for long-term files category
     */
    void setLongTermFilesCloseDelayUsecs(unsigned int usecs) {
        long_term_files_close_delay_us_ = usecs;
    }

    /**
     * Set short-term files category close delay (microseconds)
     *
     * @param usecs close delay in microseconds for short-term files category
     */
    void setShortTermFilesCloseDelayUsecs(unsigned int usecs) {
        short_term_files_close_delay_us_ = usecs;
    }

    /**
     * Get long-term files category close delay (microseconds)
     *
     * @return usecs close delay in microseconds for long-term files category
     */
    unsigned int getLongTermFilesCloseDelayUsecs() const {
        return long_term_files_close_delay_us_;
    }

    /**
     * Get short-term files category close delay (microseconds)
     *
     * @return usecs close delay in microseconds for short-term files category
     */
    unsigned int getShortTermFilesCloseDelayUsecs() const {
        return short_term_files_close_delay_us_;
    }

    /**
     * Json string representation for class information (json object)
     *
     * @return Json string representation
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

