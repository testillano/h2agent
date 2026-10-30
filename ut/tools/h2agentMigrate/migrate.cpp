// h2agent-migrate: grammar v1 <-> v2 conversion (docs/developers/GrammarV2.md).
//
// Oracles:
//  - exact v2 output for every v1 kind (the mapping table is the contract the
//    v2 parser will read in phase [0.7]),
//  - v1 -> v2 -> v1 identity on canonical v1,
//  - semantic parity: model::Transformation loads the original v1 rule and the
//    round-tripped one into the same internal representation (asString()).

#include <gtest/gtest.h>

#include <unistd.h> // getpid

#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <GrammarMigrator.hpp>
#include <Transformation.hpp>

using h2agent::migrate::GrammarMigrator;
using h2agent::migrate::Json;
using h2agent::migrate::MigrationError;

namespace {

std::string v1Of(const std::string &v2Dump) {
    return GrammarMigrator::sourceToV1(Json::parse(v2Dump));
}

/** model::Transformation internal representation of a v1 rule ("" if it fails to load). */
std::string loaded(const Json &v1Rule) {
    h2agent::model::Transformation t;
    if (!t.load(nlohmann::json::parse(v1Rule.dump()))) return "";
    return t.asString();
}

/** Checks v1 -> v2 (exact) -> v1 (identity) and Transformation parity for one rule. */
void expectRule(const std::string &v1Dump, const std::string &v2Dump) {
    SCOPED_TRACE(v1Dump);
    const Json v1 = Json::parse(v1Dump);
    const Json v2 = GrammarMigrator::ruleToV2(v1);
    EXPECT_EQ(v2.dump(), Json::parse(v2Dump).dump());
    const Json back = GrammarMigrator::ruleToV1(v2);
    EXPECT_EQ(back.dump(), v1.dump());
    const std::string original = loaded(v1);
    EXPECT_FALSE(original.empty()) << "v1 rule does not load in model::Transformation";
    EXPECT_EQ(loaded(back), original);
}

std::string errorOf(const std::function<void()> &f) {
    try {
        f();
    } catch (const MigrationError &e) {
        return e.what();
    }
    return "<no error>";
}

}  // namespace

/////////////
// SOURCES //
/////////////

TEST(GrammarMigrator_test, SourcesMapToV2Kinds) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {"request.uri", R"("requestUri")"},
        {"request.uri.path", R"("requestUriPath")"},
        {"request.uri.param.id", R"({"requestUriParam":"id"})"},
        {"request.body", R"({"requestBody":""})"},
        {"request.body./a/b", R"({"requestBody":"/a/b"})"},
        {"response.body", R"({"responseBody":""})"},
        {"response.body./a", R"({"responseBody":"/a"})"},
        {"request.header.content-type", R"({"requestHeader":"content-type"})"},
        {"request.headers", R"("requestHeaders")"},
        {"response.header.location", R"({"responseHeader":"location"})"},
        {"response.headers", R"("responseHeaders")"},
        {"response.statusCode", R"("responseStatusCode")"},
        {"eraser", R"("eraser")"},
        {"math.2*sqrt(2)", R"j({"math":"2*sqrt(2)"})j"},
        {"math.", R"({"math":""})"},
        {"random.-3.8", R"({"random":[-3,8]})"},
        {"randomset.a|b||c|", R"({"randomset":["a","b","","c",""]})"},
        {"timestamp.ns", R"({"timestamp":"ns"})"},
        {"strftime.%Y-%m-%d", R"({"strftime":"%Y-%m-%d"})"},
        {"recvseq", R"("recvseq")"},
        {"sendseq", R"("sendseq")"},
        {"var.caps.1", R"({"var":"caps.1"})"},
        {"vault.K", R"({"vault":"K"})"},
        {"vault.K./a/0", R"({"vault":"K","path":"/a/0"})"},
        {"value.", R"({"value":""})"},
        {"value.a.b c", R"({"value":"a.b c"})"},
        {"value.@{v}", R"({"value":"@{v}"})"},
        {"inState", R"("inState")"},
        {   "serverEvent.requestMethod=POST&requestUri=/x&eventNumber=-1&eventPath=/requestBody",
            R"({"serverEvent":"requestMethod=POST&requestUri=/x&eventNumber=-1&eventPath=/requestBody"})"
        },
        {"clientEvent.clientEndpointId=e&requestMethod=GET&requestUri=/y", R"({"clientEvent":"clientEndpointId=e&requestMethod=GET&requestUri=/y"})"},
        {"txtFile./tmp/a.txt", R"({"txtFile":"/tmp/a.txt"})"},
        {"binFile./tmp/a.bin", R"({"binFile":"/tmp/a.bin"})"},
        {"command.echo hi", R"({"command":"echo hi"})"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(GrammarMigrator::sourceToV2(row.first).dump(), Json::parse(row.second).dump());
        EXPECT_EQ(v1Of(row.second), row.first);
    }
}

