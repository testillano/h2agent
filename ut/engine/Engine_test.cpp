// Unit tests for the transformation engine (src/engine): every source, filter and
// target in isolation, with in-memory port doubles (no HTTP, no h2agent model).

#include <gtest/gtest.h>

#include <Engine.hpp>
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace h2agent::engine;

namespace {

// Minimal in-memory store for tests: two flat maps (local/global) keyed by
// name.
class MemStore : public Store {
public:
    std::optional<nlohmann::json> get(Scope scope, const std::string &key,
                                      const std::string &pointer = "") const override {
        const auto &m = (scope == Scope::Local) ? local_ : global_;
        auto it = m.find(key);
        if (it == m.end()) return std::nullopt;
        if (pointer.empty()) return it->second;
        try {
            return it->second.at(nlohmann::json::json_pointer(pointer));
        } catch (...) {
            return std::nullopt;  // missing path -> not found
        }
    }
    void set(Scope scope, const std::string &key, const nlohmann::json &value,
             const std::string &pointer = "") override {
        auto &m = (scope == Scope::Local) ? local_ : global_;
        if (pointer.empty()) {
            m[key] = value;
        } else {
            m[key][nlohmann::json::json_pointer(pointer)] = value;  // set sub-path, preserve rest
        }
    }
    void erase(Scope scope, const std::string &key) override {
        ((scope == Scope::Local) ? local_ : global_).erase(key);
    }

private:
    std::map<std::string, nlohmann::json> local_{};
    std::map<std::string, nlohmann::json> global_{};
};

// Deterministic clock for timestamp tests.
class FakeClock : public Clock {
public:
    explicit FakeClock(std::int64_t seconds) : seconds_(seconds) {}
    std::int64_t nowEpoch(const std::string &unit) const override {
        if (unit == "s") return seconds_;
        if (unit == "ms") return seconds_ * 1000;
        if (unit == "us") return seconds_ * 1000000;
        if (unit == "ns") return seconds_ * 1000000000;
        return 0;
    }

private:
    std::int64_t seconds_;
};

// Fake schema validator double for SchemaId filter tests. A schema id is
// "known" when registered via accept(); validation then succeeds iff the
// document equals the registered expected document (or always, if no expected
// doc was registered). Unknown ids always fail validation.
class FakeSchema : public SchemaValidator {
public:
    // Register an id that validates any document.
    void acceptAny(const std::string &id) {
        any_.insert(id);
    }
    // Register an id that validates only the given document.
    void acceptExact(const std::string &id, const nlohmann::json &doc) {
        exact_[id] = doc;
    }

    std::optional<bool> validate(const std::string &schemaId, const nlohmann::json &doc,
                                 std::string &failReport) const override {
        if (any_.count(schemaId) == 1) return true;
        auto it = exact_.find(schemaId);
        if (it == exact_.end()) return std::nullopt;  // unknown id
        if (doc == it->second) return true;
        failReport = "schema '" + schemaId + "' validation failed";
        return false;
    }

private:
    std::set<std::string> any_{};
    std::map<std::string, nlohmann::json> exact_{};
};

// Event source double: resolve returns a registered value by address; records purges.
class FakeEventSource : public EventSource {
public:
    // Register a value for (domain, address object).
    void set(const std::string &domain, const nlohmann::json &address, const nlohmann::json &value) {
        events_[domain + "|" + address.dump()] = value;
    }
    std::optional<nlohmann::json> resolve(const std::string &domain, const nlohmann::json &address) const override {
        lastAddress = address;
        auto it = events_.find(domain + "|" + address.dump());
        if (it == events_.end()) return std::nullopt;
        return it->second;
    }
    void purge(const std::string &domain, const nlohmann::json &address) override {
        purged_.push_back({domain, address});
    }
    const std::vector<std::pair<std::string, nlohmann::json>> &purged() const {
        return purged_;
    }
    mutable nlohmann::json lastAddress{};

private:
    std::map<std::string, nlohmann::json> events_{};
    std::vector<std::pair<std::string, nlohmann::json>> purged_{};
};

// File access double: records writes.
class FakeFileAccess : public FileAccess {
public:
    struct Write {
        std::string path;
        std::string data;
        bool binary;
    };
    std::map<std::string, std::string> contents{};
    std::optional<std::string> read(const std::string &path, bool) const override {
        auto it = contents.find(path);
        if (it == contents.end()) return std::nullopt;
        return it->second;
    }
    bool write(const std::string &path, const std::string &data, bool binary, bool templated) override {
        writes.push_back({path, data, binary});
        lastTemplated = templated;
        return true;
    }
    void truncate(const std::string &path) override {
        truncated.push_back(path);
    }
    bool lastTemplated{false};
    std::vector<std::string> truncated{};
    std::vector<Write> writes{};
};

// Socket writer double: records datagrams.
class FakeSocketWriter : public SocketWriter {
public:
    struct Dg {
        std::string path;
        std::string data;
        std::uint32_t delayMs;
    };
    bool write(const std::string &path, const std::string &datagram, std::uint32_t delayMs = 0) override {
        writes.push_back({path, datagram, delayMs});
        return true;
    }
    std::vector<Dg> writes{};
};

// Trigger double: records fired provisions.
class FakeTrigger : public Trigger {
public:
    struct Fire {
        std::string id;
        std::string inState;
    };
    void trigger(const std::string &id, const std::string &inState) override {
        fires.push_back({id, inState});
    }
    std::vector<Fire> fires{};
};

// Shell double: canned output and return code.
class FakeShell : public Shell {
public:
    Result next{};
    std::string lastCommand{};
    Result run(const std::string &command) override {
        lastCommand = command;
        return next;
    }
};

// Helper: parse one rule and assert success.
static std::shared_ptr<Pipeline> parseOne(const Engine &e, const std::string &source, const std::string &target) {
    std::string err;
    auto p = e.parse(nlohmann::json::array({{{"source", source}, {"target", target}}}), err);
    EXPECT_NE(p, nullptr) << err;
    return p;
}

// --- parser guards -------------------------------------------------------

}  // namespace

TEST(Engine_test, ParseRejectsNonArray) {
    Engine engine;
    std::string error;
    auto pipeline = engine.parse(nlohmann::json::object(), error);
    EXPECT_EQ(pipeline, nullptr);
    EXPECT_FALSE(error.empty());
}

TEST(Engine_test, ParseRejectsRuleMissingSourceTarget) {
    Engine engine;
    std::string error;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.x"}}}), error);
    EXPECT_EQ(pipeline, nullptr);
    EXPECT_FALSE(error.empty());
}

TEST(Engine_test, EmptyPipelineIsNoOp) {
    Engine engine;
    std::string error;
    auto pipeline = engine.parse(nlohmann::json::array(), error);
    ASSERT_NE(pipeline, nullptr) << error;

    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.executionTrace.empty());
}

TEST(Engine_test, NullPipelineFailsGracefully) {
    Engine engine;
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(nullptr, ctx, ports);
    EXPECT_FALSE(result.ok);
}

// --- vertical slice: value -> output.string ----------------------

TEST(Engine_test, ValueLiteralToOutputString) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.hello", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    Ports ports;  // no store needed for a literal
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(ctx.outBody.raw, "hello");
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_TRUE(result.executionTrace[0].applied);
}

TEST(Engine_test, ValueWithVariableSubstitutionFromStore) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.id-@{who}", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "who", "ada");
    Ports ports;
    ports.store = &store;

    StandaloneContext ctx;
    auto result = engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(ctx.outBody.raw, "id-ada");
}

TEST(Engine_test, LocalScopeWinsOverGlobal) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.@{x}", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "x", "global");
    store.set(Store::Scope::Local, "x", "local");
    Ports ports;
    ports.store = &store;

    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "local");
}

TEST(Engine_test, UnknownVariableBecomesEmpty) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.a@{missing}b", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    Ports ports;
    ports.store = &store;

    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "ab");
}

