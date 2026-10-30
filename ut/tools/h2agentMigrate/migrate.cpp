// h2agent-migrate: grammar v1 <-> v2 conversion (docs/developers/GrammarV2.md).
//
// Oracles:
//  - exact v2 output for every v1 kind (the mapping table is the contract the
//    v2 parser reads),
//  - v1 -> v2 -> v1 identity on canonical v1.
// Behaviour parity of migrated provisions is covered by the unit and component
// tests, which run migrated provisions (the v1 runtime no longer exists).

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

using h2agent::migrate::GrammarMigrator;
using h2agent::migrate::Json;
using h2agent::migrate::MigrationError;

namespace {

std::string v1Of(const std::string &v2Dump) {
    return GrammarMigrator::sourceToV1(Json::parse(v2Dump));
}

/** Checks v1 -> v2 (exact) -> v1 (identity) for one rule. */
void expectRule(const std::string &v1Dump, const std::string &v2Dump) {
    SCOPED_TRACE(v1Dump);
    const Json v1 = Json::parse(v1Dump);
    const Json v2 = GrammarMigrator::ruleToV2(v1);
    EXPECT_EQ(v2.dump(), Json::parse(v2Dump).dump());
    const Json back = GrammarMigrator::ruleToV1(v2);
    EXPECT_EQ(back.dump(), v1.dump());
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
        {"request.uri", R"({"source":"requestUri"})"},
        {"request.uri.path", R"({"source":"requestUriPath"})"},
        {"request.uri.param.id", R"({"source":"requestUriParam","name":"id"})"},
        {"request.body", R"({"source":"requestBody"})"},
        {"request.body./a/b", R"({"source":"requestBody","path":"/a/b"})"},
        {"response.body", R"({"source":"responseBody"})"},
        {"response.body./a", R"({"source":"responseBody","path":"/a"})"},
        {"request.header.content-type", R"({"source":"requestHeader","name":"content-type"})"},
        {"request.headers", R"({"source":"requestHeaders"})"},
        {"response.header.location", R"({"source":"responseHeader","name":"location"})"},
        {"response.headers", R"({"source":"responseHeaders"})"},
        {"response.statusCode", R"({"source":"responseStatusCode"})"},
        {"eraser", R"({"source":"erase"})"},
        {"math.2*sqrt(2)", R"j({"source":"math","expression":"2*sqrt(2)"})j"},
        {"math.", R"({"source":"math","expression":""})"},
        {"random.-3.8", R"({"source":"random","min":-3,"max":8})"},
        {"randomset.a|b||c|", R"({"source":"randomset","values":["a","b","","c",""]})"},
        {"timestamp.ns", R"({"source":"timestamp","unit":"ns"})"},
        {"strftime.%Y-%m-%d", R"({"source":"strftime","fmt":"%Y-%m-%d"})"},
        {"recvseq", R"({"source":"recvseq"})"},
        {"sendseq", R"({"source":"sendseq"})"},
        {"var.caps.1", R"({"source":"var","name":"caps.1"})"},
        {"vault.K", R"({"source":"vault","key":"K"})"},
        {"vault.K./a/0", R"({"source":"vault","key":"K","path":"/a/0"})"},
        {"value.", R"({"source":"literal","value":""})"},
        {"value.a.b c", R"({"source":"literal","value":"a.b c"})"},
        {"value.@{v}", R"({"source":"literal","value":"@{v}"})"},
        {"inState", R"({"source":"inState"})"},
        {   "serverEvent.requestMethod=POST&requestUri=/x&eventNumber=-1&eventPath=/requestBody",
            R"({"source":"serverEvent","requestMethod":"POST","requestUri":"/x","eventNumber":-1,"eventPath":"/requestBody"})"
        },
        {   "serverEvent.requestMethod=GET&requestUriStartsWith=/items/&eventNumber=@{n}",
            R"({"source":"serverEvent","requestMethod":"GET","requestUriStartsWith":"/items/","eventNumber":"@{n}"})"
        },
        {"serverEvent.requestMethod=GET&requestUri=/x&recvseq=7", R"({"source":"serverEvent","requestMethod":"GET","requestUri":"/x","recvseq":7})"},
        {   "clientEvent.clientEndpointId=e&requestMethod=GET&requestUri=/y&sendseq=@{seq}&eventPath=/responseBody",
            R"({"source":"clientEvent","clientEndpointId":"e","requestMethod":"GET","requestUri":"/y","sendseq":"@{seq}","eventPath":"/responseBody"})"
        },
        {"txtFile./tmp/a.txt", R"({"source":"txtFile","file":"/tmp/a.txt"})"},
        {"binFile./tmp/a.bin", R"({"source":"binFile","file":"/tmp/a.bin"})"},
        {"command.echo hi", R"({"source":"command","command":"echo hi"})"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(GrammarMigrator::sourceToV2(row.first).dump(), Json::parse(row.second).dump());
        EXPECT_EQ(v1Of(row.second), row.first);
    }
}

TEST(GrammarMigrator_test, EventAddressesAreDecodedOnce) {
    // v1 query values are url-decoded by the runtime: v2 fields hold the plain value.
    EXPECT_EQ(GrammarMigrator::sourceToV2("serverEvent.requestMethod=GET&requestUri=/app/v1/foo%3Fid%3D5%26name%3Dtest&eventNumber=1").dump(),
              R"({"source":"serverEvent","requestMethod":"GET","requestUri":"/app/v1/foo?id=5&name=test","eventNumber":1})");
    EXPECT_EQ(GrammarMigrator::sourceToV2("serverEvent.requestMethod=GET&requestUri=/a+b%2Bc").dump(),
              R"({"source":"serverEvent","requestMethod":"GET","requestUri":"/a b+c"})");
    // Back to v1, only what the v1 parser would misread is encoded:
    EXPECT_EQ(v1Of(R"({"source":"serverEvent","requestMethod":"GET","requestUri":"/app/v1/foo?id=5&name=test"})"),
              "serverEvent.requestMethod=GET&requestUri=/app/v1/foo?id=5%26name=test");
    EXPECT_EQ(v1Of(R"({"source":"serverEvent","requestUri":"/a b+c%"})"), "serverEvent.requestUri=/a b%2Bc%25");
}

TEST(GrammarMigrator_test, RandomRangeIsNormalized) {
    // '+' sign is not kept (same range).
    EXPECT_EQ(GrammarMigrator::sourceToV2("random.-15.+2").dump(), R"({"source":"random","min":-15,"max":2})");
    const Json v1 = Json::parse(R"({"source":"random.-15.+2","target":"var.r"})");
    const Json back = GrammarMigrator::ruleToV1(GrammarMigrator::ruleToV2(v1));
    EXPECT_EQ(back["source"], "random.-15.2");
}

/////////////
// TARGETS //
/////////////

TEST(GrammarMigrator_test, TargetsMapToV2KindsAndCasts) {
    struct Row {
        std::string v1, v2, cast;
    };
    const std::vector<Row> table = {
        {"response.body.string", R"({"target":"responseBodyText"})", ""},
        {"response.body.hexstring", R"({"target":"responseBodyHex"})", ""},
        {"response.body.json.string", R"({"target":"responseBody"})", "string"},
        {"response.body.json.integer./n", R"({"target":"responseBody","path":"/n"})", "integer"},
        {"response.body.json.unsigned./a/b", R"({"target":"responseBody","path":"/a/b"})", "unsigned"},
        {"response.body.json.float", R"({"target":"responseBody"})", "float"},
        {"response.body.json.boolean./ok", R"({"target":"responseBody","path":"/ok"})", "boolean"},
        {"response.body.json.object", R"({"target":"responseBody"})", "object"},
        {"response.body.json.jsonstring./doc", R"({"target":"responseBody","path":"/doc"})", "json"},
        {"request.body.string", R"({"target":"requestBodyText"})", ""},
        {"request.body.hexstring", R"({"target":"requestBodyHex"})", ""},
        {"request.body.json.integer./n", R"({"target":"requestBody","path":"/n"})", "integer"},
        {"request.body.json.jsonstring", R"({"target":"requestBody"})", "json"},
        {"response.header.location", R"({"target":"responseHeader","name":"location"})", ""},
        {"response.statusCode", R"({"target":"responseStatusCode"})", ""},
        {"response.delayMs", R"({"target":"responseDelayMs"})", ""},
        {"request.header.x-id", R"({"target":"requestHeader","name":"x-id"})", ""},
        {"request.uri", R"({"target":"requestUri"})", ""},
        {"request.method", R"({"target":"requestMethod"})", ""},
        {"request.delayMs", R"({"target":"requestDelayMs"})", ""},
        {"request.timeoutMs", R"({"target":"requestTimeoutMs"})", ""},
        {"var.x", R"({"target":"var","name":"x"})", ""},
        {"vault.K", R"({"target":"vault","key":"K"})", ""},
        {"vault.K./a", R"({"target":"vault","key":"K","path":"/a"})", ""},
        {"vault.K.json.object", R"({"target":"vault","key":"K"})", "object"},
        {"vault.K.json.object./a", R"({"target":"vault","key":"K","path":"/a"})", "object"},
        {"vault.K.json.jsonstring./a", R"({"target":"vault","key":"K","path":"/a"})", "json"},
        {"outState", R"({"target":"outState"})", ""},
        {"outState.DELETE", R"({"target":"outState","method":"DELETE"})", ""},
        {"outState.GET./app/v1/items/@{id}", R"({"target":"outState","method":"GET","uri":"/app/v1/items/@{id}"})", ""},
        {"txtFile./tmp/@{f}.log", R"({"target":"txtFile","file":"/tmp/@{f}.log"})", ""},
        {"binFile./tmp/b", R"({"target":"binFile","file":"/tmp/b"})", ""},
        {"udpSocket./tmp/udp.sock", R"({"target":"udpSocket","socket":"/tmp/udp.sock"})", ""},
        {"udpSocket./tmp/udp.sock|200", R"({"target":"udpSocket","socket":"/tmp/udp.sock","delayMs":200})", ""},
        {"udpSocket./tmp/udp.sock|@{delay}", R"({"target":"udpSocket","socket":"/tmp/udp.sock","delayMs":"@{delay}"})", ""},
        {"udpSocket./tmp/udp.sock|007", R"({"target":"udpSocket","socket":"/tmp/udp.sock","delayMs":"007"})", ""},
        {"serverEvent.requestMethod=GET&requestUri=/x&eventNumber=-1", R"({"target":"serverEvent","requestMethod":"GET","requestUri":"/x","eventNumber":-1})", ""},
        {"clientEvent.clientEndpointId=e&sendseq=3", R"({"target":"clientEvent","clientEndpointId":"e","sendseq":3})", ""},
        {"clientProvision.flow.initial", R"({"target":"clientProvision","id":"flow","inState":"initial"})", ""},
        {"clientProvision.flow.step.2", R"({"target":"clientProvision","id":"flow","inState":"step.2"})", ""},
        {"break", R"({"target":"stop"})", ""},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.v1);
        std::string cast = "<untouched>";
        EXPECT_EQ(GrammarMigrator::targetToV2(row.v1, cast).dump(), Json::parse(row.v2).dump());
        EXPECT_EQ(cast, row.cast);
        EXPECT_EQ(GrammarMigrator::targetToV1(Json::parse(row.v2), row.cast), row.v1);
    }
}