TEST(GrammarMigrator_test, RandomRangeIsNormalized) {
    // '+' sign is not kept: same range for model::Transformation.
    EXPECT_EQ(GrammarMigrator::sourceToV2("random.-15.+2").dump(), R"({"random":[-15,2]})");
    const Json v1 = Json::parse(R"({"source":"random.-15.+2","target":"var.r"})");
    const Json back = GrammarMigrator::ruleToV1(GrammarMigrator::ruleToV2(v1));
    EXPECT_EQ(back["source"], "random.-15.2");
    EXPECT_EQ(loaded(back), loaded(v1));
}

/////////////
// TARGETS //
/////////////

TEST(GrammarMigrator_test, TargetsMapToV2KindsAndCasts) {
    struct Row {
        std::string v1, v2, as;
    };
    const std::vector<Row> table = {
        {"response.body.string", R"("responseBodyText")", ""},
        {"response.body.hexstring", R"("responseBodyHex")", ""},
        {"response.body.json.string", R"({"responseBody":""})", "string"},
        {"response.body.json.integer./n", R"({"responseBody":"/n"})", "integer"},
        {"response.body.json.unsigned./a/b", R"({"responseBody":"/a/b"})", "unsigned"},
        {"response.body.json.float", R"({"responseBody":""})", "float"},
        {"response.body.json.boolean./ok", R"({"responseBody":"/ok"})", "boolean"},
        {"response.body.json.object", R"({"responseBody":""})", "object"},
        {"response.body.json.jsonstring./doc", R"({"responseBody":"/doc"})", "json"},
        {"request.body.string", R"("requestBodyText")", ""},
        {"request.body.hexstring", R"("requestBodyHex")", ""},
        {"request.body.json.integer./n", R"({"requestBody":"/n"})", "integer"},
        {"request.body.json.jsonstring", R"({"requestBody":""})", "json"},
        {"response.header.location", R"({"responseHeader":"location"})", ""},
        {"response.statusCode", R"("responseStatusCode")", ""},
        {"response.delayMs", R"("responseDelayMs")", ""},
        {"request.header.x-id", R"({"requestHeader":"x-id"})", ""},
        {"request.uri", R"("requestUri")", ""},
        {"request.method", R"("requestMethod")", ""},
        {"request.delayMs", R"("requestDelayMs")", ""},
        {"request.timeoutMs", R"("requestTimeoutMs")", ""},
        {"var.x", R"({"var":"x"})", ""},
        {"vault.K", R"({"vault":"K"})", ""},
        {"vault.K./a", R"({"vault":"K","path":"/a"})", ""},
        {"vault.K.json.object", R"({"vault":"K"})", "object"},
        {"vault.K.json.object./a", R"({"vault":"K","path":"/a"})", "object"},
        {"vault.K.json.jsonstring./a", R"({"vault":"K","path":"/a"})", "json"},
        {"outState", R"("outState")", ""},
        {"outState.DELETE", R"({"foreignOutState":"DELETE"})", ""},
        {"outState.GET./app/v1/items/@{id}", R"({"foreignOutState":"GET","uri":"/app/v1/items/@{id}"})", ""},
        {"txtFile./tmp/@{f}.log", R"({"txtFile":"/tmp/@{f}.log"})", ""},
        {"binFile./tmp/b", R"({"binFile":"/tmp/b"})", ""},
        {"udpSocket./tmp/udp.sock", R"({"udpSocket":"/tmp/udp.sock"})", ""},
        {"udpSocket./tmp/udp.sock|200", R"({"udpSocket":"/tmp/udp.sock","delayMs":200})", ""},
        {"udpSocket./tmp/udp.sock|@{delay}", R"({"udpSocket":"/tmp/udp.sock","delayMs":"@{delay}"})", ""},
        {"udpSocket./tmp/udp.sock|007", R"({"udpSocket":"/tmp/udp.sock","delayMs":"007"})", ""},
        {"serverEvent.requestMethod=GET&requestUri=/x", R"({"serverEvent":"requestMethod=GET&requestUri=/x"})", ""},
        {"clientEvent.clientEndpointId=e", R"({"clientEvent":"clientEndpointId=e"})", ""},
        {"clientProvision.flow.initial", R"({"clientProvision":"flow","inState":"initial"})", ""},
        {"clientProvision.flow.step.2", R"({"clientProvision":"flow","inState":"step.2"})", ""},
        {"break", R"("break")", ""},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.v1);
        std::string as = "<untouched>";
        EXPECT_EQ(GrammarMigrator::targetToV2(row.v1, as).dump(), Json::parse(row.v2).dump());
        EXPECT_EQ(as, row.as);
        EXPECT_EQ(GrammarMigrator::targetToV1(Json::parse(row.v2), row.as), row.v1);
    }
}

