#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>

#include <Engine.hpp>
#include <EngineGrammarV1.hpp>

using h2agent::model::EngineGrammarV1;
using Mode = h2agent::model::EngineGrammarV1::Mode;
namespace jf = ert::jsonflow;

/////////////////
// TRANSLATION //
/////////////////

TEST(EngineGrammarV1_test, ServerSourcesMapToNeutralInputOutput) {
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri", Mode::Server), "input./uri");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri.path", Mode::Server), "input./uriPath");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri.param.name", Mode::Server), "input./uriParams/name");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body", Mode::Server), "input./body");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body./a/b", Mode::Server), "input./body/a/b");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.body", Mode::Server), "output");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.body./a", Mode::Server), "output./a");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.header.x-version", Mode::Server), "input./headerMap/x-version");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.headers", Mode::Server), "input./headers");
}

TEST(EngineGrammarV1_test, StorageAndSequenceAliases) {
    EXPECT_EQ(EngineGrammarV1::translateSource("var.caps.1", Mode::Server), "local.caps.1");
    EXPECT_EQ(EngineGrammarV1::translateSource("vault.tpl./a", Mode::Server), "global.tpl./a");
    EXPECT_EQ(EngineGrammarV1::translateSource("recvseq", Mode::Server), "sequence");
    EXPECT_EQ(EngineGrammarV1::translateTarget("var.x", Mode::Server), "local.x");
    EXPECT_EQ(EngineGrammarV1::translateTarget("vault.doc.json.object./a", Mode::Server), "global.doc.json.object./a");
}

TEST(EngineGrammarV1_test, ServerTargetsMapToOutputAndMeta) {
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.body.string", Mode::Server), "output.string");
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.body.hexstring", Mode::Server), "output.hexstring");
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.body.json.integer./n", Mode::Server), "output.json.integer./n");
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.header.location", Mode::Server), "meta.header.location");
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.statusCode", Mode::Server), "meta.statusCode");
    EXPECT_EQ(EngineGrammarV1::translateTarget("response.delayMs", Mode::Server), "meta.delayMs");
}

TEST(EngineGrammarV1_test, GrammarIdenticalExpressionsPassThrough) {
    for (const std::string s : {
                "value.x", "math.1+1", "random.1.2", "randomset.a|b", "timestamp.ms", "strftime.%Y",
                "eraser", "inState", "serverEvent.requestMethod=GET", "txtFile./f", "command.ls"
            })
        EXPECT_EQ(EngineGrammarV1::translateSource(s, Mode::Server), s);
    for (const std::string t : {
                "outState", "outState.GET./x", "txtFile./f", "udpSocket./s", "break",
                "clientProvision.id.initial", "serverEvent.requestMethod=GET"
            })
        EXPECT_EQ(EngineGrammarV1::translateTarget(t, Mode::Server), t);
}

TEST(EngineGrammarV1_test, PointerTokensAreEscaped) {
    EXPECT_EQ(EngineGrammarV1::escapePointerToken("a/b~c"), "a~1b~0c");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri.param.a/b", Mode::Server), "input./uriParams/a~1b");
}

TEST(EngineGrammarV1_test, PipelineTranslatesRecursivelyAndKeepsFilters) {
    const nlohmann::json v1 = R"([
      {"source":"var.level","target":"response.body.string","filter":{"EqualTo":"critical"},
       "onFilterFail":[{"source":"recvseq","target":"vault.seen"}]}
    ])"_json;
    const nlohmann::json v2 = EngineGrammarV1::translatePipeline(v1, Mode::Server);
    EXPECT_EQ(v2[0]["source"], "local.level");
    EXPECT_EQ(v2[0]["target"], "output.string");
    EXPECT_EQ(v2[0]["filter"], v1[0]["filter"]);
    EXPECT_EQ(v2[0]["onFilterFail"][0]["source"], "sequence");
    EXPECT_EQ(v2[0]["onFilterFail"][0]["target"], "global.seen");
}

TEST(EngineGrammarV1_test, NonArrayPipelineYieldsEmptyArray) {
    EXPECT_EQ(EngineGrammarV1::translatePipeline(nlohmann::json::object(), Mode::Server), nlohmann::json::array());
}

TEST(EngineGrammarV1_test, AliasTableIsNotEmpty) {
    EXPECT_FALSE(EngineGrammarV1::aliases(Mode::Server).empty());
}

TEST(EngineGrammarV1_test, SequenceAliasesAreRoleSpecific) {
    EXPECT_EQ(EngineGrammarV1::translateSource("recvseq", Mode::Server), "sequence");
    EXPECT_EQ(EngineGrammarV1::translateSource("sendseq", Mode::Server), "n/a.sendseq");
    EXPECT_EQ(EngineGrammarV1::translateSource("sendseq", Mode::ClientRequest), "sequence");
    EXPECT_EQ(EngineGrammarV1::translateSource("sendseq", Mode::ClientResponse), "sequence");
    EXPECT_EQ(EngineGrammarV1::translateSource("recvseq", Mode::ClientRequest), "n/a.recvseq");
}

