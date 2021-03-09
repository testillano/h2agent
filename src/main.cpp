/*
 ___________________________________________
|    _     ___                        _     |
|   | |   |__ \                      | |    |
|   | |__    ) |__ _  __ _  ___ _ __ | |_   |
|   | '_ \  / // _` |/ _` |/ _ \ '_ \| __|  |  HTTP/2 AGENT FOR MOCK TESTING
|   | | | |/ /| (_| | (_| |  __/ | | | |_   |  Version 0.0.z
|   |_| |_|____\__,_|\__, |\___|_| |_|\__|  |  https://github.com/testillano/h2agent
|                     __/ |                 |
|                    |___/                  |
|___________________________________________|

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

// C
#include <libgen.h> // basename

// Standard
#include <iostream>
#include <string>
#include <algorithm>
#include <thread>

// Project
#include "version.hpp"
#include "MyAdminHttp2Server.hpp"
#include "MyHttp2Server.hpp"
#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>


const char* progname;

namespace
{
h2agent::http2server::MyAdminHttp2Server* myAdminHttp2Server = nullptr;
h2agent::http2server::MyHttp2Server* myHttp2Server = nullptr;
const char* AdminApiName = "provision";
const char* AdminApiVersion = "v1";
const nlohmann::json AdminApiSchema = R"(
{
  "type": "object",
  "additionalProperties": false,
  "properties": {
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE"]
    },
    "requestUri": {
      "type": "string"
    },
    "responseHeader": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "responseCode": {
      "type": "integer"
    },
    "responseBody": {
      "type": "object"
    },
    "responseDelayMs": {
      "type": "integer"
    },
    "replaceRules": {
      "type": "object"
    },
    "queueId":{
      "type": "integer"
    }
  },
  "required": [ "requestMethod", "requestUri", "responseCode" ]
}
)"_json;

}


/////////////////////////
// Auxiliary functions //
/////////////////////////

std::string getLocaltime()
{
    std::string result;

    char timebuffer[80];
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timebuffer, 80, "%d/%m/%y %H:%M:%S %Z", timeinfo);
    result = timebuffer;

    return result;
}

void stopServers()
{
    if (myAdminHttp2Server)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString(
              "Stopping h2agent admin service at %s", getLocaltime().c_str()), ERT_FILE_LOCATION));
        myAdminHttp2Server->stop();
    }

    if (myHttp2Server)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString(
              "Stopping h2agent mock service at %s", getLocaltime().c_str()), ERT_FILE_LOCATION));
        myHttp2Server->stop();
    }
}

void _exit(int rc)
{
    LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Terminating with exit code %d", rc), ERT_FILE_LOCATION));

    stopServers();

    ert::tracing::Logger::terminate();
    exit(rc);
}

void sighndl(int signal)
{
    LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Signal received: %d", signal), ERT_FILE_LOCATION));
    _exit(EXIT_FAILURE);
}

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc)
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "usage: " << progname << " [options]\n\nOptions:\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[-a|--admin-port <port>]\n"
       << "  Admin <port>; defaults to 8074\n\n"

       << "[-p|--server-port <port>]\n"
       << "  Server <port>; defaults to 8000\n\n"

       << "[-m|--server-api-name <name>]\n"
       << "  Server API name; defaults to empty\n\n"

       << "[-n|--server-api-version <version>]\n"
       << "  Server API version; defaults to empty\n\n"

       << "[-w|--max-worker-threads <threads>]\n"
       << "  Maximum worker threads; defaults to -1 (no limit)\n\n"

       << "[-t|--server-threads <threads>]\n"
       << "  Number of nghttp2 server threads; defaults to 1 (1 connection)\n\n"

       << "[-k|--server-key <path file>]\n"
       << "  Path file for server key to enable SSL/TLS; defaults to empty\n\n"

       << "[-c|--server-crt <path file>]\n"
       << "  Path file for server crt to enable SSL/TLS; defaults to empty\n\n"

       << "[-v|--version]\n"
       << "  Program version\n\n"

       << "[-h|--help]\n"
       << "  This help\n\n"

       << '\n';

    _exit(rc);
}

int toNumber(const std::string& value)
{
    int result{};

    try
    {
        result = std::stoul(value, nullptr, 10);
    }
    catch (...)
    {
        usage(EXIT_FAILURE);
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


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string admin_port = "8074";
    std::string server_port = "8000";
    std::string server_api_name = "";
    std::string server_api_version = "";
    int max_worker_threads = -1;
    int server_threads = 1;
    std::string server_key_file = "";
    std::string server_crt_file = "";

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-v", value)
            || cmdOptionExists(argv, argv + argc, "--version", value))
    {
        std::cout << h2agent::GIT_VERSION << '\n';
        _exit(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-l", value)
            || cmdOptionExists(argv, argv + argc, "--log-level", value))
    {
        if (!ert::tracing::Logger::setLevel(value))
        {
            usage(EXIT_FAILURE);
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-a", value)
            || cmdOptionExists(argv, argv + argc, "--admin-port", value))
    {
        admin_port = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-p", value)
            || cmdOptionExists(argv, argv + argc, "--server-port", value))
    {
        server_port = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-m", value)
            || cmdOptionExists(argv, argv + argc, "--server-api-name", value))
    {
        server_api_name = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-n", value)
            || cmdOptionExists(argv, argv + argc, "--server-api-version", value))
    {
        server_api_version = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-w", value)
            || cmdOptionExists(argv, argv + argc, "--max-worker-threads", value))
    {
        max_worker_threads = toNumber(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-t", value)
            || cmdOptionExists(argv, argv + argc, "--server-threads", value))
    {
        server_threads = toNumber(value);
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--server-key", value))
    {
        server_key_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-c", value)
            || cmdOptionExists(argv, argv + argc, "--server-crt", value))
    {
        server_crt_file = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[" << getLocaltime().c_str() << "] Starting " << progname <<
              " (version " << h2agent::GIT_VERSION << ") ..." << '\n';
    std::cout << "Admin port: " << admin_port << '\n';
    std::cout << "Server port: " << server_port << '\n';
    std::cout << "Server api name: " << ((server_api_name != "") ?
                                         server_api_name :
                                         "<none>") << '\n';
    std::cout << "Server api version: " << ((server_api_version != "") ?
                                            server_api_version :
                                            "<none>") << '\n';
    std::cout << "Max worker threads: ";

    if (max_worker_threads != -1)
    {
        std::cout << max_worker_threads;
    }
    else
    {
        std::cout << "unlimited";
    }

    std::cout << '\n';
    std::cout << "Server threads: " << server_threads << '\n';
    std::cout << "Server key file: " << ((server_key_file != "") ? server_key_file :
                                         "<not provided>") << '\n';
    std::cout << "Server crt file: " << ((server_crt_file != "") ? server_crt_file :
                                         "<not provided>") << '\n';

    if (server_key_file == "" || server_crt_file == "")
    {
        std::cout << "SSL/TLS disabled: both key & certificate must be provided" <<
                  '\n';
    }

    ert::tracing::Logger::initialize(progname);

    /*
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Admin port: %s; Threads: %d",
                              admin_port.c_str(), server_threads);
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );
    */


    myAdminHttp2Server = new h2agent::http2server::MyAdminHttp2Server(AdminApiSchema, max_worker_threads);
    myAdminHttp2Server->setApiName(AdminApiName);
    myAdminHttp2Server->setApiVersion(AdminApiVersion);

    myHttp2Server = new h2agent::http2server::MyHttp2Server(max_worker_threads);
    myHttp2Server->setApiName(server_api_name);
    myHttp2Server->setApiVersion(server_api_version);

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    std::string bind_address = "0.0.0.0";

    //std::thread t1(&h2agent::http2server::MyAdminHttp2Server::serve, myAdminHttp2Server, bind_address, admin_port, server_key_file, server_crt_file, server_threads);
    //std::thread t2(&h2agent::http2server::MyHttp2Server::serve, myHttp2Server, bind_address, server_port, server_key_file, server_crt_file, server_threads);
    int rc1, rc2;
    std::thread t1([&] { rc1 = myAdminHttp2Server->serve(bind_address, admin_port, server_key_file, server_crt_file, server_threads);});
    std::thread t2([&] { rc2 = myHttp2Server->serve(bind_address, server_port, server_key_file, server_crt_file, server_threads);});
    t1.join();
    t2.join();

    if (rc1 == EXIT_SUCCESS && rc2 == EXIT_SUCCESS)
    {
        _exit(EXIT_SUCCESS);
    }

    std::cerr << "Initialization failure. Exiting ..." << '\n';
    _exit(EXIT_FAILURE);
}