///////////
// RULES //
///////////

TEST(GrammarMigrator_test, RulesKeepV1BehaviourWithFilterLists) {
    // Full example of GrammarV2.md:
    expectRule(R"j({"source":"request.uri.path","target":"response.body.json.unsigned./uriId",
                   "filter":{"RegexReplace":{"rgx":"/app/v1/load-test/id-([0-9]+)","fmt":"$1"}}})j",
               R"j({"from":"requestUriPath","to":{"responseBody":"/uriId"},
                   "filter":[{"RegexReplace":{"rgx":"/app/v1/load-test/id-([0-9]+)","fmt":"$1"}},{"as":"unsigned"}]})j");
    // Parameterless filter stays a string:
    expectRule(R"({"source":"request.body./list","target":"var.n","filter":"Size"})",
               R"({"from":{"requestBody":"/list"},"to":{"var":"n"},"filter":["Size"]})");
    // Cast only (no v1 filter):
    expectRule(R"({"source":"value.30","target":"response.body.json.integer./age"})",
               R"({"from":{"value":"30"},"to":{"responseBody":"/age"},"filter":[{"as":"integer"}]})");
    // No filter, no cast: no 'filter' field at all:
    expectRule(R"({"source":"value.done","target":"outState.POST./x"})",
               R"({"from":{"value":"done"},"to":{"foreignOutState":"POST","uri":"/x"}})");
    expectRule(R"({"source":"recvseq","target":"vault.__core:response-delay-ms:@{recvseq}"})",
               R"({"from":"recvseq","to":{"vault":"__core:response-delay-ms:@{recvseq}"}})");
    expectRule(R"({"source":"eraser","target":"serverEvent.requestMethod=GET&requestUri=/x&eventNumber=-1"})",
               R"({"from":"eraser","to":{"serverEvent":"requestMethod=GET&requestUri=/x&eventNumber=-1"}})");
    expectRule(R"({"source":"vault.doc","target":"vault.copy.json.jsonstring./a"})",
               R"({"from":{"vault":"doc"},"to":{"vault":"copy","path":"/a"},"filter":[{"as":"json"}]})");
    expectRule(R"({"source":"value.1","target":"udpSocket./tmp/udp.sock|200","filter":{"Append":"x"}})",
               R"({"from":{"value":"1"},"to":{"udpSocket":"/tmp/udp.sock","delayMs":200},"filter":[{"Append":"x"}]})");
}

TEST(GrammarMigrator_test, OnFilterFailIsConvertedRecursively) {
    expectRule(R"({"source":"request.header.x-version","target":"var.v","filter":{"EqualTo":"2"},
                   "onFilterFail":[{"source":"value.400","target":"response.statusCode"},
                                   {"source":"value.bad","target":"var.e","filter":{"Prepend":"x"},
                                    "onFilterFail":[{"source":"value.1","target":"break"}]}]})",
               R"({"from":{"requestHeader":"x-version"},"to":{"var":"v"},"filter":[{"EqualTo":"2"}],
                   "onFilterFail":[{"from":{"value":"400"},"to":"responseStatusCode"},
                                   {"from":{"value":"bad"},"to":{"var":"e"},"filter":[{"Prepend":"x"}],
                                    "onFilterFail":[{"from":{"value":"1"},"to":"break"}]}]})");
}

////////////////
// PROVISIONS //
////////////////

