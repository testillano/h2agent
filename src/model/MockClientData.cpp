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
#include <algorithm>

#include <ert/tracing/Logger.hpp>

#include <MockClientData.hpp>
#include <MockClientEvent.hpp>


namespace h2agent
{
namespace model
{

void MockClientData::loadEvent(const DataKey &dataKey, const std::string &clientProvisionId, const std::string &previousState, const std::string &state, const std::chrono::microseconds &sendingTimestampUs, const std::chrono::microseconds &receptionTimestampUs, int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, const std::string &requestBody, DataPart &responseBodyDataPart, std::uint64_t sendSeq, std::int64_t sequence, unsigned int requestDelayMs, unsigned int timeoutMs, bool historyEnabled) {

    modifyOrInsert(dataKey.getKey(), [&](std::shared_ptr<MockEventsHistory> &entry) {
        if (!entry) entry = std::make_shared<MockClientEventsHistory>(dataKey);
        std::static_pointer_cast<MockClientEventsHistory>(entry)->loadEvent(clientProvisionId, previousState, state, sendingTimestampUs, receptionTimestampUs, responseStatusCode, requestHeaders, responseHeaders, requestBody, responseBodyDataPart, sendSeq, sequence, requestDelayMs, timeoutMs, historyEnabled);
    });
}

bool MockClientData::removeEventBySendSeq(const DataKey &dataKey, std::uint64_t sendSeq) {

    bool exists{};
    auto events = std::static_pointer_cast<MockClientEventsHistory>(get(dataKey.getKey(), exists));
    if (!exists) return false;

    bool deleted = events->removeEventBySendSeq(sendSeq);

    // Cleanup empty map entry:
    if (deleted && events->size() == 0) {
        bool aux{};
        remove(dataKey.getKey(), aux);
    }

    return deleted;
}

std::shared_ptr<MockEvent> MockClientData::getEventBySendSeq(const DataKey &dataKey, std::uint64_t sendSeq) {

    bool exists{};
    auto events = std::static_pointer_cast<MockClientEventsHistory>(get(dataKey.getKey(), exists));
    if (!exists) return nullptr;

    return events->getEventBySendSeq(sendSeq);
}

std::string MockClientData::getSequence(std::uint64_t fromTimestampUs, std::uint64_t toTimestampUs, const std::string &requestMethod, const std::regex *uriRegex) const {

    struct Entry {
        std::uint64_t timestampUs;
        std::string direction;
        std::string method;
        std::string uri;
        std::string clientProvisionId;
    };
    std::vector<Entry> entries;

    forEach([&](const mock_events_key_t&, const std::shared_ptr<MockEventsHistory> &history) {
        const auto &key = history->getKey();
        const std::string &method = key.getMethod();
        const std::string &uri = key.getUri();

        if (!requestMethod.empty() && method != requestMethod) return;
        if (uriRegex && !std::regex_search(uri, *uriRegex)) return;

        read_guard_t guard(history->getMutex());
        for (const auto &ev : history->getEvents()) {
            auto clientEv = std::static_pointer_cast<MockClientEvent>(ev);
            const auto &json = clientEv->getJson();
            std::uint64_t sendTs = json["sendingTimestampUs"].get<std::uint64_t>();
            std::uint64_t recvTs = json.contains("receptionTimestampUs") ? json["receptionTimestampUs"].get<std::uint64_t>() : 0;
            std::string provId = json.contains("clientProvisionId") ? json["clientProvisionId"].get<std::string>() : "";

            // Send entry:
            if ((!fromTimestampUs || sendTs >= fromTimestampUs) && (!toTimestampUs || sendTs <= toTimestampUs)) {
                entries.push_back({sendTs, "send", method, uri, provId});
            }
            // Recv entry:
            if (recvTs && (!fromTimestampUs || recvTs >= fromTimestampUs) && (!toTimestampUs || recvTs <= toTimestampUs)) {
                entries.push_back({recvTs, "recv", method, uri, provId});
            }
        }
    });

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.timestampUs < b.timestampUs;
    });

    nlohmann::json result = nlohmann::json::array();
    for (const auto &e : entries) {
        nlohmann::json obj;
        obj["direction"] = e.direction;
        obj["method"] = e.method;
        obj["uri"] = e.uri;
        obj["timestampUs"] = e.timestampUs;
        if (!e.clientProvisionId.empty()) obj["clientProvisionId"] = e.clientProvisionId;
        result.push_back(std::move(obj));
    }

    return result.dump();
}

}
}

