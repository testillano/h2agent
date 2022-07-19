#include <thread>

#include <SafeFile.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// SafeFile json example template:
nlohmann::json SafeFileJson = R"(
{
  "bytes": 0,
  "path": "/tmp/h2agent.ut.safefile.txt",
  "state": "closed",
  "closeDelayUsecs": 500
}
)"_json;

// SafeFile content example:
std::string SafeFileContent = "hi";

class SafeFile_test : public ::testing::Test
{
public:
    boost::asio::io_service *timers_io_service_{};
    std::thread *timers_thread_{};

    SafeFile_test() {
        timers_io_service_ =  new boost::asio::io_service();
        timers_thread_ = new std::thread([&] {
            boost::asio::io_service::work work(*timers_io_service_);
            timers_io_service_->run();
        });
    }

    ~SafeFile_test() {
        timers_io_service_->stop();
        //delete(timers_io_service_);
    }
};

TEST_F(SafeFile_test, SafeFileWithCloseDelayed)
{
    // Nothing opened:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // 1 file opened:
    h2agent::model::SafeFile fileCloseDelayed(SafeFileJson["path"], timers_io_service_, nullptr /* metrics */, 500);
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 1);
    fileCloseDelayed.empty(); // empty to ensure
    fileCloseDelayed.write(SafeFileContent);

    // Closed after 0.5 seconds:
    boost::asio::deadline_timer exitTimer(*timers_io_service_, boost::posix_time::milliseconds(1000));
    exitTimer.async_wait([&] (const boost::system::error_code& e) { timers_io_service_->stop(); });
    timers_thread_->join();

    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // Check file content:
    //fileCloseDelayed.open(std::ofstream::in);
    bool success;
    EXPECT_EQ(fileCloseDelayed.read(success), SafeFileContent);
    EXPECT_TRUE(success);

    // Check json representation:
    SafeFileJson["bytes"] = SafeFileContent.size();
    EXPECT_EQ(fileCloseDelayed.getJson(), SafeFileJson);

    // Check after emptied:
    fileCloseDelayed.empty();
    SafeFileJson["bytes"] = 0;
    EXPECT_EQ(fileCloseDelayed.getJson(), SafeFileJson);
}

TEST_F(SafeFile_test, SafeFileWithInstantClose)
{
    // Stop timers service:
    timers_io_service_->stop();
    timers_thread_->join();

    // Nothing opened:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // 1 file opened:
    h2agent::model::SafeFile fileInstantClose(SafeFileJson["path"], nullptr /* no timers io service will be used */, nullptr /* metrics */, 0 /* instant close */);
    SafeFileJson.erase("closeDelayUsecs");

    // No timers io service will be used regardless close delay value:
    fileInstantClose.setCloseDelayUs(0);

    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 1);
    fileInstantClose.empty(); // empty to ensure
    fileInstantClose.write(SafeFileContent);

    // Instant close:
    EXPECT_EQ(h2agent::model::SafeFile::CurrentOpenedFiles.load(), 0);

    // Check file content:
    //fileCloseDelayed.open(std::ofstream::in);
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