/////////////
// FILTERS //
/////////////

TEST(GrammarMigrator_test, FiltersAreFlattenedWithLowerCamelCaseOps) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"("Size")", R"({"op":"size"})"},
        {R"j({"RegexCapture":"(a)(b)"})j", R"j({"op":"regexCapture","rgx":"(a)(b)"})j"},
        {R"j({"RegexReplace":{"rgx":"id-([0-9]+)","fmt":"$1"}})j", R"j({"op":"regexReplace","rgx":"id-([0-9]+)","fmt":"$1"})j"},
        {R"({"RegexKey":"^x"})", R"({"op":"regexKey","rgx":"^x"})"},
        {R"({"Append":"@{s}"})", R"({"op":"append","value":"@{s}"})"},
        {R"({"Prepend":"p"})", R"({"op":"prepend","value":"p"})"},
        {R"({"Sum":-1.5})", R"({"op":"sum","value":-1.5})"},
        {R"({"Multiply":3})", R"({"op":"multiply","value":3})"},
        {R"({"ConditionVar":"done"})", R"({"op":"conditionVar","name":"done"})"},
        {R"({"ConditionVar":"!done"})", R"({"op":"conditionVar","name":"done","not":true})"},
        {R"({"EqualTo":"2"})", R"({"op":"equalTo","value":"2"})"},
        {R"({"DifferentFrom":"200"})", R"({"op":"differentFrom","value":"200"})"},
        {R"({"JsonConstraint":{"a":1}})", R"({"op":"jsonConstraint","value":{"a":1}})"},
        {R"({"JsonConstraint":[{"name":"x"}]})", R"({"op":"jsonConstraint","value":[{"name":"x"}]})"},
        {R"({"SchemaId":"mySchema"})", R"({"op":"schemaId","id":"mySchema"})"},
        {R"({"Split":{}})", R"({"op":"split"})"},
        {R"({"Split":{"size":2,"sep":".","numeric":true}})", R"({"op":"split","size":2,"sep":".","numeric":true})"},
        {R"({"BaseConvert":{"in":10,"out":16,"capital":true}})", R"({"op":"baseConvert","in":10,"out":16,"capital":true})"},
        {R"({"Strptime":{"fmt":"%Y","unit":"ms"}})", R"({"op":"strptime","fmt":"%Y","unit":"ms"})"},
        {R"({"Strftime":{"fmt":"%H"}})", R"({"op":"strftime","fmt":"%H"})"},
    };
    for (const auto &row : table) {
        SCOPED_TRACE(row.first);
        EXPECT_EQ(GrammarMigrator::filterToV2(Json::parse(row.first)).dump(), Json::parse(row.second).dump());
        EXPECT_EQ(GrammarMigrator::filterToV1(Json::parse(row.second)).dump(), Json::parse(row.first).dump());
    }
}

