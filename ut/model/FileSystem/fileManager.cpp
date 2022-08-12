#include <thread>

#include <FileManager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// FileManager json example template:
nlohmann::json FileManagerJson = R"(
[
  {
    "bytes": 0,
    "path": "/tmp/h2agent.ut.Beethoven.txt",
    "state": "closed"
  },
  {
    "bytes": 0,
    "path": "/tmp/h2agent.ut.Mozart.txt",
    "state": "closed"
  }
]
)"_json;

// Target file content example:
std::string FileManagerSafeFileContent = "hi";

class FileManager_test : public ::testing::Test
{
public:

    FileManager_test() {
        ;
    }

    ~FileManager_test() {
    }
};

TEST_F(FileManager_test, FileManager)
{
    h2agent::model::FileManager fm(nullptr);
    fm.enableMetrics(nullptr);

    // write:
    std::string path1, path2;
    path1 = FileManagerJson[0]["path"];
    path2 = FileManagerJson[1]["path"];

    fm.write(path2, FileManagerSafeFileContent, true /* text */, 0);
    fm.write(path1, FileManagerSafeFileContent, true /* text */, 0);

    // read:
    std::string content;
    bool success = fm.read(path1, content, true /* text */);
    EXPECT_EQ(content, FileManagerSafeFileContent);
    EXPECT_TRUE(success);

    // empty:
    fm.empty(path1);
    fm.empty(path2);
    success = fm.read(path1, content, true /* text */);
    EXPECT_EQ(content, "");
    EXPECT_TRUE(success);

    sleep(1);

    // Check json representation:
    EXPECT_EQ(fm.getJson(), FileManagerJson);

    // clear:
    fm.clear();

    // Check empty:
    EXPECT_EQ(fm.asJsonString(), "[]");
}

