#include <thread>

#include <FileManager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// SafeFile json example template:
nlohmann::json SafeFileJson = R"(
{
  "bytes": 0,
  "path": "/tmp/h2agent.ut.safefile.txt",
  "state": "closed"
}
)"_json;

nlohmann::json SafeFileFailedJson = R"(
{
  "path": "/foo/bar",
  "state": "missing"
}
)"_json;

// SafeFile content example:
std::string SafeFileContent = "hi";

class SafeFile_test : public ::testing::Test
{
public:
    h2agent::model::FileManager *file_manager_{};
    boost::asio::io_context *timers_io_context_{};
    std::thread *timers_thread_{};

    SafeFile_test() {
        file_manager_ =  new h2agent::model::FileManager(); // no metrics by default
        timers_io_context_ =  new boost::asio::io_context();
        timers_thread_ = new std::thread([&] {
            boost::asio::io_context::work work(*timers_io_context_);
            timers_io_context_->run();
        });
    }

    ~SafeFile_test() {
        timers_io_context_->stop();
        delete(timers_io_context_);
        delete(timers_thread_);
    }
};

TEST_F(SafeFile_test, SafeFileWithCloseDelayed)
{
    // Nothing opened:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // 1 file opened:
    h2agent::model::SafeFile file(file_manager_, SafeFileJson["path"], timers_io_context_);
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 1);
    file.empty(); // empty to ensure
    file.write(SafeFileContent, 5000 /* close delay value */);

    // Closed after 5000 usecs (5 ms), we wait 50 ms (10x !) to ensure it is closed:
    boost::asio::steady_timer exitTimer(*timers_io_context_, std::chrono::milliseconds(50));
    exitTimer.async_wait([&] (const boost::system::error_code& e) { timers_io_context_->stop(); });
    timers_thread_->join();

    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // Check file content:
    //file.open(std::ofstream::in);
    bool success;
    EXPECT_EQ(file.read(success), SafeFileContent);
    EXPECT_TRUE(success);

    // Check json representation:
    SafeFileJson["bytes"] = SafeFileContent.size();
    EXPECT_EQ(file.getJson(), SafeFileJson);

    // Check after emptied:
    file.empty();
    SafeFileJson["bytes"] = 0;
    EXPECT_EQ(file.getJson(), SafeFileJson);
}

TEST_F(SafeFile_test, SafeFileWithInstantClose)
{
    // Stop timers service:
    timers_io_context_->stop();
    timers_thread_->join();

    // Nothing opened:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // 1 file opened:
    h2agent::model::SafeFile fileInstantClose(file_manager_, SafeFileJson["path"], nullptr /* no timers io context will be used */);
    SafeFileJson.erase("closeDelayUsecs");

    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 1);
    fileInstantClose.empty(); // empty to ensure
    fileInstantClose.write(SafeFileContent, 0 /* close delay value */);

    // Instant close:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // Check file content:
    //file.open(std::ofstream::in);
    bool success;
    EXPECT_EQ(fileInstantClose.read(success), SafeFileContent);
    EXPECT_TRUE(success);

    // Check json representation:
    SafeFileJson["bytes"] = SafeFileContent.size();
    EXPECT_EQ(fileInstantClose.getJson(), SafeFileJson);

    // Check after emptied:
    fileInstantClose.empty();
    SafeFileJson["bytes"] = 0;
    EXPECT_EQ(fileInstantClose.getJson(), SafeFileJson);
}

TEST_F(SafeFile_test, SafeFileFailsToOpen)
{
    // Stop timers service:
    timers_io_context_->stop();
    timers_thread_->join();

    // Nothing opened:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // 1 file fails to open:
    h2agent::model::SafeFile fileInstantClose(file_manager_, "/foo/bar", nullptr /* no timers io context will be used */);

    EXPECT_EQ(fileInstantClose.getJson(), SafeFileFailedJson);
}

