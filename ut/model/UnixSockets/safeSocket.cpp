#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <thread>

#include <SocketManager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// SafeSocket json example template:
nlohmann::json SafeSocketJson = R"(
{
  "path": "/tmp/h2agent.ut.safesocket.txt",
  "socket": 0
}
)"_json;

// SafeSocket content example:
std::string SafeSocketContent = "hi";

class SafeSocket_test : public ::testing::Test
{
public:
    h2agent::model::SocketManager *socket_manager_{};
    boost::asio::io_context *timers_io_context_{};
    std::thread *timers_thread_{};

    SafeSocket_test() {
        socket_manager_ =  new h2agent::model::SocketManager(); // no metrics by default
        timers_io_context_ =  new boost::asio::io_context();
        timers_thread_ = new std::thread([&] {
            boost::asio::io_context::work work(*timers_io_context_);
            timers_io_context_->run();
        });
    }

    ~SafeSocket_test() {
        timers_io_context_->stop();
        delete(timers_io_context_);
        delete(timers_thread_);
    }
};

TEST_F(SafeSocket_test, SafeSocketWithWriteDelayed)
{
    // Socket path:
    std::string socketPath = SafeSocketJson["path"];

    // Create socket to receive:
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(sockfd < 0);

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, socketPath.c_str());

    unlink(socketPath.c_str()); // just in case, it exists

    int bindrc = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un));
    if (bindrc < 0) close(sockfd);
    ASSERT_FALSE(bindrc < 0);

    // Open socket to write:
    h2agent::model::SafeSocket socket(socket_manager_, socketPath, timers_io_context_);
    socket.write(SafeSocketContent, 5000 /* write delay value */);

    // Written after 5000 usecs (5 ms), we wait 50 ms (10x !) to ensure it is written:
    boost::asio::steady_timer exitTimer(*timers_io_context_, std::chrono::milliseconds(50));
    exitTimer.async_wait([&] (const boost::system::error_code& e) { timers_io_context_->stop(); });
    timers_thread_->join();

    // Check socket content:
    char buffer[32];
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
    ASSERT_TRUE(bytesRead > 0);
    buffer[bytesRead] = '\0';
    EXPECT_EQ(buffer, SafeSocketContent);

    // Check json representation:
    SafeSocketJson["socket"] = socket.getJson()["socket"]; // unpredictable
    EXPECT_EQ(socket.getJson(), SafeSocketJson);
}

TEST_F(SafeSocket_test, SafeSocketWithInstantWrite)
{
    // Stop timers service:
    timers_io_context_->stop();
    timers_thread_->join();

    // Socket path:
    std::string socketPath = SafeSocketJson["path"];

    // Create socket to receive:
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(sockfd < 0);

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, socketPath.c_str());

    unlink(socketPath.c_str()); // just in case, it exists

    int bindrc = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un));
    if (bindrc < 0) close(sockfd);
    ASSERT_FALSE(bindrc < 0);

    // Open socket to write:
    h2agent::model::SafeSocket socket(socket_manager_, socketPath, nullptr /* no timers io context will be used */);
    socket.write(SafeSocketContent);

    // Check socket content:
    char buffer[32];
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
    ASSERT_TRUE(bytesRead > 0);
    buffer[bytesRead] = '\0';
    EXPECT_EQ(buffer, SafeSocketContent);

    // Check json representation:
    SafeSocketJson["socket"] = socket.getJson()["socket"]; // unpredictable
    EXPECT_EQ(socket.getJson(), SafeSocketJson);
}
