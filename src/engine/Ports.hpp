/*
 ______________________________________________________________________
|                                                                      |
|   jsonFlow - application-agnostic JSON transformation engine (C++)   |
https://github.com/testillano/jsonFlow |   source -> filter -> target dataflow
language                       | Version 0.0.z
|______________________________________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Eduardo Ramos Testillano

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

namespace ert {
namespace jsonflow {

/**
 * Ports: interfaces the host (e.g. h2agent) implements so the engine core stays
 * pure, transport-agnostic and testable. For a dry-run, a host provides
 * in-memory / no-op implementations of these.
 *
 * NOTE (skeleton): signatures are provisional and will stabilize during the
 * parity-first extraction (proposal phase [0.5], section 8.5).
 */

/**
 * A source json pointer compiled ONCE at parse time (provision load), so reads
 * never parse pointer text per event. `head` is the first token (the host field,
 * e.g. "body"), `rest` the pointer below it (e.g. "/age"); `full` the whole one.
 */
struct CompiledPointer {
    std::string text{};                     // e.g. "/body/age" ("" = whole document)
    nlohmann::json::json_pointer full{};    // json_pointer(text)
    std::string head{};                     // "body" (raw token; "" for the whole document)
    std::string restText{};                 // "/age" ("" = the head field itself)
    nlohmann::json::json_pointer rest{};    // json_pointer(restText)
    std::size_t restTokens{0};              // number of tokens in `rest`

    /** False when `text` is not a valid json pointer (caller keeps the per-event path). */
    static bool compile(const std::string &text, CompiledPointer &out) {
        if (!text.empty() && text[0] != '/') return false;
        try {
            out.text = text;
            out.full = nlohmann::json::json_pointer(text);
            const std::size_t next = text.empty() ? std::string::npos : text.find('/', 1);
            out.head = text.empty() ? std::string() : text.substr(1, next == std::string::npos ? std::string::npos : next - 1);
            out.restText = (next == std::string::npos) ? std::string() : text.substr(next);
            out.rest = nlohmann::json::json_pointer(out.restText);
            out.restTokens = 0;
            for (char c : out.restText) out.restTokens += (c == '/');
        } catch (const std::exception &) {
            return false;
        }
        return true;
    }
};

/**
 * Read-only document resolved lazily by json pointer. The host implements it
 * over its native structures (no prebuilt json copy per event): only what a rule
 * actually reads is materialized. Whether an existing-but-empty node counts as
 * absent is a HOST decision (grammar semantics live in the host, not here).
 *
 * LIFETIME: an implementation may reference host data that is only valid during
 * the Engine::evaluate() call it is handed to.
 */
class Document {
public:
    virtual ~Document() = default;
    /** Value at `pointer` ("" = whole document); nullopt when absent. */
    virtual std::optional<nlohmann::json> get(const std::string &pointer) const = 0;
    /** Same, with the pointer compiled at parse (hosts override it to skip parsing). */
    virtual std::optional<nlohmann::json> get(const CompiledPointer &pointer) const {
        return get(pointer.text);
    }
};

