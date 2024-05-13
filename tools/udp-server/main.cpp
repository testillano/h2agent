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
#include <ctime>
#include <iomanip>
#include <string>
#include <regex>
#include <chrono>

#include <ert/tracing/Logger.hpp> // getLocaltime()

#define BUFFER_SIZE 256
#define COL1_WIDTH 36 // date time and microseconds
#define COL2_WIDTH 16 // sequence
#define COL3_WIDTH 32 // 256 is too much, but we could accept UDP datagrams with that size ...


const char* progname;

// Globals
int Sockfd{}; // global to allow signal wrapup
std::string UdpSocketPath{}; // global to allow signal wrapup

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc)
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "-k|--udp-socket-path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print messages each specific amount (must be positive). Defaults to 1.\n"
       << "  Setting datagrams estimated rate should take 1 second/printout and output\n"
       << "  frequency gives an idea about UDP receptions rhythm.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path /tmp/udp.sock\n\n"

       << "To stop the process you can send UDP message 'EOF':\n"
       << "   echo -n EOF | nc -u -q0 -w1 -U /tmp/udp.sock\n\n"

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
    close(Sockfd);
    unlink(UdpSocketPath.c_str());
    exit(EXIT_FAILURE);
}


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string printEach{};

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--udp-socket-path", value))
    {
        UdpSocketPath = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-e", value)
            || cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        printEach = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (UdpSocketPath.empty()) usage(EXIT_FAILURE);
    int i_printEach = (printEach.empty() ? 1:atoi(printEach.c_str()));
    if (i_printEach <= 0) usage(EXIT_FAILURE);

    std::cout << "Path: " << UdpSocketPath << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";
    std::cout << std::endl;

    Sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (Sockfd < 0) {
        perror("Error creating UDP socket !");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, UdpSocketPath.c_str());

    unlink(UdpSocketPath.c_str()); // just in case

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    if (bind(Sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un)) < 0) {
        perror("Error binding UDP socket !");
        close(Sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    std::cout << "Remember:" << '\n';
    std::cout << " To stop process: echo -n EOF | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';

    std::cout << '\n';
    std::cout << '\n';
    std::cout << "Waiting for UDP messages..." << '\n' << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << "<timestamp>"
              << std::setw(COL2_WIDTH) << std::left << "<sequence>"
              << std::setw(COL3_WIDTH) << std::left << "<udp datagram>" << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << std::string(COL1_WIDTH-1, '_')
              << std::setw(COL2_WIDTH) << std::left << std::string(COL2_WIDTH-1, '_')
              << std::setw(COL3_WIDTH) << std::left << std::string(COL3_WIDTH-1, '_') << '\n';

    std::string udpData;

    unsigned int sequence{};
    while ((bytesRead = recvfrom(Sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) > 0) {

        buffer[bytesRead] = '\0'; // Agregar terminador nulo al final del texto leído
        udpData.assign(buffer);

        // exit condition:
        if (udpData == "EOF") {
            std::cout<<  '\n' << "Exiting (EOF received) !" << '\n';
            break;
        }
        else {
            if (sequence % i_printEach == 0 || (sequence == 0) /* first one always shown :-)*/) {
                std::cout << std::setw(COL1_WIDTH) << std::left << ert::tracing::getLocaltime()
                          << std::setw(COL2_WIDTH) << std::left << sequence
                          << std::setw(COL3_WIDTH) << std::left << udpData << '\n';
            }
            sequence++;
        }
    }

    close(Sockfd);
    unlink(UdpSocketPath.c_str());

    exit(EXIT_SUCCESS);
}