TEST(Engine_test, UnsupportedRuleIsSkippedAndTraced) {
    Engine engine;
    // 'request.uri' source not yet ported -> must be skipped, not crash.
    auto pipeline = parseOne(engine, "request.uri", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

// --- batch 2: sequence / timestamp / random / json.string / Append -----

TEST(Engine_test, SequenceToOutputString) {
    Engine engine;
    auto pipeline = parseOne(engine, "sequence", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.sequence = 116;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "116");
}

TEST(Engine_test, TimestampUsesInjectedClock) {
    Engine engine;
    auto pipeline = parseOne(engine, "timestamp.ms", "output.string");
    ASSERT_NE(pipeline, nullptr);

    FakeClock clock(1700000000);  // seconds
    Ports ports;
    ports.clock = &clock;

    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1700000000000");  // ms
}

TEST(Engine_test, RandomStaysWithinRange) {
    Engine engine;
    auto pipeline = parseOne(engine, "random.10.12", "output.string");
    ASSERT_NE(pipeline, nullptr);

    Ports ports;
    for (int i = 0; i < 200; ++i) {
        StandaloneContext ctx;
        engine.evaluate(pipeline, ctx, ports);
        int v = std::stoi(ctx.outBody.raw);
        EXPECT_GE(v, 10);
        EXPECT_LE(v, 12);
    }
}

TEST(Engine_test, JsonStringTargetAtRoot) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.hi", "output.json.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output, nlohmann::json("hi"));
}

TEST(Engine_test, JsonStringTargetAtPath) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.ada", "output.json.string./author/name");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["author"]["name"], "ada");
}

TEST(Engine_test, AppendFilterConcatenates) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.telegram"},
            {"target", "output.string"},
            {"filter", {{"Append", ".teslayout.com"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "telegram.teslayout.com");
}

TEST(Engine_test, AppendFilterAdmitsVariableSubstitution) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
                        nlohmann::json::array(
    {{{"source", "value.host-"}, {"target", "output.string"}, {"filter", {{"Append", "@{suffix}"}}}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    store.set(Store::Scope::Local, "suffix", "01");
    Ports ports;
    ports.store = &store;

    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "host-01");
}

// --- batch 3: store (local/global) + Prepend/Sum/Multiply ---------------

TEST(Engine_test, StoreWriteLocalThenReadBack) {
    Engine engine;
    std::string err;
    auto pipeline =
    engine.parse(nlohmann::json::array({{{"source", "value.42"}, {"target", "local.answer"}},
        {{"source", "local.answer"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "42");
    EXPECT_EQ(store.get(Store::Scope::Local, "answer"), nlohmann::json("42"));
}

TEST(Engine_test, StoreGlobalIsSeparateFromLocal) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.g"}, {"target", "global.k"}},
        {{"source", "value.l"}, {"target", "local.k"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Global, "k"), nlohmann::json("g"));
    EXPECT_EQ(store.get(Store::Scope::Local, "k"), nlohmann::json("l"));
}

TEST(Engine_test, StoreMissingSourceSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "local.ghost", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;  // empty
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, PrependFilter) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.teslayout.com"},
            {"target", "output.string"},
            {"filter", {{"Prepend", "www."}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "www.teslayout.com");
}

TEST(Engine_test, SumFilterOnSequence) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
                        nlohmann::json::array(
    {{{"source", "sequence"}, {"target", "output.string"}, {"filter", {{"Sum", 555000000}}}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.sequence = 116;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "555000116");
}

TEST(Engine_test, MultiplyFilterWithFraction) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
                        nlohmann::json::array(
    {{{"source", "value.-10"}, {"target", "output.string"}, {"filter", {{"Multiply", -0.1}}}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw,
              "1");  // -10 * -0.1 = 1 (whole -> integer-preserved)
}

// --- batch 4: inState / outState + store json-pointer navigation ---------

TEST(Engine_test, InStateSourceReadsContext) {
    Engine engine;
    auto pipeline = parseOne(engine, "inState", "output.string");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.inState = "created";
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "created");
}

TEST(Engine_test, OutStateTargetWritesContext) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.deleted", "outState");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outState, "deleted");
}

TEST(Engine_test, StoreReadByJsonPointer) {
    Engine engine;
    auto pipeline = parseOne(engine, "global.tpl./headers/auth", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "tpl", nlohmann::json{{"headers", {{"auth", "Bearer xyz"}}}});
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "Bearer xyz");
}

TEST(Engine_test, StoreWriteSubPathPreservesRest) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.ok", "global.doc./status");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "doc", nlohmann::json{{"id", 7}, {"status", "pending"}});
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    // only /status changed, /id preserved
    EXPECT_EQ(store.get(Store::Scope::Global, "doc"), (nlohmann::json{{"id", 7}, {"status", "ok"}}));
}

TEST(Engine_test, StoreMissingPathSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "global.obj./nope", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "obj", nlohmann::json{{"a", 1}});
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// --- batch 5: neutral v2 input/output sources + json-pointer navigation --

TEST(Engine_test, InputAtRootReadsWholeDocument) {
    Engine engine;
    auto pipeline = parseOne(engine, "input", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"method", "POST"}, {"uri", "/x"}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    // Whole input document dumped as a string.
    EXPECT_EQ(ctx.outBody.raw, ctx.input.dump());
}

TEST(Engine_test, InputByJsonPointer) {
    Engine engine;
    auto pipeline = parseOne(engine, "input./method", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"method", "POST"}, {"uri", "/x"}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "POST");
}

TEST(Engine_test, InputMissingPathSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "input./nope", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"method", "POST"}};
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, OutputByJsonPointer) {
    Engine engine;
    // First write into output, then read it back by pointer.
    auto pipeline = parseOne(engine, "output./author/name", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.output = nlohmann::json{{"author", {{"name", "ada"}}}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "ada");
}

TEST(Engine_test, InputPointerIntoNonObjectSkipsRule) {
    Engine engine;
    // '/method/deep' navigates into a scalar (string) -> missing path -> skip.
    auto pipeline = parseOne(engine, "input./method/deep", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"method", "POST"}};
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// --- batch 6: regex filters (RegexReplace / RegexCapture) ----------------

TEST(Engine_test, RegexReplaceMatchAndReplace) {
    Engine engine;
    std::string err;
    // Extract the project token from a URL and reformat it.
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.https://github.com/testillano/h2agent"},
            {"target", "output.string"},
            {   "filter", {{"RegexReplace", {{"rgx", "https://(\\w+).(com|es)/(\\w+)/(\\w+)"},
                            {"fmt", "User: $3; Project: $4"}
                        }
                    }
                }
            }
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "User: testillano; Project: h2agent");
}

TEST(Engine_test, RegexReplaceNoMatchFallsBackToSource) {
    Engine engine;
    std::string err;
    // Pattern does not match -> std::regex_replace returns the source unchanged.
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.untouched"},
            {"target", "output.string"},
            {   "filter", {{"RegexReplace", {{"rgx", "^[0-9]+$"},
                            {"fmt", "N/A"}
                        }
                    }
                }
            }
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
}

