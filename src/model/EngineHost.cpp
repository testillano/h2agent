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

#include <sys/wait.h>

#include <chrono>
#include <cstdio>

#include <ert/tracing/Logger.hpp>

#include <AdminData.hpp>
#include <Configuration.hpp>
#include <EngineHost.hpp>
#include <FileManager.hpp>
#include <MockClientData.hpp>
#include <MockServerData.hpp>
#include <SocketManager.hpp>
#include <Vault.hpp>
#include <functions.hpp>
#include <keys.hpp>

namespace h2agent {
namespace model {

namespace {

std::string field(const nlohmann::json &address, const char *key) {
    auto it = address.find(key);
    return (it != address.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

// First json-pointer token of `pointer` (fixed field names, no unescape needed);
// `rest` receives the remaining pointer ("" or "/...").
std::string splitPointer(const std::string &pointer, std::string &rest) {
    rest.clear();
    if (pointer.empty() || pointer[0] != '/') return std::string();
    const std::size_t next = pointer.find('/', 1);
    if (next == std::string::npos) return pointer.substr(1);
    rest = pointer.substr(next);
    return pointer.substr(1, next - 1);
}

// RFC 6901 token unescape (~1 -> '/', ~0 -> '~').
std::string unescapeToken(const std::string &token) {
    std::string out;
    out.reserve(token.size());
    for (std::size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '~' && i + 1 < token.size() && (token[i + 1] == '0' || token[i + 1] == '1')) {
            out += (token[i + 1] == '1') ? '/' : '~';
            ++i;
        } else {
            out += token[i];
        }
    }
    return out;
}

// v1 RequestHeader/ResponseHeader: header_map find() (first value); missing fails.
std::optional<nlohmann::json> headerValue(const nghttp2::asio_http2::header_map &headers, const std::string &rest) {
    if (rest.size() < 2) return std::nullopt;
    auto it = headers.find(unescapeToken(rest.substr(1)));
    if (it == headers.end()) return std::nullopt;
    return nlohmann::json(it->second.value);
}

// v1 RequestHeaders/ResponseHeaders: [{"name","value"}...], never fails (may be empty).
nlohmann::json headerList(const nghttp2::asio_http2::header_map &headers) {
    nlohmann::json list = nlohmann::json::array();
    for (const auto &h : headers) list.push_back({{"name", h.first}, {"value", h.second.value}});
    return list;
}

void putHeaders(nlohmann::json &target, const nghttp2::asio_http2::header_map &headers) {
    nlohmann::json headerMap = nlohmann::json::object();
    for (const auto &h : headers) {
        if (!headerMap.contains(h.first)) headerMap[h.first] = h.second.value;  // v1 find(): first value
    }
    target["headerMap"] = std::move(headerMap);
    target["headers"] = headerList(headers);
}

// v1 TypeConverter::setObject: empty path => whole document; otherwise the
// pointed node, where a missing node or an empty one (null/{}/[]) is "not found".
// extract() with the pointer already compiled (`path` only tells the empty case).
std::optional<nlohmann::json> extractCompiled(const nlohmann::json &doc, const std::string &path,
        const nlohmann::json::json_pointer &ptr) {
    if (path.empty()) return doc;
    try {
        const nlohmann::json &v = doc.at(ptr);
        if (v.empty()) return std::nullopt;
        return v;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> extract(const nlohmann::json &doc, const std::string &path) {
    if (path.empty()) return doc;
    try {
        nlohmann::json v = doc.at(nlohmann::json::json_pointer(path));
        if (v.empty()) return std::nullopt;
        return v;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

}  // namespace

///////////
// STORE //
///////////

std::optional<nlohmann::json> EngineStore::get(Scope scope, const std::string &key, const std::string &pointer) const {
    if (scope == Scope::Local) {
        auto it = variables_.find(key);
        if (it == variables_.end()) return std::nullopt;
        if (pointer.empty()) return nlohmann::json(it->second);
        try {  // var values are strings; a pointer is only meaningful over a json text
            return nlohmann::json::parse(it->second).at(nlohmann::json::json_pointer(pointer));
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }
    nlohmann::json value;
    if (!vault_ || !vault_->tryGet(key, value)) return std::nullopt;
    if (pointer.empty()) return value;
    try {
        return value.at(nlohmann::json::json_pointer(pointer));
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void EngineStore::set(Scope scope, const std::string &key, const nlohmann::json &value, const std::string &pointer) {
    if (scope == Scope::Local) {
        // v1 var: string-only (the engine already serializes, this is a safety net)
        if (pointer.empty()) {
            variables_[key] = jsonToString(value);
        } else {
            nlohmann::json doc = nlohmann::json::object();
            auto it = variables_.find(key);
            if (it != variables_.end()) {
                try {
                    doc = nlohmann::json::parse(it->second);
                } catch (const std::exception &) {
                }
            }
            doc[nlohmann::json::json_pointer(pointer)] = value;
            variables_[key] = doc.dump();
        }
        return;
    }
    if (!vault_) return;
    // Vault::load/loadAtPath (not Map::add): keeps blocking-wait and SSE notifications.
    if (pointer.empty())
        vault_->load(key, value);
    else
        vault_->loadAtPath(key, pointer, value);
}

void EngineStore::setString(Scope scope, const std::string &key, std::string &&value) {
    if (scope == Scope::Local) {
        variables_[key] = std::move(value);  // v1 var: plain string, no json round trip
        return;
    }
    if (vault_) vault_->load(key, nlohmann::json(std::move(value)));
}

void EngineStore::erase(Scope scope, const std::string &key) {
    if (scope == Scope::Local) {
        variables_.erase(key);
        return;
    }
    if (!vault_) return;
    bool exists = false;
    vault_->remove(key, exists);
}

//////////////////
// EVENT SOURCE //
//////////////////

std::optional<nlohmann::json> EngineEventSource::resolve(const std::string &domain, const nlohmann::json &address) const {
    std::shared_ptr<MockEvent> event;
    try {
        if (domain == "serverEvent") {
            if (!server_) return std::nullopt;
            const std::string method = field(address, "requestMethod"), uri = field(address, "requestUri"),
                              number = field(address, "eventNumber"), recvseq = field(address, "recvseq"),
                              prefix = field(address, "requestUriStartsWith");
            if (!prefix.empty())
                event = server_->getEventByUriStartsWith(method, prefix, number);
            else if (!recvseq.empty())
                event = server_->getEventByRecvSeq(DataKey(method, uri), std::stoull(recvseq));
            else
                event = server_->getEvent(EventKey(method, uri, number));
        } else if (domain == "clientEvent") {
            if (!client_) return std::nullopt;
            const std::string endpoint = field(address, "clientEndpointId"), method = field(address, "requestMethod"),
                              uri = field(address, "requestUri"), number = field(address, "eventNumber"),
                              sendseq = field(address, "sendseq"), prefix = field(address, "requestUriStartsWith");
            DataKey dkey(endpoint, method, uri);
            if (!prefix.empty())
                event = client_->getEventByUriStartsWith(endpoint, method, prefix, number);
            else if (!sendseq.empty())
                event = client_->getEventBySendSeq(dkey, std::stoull(sendseq));
            else
                event = client_->getEvent(EventKey(dkey, number));
        }
    } catch (const std::exception &) {
        return std::nullopt;  // e.g. non numeric sequence (v1 returns false)
    }
    if (!event) return std::nullopt;
    auto v = extract(event->getJson(), field(address, "eventPath"));
    if (!v) {
        ert::tracing::Logger::warning(ert::tracing::Logger::asString("Cannot extract path '%s' from %s for address '%s'",
                                      field(address, "eventPath").c_str(), domain.c_str(),
                                      address.dump().c_str()),
                                      ERT_FILE_LOCATION);
    }
    return v;
}

void EngineEventSource::purge(const std::string &domain, const nlohmann::json &address) {
    bool deleted = false;
    try {
        if (domain == "serverEvent" && server_) {
            const std::string method = field(address, "requestMethod"), uri = field(address, "requestUri"),
                              number = field(address, "eventNumber"), recvseq = field(address, "recvseq");
            if (!recvseq.empty())
                deleted = server_->removeEventByRecvSeq(DataKey(method, uri), std::stoull(recvseq));
            else
                server_->clear(deleted, EventKey(method, uri, number));
        } else if (domain == "clientEvent" && client_) {
            const std::string endpoint = field(address, "clientEndpointId"), method = field(address, "requestMethod"),
                              uri = field(address, "requestUri"), number = field(address, "eventNumber"),
                              sendseq = field(address, "sendseq");
            DataKey dkey(endpoint, method, uri);
            if (!sendseq.empty())
                deleted = client_->removeEventBySendSeq(dkey, std::stoull(sendseq));
            else
                client_->clear(deleted, EventKey(dkey, number));
        }
    } catch (const std::exception &) {
        // non numeric sequence: nothing removed
    }
    LOGDEBUG(ert::tracing::Logger::debug(
                 ert::tracing::Logger::asString("%s purge %s: %s", domain.c_str(), address.dump().c_str(),
                         deleted ? "SUCCESS" : "NOTHING REMOVED"),
                 ERT_FILE_LOCATION));
}

/////////////////////
// FILES / SOCKETS //
/////////////////////

std::optional<std::string> EngineFileAccess::read(const std::string &path, bool binary) const {
    std::string content;
    if (files_) files_->read(path, content, !binary /* textOrBinary: true = text */);
    return content;  // v1 parity: a missing file yields "" (the rule still applies)
}

bool EngineFileAccess::write(const std::string &path, const std::string &data, bool binary, bool templatedPath) {
    if (!files_) return false;
    unsigned int closeDelayUs = 0;
    if (configuration_)
        closeDelayUs = templatedPath ? configuration_->getShortTermFilesCloseDelayUsecs()
                       : configuration_->getLongTermFilesCloseDelayUsecs();
    files_->write(path, data, !binary, closeDelayUs);
    return true;
}

void EngineFileAccess::truncate(const std::string &path) {
    if (files_) files_->empty(path);
}

bool EngineSocketWriter::write(const std::string &path, const std::string &datagram, std::uint32_t delayMs) {
    if (!sockets_) return false;
    sockets_->write(path, datagram, delayMs * 1000 /* usecs */);
    return true;
}

///////////////////////////
// CLOCK / SHELL / SCHEMA //
///////////////////////////

std::int64_t EngineClock::nowEpoch(const std::string &unit) const {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    if (unit == "ms") return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    if (unit == "us") return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    if (unit == "ns") return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

h2agent::engine::Shell::Result EngineShell::run(const std::string &command) {
    Result result;
    result.returnCode = -1;  // v1: rare case where popen fails
    FILE *fp = popen(command.c_str(), "r");
    if (fp) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp)) result.output += buffer;
        result.returnCode = WEXITSTATUS(pclose(fp));
    }
    return result;
}

std::optional<bool> EngineSchemaValidator::validate(const std::string &schemaId, const nlohmann::json &doc,
        std::string &failReport) const {
    auto schema = admin_data_ ? admin_data_->getSchemaData().find(schemaId) : nullptr;
    if (!schema) {
        ert::tracing::Logger::warning(
            ert::tracing::Logger::asString(
                "Missing schema '%s' referenced in transformation item: VALIDATION will be IGNORED", schemaId.c_str()),
            ERT_FILE_LOCATION);
        return std::nullopt;
    }
    return schema->validate(doc, failReport);
}

/////////////////
// INPUT MODEL //
/////////////////

std::optional<nlohmann::json> V1BodyDocument::get(const std::string &pointer) const {
    return extract(body_, pointer);
}

std::optional<nlohmann::json> V1BodyDocument::get(const h2agent::engine::CompiledPointer &pointer) const {
    return extractCompiled(body_, pointer.text, pointer.full);
}

std::optional<nlohmann::json> ServerInputDocument::get(const h2agent::engine::CompiledPointer &pointer) const {
    // Same resolution as the text form, without parsing the pointer per event.
    const std::string &head = pointer.head;
    if (head == "body") {
        if (body_.isJson()) return extractCompiled(body_.getJson(), pointer.restText, pointer.rest);
        return nlohmann::json(body_.str());  // v1: non-json body is the whole string
    }
    if (pointer.restTokens == 1 && (head == "headerMap" || head == "uriParams")) {
        const std::string &name = pointer.rest.back();  // unescaped token
        if (head == "headerMap") {
            auto it = headers_.find(name);
            if (it == headers_.end()) return std::nullopt;
            return nlohmann::json(it->second.value);
        }
        auto it = query_parameters_.find(name);
        if (it == query_parameters_.end()) return std::nullopt;
        return nlohmann::json(it->second);
    }
    return get(pointer.text);  // remaining fields are cheap or unusual
}

std::optional<nlohmann::json> ServerInputDocument::get(const std::string &pointer) const {
    std::string rest;
    const std::string head = splitPointer(pointer, rest);
    if (head.empty()) {  // whole document (diagnostics only: the v1 grammar never reads it)
        nlohmann::json in = nlohmann::json::object();
        in["uri"] = uri_;
        in["uriPath"] = uri_path_;
        in["uriParams"] = nlohmann::json(query_parameters_);
        if (auto b = get("/body")) in["body"] = std::move(*b);
        putHeaders(in, headers_);
        return in;
    }
    if (head == "uri" && rest.empty()) return nlohmann::json(uri_);
    if (head == "uriPath" && rest.empty()) return nlohmann::json(uri_path_);
    if (head == "uriParams") {
        if (rest.empty()) return nlohmann::json(query_parameters_);
        auto it = query_parameters_.find(unescapeToken(rest.substr(1)));
        if (it == query_parameters_.end()) return std::nullopt;  // v1: missing param fails
        return nlohmann::json(it->second);
    }
    if (head == "body") {
        if (body_.isJson()) return extract(body_.getJson(), rest);
        return nlohmann::json(body_.str());  // v1: non-json body is the whole string
    }
    if (head == "headerMap") return headerValue(headers_, rest);
    if (head == "headers" && rest.empty()) return headerList(headers_);
    return std::nullopt;
}

const nghttp2::asio_http2::header_map &ClientInputDocument::requestHeaders(nghttp2::asio_http2::header_map &merged) const {
    if (!pending_header_writes_ || pending_header_writes_->empty()) return headers_;  // common case: no copy
    merged = headers_;
    for (const auto &w : *pending_header_writes_) merged.emplace(w.first, nghttp2::asio_http2::header_value{w.second});
    return merged;
}

std::optional<nlohmann::json> ClientInputDocument::get(const std::string &pointer) const {
    std::string rest;
    const std::string head = splitPointer(pointer, rest);
    nghttp2::asio_http2::header_map merged;
    if (head.empty()) {  // whole document (diagnostics only)
        nlohmann::json in = nlohmann::json::object();
        in["uri"] = uri_;
        in["requestBodyString"] = request_body_string_;
        putHeaders(in, requestHeaders(merged));
        if (auto r = get("/response")) in["response"] = std::move(*r);
        return in;
    }
    if (head == "uri" && rest.empty()) return nlohmann::json(uri_);
    if (head == "requestBodyString" && rest.empty()) return nlohmann::json(request_body_string_);
    if (head == "headerMap") return headerValue(requestHeaders(merged), rest);
    if (head == "headers" && rest.empty()) return headerList(requestHeaders(merged));
    if (head != "response" || !response_) return std::nullopt;

    // Response phase:
    std::string leaf;
    const std::string sub = splitPointer(rest, leaf);
    if (sub.empty()) {
        nlohmann::json r = nlohmann::json::object();
        if (auto b = get("/response/body")) r["body"] = std::move(*b);
        putHeaders(r, response_->headers);
        r["statusCode"] = response_->statusCode;
        return r;
    }
    if (sub == "body") {
        if (!parsed_) {
            response_is_json_ = h2agent::model::parseJsonContent(response_->body, response_json_);
            parsed_ = true;
        }
        if (response_is_json_) return extract(response_json_, leaf);
        return nlohmann::json(response_->body);  // v1: non-json body is the whole string
    }
    if (sub == "headerMap") return headerValue(response_->headers, leaf);
    if (sub == "headers" && leaf.empty()) return headerList(response_->headers);
    if (sub == "statusCode" && leaf.empty()) return nlohmann::json(response_->statusCode);  // integer, as v1
    return std::nullopt;
}

}  // namespace model
}  // namespace h2agent
