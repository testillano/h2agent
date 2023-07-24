/*
 __________________________________________________________
|                                                          |
|             _                                            |
|            | |                                           |
|   _   _  __| |_ __   __   ___  ___ _ ____   _____ _ __   |
|  | | | |/ _` | '_ \ |__| / __|/ _ \ '__\ \ / / _ \ '__|  |  SERVER UDP UTILITY TO TEST h2agent UDP messages
|  | |_| | (_| | |_) |     \__ \  __/ |   \ V /  __/ |     |  Version 0.0.z
|   \__,_|\__,_| .__/      |___/\___|_|    \_/ \___|_|     |  https://github.com/testillano/h2agent (tools/udp-server)
|              | |                                         |
|              |_|                                         |
|__________________________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <libgen.h> // basename
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

// Standard
#include <iostream>
#include <string>
#include <regex>

#define BUFFER_SIZE 256

const char* progname;

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc)
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "--path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "--print-each <value>\n"
       << "  Print messages each specific amount (must be positive). Defaults to 1.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --path \"/tmp/my_unix_socket\"" << '\n'

       << '\n';

    exit(rc);
}

bool cmdOptionExists(char** begin, char** end, const std::string& option,
                     std::string& value)
{
    char** itr = std::find(begin, end, option);
    bool exists = (itr != end);

    if (exists && ++itr != end)
    {
        value = *itr;
    }

    return exists;
}

void sighndl(int signal)
{
    std::cout << "Signal received: " << signal << std::endl;
    //close(sockfd);
    //unlink(path.c_str());
    exit(EXIT_FAILURE);
}


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string path{};
    std::string printEach{};

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "--path", value))
    {
        path = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        printEach = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (path.empty()) usage(EXIT_FAILURE);
    int i_printEach = (printEach.empty() ? 1:atoi(printEach.c_str()));
    if (i_printEach <= 0) usage(EXIT_FAILURE);

    std::cout << "Path: " << path << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";

    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating UDP socket !");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, path.c_str());

    unlink(path.c_str()); // just in case

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un)) < 0) {
        perror("Error binding UDP socket !");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    std::cout << std::endl << "Waiting for messages ([sequence] <message>) ..." << '\n';
    int sequence = 1;
    std::string message;
    while ((bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) > 0) {

        buffer[bytesRead] = '\0'; // Agregar terminador nulo al final del texto leído
        message.assign(buffer);

        if (sequence % i_printEach == 0) {
            std::cout << "[" << sequence << "] " << message << std::endl;
        }
        sequence++;

        // exit condition:
        if (message == "EOF") {
            std::cout<<  std::endl << "Existing (EOF received) !" << '\n';
            break;
        }
    }

    close(sockfd);
    unlink(path.c_str());

    exit(EXIT_SUCCESS);
}