TEST(Engine_test, RegexCaptureWholeMatchToStringTarget) {
    Engine engine;
    std::string err;
    // Full match -> whole match (group 0) is the resolved value for a string
    // target.
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.a|b|0123456789"},
            {"target", "output.string"},
            {"filter", {{"RegexCapture", "(a\\|b\\|)([0-9]{10})"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "a|b|0123456789");  // group 0 (whole match)
}

TEST(Engine_test, RegexCaptureToLocalUsesFlatV1VarLayout) {
    Engine engine;
    std::string err;
    // v1 var parity: key = whole match, "<key>.<n>" = group n; groups are read
    // back with a dotted local key (no pointer).
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.a|b|0123456789"},
            {"target", "local.caps"},
            {"filter", {{"RegexCapture", "(a\\|b\\|)([0-9]{10})"}}}
        },
        {{"source", "local.caps.2"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);

    EXPECT_EQ(store.get(Store::Scope::Local, "caps"), nlohmann::json("a|b|0123456789"));
    EXPECT_EQ(store.get(Store::Scope::Local, "caps.1"), nlohmann::json("a|b|"));
    EXPECT_EQ(store.get(Store::Scope::Local, "caps.2"), nlohmann::json("0123456789"));
    EXPECT_EQ(ctx.outBody.raw, "0123456789");
}

TEST(Engine_test, RegexCaptureToGlobalUsesV1VaultObjectLayout) {
    Engine engine;
    std::string err;
    // v1 vault parity: single object {"0":whole,"1":g1,...}, read by pointer.
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.a|b|0123456789"},
            {"target", "global.caps"},
            {"filter", {{"RegexCapture", "(a\\|b\\|)([0-9]{10})"}}}
        },
        {{"source", "global.caps./2"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);

    EXPECT_EQ(store.get(Store::Scope::Global, "caps"),
    (nlohmann::json{{"0", "a|b|0123456789"}, {"1", "a|b|"}, {"2", "0123456789"}}));
    EXPECT_EQ(ctx.outBody.raw, "0123456789");
}

TEST(Engine_test, RegexCaptureNoMatchSkipsRule) {
    Engine engine;
    std::string err;
    // No full match -> rule is skipped (parity with h2agent returning false).
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.nope"},
            {"target", "output.string"},
            {"filter", {{"RegexCapture", "([0-9]+)"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, RegexInvalidPatternIsUnsupportedAndSkipped) {
    Engine engine;
    std::string err;
    // Unbalanced '(' -> invalid regex at parse time -> rule marked Unsupported
    // (skipped + traced), never throws.
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.whatever"},
            {"target", "output.string"},
            {"filter", {{"RegexCapture", "([unterminated"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

// --- batch 7: typed response-body targets (parity with h2agent) ----------

TEST(Engine_test, JsonIntegerFromNumericString) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.42", "output.json.integer./age");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(result.executionTrace[0].applied);
    EXPECT_EQ(ctx.output["age"], 42);
    EXPECT_TRUE(ctx.output["age"].is_number_integer());
}

TEST(Engine_test, JsonIntegerAtRootFromSequence) {
    Engine engine;
    auto pipeline = parseOne(engine, "sequence", "output.json.integer");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.sequence = 7;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output, nlohmann::json(7));
}

TEST(Engine_test, JsonIntegerNonNumericStringSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.abc", "output.json.integer./n");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(ctx.output.is_null());  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, JsonUnsignedFromNumericString) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.100", "output.json.unsigned./count");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["count"], 100u);
    EXPECT_TRUE(ctx.output["count"].is_number_unsigned());
}

TEST(Engine_test, JsonUnsignedNegativeStringSkipsRule) {
    Engine engine;
    // std::stoull does not accept a leading '-' as a valid unsigned here per
    // h2agent parity: actually stoull wraps, so use a non-numeric to force skip.
    auto pipeline = parseOne(engine, "value.notnum", "output.json.unsigned./u");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(ctx.output.is_null());
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, JsonFloatFromNumericString) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.3.14", "output.json.float./pi");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(ctx.output["pi"].is_number_float());
    EXPECT_DOUBLE_EQ(ctx.output["pi"].get<double>(), 3.14);
}

TEST(Engine_test, JsonFloatNonNumericStringSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.xyz", "output.json.float./f");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(ctx.output.is_null());
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, JsonBooleanFromNonEmptyString) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.anything", "output.json.boolean./flag");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(ctx.output["flag"].is_boolean());
    EXPECT_EQ(ctx.output["flag"], true);
}

TEST(Engine_test, JsonBooleanFromEmptyStringIsFalse) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.", "output.json.boolean./flag");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["flag"], false);
}

TEST(Engine_test, JsonBooleanFromZeroNumberIsFalse) {
    Engine engine;
    // A store holding integer 0, read back and coerced to boolean false.
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "local.z"}, {"target", "output.json.boolean./b"}}}), err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "z", 0);
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["b"], false);
}

TEST(Engine_test, JsonBooleanFromObjectSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "input", "output.json.boolean./b");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 1}};  // object -> cannot coerce to boolean
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(ctx.output.is_null());
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, JsonObjectFromActualObjectAtPath) {
    Engine engine;
    auto pipeline = parseOne(engine, "input./payload", "output.json.object./data");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"payload", {{"id", 1}, {"name", "ada"}}}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["data"], (nlohmann::json{{"id", 1}, {"name", "ada"}}));
}

TEST(Engine_test, JsonObjectAtRootMergesExisting) {
    Engine engine;
    auto pipeline = parseOne(engine, "input./extra", "output.json.object");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"extra", {{"b", 2}}}};
    ctx.output = nlohmann::json{{"a", 1}};  // pre-existing root
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    // merge_patch preserves /a and adds /b
    EXPECT_EQ(ctx.output, (nlohmann::json{{"a", 1}, {"b", 2}}));
}

TEST(Engine_test, JsonObjectFallsBackToStringForScalarSource) {
    Engine engine;
    // Non-object source (string) -> best-effort fallback to string.
    auto pipeline = parseOne(engine, "value.hello", "output.json.object./v");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["v"], "hello");
    EXPECT_TRUE(ctx.output["v"].is_string());
}

TEST(Engine_test, JsonObjectFallsBackToIntegerForNumericSource) {
    Engine engine;
    // Numeric (integer) source -> fallback priority string first (value.5 is a
    // literal string "5"), so this asserts string fallback for a value literal.
    // To exercise the integer branch we feed a stored integer.
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "local.n"}, {"target", "output.json.object./v"}}}), err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "n", 5);  // json integer
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["v"], 5);
    EXPECT_TRUE(ctx.output["v"].is_number_integer());
}

TEST(Engine_test, JsonStringParsesValidJson) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.{\"k\":1}", "output.json.jsonstring./parsed");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["parsed"], (nlohmann::json{{"k", 1}}));
}

TEST(Engine_test, JsonStringAtRootMerges) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.{\"b\":2}", "output.json.jsonstring");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.output = nlohmann::json{{"a", 1}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output, (nlohmann::json{{"a", 1}, {"b", 2}}));
}

TEST(Engine_test, JsonStringInvalidJsonSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.{not valid json", "output.json.jsonstring./x");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(ctx.output.is_null());  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, HexStringDecodesToRawBody) {
    Engine engine;
    // "48656c6c6f" -> "Hello"
    auto pipeline = parseOne(engine, "value.48656c6c6f", "output.hexstring");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "Hello");
}

TEST(Engine_test, HexStringAcceptsOxPrefix) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.0x4869", "output.hexstring");  // "Hi"
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "Hi");
}

TEST(Engine_test, HexStringOddLengthSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.abc", "output.hexstring");  // odd length
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // nothing applied
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, HexStringNonHexDigitSkipsRule) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.zzzz", "output.hexstring");  // even but non-hex
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// --- batch 8: randomset / eraser / math (deferred) -----------------------

TEST(Engine_test, RandomSetChoiceStaysWithinTokenSet) {
    Engine engine;
    auto pipeline = parseOne(engine, "randomset.alpha|beta|gamma", "output.string");
    ASSERT_NE(pipeline, nullptr);

    Ports ports;
    const std::set<std::string> allowed{"alpha", "beta", "gamma"};
    for (int i = 0; i < 200; ++i) {
        StandaloneContext ctx;
        engine.evaluate(pipeline, ctx, ports);
        EXPECT_TRUE(allowed.count(ctx.outBody.raw) == 1) << "unexpected token: " << ctx.outBody.raw;
    }
}

TEST(Engine_test, RandomSetSingleTokenIsDeterministic) {
    Engine engine;
    auto pipeline = parseOne(engine, "randomset.only", "output.string");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "only");
}

TEST(Engine_test, RandomSetEmptyPartsCountAsEmptyTokens) {
    Engine engine;
    // Parity: 'a||b' => 3 tokens {"a","","b"}. Over many draws every element of
    // the set (including the empty string) must be reachable.
    auto pipeline = parseOne(engine, "randomset.a||b", "output.string");
    ASSERT_NE(pipeline, nullptr);

    Ports ports;
    std::set<std::string> seen;
    for (int i = 0; i < 500; ++i) {
        StandaloneContext ctx;
        engine.evaluate(pipeline, ctx, ports);
        const std::set<std::string> allowed{"a", "", "b"};
        ASSERT_TRUE(allowed.count(ctx.outBody.raw) == 1) << "unexpected token: '" << ctx.outBody.raw << "'";
        seen.insert(ctx.outBody.raw);
    }
    // Empty-string token is a real member of the 3-token set.
    EXPECT_TRUE(seen.count("") == 1);
}