/** Document over an existing json (referenced, not copied): absent = missing node. */
class JsonDocument : public Document {
    const nlohmann::json &doc_;

public:
    explicit JsonDocument(const nlohmann::json &doc) : doc_(doc) {}
    std::optional<nlohmann::json> get(const CompiledPointer &pointer) const override {
        if (pointer.text.empty()) return doc_;
        try {
            return doc_.at(pointer.full);
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }
    std::optional<nlohmann::json> get(const std::string &pointer) const override {
        if (pointer.empty()) return doc_;
        try {
            return doc_.at(nlohmann::json::json_pointer(pointer));
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }
};

/**
 * Key-value store. Unifies the former var (local, string-only) and vault
 * (global, typed) into a single typed store with an explicit scope. This is the
 * interface whose extraction forces the var/vault unification of proposal
 * phase [1].
 *
 * THREAD-SAFETY CONTRACT: implementations MUST be safe for concurrent calls
 * from multiple threads (the engine adds no locking of its own). A set()
 * targeting a json-pointer sub-path MUST be atomic per key (read-modify-write
 * under a single lock), matching h2agent's sharded, shared_mutex-backed
 * Map/Vault. Recommended implementation: per-key (sharded) locking to avoid
 * serializing independent keys on the hot path.
 */
class Store {
public:
    enum class Scope { Local, Global };

    virtual ~Store() = default;

    /** Read a value, optionally navigating a json-pointer path. */
    virtual std::optional<nlohmann::json> get(Scope scope, const std::string &key,
            const std::string &pointer = "") const = 0;

    /** Write a value, optionally at a json-pointer sub-path. */
    virtual void set(Scope scope, const std::string &key, const nlohmann::json &value,
                     const std::string &pointer = "") = 0;

    /** Remove a value (eraser semantics). */
    /** Whole-value string write (the common var/vault case): lets the host store it
     * without json round trips. Default: plain set(). */
    virtual void setString(Scope scope, const std::string &key, std::string &&value) {
        set(scope, key, nlohmann::json(std::move(value)), "");
    }
    virtual void erase(Scope scope, const std::string &key) = 0;
};

/** Access to stored server/client event history for event-source addressing. */
class EventSource {
public:
    virtual ~EventSource() = default;

    /**
     * Resolve an event (and optional sub-path).
     *
     * @param domain  "serverEvent" or "clientEvent".
     * @param address Already tokenized, URL-decoded and @{var}-substituted address
     *                fields as a flat JSON object of strings (e.g. requestMethod,
     *                requestUri, eventNumber, eventPath, recvseq, sendseq,
     *                requestUriStartsWith, clientEndpointId). Field semantics are
     *                interpreted by the host adapter.
     */
    virtual std::optional<nlohmann::json> resolve(const std::string &domain, const nlohmann::json &address) const = 0;

    /** Purge events matching the address (eraser-as-purge). Same args as resolve. */
    virtual void purge(const std::string &domain, const nlohmann::json &address) = 0;
};

/** File read/write access (txtFile/binFile sources and targets). */
class FileAccess {
public:
    virtual ~FileAccess() = default;
    virtual std::optional<std::string> read(const std::string &path, bool binary) const = 0;
    /**
     * @param templatedPath true when the configured path contained @{var}
     *        (dynamic name). h2agent uses it to choose short-term (templated) vs
     *        long-term (fixed path) close delays.
     */
    virtual bool write(const std::string &path, const std::string &data, bool binary, bool templatedPath) = 0;
    /** Truncate/empty the file (eraser source on a file target). */
    virtual void truncate(const std::string &path) = 0;
};

/** UDP-through-unix-socket writer (udpSocket target). */
class SocketWriter {
public:
    virtual ~SocketWriter() = default;
    virtual bool write(const std::string &path, const std::string &datagram, std::uint32_t delayMs = 0) = 0;
};

/** Clock abstraction (timestamp source) - injectable for deterministic tests.
 */
class Clock {
public:
    virtual ~Clock() = default;
    /** Current UNIX epoch in the requested unit (s, ms, us, ns). */
    virtual std::int64_t nowEpoch(const std::string &unit) const = 0;
};

/** Shell command execution (command source). Dangerous; host may disable it. */
class Shell {
public:
    virtual ~Shell() = default;
    struct Result {
        std::string output{};
        int returnCode{0};
    };
    virtual Result run(const std::string &command) = 0;
};

/** Fire-and-forget flow trigger (clientProvision target). */
class Trigger {
public:
    virtual ~Trigger() = default;
    virtual void trigger(const std::string &provisionId, const std::string &inState) = 0;
};

/**
 * JSON schema validation (SchemaId filter). The engine does NOT store or compile
 * schemas: the schema registry is a host resource (h2agent keeps a thread-safe
 * Map of id->schema). The engine only asks the host to validate a fragment
 * against a registered schema id. Returns false if invalid OR the id is unknown.
 */
class SchemaValidator {
public:
    virtual ~SchemaValidator() = default;
    /**
     * Validate 'doc' against the registered schema 'schemaId'.
     * @return std::nullopt when the schema id is unknown (h2agent then IGNORES
     *         the validation and keeps the source value); true/false otherwise,
     *         filling failReport on false.
     */
    virtual std::optional<bool> validate(const std::string &schemaId, const nlohmann::json &doc,
                                         std::string &failReport) const = 0;
};

/**
 * Aggregate of all ports handed to the engine. Any member may be null when the
 * corresponding capability is not needed (the engine skips rules that require a
 * missing port and records the skip in the execution trace).
 */
struct Ports {
    Store *store{nullptr};
    EventSource *events{nullptr};
    FileAccess *files{nullptr};
    SocketWriter *sockets{nullptr};
    Clock *clock{nullptr};
    Shell *shell{nullptr};
    Trigger *trigger{nullptr};
    SchemaValidator *schema{nullptr};
};

}  // namespace jsonflow
}  // namespace ert
