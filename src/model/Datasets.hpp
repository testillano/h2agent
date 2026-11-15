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

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

/**
 * A table of text values loaded from a file: a header (column names) and rows,
 * every row with one value per column. Kept as a flat row-major text vector
 * (no json per value): a row object is only built when a source reads it.
 */
struct Dataset
{
    std::vector<std::string> columns{};
    std::vector<std::string> values{}; // rows * columns.size(), row-major
    std::size_t rows{};

    /** Row `index` (already in range) as {"<column>": "<value>", ...}. */
    nlohmann::json row(std::size_t index) const;
};

/**
 * Datasets store: id -> dataset, separate from the vault (not listed, cleared
 * or read through it). Thread safe: a reader keeps the dataset it got alive
 * while a load replaces it.
 *
 * File format (see docs/api/README.md#datasets):
 * - fields separated by one or more blanks (spaces or tabs), leading and trailing
 *   blanks ignored;
 * - '#' lines (first non blank character) and empty lines ignored;
 * - the first remaining line is the header: unique column names;
 * - every row has as many fields as the header;
 * - values are raw text; with decode, every value is percent-decoded (%XX) at
 *   load time, so a value may hold a blank (%20) or a '%' (%25).
 */
class Datasets
{
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::shared_ptr<const Dataset>> map_;

public:
    Datasets() = default;

    /**
     * Parses a dataset text.
     * @param error filled on failure, with the 1-based line when it applies
     * @return false on a format error
     */
    static bool parse(const std::string &text, bool decode, Dataset &out, std::string &error);

    /** Default id of a file: its basename without the last extension. */
    static std::string idFromPath(const std::string &path);

    /** Valid id: [A-Za-z0-9_.-]+ */
    static bool validId(const std::string &id);

    /**
     * Loads (replaces) dataset `id` from a file.
     * @param id empty for the default (idFromPath)
     * @param summary on success, {"id", "columns", "rows"}
     * @param error on failure
     * @return 200 (loaded), 400 (bad id or format) or 404 (file not readable)
     */
    int loadFile(const std::string &id, const std::string &path, bool decode, nlohmann::json &summary, std::string &error);

    /** Adds (replaces) a parsed dataset. */
    void set(const std::string &id, Dataset &&dataset);

    /** Summary of every dataset: [{"id", "columns", "rows"}], by id. */
    nlohmann::json summary() const;

    /** Summary of one dataset; nullopt when unknown. */
    std::optional<nlohmann::json> summary(const std::string &id) const;

    /** @return true when something was removed */
    bool clear();
    bool remove(const std::string &id);

    /**
     * Row of dataset `id` at `index` modulo its rows (a negative index counts
     * from the end: -1 is the last row), as an object; nullopt when the dataset
     * is unknown or empty.
     */
    std::optional<nlohmann::json> row(const std::string &id, long long index) const;
};

}
}