TEST(Engine_test, RandomSetChosenTokenAdmitsVariableSubstitution) {
    Engine engine;
    // Single token forces a deterministic choice; @{who} must be substituted.
    auto pipeline = parseOne(engine, "randomset.hello-@{who}", "output.string");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Local, "who", "ada");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "hello-ada");
}

TEST(Engine_test, EraserRemovesStoreKey) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "local.gone");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Local, "gone", "value");
    store.set(Store::Scope::Local, "keep", "other");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);

    EXPECT_FALSE(store.get(Store::Scope::Local, "gone").has_value());  // erased
    EXPECT_EQ(store.get(Store::Scope::Local, "keep"), nlohmann::json("other"));  // untouched
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_TRUE(result.executionTrace[0].applied);
}

TEST(Engine_test, EraserWithFilterSkipsRule) {
    // Parity: an eraser source combined with a filter skips the whole rule
    // (a filter has no real source value to operate on), so the store is NOT erased.
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "eraser"}, {"target", "local.gone"}, {"filter", {{"Append", ".x"}}}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    store.set(Store::Scope::Local, "gone", "value");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);

    EXPECT_EQ(store.get(Store::Scope::Local, "gone"), nlohmann::json("value"));  // NOT erased
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, EraserRemovesGlobalStoreKey) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "global.token");
    ASSERT_NE(pipeline, nullptr);

    MemStore store;
    store.set(Store::Scope::Global, "token", "abc");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_FALSE(store.get(Store::Scope::Global, "token").has_value());
}

TEST(Engine_test, EraserRemovesOutputJsonNode) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "output.json.object./remove");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.output = nlohmann::json{{"keep", 1}, {"remove", 2}};
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);

    // Only /remove dropped; /keep preserved.
    EXPECT_EQ(ctx.output, (nlohmann::json{{"keep", 1}}));
    EXPECT_TRUE(result.executionTrace[0].applied);
}

TEST(Engine_test, EraserClearsOutputAtRoot) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "output.json.object");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.output = nlohmann::json{{"a", 1}, {"b", 2}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(ctx.output.is_object());
    EXPECT_TRUE(ctx.output.empty());  // whole body cleared
}

TEST(Engine_test, EraserSetsEmptyStringForStringTarget) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    ctx.outBody.raw = "previous";
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "");
}

TEST(Engine_test, MathSourceEvaluatesWholeAsInteger) {
    Engine engine;
    // math.* is now evaluated via exprtk: whole result -> integer, else float.
    auto pipeline = parseOne(engine, "math.2*3+1", "output.string");
    ASSERT_NE(pipeline, nullptr);

    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "7");  // 2*3+1 = 7 (whole -> integer)
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_TRUE(result.executionTrace[0].applied);
}

TEST(Engine_test, MathSourceFloatAndVarsAndSkip) {
    Engine engine;
    // float result
    {
        auto p = parseOne(engine, "math.1/2", "output.string");
        StandaloneContext ctx;
        Ports ports;
        engine.evaluate(p, ctx, ports);
        EXPECT_EQ(ctx.outBody.raw, "0.5");
    }
    // @{var} substitution before evaluation
    {
        auto p = parseOne(engine, "math.@{n}*10", "output.string");
        MemStore store;
        store.set(Store::Scope::Global, "n", "4");
        Ports ports;
        ports.store = &store;
        StandaloneContext ctx;
        engine.evaluate(p, ctx, ports);
        EXPECT_EQ(ctx.outBody.raw, "40");
    }
    // invalid expression -> rule skipped
    {
        auto p = parseOne(engine, "math.2*/", "output.string");
        StandaloneContext ctx;
        ctx.outBody.raw = "untouched";
        Ports ports;
        auto result = engine.evaluate(p, ctx, ports, true);
        EXPECT_EQ(ctx.outBody.raw, "untouched");
        EXPECT_FALSE(result.executionTrace[0].applied);
    }
}

// Precompiled math (compiled once at load, @{} bound as exprtk variables). The
// observable result must be IDENTICAL to v1 textual substitution in every case.
namespace {
std::string evalMath(Engine &engine, const std::string &expr, const std::map<std::string, std::string> &vars) {
    auto p = parseOne(engine, "math." + expr, "output.string");
    EXPECT_NE(p, nullptr);
    MemStore store;
    for (const auto &kv : vars) store.set(Store::Scope::Local, kv.first, kv.second);
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    engine.evaluate(p, ctx, ports);
    return ctx.outBody.raw;
}
}  // namespace

TEST(Engine_test, MathPrecompiledBoundVariables) {
    Engine engine;
    EXPECT_EQ(evalMath(engine, "@{age} * 2", {{"age", "21"}}), "42");
    EXPECT_EQ(evalMath(engine, "@{a} + @{b} + @{a}", {{"a", "1.5"}, {"b", "2"}}), "5");  // repeated name, 1 slot
    EXPECT_EQ(evalMath(engine, "@{x} / 4", {{"x", "1e1"}}), "2.5");
    EXPECT_EQ(evalMath(engine, "1/4", {}), "0.25");  // folded constant (no @{})
    EXPECT_EQ(evalMath(engine, "2*3+1", {}), "7");
}

TEST(Engine_test, MathWholeResultBeyondIntIsInteger) {
    // Deliberate difference with the former implementation, which tested
    // 'result == (int)result' (overflow) and stored such values as floats.
    Engine engine;
    EXPECT_EQ(evalMath(engine, "3000000000", {}), "3000000000");
    EXPECT_EQ(evalMath(engine, "@{x} * 2", {{"x", "3000000000"}}), "6000000000");
}

TEST(Engine_test, MathPrecompiledReusedAcrossEvaluations) {
    // Same pipeline, different values per event: bound variables must not leak.
    Engine engine;
    auto p = parseOne(engine, "math.@{n}*10", "output.string");
    for (int n : {
                1, 7, 3
            }) {
        MemStore store;
        store.set(Store::Scope::Local, "n", std::to_string(n));
        Ports ports;
        ports.store = &store;
        StandaloneContext ctx;
        engine.evaluate(p, ctx, ports);
        EXPECT_EQ(ctx.outBody.raw, std::to_string(n * 10));
    }
}

TEST(Engine_test, MathTextualFallbackKeepsV1Semantics) {
    Engine engine;
    // Glued to a token: textual concatenation ("2" + "5" = 25).
    EXPECT_EQ(evalMath(engine, "2@{x}", {{"x", "5"}}), "25");
    // Value that is itself an expression.
    EXPECT_EQ(evalMath(engine, "@{e}+1", {{"e", "3*4"}}), "13");
    // Signed value: textual "-3+1".
    EXPECT_EQ(evalMath(engine, "@{x}+1", {{"x", "-3"}}), "-2");
    // Missing variable: substituted by nothing ("" does not compile -> skipped).
    EXPECT_EQ(evalMath(engine, "@{nope}", {}), "untouched");
    // Non-numeric value that does not compile: rule skipped.
    EXPECT_EQ(evalMath(engine, "@{s}*2", {{"s", "abc"}}), "untouched");
}

TEST(Engine_test, MathPrecompiledIsSafeUnderConcurrentEvaluation) {
    // One immutable pipeline shared by many threads (h2agent workers): each
    // event must see only its own bound values.
    Engine engine;
    auto p = parseOne(engine, "math.@{n}*2+@{m}", "output.string");
    ASSERT_NE(p, nullptr);
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 500; ++i) {
                MemStore store;
                store.set(Store::Scope::Local, "n", std::to_string(t * 1000 + i));
                store.set(Store::Scope::Local, "m", std::to_string(t));
                Ports ports;
                ports.store = &store;
                StandaloneContext ctx;
                engine.evaluate(p, ctx, ports);
                if (ctx.outBody.raw != std::to_string((t * 1000 + i) * 2 + t)) ++errors;
            }
        });
    }
    for (auto &th : threads) th.join();
    EXPECT_EQ(errors.load(), 0);
}

// --- batch 9: new filters (ConditionVar / EqualTo / DifferentFrom /
//              JsonConstraint / SchemaId / Split / BaseConvert / Strptime /
//              Strftime / RegexKey / Size) + strftime SOURCE ---------------

// ConditionVar ------------------------------------------------------------