///////////
// RULES //
///////////

TEST(GrammarMigrator_test, RulesKeepV1BehaviourWithFilterLists) {
    // Full example of GrammarV2.md:
    expectRule(R"j({"source":"request.uri.path","target":"response.body.json.unsigned./uriId",
                   "filter":{"RegexReplace":{"rgx":"/app/v1/load-test/id-([0-9]+)","fmt":"$1"}}})j",
               R"j({"from":{"source":"requestUriPath"},
                   "filter":[{"op":"regexReplace","rgx":"/app/v1/load-test/id-([0-9]+)","fmt":"$1"},{"op":"cast","type":"unsigned"}],
                   "to":{"target":"responseBody","path":"/uriId"}})j");
    expectRule(R"({"source":"request.body./list","target":"var.n","filter":"Size"})",
               R"({"from":{"source":"requestBody","path":"/list"},"filter":[{"op":"size"}],"to":{"target":"var","name":"n"}})");
    // Cast only (no v1 filter):
    expectRule(R"({"source":"value.30","target":"response.body.json.integer./age"})",
               R"({"from":{"source":"literal","value":"30"},"filter":[{"op":"cast","type":"integer"}],"to":{"target":"responseBody","path":"/age"}})");
    // A cast to string is redundant on a json body target fed by a text source (dropped):
    expectRule(R"({"source":"value.hello","target":"response.body.json.string./greeting"})",
               R"({"from":{"source":"literal","value":"hello"},"to":{"target":"responseBody","path":"/greeting"}})");
    expectRule(R"({"source":"request.header.x-id","target":"request.body.json.string./id","filter":{"Append":"-1"}})",
               R"({"from":{"source":"requestHeader","name":"x-id"},"filter":[{"op":"append","value":"-1"}],"to":{"target":"requestBody","path":"/id"}})");
    // ... but kept for any other source (it converts json, numbers...):
    expectRule(R"({"source":"request.body./a","target":"response.body.json.string./a"})",
               R"({"from":{"source":"requestBody","path":"/a"},"filter":[{"op":"cast","type":"string"}],"to":{"target":"responseBody","path":"/a"}})");
    // No filter, no cast: no 'filter' field at all:
    expectRule(R"({"source":"value.done","target":"outState.POST./x"})",
               R"({"from":{"source":"literal","value":"done"},"to":{"target":"outState","method":"POST","uri":"/x"}})");
    expectRule(R"({"source":"recvseq","target":"vault.__core:response-delay-ms:@{recvseq}"})",
               R"({"from":{"source":"recvseq"},"to":{"target":"vault","key":"__core:response-delay-ms:@{recvseq}"}})");
    // eraser on a typed body target: the cast keeps the v1 erase flavour (object removes the node):
    expectRule(R"({"source":"eraser","target":"response.body.json.object./a"})",
               R"({"from":{"source":"erase"},"filter":[{"op":"cast","type":"object"}],"to":{"target":"responseBody","path":"/a"}})");
    expectRule(R"({"source":"eraser","target":"serverEvent.requestMethod=GET&requestUri=/x&eventNumber=-1"})",
               R"({"from":{"source":"erase"},"to":{"target":"serverEvent","requestMethod":"GET","requestUri":"/x","eventNumber":-1}})");
    expectRule(R"({"source":"vault.doc","target":"vault.copy.json.jsonstring./a"})",
               R"({"from":{"source":"vault","key":"doc"},"filter":[{"op":"cast","type":"json"}],"to":{"target":"vault","key":"copy","path":"/a"}})");
    expectRule(R"({"source":"value.1","target":"udpSocket./tmp/udp.sock|200","filter":{"Append":"x"}})",
               R"({"from":{"source":"literal","value":"1"},"filter":[{"op":"append","value":"x"}],"to":{"target":"udpSocket","socket":"/tmp/udp.sock","delayMs":200}})");
    expectRule(R"({"source":"value.1","target":"break","filter":{"ConditionVar":"!ok"}})",
               R"({"from":{"source":"literal","value":"1"},"filter":[{"op":"conditionVar","name":"ok","not":true}],"to":{"target":"stop"}})");
}

