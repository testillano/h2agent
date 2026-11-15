#include <Datasets.hpp>

#include <climits>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

using h2agent::model::Dataset;
using h2agent::model::Datasets;

namespace {
Dataset parsed(const std::string &text, bool decode = false) {
    Dataset d;
    std::string error;
    EXPECT_TRUE(Datasets::parse(text, decode, d, error)) << error;
    return d;
}

std::string parseError(const std::string &text, bool decode = false) {
    Dataset d;
    std::string error;
    EXPECT_FALSE(Datasets::parse(text, decode, d, error));
    return error;
}
}  // namespace

TEST(Datasets_test, ParseBlanksCommentsAndHeader) {
    const Dataset d = parsed(
                          "# crew of the ship\n"
                          "\n"
                          "name      role       deck\n"
                          "   # an indented comment\n"
                          "alice     captain    1\n"
                          "bob\tengineer\t\t3   \r\n"
                          "\t\n"
                          "  carol   cook       2\n");
    EXPECT_EQ(d.columns, (std::vector<std::string> {"name", "role", "deck"}));
    EXPECT_EQ(d.rows, 3u);
    EXPECT_EQ(d.row(0), R"({"name":"alice","role":"captain","deck":"1"})"_json);
    EXPECT_EQ(d.row(1), R"({"name":"bob","role":"engineer","deck":"3"})"_json);
    EXPECT_EQ(d.row(2), R"({"name":"carol","role":"cook","deck":"2"})"_json);

    // a header and no rows is a valid (empty) dataset; no header is not
    EXPECT_EQ(parsed("name role\n").rows, 0u);
    EXPECT_EQ(parseError("# only comments\n\n"), "no header line");
    EXPECT_EQ(parseError(""), "no header line");
}

TEST(Datasets_test, ParseErrorsGiveTheLine) {
    EXPECT_EQ(parseError("name role\nalice captain\nbob\n"), "line 3: expected 2 fields, found 1");
    EXPECT_EQ(parseError("# c\nname role\nalice captain extra\n"), "line 3: expected 2 fields, found 3");
    EXPECT_EQ(parseError("name role name\n"), "line 1: repeated column 'name'");
    // without decode, '%' is a plain character
    EXPECT_EQ(parsed("item\n100%\n").row(0)["item"], "100%");
}

TEST(Datasets_test, ParseDecode) {
    const Dataset d = parsed("item           note\n"
                             "cargo%20box    100%25\n"
                             "a+b            %3Bx%3b\n", true);
    EXPECT_EQ(d.row(0), R"({"item":"cargo box","note":"100%"})"_json);
    EXPECT_EQ(d.row(1), R"({"item":"a+b","note":";x;"})"_json);  // '+' kept; any case hex
    EXPECT_EQ(parseError("item\n50%\n", true), "line 2: invalid percent-encoding in '50%'");
    EXPECT_EQ(parseError("item\n%2\n", true), "line 2: invalid percent-encoding in '%2'");
    EXPECT_EQ(parseError("item\n%zz\n", true), "line 2: invalid percent-encoding in '%zz'");
}

TEST(Datasets_test, RowModulo) {
    Datasets store;
    store.set("crew", parsed("name\nalice\nbob\ncarol\n"));
    const auto name = [&](long long i) {
        auto r = store.row("crew", i);
        return r ? (*r)["name"].get<std::string>() : std::string("<none>");
    };
    EXPECT_EQ(name(0), "alice");
    EXPECT_EQ(name(2), "carol");
    EXPECT_EQ(name(3), "alice");               // repeat beyond the end
    EXPECT_EQ(name(1000000000001LL), "carol"); // large: 10^12 + 1 = 3 * 333333333333 + 2
    EXPECT_EQ(name(-1), "carol");              // negative from the end
    EXPECT_EQ(name(-3), "alice");
    EXPECT_EQ(name(-4), "carol");
    EXPECT_EQ(name(LLONG_MIN), name(LLONG_MIN % 3 + 3));

    EXPECT_FALSE(store.row("cargo", 0).has_value());  // unknown
    store.set("empty", parsed("name\n"));
    EXPECT_FALSE(store.row("empty", 0).has_value());  // empty
}

TEST(Datasets_test, SummaryReplaceRemoveClear) {
    Datasets store;
    EXPECT_EQ(store.summary(), nlohmann::json::array());
    EXPECT_FALSE(store.clear());
    store.set("crew", parsed("name role\nalice captain\n"));
    store.set("cargo", parsed("item\nbox\ncrate\n"));
    EXPECT_EQ(store.summary(), R"([{"id":"cargo","columns":["item"],"rows":2},{"id":"crew","columns":["name","role"],"rows":1}])"_json);
    EXPECT_EQ(store.summary("crew").value()["rows"], 1);
    EXPECT_FALSE(store.summary("ghost").has_value());

    // a reader keeps its row while the dataset is replaced
    auto before = store.row("crew", 0);
    store.set("crew", parsed("name role\nbob engineer\n"));
    EXPECT_EQ((*before)["name"], "alice");
    EXPECT_EQ((*store.row("crew", 0))["name"], "bob");

    EXPECT_TRUE(store.remove("crew"));
    EXPECT_FALSE(store.remove("crew"));
    EXPECT_TRUE(store.clear());
    EXPECT_EQ(store.summary(), nlohmann::json::array());
}

TEST(Datasets_test, LoadFile) {
    const std::string path = "/tmp/h2agent-ut-crew.v1.txt";
    {
        std::ofstream f(path);
        f << "name role\nalice captain\nbob engineer\n";
    }
    Datasets store;
    nlohmann::json summary;
    std::string error;
    EXPECT_EQ(store.loadFile("", path, false, summary, error), 200) << error;
    EXPECT_EQ(summary, R"({"id":"h2agent-ut-crew.v1","columns":["name","role"],"rows":2})"_json);  // basename without the last extension
    EXPECT_EQ(store.loadFile("crew", path, false, summary, error), 200) << error;
    EXPECT_EQ(summary["id"], "crew");
    EXPECT_EQ(store.summary().size(), 2u);

    EXPECT_EQ(store.loadFile("crew", "/tmp/h2agent-ut-missing.txt", false, summary, error), 404);
    EXPECT_EQ(store.loadFile("bad id", path, false, summary, error), 400);
    EXPECT_EQ(store.loadFile("a/b", path, false, summary, error), 400);
    {
        std::ofstream f(path);
        f << "name role\nalice\n";
    }
    EXPECT_EQ(store.loadFile("crew", path, false, summary, error), 400);
    EXPECT_EQ(error, "file '" + path + "', line 2: expected 2 fields, found 1");
    EXPECT_EQ((*store.row("crew", 1))["name"], "bob");  // a failed load keeps the former one
    std::remove(path.c_str());

    EXPECT_EQ(Datasets::idFromPath("/config/crew.txt"), "crew");
    EXPECT_EQ(Datasets::idFromPath("crew"), "crew");
    EXPECT_EQ(Datasets::idFromPath("/a.b/.hidden"), ".hidden");
}

TEST(Datasets_test, ConcurrentReadsAndLoads) {
    Datasets store;
    store.set("crew", parsed("name\nalice\nbob\n"));
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&store]() {
            for (int i = 0; i < 2000; ++i) {
                auto r = store.row("crew", i);
                ASSERT_TRUE(r.has_value());
                ASSERT_TRUE((*r)["name"].is_string());
            }
        });
    }
    for (int i = 0; i < 200; ++i) store.set("crew", parsed(i % 2 ? "name\ncarol\n" : "name\nalice\nbob\n"));
    for (auto &th : threads) th.join();
}