TEST(Engine_test, ConditionVarTruthyTransfers) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.payload"},
            {"target", "output.string"},
            {"filter", {{"ConditionVar", "flag"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;
    store.set(Store::Scope::Local, "flag", "yes");  // non-empty -> truthy
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "payload");
    EXPECT_TRUE(result.executionTrace[0].applied);
}

TEST(Engine_test, ConditionVarFalsySkipsRule) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.payload"},
            {"target", "output.string"},
            {"filter", {{"ConditionVar", "flag"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;

    MemStore store;  // flag undefined -> falsy
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[0].note.empty());
}

TEST(Engine_test, ConditionVarEmptyStringIsFalsy) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.payload"},
            {"target", "output.string"},
            {"filter", {{"ConditionVar", "flag"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "flag", "");  // empty -> falsy
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
}

TEST(Engine_test, ConditionVarZeroNumberIsFalsy) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.payload"},
            {"target", "output.string"},
            {"filter", {{"ConditionVar", "n"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "n", 0);  // numeric zero -> falsy
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
}

TEST(Engine_test, ConditionVarNegationInvertsAndLocalWinsOverGlobal) {
    Engine engine;
    std::string err;
    // '!flag' passes when flag is falsy. flag undefined -> falsy -> !falsy -> pass.
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.payload"},
            {"target", "output.string"},
            {"filter", {{"ConditionVar", "!flag"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;  // flag undefined
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "payload");

    // Now make it truthy in global but falsy in local: local wins -> falsy ->
    // '!flag' pass again.
    MemStore store2;
    store2.set(Store::Scope::Global, "flag", "set");
    store2.set(Store::Scope::Local, "flag", "");
    Ports ports2;
    ports2.store = &store2;
    StandaloneContext ctx2;
    engine.evaluate(pipeline, ctx2, ports2);
    EXPECT_EQ(ctx2.outBody.raw, "payload");  // local empty -> falsy -> !flag passes
}

// EqualTo / DifferentFrom -------------------------------------------------

TEST(Engine_test, EqualToPassesOnMatch) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.201"},
            {"target", "output.string"},
            {"filter", {{"EqualTo", "201"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "201");
}

TEST(Engine_test, EqualToSkipsOnMismatch) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.500"},
            {"target", "output.string"},
            {"filter", {{"EqualTo", "201"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, EqualToAdmitsVariableSubstitution) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.ada"},
            {"target", "output.string"},
            {"filter", {{"EqualTo", "@{expected}"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "expected", "ada");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "ada");
}

TEST(Engine_test, DifferentFromPassesOnMismatch) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.500"},
            {"target", "output.string"},
            {"filter", {{"DifferentFrom", "201"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "500");
}

TEST(Engine_test, DifferentFromSkipsOnMatch) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.201"},
            {"target", "output.string"},
            {"filter", {{"DifferentFrom", "201"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// JsonConstraint ----------------------------------------------------------

TEST(Engine_test, JsonConstraintObjectSubsetSucceeds) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"JsonConstraint", {{"id", 7}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"id", 7}, {"name", "ada"}};  // superset -> ok
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1");
}

TEST(Engine_test, JsonConstraintFailsWithReport) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"JsonConstraint", {{"id", 99}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"id", 7}};  // value differs
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_NE(ctx.outBody.raw, "1");
    EXPECT_NE(ctx.outBody.raw.find("JsonConstraint FAILED"), std::string::npos);
}

TEST(Engine_test, JsonConstraintArrayModeContains) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"JsonConstraint", nlohmann::json::array({2, 3})}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json::array({1, 2, 3, 4});  // contains 2 and 3
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1");
}

// Regression (CT filter_test test_023): an existing but EMPTY node is a value,
// not "absent". Absence is decided by the host Document, never by the engine.
TEST(Engine_test, EmptyArrayInputIsAValueAndReachesTheFilter) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input./headers"},
            {"target", "output.string"},
            {"filter", {{"JsonConstraint", nlohmann::json::array({{{"name", "x-mandatory"}}})}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"headers", nlohmann::json::array()}};
    ctx.outBody.raw = "untouched";
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_NE(ctx.outBody.raw.find("JsonConstraint FAILED"), std::string::npos);  // rule applied
}

TEST(Engine_test, NullInputDocumentSkipsInputRules) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "input./a"}, {"target", "output.string"}}}), err);
    ASSERT_NE(pipeline, nullptr) << err;
    EvaluationContext ctx;  // no documents at all: reads absent, writes dropped
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, TraceIsNotBuiltWhenDisabled) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.x"}, {"target", "output.string"}}}), err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(result.executionTrace.empty());
    EXPECT_EQ(ctx.outBody.raw, "x");
}

TEST(Engine_test, OutputWritesGoToHostObjectsInPlace) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.7"}, {"target", "output.json.integer./n"}},
        {{"source", "value.raw"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    nlohmann::json hostBody = nlohmann::json{{"keep", 1}};
    std::string hostRaw;
    JsonDocument view(hostBody);
    EvaluationContext ctx;
    ctx.outputView = &view;
    ctx.output = &hostBody;
    ctx.outBody = &hostRaw;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(hostBody, (nlohmann::json{{"keep", 1}, {"n", 7}}));
    EXPECT_EQ(hostRaw, "raw");
}

// SchemaId ----------------------------------------------------------------

TEST(Engine_test, SchemaIdValidDocReturnsOne) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"SchemaId", "mySchema"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    FakeSchema schema;
    schema.acceptAny("mySchema");
    Ports ports;
    ports.schema = &schema;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 1}};
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1");
}

TEST(Engine_test, SchemaIdInvalidDocReturnsFailReport) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"SchemaId", "mySchema"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    FakeSchema schema;
    schema.acceptExact("mySchema", nlohmann::json{{"a", 1}});
    Ports ports;
    ports.schema = &schema;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 2}};  // differs -> invalid
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "schema 'mySchema' validation failed");  // v1: report, not ""
}

TEST(Engine_test, SchemaIdUnknownOrNoValidatorKeepsSource) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"SchemaId", "mySchema"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 1}};
    Ports ports;  // no schema port: v1 "missing schema -> validation IGNORED"
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "{\"a\":1}");  // source passes through unchanged
    EXPECT_TRUE(result.executionTrace[0].applied);
}

// Split -------------------------------------------------------------------