TEST(GrammarMigrator_test, OnFilterFailIsConvertedRecursively) {
    expectRule(R"({"source":"request.header.x-version","target":"var.v","filter":{"EqualTo":"2"},
                   "onFilterFail":[{"source":"value.400","target":"response.statusCode"},
                                   {"source":"value.bad","target":"var.e","filter":{"Prepend":"x"},
                                    "onFilterFail":[{"source":"value.1","target":"break"}]}]})",
               R"({"from":{"source":"requestHeader","name":"x-version"},"filter":[{"op":"equalTo","value":"2"}],"to":{"target":"var","name":"v"},
                   "onFilterFail":[{"from":{"source":"literal","value":"400"},"to":{"target":"responseStatusCode"}},
                                   {"from":{"source":"literal","value":"bad"},"filter":[{"op":"prepend","value":"x"}],"to":{"target":"var","name":"e"},
                                    "onFilterFail":[{"from":{"source":"literal","value":"1"},"to":{"target":"stop"}}]}]})");
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
    EXPECT_EQ(v2.dump(), R"([{"requestMethod":"GET","requestUri":"/a","responseCode":200,"transform":[{"from":{"source":"recvseq"},"to":{"target":"var","name":"s"}}],"responseBody":{"z":1,"a":2}},{"requestMethod":"POST","requestUri":"/b","responseCode":201}])");
    EXPECT_EQ(GrammarMigrator::provisionsToV1(v2).dump(), server.dump());

    const Json client = Json::parse(R"({"id":"flow","endpoint":"e","requestMethod":"POST",
      "transform":[{"source":"sendseq","target":"request.body.json.unsigned./seq"}],
      "onResponseTransform":[{"source":"response.statusCode","target":"var.code"}]})");
    const Json clientV2 = GrammarMigrator::provisionsToV2(client);
    EXPECT_EQ(clientV2["transform"][0].dump(), R"({"from":{"source":"sendseq"},"filter":[{"op":"cast","type":"unsigned"}],"to":{"target":"requestBody","path":"/seq"}})");
    EXPECT_EQ(clientV2["onResponseTransform"][0].dump(), R"({"from":{"source":"responseStatusCode"},"to":{"target":"var","name":"code"}})");
    EXPECT_EQ(GrammarMigrator::provisionsToV1(clientV2).dump(), client.dump());
}

