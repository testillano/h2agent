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
#include <ctime>
#include <iomanip>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <regex>

#include <ert/tracing/Logger.hpp> // getLocaltime()

#define COL1_WIDTH 36 // date time and microseconds
#define COL2_WIDTH 10 // relative seconds from beginning
#define COL3_WIDTH 16 // sequence
#define COL4_WIDTH 32 // 256 is too much, but we could accept UDP datagrams with that size ...


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
       << "  Events per second. Floats are allowed (0.016667 would mean 1 tick per minute),\n"
       << "  negative number means unlimited (depends on your hardware) and 0 is prohibited.\n"
       << "  Defaults to 1.\n\n"

       << "[-i|--initial <value>]\n"
       << "  Initial value for datagram. Defaults to 0.\n\n"

       << "[-f|--final <value>]\n"
       << "  Final value for datagram. Defaults to unlimited.\n\n"

       << "[--pattern <value>]\n"
       << "  Pattern to be parsed by sequence (@{seq} is replaced by sequence). Defaults to '@{seq}'.\n"
       << "  This parameter can occur multiple times to create a random set. For example, passing\n"
       << "  '--pattern foo --pattern foo --pattern bar', there is a probability of 2/3 to select\n"
       << "  'foo' and 1/3 to select 'bar'.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print messages each specific amount (must be positive). Defaults to 1.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path /tmp/udp.sock --eps 3500 --initial 555000000 --final 555999999 --pattern \"foo/bar/@{seq}\"\n"
       << "   " << progname << " --udp-socket-path /tmp/udp.sock --final 0 --pattern STATS # sends 1 single datagram 'STATS' to the server\n\n"

       << "To stop the process, just interrupt it.\n"

       << '\n';

    if (rc != 0 && !errorMessage.empty())
    {
        ss << errorMessage << '\n';
    }

    exit(rc);
}

unsigned long long int toLong(const std::string& value)
{
    unsigned long long int result = 0;

    try
    {
        result = std::stoull(value);
    }
    catch (...)
    {
        usage(EXIT_FAILURE, std::string("Error in number conversion for '" + value + "' !"));
    }

    return result;
}

double toDouble(const std::string& value)
{
    double result = 0;

    try
    {
        result = std::stod(value);
    }
    catch (...)
    {
        usage(EXIT_FAILURE, std::string("Error in number conversion for '" + value + "' !"));
    }

    return result;
}

char **cmdOptionExists(char** begin, char** end, const std::string& option, std::string& value)
{
    char** result = std::find(begin, end, option);
    bool exists = (result != end);

    if (exists) {
        if (++result != end)
        {
            value = *result;
        }
    }
    else {
        result = nullptr;
    }

    return result;
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
    srand(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    int i_printEach = 1; // default
    std::string udpSocketPath{};
    unsigned long long int initialValue{};
    unsigned long long int finalValue = std::numeric_limits<unsigned long long>::max();
    std::vector<std::string> patterns{};
    double eps = 1.0;

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
        eps = toDouble(value);
        if (eps == 0) usage(EXIT_FAILURE);
    }

    if (cmdOptionExists(argv, argv + argc, "-i", value)
            || cmdOptionExists(argv, argv + argc, "--initial", value))
    {
        initialValue = toLong(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-f", value)
            || cmdOptionExists(argv, argv + argc, "--final", value))
    {
        finalValue = toLong(value);
    }

    char **next = argv;
    while ((next = cmdOptionExists(next, argv + argc, "--pattern", value)))
    {
        patterns.push_back(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-e", value)
            || cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        i_printEach = atoi(value.c_str());
        if (i_printEach <= 0) usage(EXIT_FAILURE);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (udpSocketPath.empty()) usage(EXIT_FAILURE);
    if (patterns.empty()) patterns.push_back("@{seq}"); // default if no --pattern parameter is provided

    std::cout << "Path: " << udpSocketPath << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";
    std::cout << "Range: [" << initialValue << ", " << finalValue << "]\n";
    if (patterns.size() == 1) {
        std::cout << "Pattern: " << patterns[0] << "\n";
    }
    else {
        std::cout << "Patterns (random subset): ";
        for(auto it: patterns) {
            std::cout << " '" << it << "'";
        }
        std::cout << '\n';
    }
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
    std::cout << std::setw(COL1_WIDTH) << std::left << "<timestamp>"
              << std::setw(COL2_WIDTH) << std::left << "<time(s)>"
              << std::setw(COL3_WIDTH) << std::left << "<sequence>"
              << std::setw(COL4_WIDTH) << std::left << "<udp datagram>" << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << std::string(COL1_WIDTH-1, '_')
              << std::setw(COL2_WIDTH) << std::left << std::string(COL2_WIDTH-1, '_')
              << std::setw(COL3_WIDTH) << std::left << std::string(COL3_WIDTH-1, '_')
              << std::setw(COL4_WIDTH) << std::left << std::string(COL4_WIDTH-1, '_') << '\n';

    std::string udpDataSeq{}, udpData{};
    std::string::size_type pos = 0u;
    std::string from = "@{seq}";
    long long int periodNS = (long long int)(1000000000.0/eps);

    unsigned long long int sequence{};
    auto startTimeNS = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    int patternsSize = patterns.size();

    while (true) {

        udpDataSeq = std::to_string(initialValue + sequence);

        if (patternsSize == 1) {
            udpData = udpData = patterns[0];
        }
        else { // random subset
            udpData = patterns[rand () % patternsSize];
        }

        // search/replace @{seq} by 'udpDataSeq':
        pos = 0u;
        while((pos = udpData.find(from, pos)) != std::string::npos) {
            udpData.replace(pos, from.length(), udpDataSeq);
            pos += udpDataSeq.length();
        }

        // exit condition:
        if (initialValue + sequence > finalValue) {
            std::cout << '\n' << "Existing (range covered) !" << '\n';
            break;
        }
        else {
            sequence++;
            if (sequence % i_printEach == 0 || (sequence == 1) /* first one always shown :-)*/) {
                std::cout << std::setw(COL1_WIDTH) << std::left << ert::tracing::getLocaltime()
                          << std::setw(COL2_WIDTH) << std::left << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch() - startTimeNS).count()
                          << std::setw(COL3_WIDTH) << std::left << sequence
                          << std::setw(COL4_WIDTH) << std::left << udpData << '\n';
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