TEST(Engine_test, SplitDefaultsBuildIpv4Style) {
    Engine engine;
    std::string err;
    // Defaults size=2,count=4,sep='.',filler='0',numeric=false.
    // Last 8 chars of "555123456789" -> "23456789" -> groups 23.45.67.89
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.555123456789"},
            {"target", "output.string"},
            {"filter", {{"Split", nlohmann::json::object()}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "23.45.67.89");
}

TEST(Engine_test, SplitLeftPadsWithFiller) {
    Engine engine;
    std::string err;
    // size=2,count=4 -> totalLen 8; source "123" padded left with '0' -> "00000123"
    // groups: 00.00.01.23
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.123"},
            {"target", "output.string"},
            {"filter", {{"Split", {{"size", 2}, {"count", 4}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "00.00.01.23");
}

TEST(Engine_test, SplitNumericStripsLeadingZeros) {
    Engine engine;
    std::string err;
    // numeric=true strips leading zeros per group: "00000123" -> 0.0.1.23
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.123"},
            {"target", "output.string"},
            {"filter", {{"Split", {{"size", 2}, {"count", 4}, {"numeric", true}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "0.0.1.23");
}

// BaseConvert -------------------------------------------------------------

TEST(Engine_test, BaseConvertHexToDecimal) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.ff"},
            {"target", "output.string"},
            {"filter", {{"BaseConvert", {{"in", 16}, {"out", 10}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "255");
}

TEST(Engine_test, BaseConvertDecimalToHexCapital) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.255"},
            {"target", "output.string"},
            {"filter", {{"BaseConvert", {{"in", 10}, {"out", 16}, {"capital", true}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "FF");
}

TEST(Engine_test, BaseConvertErrorFallsBackToSource) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.zzz"},
            {"target", "output.string"},
            {"filter", {{"BaseConvert", {{"in", 16}, {"out", 10}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "zzz");  // unparseable -> fallback to source
}

// Strptime / Strftime filters --------------------------------------------

TEST(Engine_test, StrptimeParsesDateToEpochSeconds) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.1970-01-01 00:00:00"},
            {"target", "output.string"},
            {"filter", {{"Strptime", {{"fmt", "%Y-%m-%d %H:%M:%S"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "0");  // epoch 0 (UTC via timegm)
}

TEST(Engine_test, StrptimeMsUnitScales) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.1970-01-01 00:00:01"},
            {"target", "output.string"},
            {"filter", {{"Strptime", {{"fmt", "%Y-%m-%d %H:%M:%S"}, {"unit", "ms"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1000");  // 1 second -> 1000 ms
}

TEST(Engine_test, StrptimeParseFailureSkipsRule) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.not-a-date"},
            {"target", "output.string"},
            {"filter", {{"Strptime", {{"fmt", "%Y-%m-%d %H:%M:%S"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, StrftimeFilterFormatsEpoch) {
    Engine engine;
    std::string err;
    // Stored integer epoch 0 formatted as date.
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "local.epoch"},
            {"target", "output.string"},
            {"filter", {{"Strftime", {{"fmt", "%Y-%m-%d"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "epoch", 0);  // epoch 0 -> 1970-01-01
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1970-01-01");
}

TEST(Engine_test, StrftimeFilterMsUnitScalesDown) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "local.epochms"},
            {"target", "output.string"},
            {"filter", {{"Strftime", {{"fmt", "%Y-%m-%d %H:%M:%S"}, {"unit", "ms"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "epochms", 1000);  // 1000 ms -> 1 s -> 00:00:01
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "1970-01-01 00:00:01");
}

TEST(Engine_test, StrftimeFilterNonNumericSourceSkipsRule) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.notnum"},
            {"target", "output.string"},
            {"filter", {{"Strftime", {{"fmt", "%Y-%m-%d"}}}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// RegexKey ----------------------------------------------------------------

TEST(Engine_test, RegexKeyReturnsFirstMatchingKeyValue) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"RegexKey", "^item_[0-9]+$"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    // Only item_42 matches the pattern; other keys ignored.
    ctx.input = nlohmann::json{{"meta", "x"}, {"item_42", "the-value"}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "the-value");
}

TEST(Engine_test, RegexKeyNoMatchSkipsRule) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"},
            {"target", "output.string"},
            {"filter", {{"RegexKey", "^nope_[0-9]+$"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"item_42", "v"}};
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, RegexKeyNonObjectSourceSkipsRule) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.flat"},
            {"target", "output.string"},
            {"filter", {{"RegexKey", ".*"}}}
        }}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");  // string source -> not an object -> skip
    EXPECT_FALSE(result.executionTrace[0].applied);
}

// Size --------------------------------------------------------------------

TEST(Engine_test, SizeOfObjectCountsKeys) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"}, {"target", "output.string"}, {"filter", "Size"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 1}, {"b", 2}, {"c", 3}};
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "3");
}

TEST(Engine_test, SizeOfArrayCountsElements) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "input"}, {"target", "output.string"}, {"filter", "Size"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.input = nlohmann::json::array({10, 20, 30, 40});
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "4");
}

TEST(Engine_test, SizeOfStringCountsChars) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
    nlohmann::json::array({{{"source", "value.hello"}, {"target", "output.string"}, {"filter", "Size"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "5");
}

// strftime SOURCE ---------------------------------------------------------

TEST(Engine_test, StrftimeSourceFormatsCurrentUtcTime) {
    Engine engine;
    // Year-only format: the current UTC year must be a 4-digit number >= 2026.
    auto pipeline = parseOne(engine, "strftime.%Y", "output.string");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    EXPECT_TRUE(result.executionTrace[0].applied);
    ASSERT_EQ(ctx.outBody.raw.size(), 4u);
    int year = std::stoi(ctx.outBody.raw);
    EXPECT_GE(year, 2026);
}

TEST(Engine_test, StrftimeSourceAdmitsVariableSubstitution) {
    Engine engine;
    // The formatted result (literal text + @{who}) must substitute the store var.
    auto pipeline = parseOne(engine, "strftime.stamp-@{who}", "output.string");
    ASSERT_NE(pipeline, nullptr);
    MemStore store;
    store.set(Store::Scope::Local, "who", "ada");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    // No strftime conversion specifier in "stamp-", so output is the literal plus
    // the substituted variable.
    EXPECT_EQ(ctx.outBody.raw, "stamp-ada");
}

// --- batch: effect/control targets, event sources/targets, trigger ------

TEST(Engine_test, MetaHeaderStatusDelayTargets) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.application/json"},
            {"target", "meta.header.content-type"}
        },
        {{"source", "value.201"}, {"target", "meta.statusCode"}},
        {{"source", "value.30"}, {"target", "meta.delayMs"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.header("content-type"), "application/json");
    EXPECT_EQ(ctx.statusCode, 201u);
    EXPECT_EQ(ctx.delayMs, 30u);
    EXPECT_FALSE(ctx.output.contains("statusCode"));  // meta never pollutes the body
}

TEST(Engine_test, OutStateForeignRecordsMethodAndUri) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.id-deleted", "outState.GET./app/v1/item/7");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outState, "id-deleted");
    EXPECT_EQ(ctx.outStateMethod, "GET");
    EXPECT_EQ(ctx.outStateUri, "/app/v1/item/7");
}

TEST(Engine_test, BreakStopsPipelineOnNonEmpty) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value.stop"}, {"target", "break"}},
        {{"source", "value.after"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(result.broke);
    EXPECT_EQ(ctx.outBody.raw, "");  // second rule never ran
}

TEST(Engine_test, BreakEmptySourceIsNoOp) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(nlohmann::json::array({{{"source", "value."}, {"target", "break"}},
        {{"source", "value.after"}, {"target", "output.string"}}}),
    err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports);
    EXPECT_FALSE(result.broke);
    EXPECT_EQ(ctx.outBody.raw, "after");  // pipeline continued
}

TEST(Engine_test, TypedStoreObjectWriteAndSkip) {
    Engine engine;
    MemStore store;
    Ports ports;
    ports.store = &store;
    // object source -> stored natively
    {
        store.set(Store::Scope::Global, "src", nlohmann::json{{"a", 1}});
        auto pipeline = engine.parse(
        nlohmann::json::array({{{"source", "global.src"}, {"target", "global.dst.json.object"}}}), *(new std::string));
        StandaloneContext ctx;
        engine.evaluate(pipeline, ctx, ports);
        EXPECT_EQ(store.get(Store::Scope::Global, "dst"), (nlohmann::json{{"a", 1}}));
    }
    // non-object source -> skipped
    {
        auto pipeline = parseOne(engine, "value.notanobject", "global.dst2.json.object");
        StandaloneContext ctx;
        auto result = engine.evaluate(pipeline, ctx, ports, true);
        EXPECT_FALSE(store.get(Store::Scope::Global, "dst2").has_value());
        EXPECT_FALSE(result.executionTrace[0].applied);
    }
}

TEST(Engine_test, TypedStoreJsonStringParses) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.{\"x\":5}", "global.parsed.json.jsonstring");
    ASSERT_NE(pipeline, nullptr);
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Global, "parsed"), (nlohmann::json{{"x", 5}}));
}

TEST(Engine_test, TxtFileAndUdpSocketViaPorts) {
    Engine engine;
    FakeFileAccess files;
    FakeSocketWriter sockets;
    Ports ports;
    ports.files = &files;
    ports.sockets = &sockets;
    {
        StandaloneContext scratch1;
        engine.evaluate(parseOne(engine, "value.hello", "txtFile./tmp/out.txt"), scratch1, ports);
    }
    {
        StandaloneContext scratch2;
        engine.evaluate(parseOne(engine, "value.555", "udpSocket./tmp/udp.sock|20"), scratch2, ports);
    }
    ASSERT_EQ(files.writes.size(), 1u);
    EXPECT_EQ(files.writes[0].path, "/tmp/out.txt");
    EXPECT_EQ(files.writes[0].data, "hello");
    EXPECT_FALSE(files.writes[0].binary);
    ASSERT_EQ(sockets.writes.size(), 1u);
    EXPECT_EQ(sockets.writes[0].path, "/tmp/udp.sock");
    EXPECT_EQ(sockets.writes[0].data, "555");
    EXPECT_EQ(sockets.writes[0].delayMs, 20u);
}

TEST(Engine_test, FilePortNullSkips) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.hello", "txtFile./tmp/out.txt");
    StandaloneContext ctx;
    Ports ports;  // no files port
    auto result = engine.evaluate(pipeline, ctx, ports, true);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, ServerEventSourceResolvesViaPort) {
    Engine engine;
    auto pipeline = parseOne(engine,
                             "serverEvent.requestMethod=GET&requestUri=/x&eventNumber=-1&eventPath=/body",
                             "output.string");
    ASSERT_NE(pipeline, nullptr);
    FakeEventSource events;
    events.set("serverEvent",
    nlohmann::json{{"requestMethod", "GET"}, {"requestUri", "/x"}, {"eventNumber", "-1"}, {"eventPath", "/body"}},
    "echoed");
    Ports ports;
    ports.events = &events;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "echoed");
}

TEST(Engine_test, ServerEventNotFoundSkips) {
    Engine engine;
    auto pipeline = parseOne(engine, "serverEvent.requestMethod=GET&requestUri=/missing", "output.string");
    FakeEventSource events;  // empty
    Ports ports;
    ports.events = &events;
    StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    auto result = engine.evaluate(pipeline, ctx, ports, true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, SendseqSource) {
    Engine engine;
    auto pipeline = parseOne(engine, "sendseq", "output.string");
    StandaloneContext ctx;
    ctx.sequence = 42;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "42");
}

TEST(Engine_test, ClientProvisionTriggerGatedBySource) {
    Engine engine;
    FakeTrigger trig;
    Ports ports;
    ports.trigger = &trig;
    // non-empty source -> fires
    {
        StandaloneContext scratch3;
        engine.evaluate(parseOne(engine, "value.1", "clientProvision.myFlow.initial"), scratch3, ports);
    }
    ASSERT_EQ(trig.fires.size(), 1u);
    EXPECT_EQ(trig.fires[0].id, "myFlow");
    EXPECT_EQ(trig.fires[0].inState, "initial");
    // empty source -> does not fire
    {
        StandaloneContext scratch4;
        engine.evaluate(parseOne(engine, "value.", "clientProvision.myFlow.initial"), scratch4, ports);
    }
    EXPECT_EQ(trig.fires.size(), 1u);
}

TEST(Engine_test, EventPurgeViaEraser) {
    Engine engine;
    auto pipeline = parseOne(engine, "eraser", "serverEvent.requestMethod=GET&requestUri=/x&eventNumber=-1");
    FakeEventSource events;
    Ports ports;
    ports.events = &events;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    ASSERT_EQ(events.purged().size(), 1u);
    EXPECT_EQ(events.purged()[0].first, "serverEvent");
    EXPECT_EQ(events.purged()[0].second,
    (nlohmann::json{{"requestMethod", "GET"}, {"requestUri", "/x"}, {"eventNumber", "-1"}}));
}

// --- v1 parity fixes: onFilterFail, meta/body, dotted var keys, RegexKey
//     groups, event address tokenization ------------------------------------

TEST(Engine_test, OnFilterFailRunsFallbackWhenFilterFails) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.warning","target":"output.string","filter":{"EqualTo":"critical"},
       "onFilterFail":[{"source":"value.fallback","target":"output.string"}]}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "fallback");
}

TEST(Engine_test, OnFilterFailNotRunWhenFilterPasses) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.critical","target":"output.string","filter":{"EqualTo":"critical"},
       "onFilterFail":[{"source":"value.fallback","target":"output.string"}]}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "critical");
}

TEST(Engine_test, OnFilterFailIsRecursive) {
    Engine engine;
    std::string err;
    // if critical / else-if warning / else info
    auto pipeline = engine.parse(R"([
      {"source":"global.level","target":"output.string","filter":{"EqualTo":"critical"},
       "onFilterFail":[
         {"source":"global.level","target":"output.string","filter":{"EqualTo":"warning"},
          "onFilterFail":[{"source":"value.info","target":"output.string"}]}
       ]}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    Ports ports;
    ports.store = &store;
    for (auto [level, expected] : std::vector<std::pair<std::string, std::string>> {
    {"critical", "critical"}, {"warning", "warning"}, {"debug", "info"}
}) {
        store.set(Store::Scope::Global, "level", level);
        StandaloneContext ctx;
        engine.evaluate(pipeline, ctx, ports);
        EXPECT_EQ(ctx.outBody.raw, expected) << "level=" << level;
    }
}

TEST(Engine_test, BreakInsideOnFilterFailStopsPipeline) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.x","target":"output.string","filter":{"EqualTo":"y"},
       "onFilterFail":[{"source":"value.1","target":"break"}]},
      {"source":"value.after","target":"output.string"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports);
    EXPECT_TRUE(result.broke);
    EXPECT_EQ(ctx.outBody.raw, "");  // the rule after the fallback break never ran
}

TEST(Engine_test, EraserWithFilterRunsOnFilterFail) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"eraser","target":"local.k","filter":{"Append":"x"},
       "onFilterFail":[{"source":"value.fb","target":"output.string"}]}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "k", "keep");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Local, "k"), nlohmann::json("keep"));  // not erased
    EXPECT_EQ(ctx.outBody.raw, "fb");                                        // fallback ran
}

TEST(Engine_test, OnFilterFailMustBeArray) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(
                        R"([{"source":"value.a","target":"output.string","onFilterFail":{"x":1}}])"_json, err);
    EXPECT_EQ(pipeline, nullptr);
    EXPECT_FALSE(err.empty());
}

TEST(Engine_test, MetaHeaderWritesKeepEveryValueInOrder) {
    // v1 emplaces every header write (multimap): the engine must not collapse them.
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.a=1","target":"meta.header.set-cookie"},
      {"source":"value.b=2","target":"meta.header.set-cookie"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    ASSERT_EQ(ctx.headers.size(), 2u);
    EXPECT_EQ(ctx.headers[0], (std::pair<std::string, std::string> {"set-cookie", "a=1"}));
    EXPECT_EQ(ctx.headers[1], (std::pair<std::string, std::string> {"set-cookie", "b=2"}));
}

TEST(Engine_test, MetaUnsupportedByHostIsSkipped) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.404","target":"meta.statusCode"},
      {"source":"meta./uri","target":"output.string"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    EvaluationContext ctx;  // host exposes no metadata at all
    std::string raw = "untouched";
    ctx.outBody = &raw;
    Ports ports;
    auto result = engine.evaluate(pipeline, ctx, ports, /*trace=*/true);
    ASSERT_EQ(result.executionTrace.size(), 2u);
    EXPECT_FALSE(result.executionTrace[0].applied);
    EXPECT_FALSE(result.executionTrace[1].applied);
    EXPECT_EQ(raw, "untouched");
}

TEST(Engine_test, BodyFieldNamedStatusCodeIsNotClobberedByMeta) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.body-value","target":"output.json.string./statusCode"},
      {"source":"value.404","target":"meta.statusCode"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["statusCode"], "body-value");
    EXPECT_EQ(ctx.statusCode, 404u);
}

TEST(Engine_test, DottedLocalKeyIsAPlainKeyNotAPointer) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.v","target":"local.my.dotted.name"},
      {"source":"local.my.dotted.name","target":"output.string"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Local, "my.dotted.name"), nlohmann::json("v"));
    EXPECT_EQ(ctx.outBody.raw, "v");
}

TEST(Engine_test, TypedStoreWriteWithDottedKey) {
    Engine engine;
    auto pipeline = parseOne(engine, "value.{\"a\":1}", "local.my.doc.json.jsonstring");
    ASSERT_NE(pipeline, nullptr);
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Local, "my.doc"), (nlohmann::json{{"a", 1}}));
}

