/*
 ____________________________________________________
|             _                 _ _            _     |
|            | |               | (_)          | |    |
|   _   _  __| |_ __   __   ___| |_  ___ _ __ | |_   |
|  | | | |/ _` | '_ \ |__| / __| | |/ _ \ '_ \| __|  |  CLIENT UDP UTILITY TO TEST UDP messages towards udp-server/udp-server-h2client
|  | |_| | (_| | |_) |    | (__| | |  __/ | | | |_   |  Version 0.0.z
|   \__,_|\__,_| .__/      \___|_|_|\___|_| |_|\__|  |  https://github.com/testillano/h2agent (tools/udp-client)
|              | |                                   |
|              |_|                                   |
|____________________________________________________|

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
#include <iomanip>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <regex>

#define COL1_WIDTH 16 // sequence
#define COL2_WIDTH 32 // 256 is too much, but we could accept UDP datagrams with that size ...


const char* progname;

// Globals
int Sockfd{}; // global to allow signal wrapup

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "-k|--udp-socket-path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "[--eps <value>]\n"
       << "  Events per second. Defaults to 1 (negative number means unlimited: depends on your hardware).\n\n"

       << "[-i|--initial <value>]\n"
       << "  Initial value for datagram. Defaults to 0.\n\n"

       << "[-f|--final <value>]\n"
       << "  Final value for datagram. Defaults to unlimited.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print messages each specific amount (must be positive). Defaults to 1000.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path \"/tmp/my_unix_socket\" --eps 3500 --initial 555000000 --final 555999999\n\n"

       << "To stop the process, just interrupt it.\n"

       << '\n';

    if (rc != 0 && !errorMessage.empty())
    {
        ss << errorMessage << '\n';
    }

    exit(rc);
}

unsigned long long int toNumber(const std::string& value)
{
    unsigned long long int result = 0;

    try
    {
        result = std::stoull(value, nullptr, 10);
    }
    catch (...)
    {
        usage(EXIT_FAILURE, std::string("Error in number conversion for '" + value + "' !"));
    }

    return result;
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
    exit(EXIT_FAILURE);
}


///////////////////
// MAIN FUNCTION //
//
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string printEach = "1000";
    std::string udpSocketPath{};
    unsigned long long int initialValue{};
    unsigned long long int finalValue = std::numeric_limits<unsigned long long>::max();
    int eps = 1;

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--udp-socket-path", value))
    {
        udpSocketPath = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--eps", value))
    {
        eps = (int)toNumber(value);
        if (eps < 0) eps = -1;
    }

    if (cmdOptionExists(argv, argv + argc, "-i", value)
            || cmdOptionExists(argv, argv + argc, "--initial", value))
    {
        initialValue = toNumber(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-f", value)
            || cmdOptionExists(argv, argv + argc, "--final", value))
    {
        finalValue = toNumber(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-e", value)
            || cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        printEach = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (udpSocketPath.empty()) usage(EXIT_FAILURE);
    int i_printEach = (printEach.empty() ? 1:atoi(printEach.c_str()));
    if (i_printEach <= 0) usage(EXIT_FAILURE);

    std::cout << "Path: " << udpSocketPath << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";
    std::cout << "Range: [" << initialValue << ", " << finalValue << "]\n";
    std::cout << "Events per second: ";
    if (eps > 0) std::cout << eps;
    else std::cout << "unlimited";
    std::cout << "\n";

    Sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (Sockfd < 0) {
        perror("Error creating UDP socket !");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, udpSocketPath.c_str());

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    std::cout << '\n';
    std::cout << '\n';
    std::cout << "Generating UDP messages..." << '\n' << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << "<sequence>"
              << std::setw(COL2_WIDTH) << std::left << "<udp datagram>" << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << "__________"
              << std::setw(COL2_WIDTH) << std::left << "______________" << '\n';

    std::string udpData;
    int periodNS = 1000000000/eps;

    unsigned long long int sequence{};
    auto startTimeNS = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());

    while (true) {

        udpData = std::to_string(initialValue + sequence);

        // exit condition:
        if (initialValue + sequence > finalValue) {
            std::cout << '\n' << "Existing (range covered) !" << '\n';
            break;
        }
        else {
            sequence++;
            if (sequence % i_printEach == 0 || (sequence == 1) /* first one always shown :-)*/) {
                std::cout << std::setw(COL1_WIDTH) << std::left << sequence
                          << std::setw(COL2_WIDTH) << std::left << udpData << '\n';
            }
        }

        sendto(Sockfd, udpData.c_str(), udpData.length(), 0, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un));

        // Adjust time:
        //std::this_thread::sleep_for(std::chrono::nanoseconds(periodNS)); // this is not accurate
        if (eps > 0) {
            auto futureTimeNS = startTimeNS + std::chrono::nanoseconds(periodNS * sequence);
            auto elapsedNS = std::chrono::duration_cast<std::chrono::nanoseconds>(futureTimeNS - std::chrono::system_clock::now().time_since_epoch());
            std::this_thread::sleep_for(std::chrono::nanoseconds(elapsedNS));
        }
    }

    close(Sockfd);

    exit(EXIT_SUCCESS);
}