TEST(EngineGrammarV1_test, ClientRequestSources) {
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri", Mode::ClientRequest), "meta./uri");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.header.x", Mode::ClientRequest), "input./headerMap/x");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.headers", Mode::ClientRequest), "input./headers");
    // request.body depends on whether a json body target exists (v1 semantics)
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body./a", Mode::ClientRequest, false), "input./requestBodyString");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body./a", Mode::ClientRequest, true), "output./a");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body", Mode::ClientRequest, true), "output");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.body", Mode::ClientRequest), "n/a.response.body");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri.path", Mode::ClientRequest), "n/a.request.uri.path");
}

TEST(EngineGrammarV1_test, ClientRequestTargets) {
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.body.string", Mode::ClientRequest), "output.string");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.body.json.integer./n", Mode::ClientRequest),
              "output.json.integer./n");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.header.x-id", Mode::ClientRequest), "meta.header.x-id");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.uri", Mode::ClientRequest), "meta.uri");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.method", Mode::ClientRequest), "meta.method");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.delayMs", Mode::ClientRequest), "meta.delayMs");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.timeoutMs", Mode::ClientRequest), "meta.timeoutMs");
    // server-only targets are v1 no-ops on the client side
    EXPECT_EQ(EngineGrammarV1::translateTarget("clientProvision.a.b", Mode::ClientRequest), "n/a.clientProvision.a.b");
    EXPECT_EQ(EngineGrammarV1::translateTarget("serverEvent.requestMethod=GET", Mode::ClientRequest),
              "n/a.serverEvent.requestMethod=GET");
    EXPECT_EQ(EngineGrammarV1::translateTarget("clientEvent.requestMethod=GET", Mode::ClientRequest),
              "clientEvent.requestMethod=GET");
}

TEST(EngineGrammarV1_test, ClientResponseSourcesAndTargets) {
    EXPECT_EQ(EngineGrammarV1::translateSource("response.body", Mode::ClientResponse), "input./response/body");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.body./a", Mode::ClientResponse), "input./response/body/a");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.header.ct", Mode::ClientResponse),
              "input./response/headerMap/ct");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.headers", Mode::ClientResponse), "input./response/headers");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.statusCode", Mode::ClientResponse),
              "input./response/statusCode");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.uri", Mode::ClientResponse), "input./uri");
    EXPECT_EQ(EngineGrammarV1::translateSource("request.body./a", Mode::ClientResponse), "input./requestBodyString");
    // request.* targets only touch throw-away copies in v1's response phase
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.uri", Mode::ClientResponse), "n/a.request.uri");
    EXPECT_EQ(EngineGrammarV1::translateTarget("var.status", Mode::ClientResponse), "local.status");
}

TEST(EngineGrammarV1_test, ServerNotApplicableExpressions) {
    EXPECT_EQ(EngineGrammarV1::translateSource("response.statusCode", Mode::Server), "n/a.response.statusCode");
    EXPECT_EQ(EngineGrammarV1::translateSource("response.headers", Mode::Server), "n/a.response.headers");
    EXPECT_EQ(EngineGrammarV1::translateTarget("request.uri", Mode::Server), "n/a.request.uri");
    EXPECT_EQ(EngineGrammarV1::translateTarget("clientEvent.requestMethod=GET", Mode::Server),
              "n/a.clientEvent.requestMethod=GET");
}

TEST(EngineGrammarV1_test, NotApplicableIsSkippedByTheEngine) {
    jf::Engine engine;
    std::string err;
    auto pipeline = engine.parse(
                        EngineGrammarV1::translatePipeline(R"([{"source":"sendseq","target":"response.body.string"}])"_json,
                                Mode::Server),
                        err);
    ASSERT_NE(pipeline, nullptr) << err;
    jf::StandaloneContext ctx;
    ctx.outBody.raw = "untouched";
    jf::Ports ports;
    auto r = engine.evaluate(pipeline, ctx, ports, true);
    EXPECT_EQ(ctx.outBody.raw, "untouched");
    EXPECT_FALSE(r.executionTrace[0].applied);
}

////////////////////////////////////////////////////////////////////////////////
// END-TO-END REPLAY OF h2agent v1 ORACLES (ut/model/Transformation/transform.cpp)
// The fixture values below mirror Transform_test: uri, headers, body, sequence
// 74, vault myVault=myVault_value, provision body {"bar":"bar-1","remove-me":0}.
////////////////////////////////////////////////////////////////////////////////

