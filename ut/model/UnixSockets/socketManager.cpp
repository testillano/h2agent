#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <thread>

#include <SocketManager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// SocketManager json example template:
nlohmann::json SocketManagerJson = R"(
[
  {
    "socket": 0,
    "path": "/tmp/h2agent.ut.socket.Beethoven"
  },
  {
    "socket": 0,
    "path": "/tmp/h2agent.ut.socket.Mozart"
  }
]
)"_json;

// Target file content example:
std::string SocketManagerSafeSocketContent1 = "hi";
std::string SocketManagerSafeSocketContent2 = "bye";

class SocketManager_test : public ::testing::Test
{
public:

    SocketManager_test() {
        ;
    }

    ~SocketManager_test() {
    }
};

TEST_F(SocketManager_test, SocketManager)
{
    h2agent::model::SocketManager sm(nullptr);
    sm.enableMetrics(nullptr, "");

    // Socket paths:
    std::string path1, path2;
    path1 = SocketManagerJson[0]["path"];
    path2 = SocketManagerJson[1]["path"];

    // Create sockets to receive:
    int sockfd1 = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(sockfd1 < 0);
    int sockfd2 = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(sockfd2 < 0);

    struct sockaddr_un serverAddr1, serverAddr2;
    memset(&serverAddr1, 0, sizeof(struct sockaddr_un));
    serverAddr1.sun_family = AF_UNIX;
    strcpy(serverAddr1.sun_path, path1.c_str());
    memset(&serverAddr2, 0, sizeof(struct sockaddr_un));
    serverAddr2.sun_family = AF_UNIX;
    strcpy(serverAddr2.sun_path, path2.c_str());

    unlink(path1.c_str()); // just in case, it exists
    unlink(path2.c_str()); // just in case, it exists

    int bindrc1 = bind(sockfd1, (struct sockaddr*)&serverAddr1, sizeof(struct sockaddr_un));
    if (bindrc1 < 0) close(sockfd1);
    ASSERT_FALSE(bindrc1 < 0);

    int bindrc2 = bind(sockfd2, (struct sockaddr*)&serverAddr2, sizeof(struct sockaddr_un));
    if (bindrc2 < 0) close(sockfd2);
    ASSERT_FALSE(bindrc2 < 0);

    // write:
    sm.write(path2, SocketManagerSafeSocketContent2, 0);
    sm.write(path1, SocketManagerSafeSocketContent1, 0);

    // read:
    // Check socket content:
    char buffer[32];
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    ssize_t bytesRead = recvfrom(sockfd1, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
    ASSERT_TRUE(bytesRead > 0);
    buffer[bytesRead] = '\0';
    EXPECT_EQ(buffer, SocketManagerSafeSocketContent1);

    bytesRead = recvfrom(sockfd2, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
    ASSERT_TRUE(bytesRead > 0);
    buffer[bytesRead] = '\0';
    EXPECT_EQ(buffer, SocketManagerSafeSocketContent2);

    // Check json representation:
    SocketManagerJson[0]["socket"] = sm.getJson()[0]["socket"];
    SocketManagerJson[1]["socket"] = sm.getJson()[1]["socket"];
    EXPECT_EQ(sm.getJson(), SocketManagerJson);

    // clear:
    sm.clear();

    // Check empty:
    EXPECT_EQ(sm.asJsonString(), "[]");

    // Cleanup sockets
    close(sockfd1);
    close(sockfd2);
    unlink(path1.c_str());
    unlink(path2.c_str());
}

