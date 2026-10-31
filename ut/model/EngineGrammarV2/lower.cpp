// EngineGrammarV2: grammar v2 rules lowered to the engine representation, per
// provision context (server, client request, client response).

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include <Engine.hpp>
#include <EngineGrammarV2.hpp>

using h2agent::model::EngineGrammarV2;
using Mode = h2agent::model::EngineGrammarV2::Mode;

namespace {

/** Lowers one rule; returns the engine rule dump or "error: <reason>". */
std::string lowered(const std::string &v2Rule, Mode mode, bool usesJsonBodyTarget = false) {
    std::string error;
    const nlohmann::json out = EngineGrammarV2::lowerPipeline(nlohmann::json::array({nlohmann::json::parse(v2Rule)}), mode,
                               usesJsonBodyTarget, error);
    if (out.is_null()) return "error: " + error;
    return out[0].dump();
}

std::string engineRule(const std::string &source, const std::string &target, const std::string &filter = "") {
    nlohmann::json r = {{"source", source}, {"target", target}};
    if (!filter.empty()) r["filter"] = nlohmann::json::parse(filter);
    return r.dump();
}

}  // namespace

TEST(EngineGrammarV2_test, ServerSourcesAndTargets) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"({"from":{"source":"requestUri"},"to":{"target":"responseBodyText"}})", engineRule("input./uri", "output.string")},
        {R"({"from":{"source":"requestUriPath"},"to":{"target":"responseBodyHex"}})", engineRule("input./uriPath", "output.hexstring")},
        {R"({"from":{"source":"requestUriParam","name":"a/b"},"to":{"target":"var","name":"p"}})", engineRule("input./uriParams/a~1b", "local.p")},
        {R"({"from":{"source":"requestBody"},"filter":[{"op":"cast","type":"object"}],"to":{"target":"responseBody"}})", engineRule("input./body", "output.json.object")},
        {R"({"from":{"source":"requestBody","path":"/a/b"},"filter":[{"op":"cast","type":"json"}],"to":{"target":"responseBody","path":"/x"}})", engineRule("input./body/a/b", "output.json.jsonstring./x")},
        {R"({"from":{"source":"literal","value":"hi"},"to":{"target":"responseBody","path":"/greeting"}})", engineRule("value.hi", "output.json./greeting")},
        {R"({"from":{"source":"responseBody","path":"/a"},"to":{"target":"vault","key":"K","path":"/b"}})", engineRule("output./a", "global.K./b")},
        {R"({"from":{"source":"var","name":"doc","path":"/a"},"to":{"target":"var","name":"copy","path":"/b/c"}})", engineRule("local.doc./a", "local.copy./b/c")},
        {R"({"from":{"source":"var","name":"uri_parts.1"},"to":{"target":"var","name":"v"}})", engineRule("local.uri_parts.1", "local.v")},
        {R"({"from":{"source":"requestHeader","name":"x-id"},"to":{"target":"responseHeader","name":"x-id"}})", engineRule("input./headerMap/x-id", "meta.header.x-id")},
        {R"({"from":{"source":"requestHeaders"},"filter":[{"op":"cast","type":"object"}],"to":{"target":"vault","key":"H"}})", engineRule("input./headers", "global.H.json.object")},
        {R"({"from":{"source":"recvseq"},"to":{"target":"responseStatusCode"}})", engineRule("sequence", "meta.statusCode")},
        {R"({"from":{"source":"sendseq"},"to":{"target":"responseDelayMs"}})", engineRule("n/a.sendseq", "meta.delayMs")},
        {R"({"from":{"source":"literal","value":"s"},"to":{"target":"outState","method":"GET","uri":"/x"}})", engineRule("value.s", "outState.GET./x")},
        {R"({"from":{"source":"literal","value":"1"},"to":{"target":"clientProvision","id":"flow","inState":"initial"}})", engineRule("value.1", "clientProvision.flow.initial")},
        {R"({"from":{"source":"erase"},"to":{"target":"serverEvent","requestMethod":"GET","requestUri":"/a?b=1&c=2","eventNumber":-1}})", engineRule("eraser", "serverEvent.eventNumber=-1&requestMethod=GET&requestUri=/a?b=1%26c=2")},
        {R"({"from":{"source":"literal","value":"x"},"to":{"target":"udpSocket","socket":"/tmp/s","delayMs":"@{d}"}})", engineRule("value.x", "udpSocket./tmp/s|@{d}")},
        {R"({"from":{"source":"literal","value":"1"},"to":{"target":"stop"}})", engineRule("value.1", "break")},
        {R"({"from":{"source":"literal","value":"1"},"to":{"target":"clientEvent","clientEndpointId":"e"}})", engineRule("value.1", "n/a.clientEvent")},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(lowered(row.first, Mode::Server), row.second);
    }
}