namespace {

class MapStore : public jf::Store {
public:
    std::map<std::string, nlohmann::json> local{}, global{};
    std::optional<nlohmann::json> get(Scope s, const std::string &k, const std::string &ptr) const override {
        const auto &m = s == Scope::Local ? local : global;
        auto it = m.find(k);
        if (it == m.end()) return std::nullopt;
        if (ptr.empty()) return it->second;
        try {
            return it->second.at(nlohmann::json::json_pointer(ptr));
        } catch (...) {
            return std::nullopt;
        }
    }
    void set(Scope s, const std::string &k, const nlohmann::json &v, const std::string &ptr) override {
        auto &m = s == Scope::Local ? local : global;
        // v1 var parity: local values are strings (objects serialized)
        nlohmann::json val = (s == Scope::Local && !v.is_string()) ? nlohmann::json(v.dump()) : v;
        if (ptr.empty())
            m[k] = val;
        else
            m[k][nlohmann::json::json_pointer(ptr)] = val;
    }
    void erase(Scope s, const std::string &k) override {
        (s == Scope::Local ? local : global).erase(k);
    }
};

struct Replay {
    MapStore store;
    jf::StandaloneContext ctx;
    jf::Ports ports;
    Replay() {
        store.global["myVault"] = "myVault_value";
        ports.store = &store;
        ctx.input = R"({
          "uri":"/app/v1/foo/bar/1?name=test","uriPath":"/app/v1/foo/bar/1",
          "uriParams":{"name":"test"},
          "body":{"foo":1,"node1":{"node2":"value-of-node1-node2","delaymilliseconds":25}},
          "headerMap":{"x-version":"1.0.0","content-type":"application/json"},
          "headers":[{"name":"content-type","value":"application/json"},{"name":"x-version","value":"1.0.0"}]
        })"_json;
        ctx.output = R"({"bar":"bar-1","remove-me":0})"_json;
        ctx.outBody.raw = ctx.output.dump();
        ctx.sequence = 74;
        ctx.inState = "initial";
    }
    // run one v1 item; returns the response body as h2agent would produce it
    std::string run(const nlohmann::json &v1Item) {
        jf::Engine engine;
        std::string err;
        auto pipeline =
            engine.parse(EngineGrammarV1::translatePipeline(nlohmann::json::array({v1Item}), Mode::Server), err);
        EXPECT_NE(pipeline, nullptr) << err;
        engine.evaluate(pipeline, ctx, ports);
        const std::string t = v1Item.value("target", "");
        return t.rfind("response.body.json.", 0) == 0 ? ctx.output.dump() : ctx.outBody.raw;
    }
};

}  // namespace

TEST(EngineGrammarV1_replay, SourceRequestUri) {
    Replay r;
    EXPECT_EQ(r.run(R"({"source":"request.uri","target":"response.body.string"})"_json), "/app/v1/foo/bar/1?name=test");
}

TEST(EngineGrammarV1_replay, SourceRequestUriPath) {
    Replay r;
    EXPECT_EQ(r.run(R"({"source":"request.uri.path","target":"response.body.string"})"_json), "/app/v1/foo/bar/1");
}

TEST(EngineGrammarV1_replay, SourceRequestUriParamAndUnknown) {
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"request.uri.param.name","target":"response.body.string"})"_json), "test");
    }
    {
        Replay r;  // unknown param: rule skipped, provision body kept
        EXPECT_EQ(r.run(R"({"source":"request.uri.param.missing","target":"response.body.string"})"_json),
                  "{\"bar\":\"bar-1\",\"remove-me\":0}");
    }
}

TEST(EngineGrammarV1_replay, SourceRequestBodyWholeAndPath) {
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"request.body","target":"response.body.string"})"_json),
                  "{\"foo\":1,\"node1\":{\"delaymilliseconds\":25,\"node2\":\"value-of-node1-node2\"}}");
    }
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"request.body./foo","target":"response.body.string"})"_json), "1");
    }
}

TEST(EngineGrammarV1_replay, SourceRequestHeader) {
    Replay r;
    EXPECT_EQ(r.run(R"({"source":"request.header.x-version","target":"response.body.string"})"_json), "1.0.0");
}

TEST(EngineGrammarV1_replay, SourceRecvseqVaultValueInState) {
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"recvseq","target":"response.body.string"})"_json), "74");
    }
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"vault.myVault","target":"response.body.string"})"_json), "myVault_value");
    }
    {
        Replay r;
        EXPECT_EQ(r.run(R"({"source":"inState","target":"response.body.string"})"_json), "initial");
    }
}

TEST(EngineGrammarV1_replay, TargetResponseBodyJsonInteger) {
    Replay r;
    EXPECT_EQ(r.run(R"({"source":"request.body./foo","target":"response.body.json.integer./foo"})"_json),
              "{\"bar\":\"bar-1\",\"foo\":1,\"remove-me\":0}");
}
