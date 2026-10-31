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

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nghttp2/asio_http2_server.h>
#include <nlohmann/json.hpp>

#include <ert/http2comm/Http2Client.hpp>

#include <DataPart.hpp>
#include <Ports.hpp>
#include <Variables.hpp>

namespace h2agent {
namespace model {

class Vault;
class MockServerData;
class MockClientData;
class FileManager;
class SocketManager;
class Configuration;
class AdminData;

/**
 * Engine ports implemented over h2agent resources (proposal 8.3, host side).
 * Every class reproduces the exact v1 behavior of the former
 * AdminServerProvision/AdminClientProvision process* functions.
 */

/** Local scope = per-chain variables (native json, see Variables); global = Vault. */
class EngineStore : public h2agent::engine::Store {
    Variables &variables_;
    Vault *vault_;

public:
    EngineStore(Variables &variables, Vault *vault) : variables_(variables), vault_(vault) {}
    std::optional<nlohmann::json> get(Scope scope, const std::string &key, const std::string &pointer) const override;
    void set(Scope scope, const std::string &key, const nlohmann::json &value, const std::string &pointer) override;
    void setString(Scope scope, const std::string &key, std::string &&value) override;
    void erase(Scope scope, const std::string &key) override;
};

/** serverEvent / clientEvent addressing over MockServerData / MockClientData. */
class EngineEventSource : public h2agent::engine::EventSource {
    MockServerData *server_;
    MockClientData *client_;

public:
    EngineEventSource(MockServerData *server, MockClientData *client) : server_(server), client_(client) {}
    std::optional<nlohmann::json> resolve(const std::string &domain, const nlohmann::json &address) const override;
    void purge(const std::string &domain, const nlohmann::json &address) override;
};

/** txtFile/binFile over FileManager (long/short-term close delays from Configuration). */
class EngineFileAccess : public h2agent::engine::FileAccess {
    FileManager *files_;
    Configuration *configuration_;

public:
    EngineFileAccess(FileManager *files, Configuration *configuration) : files_(files), configuration_(configuration) {}
    std::optional<std::string> read(const std::string &path, bool binary) const override;
    bool write(const std::string &path, const std::string &data, bool binary, bool templatedPath) override;
    void truncate(const std::string &path) override;
};

/** udpSocket over SocketManager. */
class EngineSocketWriter : public h2agent::engine::SocketWriter {
    SocketManager *sockets_;

public:
    explicit EngineSocketWriter(SocketManager *sockets) : sockets_(sockets) {}
    bool write(const std::string &path, const std::string &datagram, std::uint32_t delayMs) override;
};

/** Wall clock. */
class EngineClock : public h2agent::engine::Clock {
public:
    std::int64_t nowEpoch(const std::string &unit) const override;
};

/** command source via popen (v1 parity). */
class EngineShell : public h2agent::engine::Shell {
public:
    Result run(const std::string &command) override;
};

/** SchemaId filter over the AdminData schema registry. */
class EngineSchemaValidator : public h2agent::engine::SchemaValidator {
    AdminData *admin_data_;

public:
    explicit EngineSchemaValidator(AdminData *adminData) : admin_data_(adminData) {}
    std::optional<bool> validate(const std::string &schemaId, const nlohmann::json &doc,
                                 std::string &failReport) const override;
};

/** clientProvision target: collects triggers executed by the caller after responding. */
class EngineTrigger : public h2agent::engine::Trigger {
    std::vector<std::pair<std::string, std::string>> &triggers_;

public:
    explicit EngineTrigger(std::vector<std::pair<std::string, std::string>> &triggers) : triggers_(triggers) {}
    void trigger(const std::string &provisionId, const std::string &inState) override {
        triggers_.emplace_back(provisionId, inState);
    }
};

/**
 * v1 body semantics (TypeConverter::setObject) over a referenced json: empty
 * pointer = whole document; otherwise the pointed node, where a missing node or
 * an empty one (null/{}/[]) is absent. Used for request/response body reads.
 */
class V1BodyDocument : public h2agent::engine::Document {
    const nlohmann::json &body_;

public:
    explicit V1BodyDocument(const nlohmann::json &body) : body_(body) {}
    using h2agent::engine::Document::get;
    std::optional<nlohmann::json> get(const std::string &pointer) const override;
    std::optional<nlohmann::json> get(const h2agent::engine::CompiledPointer &pointer) const override;
};

/**
 * Lazy neutral input of a server reception (see EngineGrammarV2.hpp), resolved
 * over the native request structures (nothing is copied up front):
 *   /uri, /uriPath, /uriParams/<p>, /body[/<path>], /headerMap/<h>, /headers.
 * Body: decoded json with v1 body semantics, or the raw string when not json
 * (v1 returns the whole string whatever the path). /headers is always an array
 * (possibly empty: v1 RequestHeaders never fails).
 */
class ServerInputDocument : public h2agent::engine::Document {
    const std::string &uri_;
    const std::string &uri_path_;
    const std::map<std::string, std::string> &query_parameters_;
    const DataPart &body_;
    const nghttp2::asio_http2::header_map &headers_;

public:
    ServerInputDocument(const std::string &uri, const std::string &uriPath,
                        const std::map<std::string, std::string> &queryParameters, const DataPart &body,
                        const nghttp2::asio_http2::header_map &headers)
        : uri_(uri), uri_path_(uriPath), query_parameters_(queryParameters), body_(body), headers_(headers) {}
    using h2agent::engine::Document::get;
    std::optional<nlohmann::json> get(const std::string &pointer) const override;
    std::optional<nlohmann::json> get(const h2agent::engine::CompiledPointer &pointer) const override;
};

/**
 * Lazy neutral input of a client provision (see EngineGrammarV2.hpp):
 *   /uri, /requestBodyString, /headerMap/<h>, /headers and, in the response
 *   phase (response != nullptr), /response/{body[/<path>],headerMap/<h>,headers,statusCode}.
 * The response body is parsed once on first access (json with v1 body
 * semantics, or the raw string when not json, as v1 does).
 */
class ClientInputDocument : public h2agent::engine::Document {
    const std::string &uri_;
    const std::string &request_body_string_;
    const nghttp2::asio_http2::header_map &headers_;
    const ert::http2comm::Http2Client::response *response_;
    // Request header writes done so far by the pipeline (request phase). v1 emplaced them
    // into the live header map, so later request.header(s) reads in the same pipeline saw them.
    const std::vector<std::pair<std::string, std::string>> *pending_header_writes_{nullptr};
    const nghttp2::asio_http2::header_map &requestHeaders(nghttp2::asio_http2::header_map &merged) const;
    mutable bool parsed_{false};
    mutable bool response_is_json_{false};
    mutable nlohmann::json response_json_{};

public:
    ClientInputDocument(const std::string &uri, const std::string &requestBodyString,
                        const nghttp2::asio_http2::header_map &headers,
                        const ert::http2comm::Http2Client::response *response = nullptr)
        : uri_(uri), request_body_string_(requestBodyString), headers_(headers), response_(response) {}
    using h2agent::engine::Document::get;  // compiled pointers fall back to the text form
    std::optional<nlohmann::json> get(const std::string &pointer) const override;
    /** Request phase: make header writes of the running pipeline visible to header reads (v1). */
    void setPendingHeaderWrites(const std::vector<std::pair<std::string, std::string>> *writes) {
        pending_header_writes_ = writes;
    }
};

}  // namespace model
}  // namespace h2agent