TEST(GrammarMigrator_test, ProvisionsKeepEveryOtherFieldInOrder) {
    const Json server = Json::parse(R"([
      {"requestMethod":"GET","requestUri":"/a","responseCode":200,"transform":[{"source":"recvseq","target":"var.s"}],"responseBody":{"z":1,"a":2}},
      {"requestMethod":"POST","requestUri":"/b","responseCode":201}
    ])");
    const Json v2 = GrammarMigrator::provisionsToV2(server);
    EXPECT_EQ(v2.dump(), R"([{"requestMethod":"GET","requestUri":"/a","responseCode":200,"transform":[{"from":"recvseq","to":{"var":"s"}}],"responseBody":{"z":1,"a":2}},{"requestMethod":"POST","requestUri":"/b","responseCode":201}])");
    EXPECT_EQ(GrammarMigrator::provisionsToV1(v2).dump(), server.dump());

    const Json client = Json::parse(R"({"id":"flow","endpoint":"e","requestMethod":"POST",
      "transform":[{"source":"sendseq","target":"request.body.json.unsigned./seq"}],
      "onResponseTransform":[{"source":"response.statusCode","target":"var.code"}]})");
    const Json clientV2 = GrammarMigrator::provisionsToV2(client);
    EXPECT_EQ(clientV2["transform"][0].dump(), R"({"from":"sendseq","to":{"requestBody":"/seq"},"filter":[{"as":"unsigned"}]})");
    EXPECT_EQ(clientV2["onResponseTransform"][0].dump(), R"({"from":"responseStatusCode","to":{"var":"code"}})");
    EXPECT_EQ(GrammarMigrator::provisionsToV1(clientV2).dump(), client.dump());
}

TEST(GrammarMigrator_test, DetectsGrammar) {
    using G = GrammarMigrator::Grammar;
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"requestMethod":"GET"})")), G::None);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"([{"transform":[{"source":"recvseq","target":"var.s"}]}])")), G::V1);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"transform":[{"from":"recvseq","to":{"var":"s"}}]})")), G::V2);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"transform":[{"from":"recvseq","to":{"var":"s"},
        "onFilterFail":[{"source":"value.1","target":"break"}]}]})")), G::Mixed);
}

////////////
// ERRORS //
////////////

TEST(GrammarMigrator_test, InvalidV1IsRejectedWithLocation) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"([{"transform":[{"source":"foo.bar","target":"var.x"}]}])", "/0/transform/0/source: cannot identify v1 source 'foo.bar'"},
        {R"({"transform":[{"source":"recvseq","target":"response.body.json.int"}]})", "/transform/0/target: unknown json type"},
        {R"({"transform":[{"source":"vault.a.b","target":"var.x"}]})", "/transform/0/source: vault node path"},
        {R"({"transform":[{"source":"recvseq","target":"vault.a.json.integer"}]})", "/transform/0/target: v1 target 'vault."},
        {R"({"transform":[{"source":"timestamp.h","target":"var.x"}]})", "timestamp unit"},
        {R"({"transform":[{"source":"random.1.x","target":"var.x"}]})", "random.<min>.<max>"},
        {R"({"transform":[{"source":"random.1.99999999999","target":"var.x"}]})", "out of integer limits"},
        {R"({"transform":[{"source":"var.","target":"var.x"}]})", "missing argument"},
        {R"({"transform":[{"source":"recvseq","target":"clientProvision.flow"}]})", "clientProvision.<id>.<inState>"},
        {R"({"transform":[{"source":"recvseq","target":"outState.PATCH"}]})", "outState.<POST|GET|PUT|DELETE|HEAD>"},
        {R"({"transform":[{"source":"recvseq","target":"udpSocket.|1"}]})", "missing socket path"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":"Trim"}]})", "/transform/0/filter: unknown filter 'Trim'"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":{"Append":"a","Prepend":"b"}}]})", "one key"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","extra":1}]})", "unexpected field 'extra'"},
        {R"({"transform":[{"source":"recvseq"}]})", "/transform/0/target: missing"},
        {R"({"transform":[{"from":"recvseq","to":{"var":"s"}}]})", "/transform/0: rule is already v2"},
        {R"({"transform":{"source":"recvseq","target":"var.s"}})", "/transform: must be an array"},
        {R"([1])", "/0: a provision must be an object"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        const std::string error = errorOf([&] { GrammarMigrator::provisionsToV2(Json::parse(row.first)); });
        EXPECT_NE(error.find(row.second), std::string::npos) << error;
    }
}

