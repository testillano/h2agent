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
    "state": "closed",
    "readCache": "true"
  },
  {
    "bytes": 0,
    "path": "/tmp/h2agent.ut.Mozart.txt",
    "state": "closed"
  }
]
)"_json;

nlohmann::json confJsonDisabled = R"(
{
  "readCache":"disabled"
}
)"_json;

nlohmann::json confJsonEnabled = R"(
{
  "readCache":"enabled"
}
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
    fm.enableMetrics(nullptr, "");

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

    // read with cache:
    fm.enableReadCache(true);
    success = fm.read(path1, content, true /* text */);
    EXPECT_EQ(content, FileManagerSafeFileContent);
    EXPECT_TRUE(success);

    // Write in existent safe file:
    fm.write(path1, FileManagerSafeFileContent, true /* text */, 0);
    success = fm.read(path1, content, true /* text */);
    EXPECT_EQ(content, FileManagerSafeFileContent);
    EXPECT_TRUE(success);

    // empty:
    fm.empty(path1);
    fm.empty(path2);

    // read path1 (was cached)
    success = fm.read(path1, content, true /* text */);
    EXPECT_EQ(content, FileManagerSafeFileContent);
    EXPECT_TRUE(success);

    // read path2 (not cached: will be read again)
    fm.enableReadCache(false);
    success = fm.read(path2, content, true /* text */);
    EXPECT_EQ(content, "");
    EXPECT_TRUE(success);

    //sleep(1);

    // Check json representation:
    EXPECT_EQ(fm.getJson(), FileManagerJson);

    // clear:
    fm.clear();

    // Check empty:
    EXPECT_EQ(fm.asJsonString(), "[]");

    // Check configurations:
    fm.enableReadCache(true);
    EXPECT_EQ(fm.getConfigurationJson(), confJsonEnabled);
    EXPECT_EQ(fm.configurationAsJsonString(), confJsonEnabled.dump());

    fm.enableReadCache(false);
    EXPECT_EQ(fm.getConfigurationJson(), confJsonDisabled);
    EXPECT_EQ(fm.configurationAsJsonString(), confJsonDisabled.dump());

    // empty unknown:
    fm.empty("/tmp/newfile");
    success = fm.read("/tmp/newfile", content, true /* text */);
    EXPECT_EQ(content, "");
    EXPECT_TRUE(success);

    // read unknown:
    success = fm.read("/tmp/newfile2", content, true /* text */);
    EXPECT_EQ(content, "");
    EXPECT_FALSE(success);

    // read unknown ninary:
    success = fm.read("/tmp/newfile3", content, false /* binary */);
    EXPECT_EQ(content, "");
    EXPECT_FALSE(success);
}