TEST(GrammarMigrator_test, DetectsGrammar) {
    using G = GrammarMigrator::Grammar;
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"requestMethod":"GET"})")), G::None);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"([{"transform":[{"source":"recvseq","target":"var.s"}]}])")), G::V1);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"transform":[{"from":{"source":"recvseq"},"to":{"target":"var","name":"s"}}]})")), G::V2);
    EXPECT_EQ(GrammarMigrator::detect(Json::parse(R"({"transform":[{"from":{"source":"recvseq"},"to":{"target":"var","name":"s"},
        "onFilterFail":[{"source":"value.1","target":"break"}]}]})")), G::Mixed);
}

////////////
// ERRORS //
////////////

TEST(GrammarMigrator_test, InvalidV1IsRejectedWithLocation) {
    const std::vector<std::pair<std::string, std::string>> table = {
        {R"([{"transform":[{"source":"foo.bar","target":"var.x"}]}])", "/0/transform/0/source: cannot identify v1 source 'foo.bar'"},
        {R"({"transform":[{"source":"recvseq","target":"response.body.json.int"}]})", "/transform/0/target: unknown json type"},
        {R"({"transform":[{"source":"request.body","target":"response.body.json.string.target"}]})", "/transform/0/target: body path must be a json pointer"},
        {R"({"transform":[{"source":"request.body.a","target":"var.x"}]})", "/transform/0/source: body path must be a json pointer"},
        {R"({"transform":[{"source":"vault.a.b","target":"var.x"}]})", "/transform/0/source: vault node path"},
        {R"({"transform":[{"source":"recvseq","target":"vault.a.json.integer"}]})", "/transform/0/target: v1 target 'vault."},
        {R"({"transform":[{"source":"timestamp.h","target":"var.x"}]})", "timestamp unit"},
        {R"({"transform":[{"source":"random.1.x","target":"var.x"}]})", "random.<min>.<max>"},
        {R"({"transform":[{"source":"random.1.99999999999","target":"var.x"}]})", "out of integer limits"},
        {R"({"transform":[{"source":"var.","target":"var.x"}]})", "missing argument"},
        {R"({"transform":[{"source":"recvseq","target":"clientProvision.flow"}]})", "clientProvision.<id>.<inState>"},
        {R"({"transform":[{"source":"recvseq","target":"outState.PATCH"}]})", "outState.<POST|GET|PUT|DELETE|HEAD>"},
        {R"({"transform":[{"source":"recvseq","target":"udpSocket.|1"}]})", "missing socket path"},
        {R"({"transform":[{"source":"serverEvent.requestMethod=GET&foo=1","target":"var.x"}]})", "unknown serverEvent address field 'foo'"},
        {R"({"transform":[{"source":"serverEvent.requestUri=/a&requestUri=/b","target":"var.x"}]})", "repeated serverEvent address field 'requestUri'"},
        {R"({"transform":[{"source":"serverEvent.eventNumber=last","target":"var.x"}]})", "must be a number or a text with variables"},
        {R"({"transform":[{"source":"eraser","target":"serverEvent.requestMethod=GET&eventPath=/a"}]})", "unknown serverEvent address field 'eventPath'"},
        {R"({"transform":[{"source":"eraser","target":"var.x","filter":{"ConditionVar":"c"}}]})", "/transform/0/filter: v1 never applies an 'eraser' rule with a filter"},
        {R"({"transform":[{"source":"value.x","target":"serverEvent.requestMethod=GET"}]})", "/transform/0/source: v1 never applies an event purge target"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":"Trim"}]})", "/transform/0/filter: unknown filter 'Trim'"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":{"Append":"a","Prepend":"b"}}]})", "one key"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":{"Append":1}}]})", "Append argument must be a string"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":{"RegexReplace":{"rgx":"a"}}}]})", "RegexReplace requires 'fmt'"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","filter":{"ConditionVar":"!"}}]})", "ConditionVar needs a variable name"},
        {R"({"transform":[{"source":"recvseq","target":"var.x","extra":1}]})", "unexpected field 'extra'"},
        {R"({"transform":[{"source":"recvseq"}]})", "/transform/0/target: missing"},
        {R"({"transform":[{"from":{"source":"recvseq"},"to":{"target":"var","name":"s"}}]})", "/transform/0: rule is already v2"},
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
        {R"({"from":"recvseq","to":{"target":"var","name":"x"}})", "/r/from: must be an object with a 'source' kind"},
        {R"({"from":{"source":"vault"},"to":{"target":"stop"}})", "source 'vault' requires 'key'"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"var"}})", "target 'var' requires 'name'"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"nope"}})", "unknown target 'nope'"},
        {R"({"from":{"source":"nope"},"to":{"target":"stop"}})", "unknown source 'nope'"},
        {R"({"from":{"source":"recvseq","name":"x"},"to":{"target":"stop"}})", "unknown field 'name' for source 'recvseq' (valid: none)"},
        {R"({"from":{"source":"vault","key":"K","extra":1},"to":{"target":"stop"}})", "unknown field 'extra' for source 'vault' (valid: key, path)"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"udpSocket","socket":"/s","delay":1}})", "unknown field 'delay' for target 'udpSocket' (valid: socket, delayMs)"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"clientProvision","id":"flow"}})", "target 'clientProvision' requires 'inState'"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"outState","uri":"/x"}})", "'uri' requires 'method'"},
        {R"({"from":{"source":"requestBody","path":"a"},"to":{"target":"stop"}})", "'path' must be a json pointer"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"size","x":1}],"to":{"target":"stop"}})", "unknown field 'x' for op 'size' (valid: none)"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"Size"}],"to":{"target":"stop"}})", "unknown filter op 'Size'"},
        {R"({"from":{"source":"recvseq"},"filter":{"op":"size"},"to":{"target":"stop"}})", "/r/filter: must be a non-empty list"},
        {R"({"from":{"source":"serverEvent","requestUri":"/x","eventNumber":"last"},"to":{"target":"stop"}})", "'eventNumber' must be an integer or a text with variables"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"serverEvent","eventPath":"/a"}})", "unknown field 'eventPath' for target 'serverEvent'"},
        // richer than v1:
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"append","value":"a"},{"op":"prepend","value":"b"}],"to":{"target":"stop"}})", "several filters is not expressible"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"cast","type":"integer"},{"op":"append","value":"a"}],"to":{"target":"var","name":"x"}})", "as the last filter"},
        {R"({"from":{"source":"recvseq"},"to":{"target":"responseBody","path":"/a"}})", "needs a final cast filter"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"cast","type":"integer"}],"to":{"target":"var","name":"x"}})", "cast to 'integer' on target 'var'"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"cast","type":"integer"}],"to":{"target":"vault","key":"K"}})", "on target 'vault'"},
        {R"({"from":{"source":"recvseq"},"filter":[{"op":"cast","type":"date"}],"to":{"target":"responseBody"}})", "unknown cast type 'date'"},
        {R"({"from":{"source":"erase"},"filter":[{"op":"conditionVar","name":"c"}],"to":{"target":"vault","key":"K"}})", "a conditional erase is not expressible in v1"},
        {R"({"from":{"source":"randomset","values":["a|b"]},"to":{"target":"stop"}})", "not expressible in v1"},
        {R"({"from":{"source":"vault","key":"a.b"},"to":{"target":"stop"}})", "contain no dots"},
        {R"({"from":{"source":"random","min":1},"to":{"target":"stop"}})", "source 'random' requires 'max'"},
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
    write(dir / "sub" / "provision.v2.json", R"({"transform":[{"from":{"source":"recvseq"},"to":{"target":"var","name":"s"}}]})");
    write(dir / "sub" / "conftest.py", "RULE = '{\"source\": \"request.uri\", \"target\": \"var.u\"}'\n");
    // v2 literals ("source"/"target" discriminators with dotless kind names) are not v1 leftovers:
    write(dir / "sub" / "v2_test.py", "RULE = {\"from\": {\"source\": \"requestBody\"}, \"to\": {\"target\": \"vault\", \"key\": \"k\"}}\n");
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