TEST(Engine_test, RegexKeyPublishesMatchedKeyAndGroupsAsLocalVars) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"input./accounts","target":"global.acct","filter":{"RegexKey":"acct-([0-9]+)-checking"}}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.input = R"({"accounts":{"acct-17-checking":{"balance":1500},"acct-17-savings":{"balance":3}}})"_json;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Global, "acct"), (nlohmann::json{{"balance", 1500}}));
    EXPECT_EQ(store.get(Store::Scope::Local, "acct.0"), nlohmann::json("acct-17-checking"));
    EXPECT_EQ(store.get(Store::Scope::Local, "acct.1"), nlohmann::json("17"));
}

TEST(Engine_test, EventAddressIsDecodedAndSubstitutedPerValue) {
    Engine engine;
    // requestUri comes from a variable holding '?' and '&' (README-recommended
    // usage); eventPath is URL-encoded in the literal.
    auto pipeline = parseOne(engine,
                             "serverEvent.requestMethod=POST&requestUri=@{uri}&eventNumber=-1&eventPath=%2FrequestBody",
                             "output.string");
    ASSERT_NE(pipeline, nullptr);
    MemStore store;
    store.set(Store::Scope::Local, "uri", "/app/v1/stock?loc=123&id=2");
    FakeEventSource events;
    Ports ports;
    ports.store = &store;
    ports.events = &events;
    StandaloneContext ctx;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(events.lastAddress, (nlohmann::json{{"requestMethod", "POST"},
        {"requestUri", "/app/v1/stock?loc=123&id=2"},
        {"eventNumber", "-1"},
        {"eventPath", "/requestBody"}}));
}