TEST(GrammarMigrator_test, V2WithoutV1EquivalentIsRejected) {
    const std::vector<std::pair<std::string, std::string>> table = {
        // one valid form per kind:
        {R"({"from":{"requestHeaders":true},"to":{"var":"x"}})", "/from: unknown kind"},
        {R"({"from":"vault","to":{"var":"x"}})", "/from: kind 'vault' requires an argument"},
        {R"({"from":"recvseq","to":{"var":"x"},"filter":[{"Size":true}]})", "'Size' takes no arguments"},
        {R"({"from":"recvseq","to":"var"})", "/to: kind 'var' requires an argument"},
        {R"({"from":"recvseq","to":"nope"})", "unknown target kind 'nope'"},
        // structure:
        {R"({"from":{"vault":"K","path":"/a","extra":1},"to":"break"})", "unknown field 'extra' for kind 'vault' (valid: path)"},
        {R"({"from":{"var":"a","value":"b"},"to":"break"})", "more than one kind"},
        {R"({"from":"recvseq","to":{"udpSocket":"/s","delay":1}})", "unknown field 'delay' for kind 'udpSocket' (valid: delayMs)"},
        {R"({"from":"recvseq","to":{"clientProvision":"flow"}})", "requires 'inState'"},
        {R"({"from":"recvseq","to":{"var":"x"},"filter":{"Append":"a"}})", "/filter: must be a list"},
        // richer than v1:
        {R"({"from":"recvseq","to":{"var":"x"},"filter":[{"Append":"a"},{"Prepend":"b"}]})", "several filters is not expressible"},
        {R"({"from":"recvseq","to":{"var":"x"},"filter":[{"as":"integer"},{"Append":"a"}]})", "as the last filter"},
        {R"({"from":"recvseq","to":{"responseBody":"/a"}})", "needs a final {\"as\": <type>}"},
        {R"({"from":"recvseq","to":{"var":"x"},"filter":[{"as":"integer"}]})", "{\"as\": \"integer\"} on target 'var'"},
        {R"({"from":"recvseq","to":{"vault":"K"},"filter":[{"as":"integer"}]})", "on target 'vault'"},
        {R"({"from":"recvseq","to":{"responseBody":""},"filter":[{"as":"date"}]})", "unknown cast"},
        {R"({"from":{"randomset":["a|b"]},"to":"break"})", "not expressible in v1"},
        {R"({"from":{"vault":"a.b"},"to":"break"})", "contain no dots"},
        {R"({"from":{"random":[1]},"to":"break"})", "'random' must be"},
        {R"({"source":"recvseq","target":"var.x"})", "rule is already v1"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        const std::string error = errorOf([&] { GrammarMigrator::ruleToV1(Json::parse(row.first), "/r"); });
        EXPECT_NE(error.find(row.second), std::string::npos) << error;
    }
}

//////////
// SCAN //
//////////

TEST(GrammarMigrator_test, ScanReportsV1Leftovers) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("h2agent-migrate-scan-" + std::to_string(::getpid()));
    fs::create_directories(dir / "sub");
    fs::create_directories(dir / ".git");
    const auto write = [](const fs::path &p, const std::string &content) {
        std::ofstream(p) << content;
    };

    write(dir / "test.sh", "curl http://h:8074/admin/v1/server-provision -d @p.json\necho ok\n");
    write(dir / "sub" / "provision.json", R"({"transform":[{"source":"recvseq","target":"var.s"}]})");
    write(dir / "sub" / "provision.v2.json", R"({"transform":[{"from":"recvseq","to":{"var":"s"}}]})");
    write(dir / "sub" / "conftest.py", "RULE = '{\"source\": \"request.uri\", \"target\": \"var.u\"}'\n");
    write(dir / ".git" / "config", "/admin/v1\n");
    write(dir / "binary.dat", std::string("/admin/v1\0", 10));

    const auto hits = GrammarMigrator::scan(dir.string());
    std::vector<std::string> found;
    for (const auto &h : hits) found.push_back(fs::path(h.file).filename().string() + ":" + std::to_string(h.line) + ":" + h.kind);
    EXPECT_EQ(found, (std::vector<std::string> {"conftest.py:1:v1-rule-literal", "provision.json:0:v1-provision", "test.sh:1:admin-v1-path"}));

    EXPECT_TRUE(GrammarMigrator::scan((dir / "sub" / "provision.v2.json").string()).empty());
    EXPECT_NE(errorOf([&] { GrammarMigrator::scan((dir / "missing").string()); }).find("not a file or directory"), std::string::npos);
    fs::remove_all(dir);
}