TEST(EngineGrammarV2_test, CommonSources) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"({"source":"var","name":"caps.1"})", "local.caps.1"},
        {R"({"source":"vault","key":"K","path":"/a"})", "global.K./a"},
        {R"({"source":"math","expression":"1+@{x}"})", "math.1+@{x}"},
        {R"({"source":"random","min":-3,"max":8})", "random.-3.8"},
        {R"({"source":"randomset","values":["a","","b"]})", "randomset.a||b"},
        {R"({"source":"timestamp","unit":"ms"})", "timestamp.ms"},
        {R"({"source":"strftime","fmt":"%Y"})", "strftime.%Y"},
        {R"({"source":"inState"})", "inState"},
        {R"({"source":"txtFile","file":"/tmp/a"})", "txtFile./tmp/a"},
        {R"({"source":"command","command":"echo"})", "command.echo"},
        {   R"({"source":"clientEvent","clientEndpointId":"e","sendseq":"@{seq}","eventPath":"/responseBody"})",
            "clientEvent.clientEndpointId=e&eventPath=/responseBody&sendseq=@{seq}"
        },
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        const std::string rule = R"({"from":)" + row.first + R"(,"to":{"target":"var","name":"v"}})";
        EXPECT_EQ(lowered(rule, Mode::Server), engineRule(row.second, "local.v"));
    }
}

TEST(EngineGrammarV2_test, ClientRequestAndResponseContexts) {
    // request: requestBody reads the body being built only when a json body target exists (v1 parity)
    EXPECT_EQ(lowered(R"({"from":{"source":"requestBody","path":"/a"},"to":{"target":"var","name":"v"}})", Mode::ClientRequest, true),
              engineRule("output./a", "local.v"));
    EXPECT_EQ(lowered(R"({"from":{"source":"requestBody","path":"/a"},"to":{"target":"var","name":"v"}})", Mode::ClientRequest, false),
              engineRule("input./requestBodyString", "local.v"));
    EXPECT_EQ(lowered(R"({"from":{"source":"requestUri"},"to":{"target":"requestUri"}})", Mode::ClientRequest), engineRule("meta./uri", "meta.uri"));
    EXPECT_EQ(lowered(R"({"from":{"source":"sendseq"},"filter":[{"op":"cast","type":"unsigned"}],"to":{"target":"requestBody","path":"/seq"}})", Mode::ClientRequest),
              engineRule("sequence", "output.json.unsigned./seq"));
    EXPECT_EQ(lowered(R"({"from":{"source":"literal","value":"PUT"},"to":{"target":"requestMethod"}})", Mode::ClientRequest), engineRule("value.PUT", "meta.method"));
    EXPECT_EQ(lowered(R"({"from":{"source":"literal","value":"5"},"to":{"target":"requestTimeoutMs"}})", Mode::ClientRequest), engineRule("value.5", "meta.timeoutMs"));
    EXPECT_EQ(lowered(R"({"from":{"source":"literal","value":"a"},"to":{"target":"requestHeader","name":"h"}})", Mode::ClientRequest), engineRule("value.a", "meta.header.h"));
    EXPECT_EQ(lowered(R"({"from":{"source":"responseStatusCode"},"to":{"target":"var","name":"c"}})", Mode::ClientRequest), engineRule("n/a.responseStatusCode", "local.c"));
    // response:
    EXPECT_EQ(lowered(R"({"from":{"source":"responseBody","path":"/t"},"to":{"target":"var","name":"t"}})", Mode::ClientResponse),
              engineRule("input./response/body/t", "local.t"));
    EXPECT_EQ(lowered(R"({"from":{"source":"responseHeader","name":"location"},"to":{"target":"outState"}})", Mode::ClientResponse),
              engineRule("input./response/headerMap/location", "outState"));
    EXPECT_EQ(lowered(R"({"from":{"source":"responseStatusCode"},"to":{"target":"stop"}})", Mode::ClientResponse),
              engineRule("input./response/statusCode", "break"));
    EXPECT_EQ(lowered(R"({"from":{"source":"requestBody"},"to":{"target":"var","name":"b"}})", Mode::ClientResponse),
              engineRule("input./requestBodyString", "local.b"));
    EXPECT_EQ(lowered(R"({"from":{"source":"literal","value":"x"},"to":{"target":"requestUri"}})", Mode::ClientResponse), engineRule("value.x", "n/a.requestUri"));
}