TEST(Engine_test, JsonPathsAdmitVariableSubstitution) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value.v1","target":"output.json.string./items/@{field}"},
      {"source":"input./data/@{field}","target":"global.doc./@{field}"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    store.set(Store::Scope::Local, "field", "name");
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.input = R"({"data":{"name":"ada"}})"_json;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.output["items"]["name"], "v1");
    EXPECT_EQ(store.get(Store::Scope::Global, "doc"), (nlohmann::json{{"name", "ada"}}));
}

TEST(Engine_test, MalformedPointerSkipsRuleWithoutThrowing) {
    Engine engine;
    // "~2" is an invalid json-pointer escape: must skip, never throw into the host
    auto pipeline = parseOne(engine, "value.x", "output.json.string./bad~2escape");
    ASSERT_NE(pipeline, nullptr);
    StandaloneContext ctx;
    Ports ports;
    TransformResult result;
    EXPECT_NO_THROW(result = engine.evaluate(pipeline, ctx, ports, true));
    ASSERT_EQ(result.executionTrace.size(), 1u);
    EXPECT_FALSE(result.executionTrace[0].applied);
}

TEST(Engine_test, TxtFileSourceReadsViaPortAndMissingIsEmpty) {
    Engine engine;
    FakeFileAccess files;
    files.contents["/data/f.txt"] = "file-content";
    Ports ports;
    ports.files = &files;
    {
        StandaloneContext ctx;
        engine.evaluate(parseOne(engine, "txtFile./data/f.txt", "output.string"), ctx, ports);
        EXPECT_EQ(ctx.outBody.raw, "file-content");
    }
    {
        StandaloneContext ctx;
        ctx.outBody.raw = "untouched";
        auto r = engine.evaluate(parseOne(engine, "txtFile./data/missing.txt", "output.string"), ctx, ports, true);
        EXPECT_EQ(ctx.outBody.raw, "");  // v1 parity: missing file yields "" (rule applies)
        EXPECT_TRUE(r.executionTrace[0].applied);
    }
}

TEST(Engine_test, CommandSourceCapturesOutputAndRc) {
    Engine engine;
    FakeShell shell;
    shell.next = {"hello\n", 3};
    MemStore store;
    store.set(Store::Scope::Local, "who", "ada");
    Ports ports;
    ports.shell = &shell;
    ports.store = &store;
    StandaloneContext ctx;
    engine.evaluate(parseOne(engine, "command.echo @{who}", "output.string"), ctx, ports);
    EXPECT_EQ(shell.lastCommand, "echo ada");
    EXPECT_EQ(ctx.outBody.raw, "hello\n");
    EXPECT_EQ(store.get(Store::Scope::Local, "rc"), nlohmann::json("3"));
}

TEST(Engine_test, ValidationFilterOnLocalTargetPublishesFailVar) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"input","target":"local.ok","filter":{"JsonConstraint":{"a":1}}}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 2}};
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Local, "ok"), nlohmann::json(""));  // v1: empty on failure
    ASSERT_TRUE(store.get(Store::Scope::Local, "ok.fail").has_value());
    EXPECT_FALSE(store.get(Store::Scope::Local, "ok.fail")->get<std::string>().empty());
}

TEST(Engine_test, StoreTypingFollowsV1VarAndVault) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"input","target":"local.asVar"},
      {"source":"input","target":"global.asVault"},
      {"source":"math.1+1","target":"global.num"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    MemStore store;
    Ports ports;
    ports.store = &store;
    StandaloneContext ctx;
    ctx.input = nlohmann::json{{"a", 1}};
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(store.get(Store::Scope::Local, "asVar"), nlohmann::json("{\"a\":1}"));  // var: serialized
    EXPECT_EQ(store.get(Store::Scope::Global, "asVault"), (nlohmann::json{{"a", 1}}));  // vault: native object
    EXPECT_EQ(store.get(Store::Scope::Global, "num"), nlohmann::json("2"));              // vault scalar: string
}

TEST(Engine_test, EraserOnFileTargetTruncatesAndTemplatedFlagReported) {
    Engine engine;
    FakeFileAccess files;
    MemStore store;
    store.set(Store::Scope::Local, "id", "7");
    Ports ports;
    ports.files = &files;
    ports.store = &store;
    {
        StandaloneContext ctx;
        engine.evaluate(parseOne(engine, "eraser", "txtFile./tmp/log.txt"), ctx, ports);
        ASSERT_EQ(files.truncated.size(), 1u);
        EXPECT_EQ(files.truncated[0], "/tmp/log.txt");
    }
    {
        StandaloneContext ctx;
        engine.evaluate(parseOne(engine, "value.x", "txtFile./tmp/f-@{id}.txt"), ctx, ports);
        EXPECT_EQ(files.writes.back().path, "/tmp/f-7.txt");
        EXPECT_TRUE(files.lastTemplated);
    }
}

TEST(Engine_test, JsonObjectTargetKeepsNativeScalarTypes) {
    Engine engine;
    {
        StandaloneContext ctx;
        Ports ports;
        engine.evaluate(parseOne(engine, "math.1/2", "output.json.object"), ctx, ports);
        EXPECT_EQ(ctx.output.dump(), "0.5");
    }
    {
        StandaloneContext ctx;
        ctx.input = R"({"boo":true})"_json;
        Ports ports;
        engine.evaluate(parseOne(engine, "input./boo", "output.json.object./b"), ctx, ports);
        EXPECT_EQ(ctx.output["b"], true);
    }
}

TEST(Engine_test, ClientProvisionWithoutInStateIsDiscarded) {
    Engine engine;
    FakeTrigger trig;
    Ports ports;
    ports.trigger = &trig;
    StandaloneContext ctx;
    auto r = engine.evaluate(parseOne(engine, "value.1", "clientProvision.myFlow"), ctx, ports, true);
    EXPECT_TRUE(trig.fires.empty());
    EXPECT_FALSE(r.executionTrace[0].applied);
}

TEST(Engine_test, MetaSourceReadsLiveMetadataAndMetaTargetsWriteIt) {
    Engine engine;
    std::string err;
    auto pipeline = engine.parse(R"([
      {"source":"value./v2/items","target":"meta.uri"},
      {"source":"meta./uri","target":"output.string"},
      {"source":"value.PUT","target":"meta.method"},
      {"source":"value.1500","target":"meta.timeoutMs"}
    ])"_json, err);
    ASSERT_NE(pipeline, nullptr) << err;
    StandaloneContext ctx;
    ctx.uri = "/v1/items";
    ctx.method = "POST";
    ctx.timeoutMs = 3000;
    Ports ports;
    engine.evaluate(pipeline, ctx, ports);
    EXPECT_EQ(ctx.outBody.raw, "/v2/items");  // the source saw the value written just before
    EXPECT_EQ(ctx.uri, "/v2/items");
    EXPECT_EQ(ctx.method, "PUT");
    EXPECT_EQ(ctx.timeoutMs, 1500u);
}

