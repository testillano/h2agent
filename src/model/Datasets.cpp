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

#include <cctype>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

#include <Datasets.hpp>

namespace h2agent
{
namespace model
{

namespace
{

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Strict %XX decoding ('+' is a literal plus): false on a '%' not followed by two hex digits.
bool percentDecode(const std::string &in, std::string &out) {
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '%') {
            out += in[i];
            continue;
        }
        if (i + 2 >= in.size()) return false;  // needs two more characters
        const int hi = hexValue(in[i + 1]), lo = hexValue(in[i + 2]);
        if (hi < 0 || lo < 0) return false;
        out += static_cast<char>((hi << 4) | lo);
        i += 2;
    }
    return true;
}

std::vector<std::string> splitBlanks(const std::string &line) {
    std::vector<std::string> fields;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i == line.size()) break;
        std::size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t') ++j;
        fields.emplace_back(line, i, j - i);
        i = j;
    }
    return fields;
}

nlohmann::json summaryOf(const std::string &id, const Dataset &d) {
    nlohmann::json s;
    s["id"] = id;
    s["columns"] = d.columns;
    s["rows"] = d.rows;
    return s;
}

}

nlohmann::json Dataset::row(std::size_t index) const {
    nlohmann::json result = nlohmann::json::object();
    const std::size_t base = index * columns.size();
    for (std::size_t c = 0; c < columns.size(); ++c) result[columns[c]] = values[base + c];
    return result;
}

bool Datasets::parse(const std::string &text, bool decode, Dataset &out, std::string &error) {
    out = Dataset{};
    std::istringstream stream(text);
    std::string line;
    std::size_t lineNumber = 0;
    bool header = true;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF files
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        std::vector<std::string> fields = splitBlanks(line);
        const std::string where = "line " + std::to_string(lineNumber) + ": ";
        if (header) {
            std::set<std::string> unique;
            for (const auto &name : fields) {
                if (!unique.insert(name).second) {
                    error = where + "repeated column '" + name + "'";
                    return false;
                }
            }
            out.columns = std::move(fields);
            header = false;
            continue;
        }
        if (fields.size() != out.columns.size()) {
            error = where + "expected " + std::to_string(out.columns.size()) + " fields, found " + std::to_string(fields.size());
            return false;
        }
        for (auto &field : fields) {
            if (decode) {
                std::string decoded;
                if (!percentDecode(field, decoded)) {
                    error = where + "invalid percent-encoding in '" + field + "'";
                    return false;
                }
                field = std::move(decoded);
            }
            out.values.push_back(std::move(field));
        }
        ++out.rows;
    }
    if (header) {
        error = "no header line";
        return false;
    }
    return true;
}

std::string Datasets::idFromPath(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base.erase(dot);
    return base;
}

bool Datasets::validId(const std::string &id) {
    if (id.empty()) return false;
    for (char c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.' && c != '-') return false;
    }
    return true;
}

int Datasets::loadFile(const std::string &id, const std::string &path, bool decode, nlohmann::json &summary, std::string &error) {
    const std::string effectiveId = id.empty() ? idFromPath(path) : id;
    if (!validId(effectiveId)) {
        error = "invalid dataset id '" + effectiveId + "' (allowed: letters, digits, '_', '.', '-')";
        return 400;
    }
    std::ifstream ifs(path);
    if (path.empty() || !ifs.is_open()) {
        error = "cannot read file '" + path + "'";
        return 404;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    Dataset dataset;
    if (!parse(buffer.str(), decode, dataset, error)) {
        error = "file '" + path + "', " + error;
        return 400;
    }
    summary = summaryOf(effectiveId, dataset);
    set(effectiveId, std::move(dataset));
    return 200;
}

void Datasets::set(const std::string &id, Dataset &&dataset) {
    auto shared = std::make_shared<const Dataset>(std::move(dataset));
    std::unique_lock<std::shared_mutex> lock(mutex_);
    map_[id] = std::move(shared);
}

nlohmann::json Datasets::summary() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    nlohmann::json result = nlohmann::json::array();
    for (const auto &entry : map_) result.push_back(summaryOf(entry.first, *entry.second));
    return result;
}

std::optional<nlohmann::json> Datasets::summary(const std::string &id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = map_.find(id);
    if (it == map_.end()) return std::nullopt;
    return summaryOf(it->first, *it->second);
}

bool Datasets::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const bool any = !map_.empty();
    map_.clear();
    return any;
}

bool Datasets::remove(const std::string &id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return map_.erase(id) > 0;
}

std::optional<nlohmann::json> Datasets::row(const std::string &id, long long index) const {
    std::shared_ptr<const Dataset> dataset;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(id);
        if (it == map_.end()) return std::nullopt;
        dataset = it->second;
    }
    if (dataset->rows == 0) return std::nullopt;
    const long long n = static_cast<long long>(dataset->rows);
    long long r = index % n;
    if (r < 0) r += n;  // normalised to [0, n)
    return dataset->row(static_cast<std::size_t>(r));
}

}
}