TEST(EngineGrammarV2_test, FiltersUseTheEngineShape) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"({"op":"size"})", R"("Size")"},
        {R"({"op":"regexReplace","rgx":"a","fmt":"b"})", R"({"RegexReplace":{"fmt":"b","rgx":"a"}})"},
        {R"j({"op":"regexCapture","rgx":"(a)"})j", R"j({"RegexCapture":"(a)"})j"},
        {R"({"op":"conditionVar","name":"c"})", R"({"ConditionVar":"c"})"},
        {R"({"op":"conditionVar","name":"c","not":true})", R"({"ConditionVar":"!c"})"},
        {R"({"op":"sum","value":2.5})", R"({"Sum":2.5})"},
        {R"({"op":"jsonConstraint","value":{"a":1}})", R"({"JsonConstraint":{"a":1}})"},
        {R"({"op":"schemaId","id":"s"})", R"({"SchemaId":"s"})"},
        {R"({"op":"split","size":2})", R"({"Split":{"size":2}})"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        const std::string rule = R"({"from":{"source":"literal","value":"x"},"filter":[)" + row.first + R"(],"to":{"target":"var","name":"v"}})";
        EXPECT_EQ(lowered(rule, Mode::Server), engineRule("value.x", "local.v", row.second));
    }
    // onFilterFail is lowered recursively:
    EXPECT_EQ(lowered(R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"equalTo","value":"y"}],"to":{"target":"var","name":"v"},
                        "onFilterFail":[{"from":{"source":"literal","value":"1"},"to":{"target":"stop"}}]})", Mode::Server),
              R"({"filter":{"EqualTo":"y"},"onFilterFail":[{"source":"value.1","target":"break"}],"source":"value.x","target":"local.v"})");
}

TEST(EngineGrammarV2_test, FilterChainsAndCastsAreLowered) {
    const std::vector<std::pair<std::string, std::string>> table = {
        // chain, in list order:
        {   R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"append","value":"a"},{"op":"prepend","value":"b"}],"to":{"target":"var","name":"v"}})",
            engineRule("value.x", "local.v", R"([{"Append":"a"},{"Prepend":"b"}])")
        },
        // a cast which is not the last filter runs as a chain filter:
        {   R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"cast","type":"integer"},{"op":"size"}],"to":{"target":"responseBody"}})",
            engineRule("value.x", "output.json", R"([{"Cast":"integer"},"Size"])")
        },
        // a final cast on a typed target is the target typing (filters before it are kept):
        {   R"({"from":{"source":"literal","value":"7"},"filter":[{"op":"sum","value":1},{"op":"cast","type":"integer"}],"to":{"target":"responseBody","path":"/n"}})",
            engineRule("value.7", "output.json.integer./n", R"({"Sum":1})")
        },
        // a final cast on any other target runs as a filter:
        {   R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"cast","type":"integer"}],"to":{"target":"var","name":"v"}})",
            engineRule("value.x", "local.v", R"({"Cast":"integer"})")
        },
        {   R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"cast","type":"integer"}],"to":{"target":"vault","key":"K"}})",
            engineRule("value.x", "global.K", R"({"Cast":"integer"})")
        },
        {   R"({"from":{"source":"literal","value":"x"},"filter":[{"op":"cast","type":"json"}],"to":{"target":"responseHeader","name":"h"}})",
            engineRule("value.x", "meta.header.h", R"({"Cast":"json"})")
        },
        // conditional erase (conditions kept, the final cast on a body target types it):
        {   R"({"from":{"source":"erase"},"filter":[{"op":"conditionVar","name":"c"}],"to":{"target":"var","name":"v"}})",
            engineRule("eraser", "local.v", R"({"ConditionVar":"c"})")
        },
        {   R"({"from":{"source":"erase"},"filter":[{"op":"conditionVar","name":"c"},{"op":"conditionVar","name":"d","not":true},{"op":"cast","type":"object"}],"to":{"target":"responseBody","path":"/a"}})",
            engineRule("eraser", "output.json.object./a", R"([{"ConditionVar":"c"},{"ConditionVar":"!d"}])")
        },
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(lowered(row.first, Mode::Server), row.second);
    }
}

TEST(EngineGrammarV2_test, FeaturesNotSupportedYetAreRejected) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"({"from":{"source":"randomset","values":["a|b"]},"to":{"target":"var","name":"v"}})", "randomset value with '|'"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(lowered(row.first, Mode::Server), "error: " + row.second + " is not supported yet");
    }
}

TEST(EngineGrammarV2_test, SummaryAndEngineParsing) {
    const nlohmann::json rules = nlohmann::json::parse(R"([
      {"from":{"source":"requestBody","path":"/a"},"to":{"target":"responseBody","path":"/a"}},
      {"from":{"source":"serverEvent","requestMethod":"GET","requestUri":"/x"},"to":{"target":"var","name":"e"}},
      {"from":{"source":"literal","value":"1"},"to":{"target":"stop"}}
    ])");
    const auto s = EngineGrammarV2::summarize(rules, Mode::Server);
    EXPECT_TRUE(s.usesRequestBody);
    EXPECT_TRUE(s.usesJsonBodyTarget);
    EXPECT_TRUE(s.needsStorage);
    EXPECT_TRUE(s.endsWithStop);
    const auto c = EngineGrammarV2::summarize(rules, Mode::ClientRequest);
    EXPECT_FALSE(c.usesJsonBodyTarget);  // responseBody is not the client request body

    // The lowered pipeline is accepted by the engine:
    std::string error;
    const nlohmann::json lowered = EngineGrammarV2::lowerPipeline(rules, Mode::Server, false, error);
    ASSERT_FALSE(lowered.is_null()) << error;
    EXPECT_TRUE(h2agent::engine::Engine{}.parse(lowered, error) != nullptr) << error;
}
