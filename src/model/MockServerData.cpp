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

#include <MockServerData.hpp>


namespace h2agent
{
namespace model
{

void MockServerData::loadEvent(const DataKey &dataKey, const std::string &previousState, const std::string &state, const std::chrono::microseconds &receptionTimestampUs, unsigned int responseStatusCode, const nghttp2::asio_http2::header_map &requestHeaders, const nghttp2::asio_http2::header_map &responseHeaders, DataPart &requestBodyDataPart, const std::string &responseBody, std::uint64_t serverSequence, unsigned int responseDelayMs, bool historyEnabled, const std::string &virtualOriginComingFromMethod, const std::string &virtualOriginComingFromUri) {

    bool maiden{};
    auto events = std::static_pointer_cast<MockServerEventsHistory>(getEvents(dataKey, maiden));

    events->loadEvent(previousState, state, receptionTimestampUs, responseStatusCode, requestHeaders, responseHeaders, requestBodyDataPart, responseBody, serverSequence, responseDelayMs, historyEnabled, virtualOriginComingFromMethod, virtualOriginComingFromUri);

    if (maiden) add(dataKey.getKey(), events); // push the key in the map:

    last_loaded_event_ = std::static_pointer_cast<MockServerEvent>(events->getEvent(1, true /* reverse: last */));
}

bool MockServerData::removeEventByRecvSeq(const DataKey &dataKey, std::uint64_t recvSeq) {

    bool exists{};
    auto events = std::static_pointer_cast<MockServerEventsHistory>(get(dataKey.getKey(), exists));
    if (!exists) return false;

    bool deleted = events->removeEventByRecvSeq(recvSeq);

    // Cleanup empty map entry:
    if (deleted && events->size() == 0) {
        bool aux{};
        remove(dataKey.getKey(), aux);
    }

    return deleted;
}

std::shared_ptr<MockEvent> MockServerData::getEventByRecvSeq(const DataKey &dataKey, std::uint64_t recvSeq) {

    bool exists{};
    auto events = std::static_pointer_cast<MockServerEventsHistory>(get(dataKey.getKey(), exists));
    if (!exists) return nullptr;

    return events->getEventByRecvSeq(recvSeq);
}

std::string MockServerData::getSequence(std::uint64_t fromTimestampUs, std::uint64_t toTimestampUs, const std::string &requestMethod, const std::regex *uriRegex) const {

    struct Entry {
        std::uint64_t timestampUs;
        std::string direction;
        std::string method;
        std::string uri;
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
            auto serverEv = std::static_pointer_cast<MockServerEvent>(ev);
            std::uint64_t recTs = serverEv->getJson()["receptionTimestampUs"].get<std::uint64_t>();
            std::uint64_t sendTs = serverEv->getSendingTimestampUs();

            // Recv entry:
            if ((!fromTimestampUs || recTs >= fromTimestampUs) && (!toTimestampUs || recTs <= toTimestampUs)) {
                entries.push_back({recTs, "recv", method, uri});
            }
            // Send entry:
            if (sendTs && (!fromTimestampUs || sendTs >= fromTimestampUs) && (!toTimestampUs || sendTs <= toTimestampUs)) {
                entries.push_back({sendTs, "send", method, uri});
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
        result.push_back(std::move(obj));
    }

    return result.dump();
}

}
}

