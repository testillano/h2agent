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
#include <sstream>
#include <algorithm>
#include <thread>

#include <boost/bind.hpp>

// Project
#include "version.hpp"
#include <functions.hpp>
#include "MyAdminHttp2Server.hpp"
#include "MyHttp2Server.hpp"
#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/metrics/Metrics.hpp>


const char* progname;

namespace
{
h2agent::http2server::MyAdminHttp2Server* myAdminHttp2Server = nullptr;
h2agent::http2server::MyHttp2Server* myHttp2Server = nullptr;
boost::asio::io_service *timersIoService = nullptr;

const char* AdminApiName = "admin";
const char* AdminApiVersion = "v1";
}

/////////////////////////
// Auxiliary functions //
/////////////////////////

// Transform input in the form "<double> <double> .. <double>" to bucket boundaries vector
// Returns the final string ignoring non-double values scanned.
std::string loadHistogramBoundaries(const std::string &input, ert::metrics::bucket_boundaries_t &boundaries) {
    std::string result;
    std::stringstream ss(input);
    double value;
    while (ss >> value) {
        boundaries.push_back(value);
        result += (std::to_string(value) + " ");
    }

    return result;
}

/*
int getThreadCount() {
    char buf[512];
    int fd = open("/proc/self/stat", O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    int thread_count = 0;
    if (read(fd, buf, sizeof(buf)) > 0) {
        char* s = strchr(buf, ')');
        if (s != NULL) {
            // Read 18th integer field after the command name
            for (int field = 0; *s != ' ' || ++field < 18; s++) ;
            thread_count = atoi(s + 1);
        }
    }

    close(fd);
    return thread_count;
}
*/

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

void stopAgent()
{
    if (timersIoService)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString(
                       "Stopping h2agent timers service at %s", getLocaltime().c_str()), ERT_FILE_LOCATION));
        timersIoService->stop();
        //delete(timersIoService);
    }

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

    stopAgent();

    LOGWARNING(ert::tracing::Logger::warning("Stopping logger", ERT_FILE_LOCATION));

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

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[--ipv6]\n"
       << "  IP stack configured for IPv6. Defaults to IPv4.\n\n"

       << "[-a|--admin-port <port>]\n"
       << "  Admin <port>; defaults to 8074.\n\n"

       << "[-p|--server-port <port>]\n"
       << "  Server <port>; defaults to 8000.\n\n"

       << "[-m|--server-api-name <name>]\n"
       << "  Server API name; defaults to empty.\n\n"

       << "[-n|--server-api-version <version>]\n"
       << "  Server API version; defaults to empty.\n\n"

       << "[-w|--worker-threads <threads>]\n"
       << "  Number of worker threads; defaults to a mimimum of 2 threads except if hardware\n"
       << "   concurrency permits a greater margin taking into account other process threads.\n"
       << "  Normally, 1 thread should be enough even for complex logic provisioned.\n\n"

       << "[-t|--server-threads <threads>]\n"
       << "  Number of nghttp2 server threads; defaults to 1 (1 connection).\n\n"

       << "[-k|--server-key <path file>]\n"
       << "  Path file for server key to enable SSL/TLS; unsecured by default.\n\n"

       << "[--server-key-password <password>]\n"
       << "  When using SSL/TLS this may provided to avoid 'PEM pass phrase' prompt at process\n"
       << "   start.\n\n"

       << "[-c|--server-crt <path file>]\n"
       << "  Path file for server crt to enable SSL/TLS; unsecured by default.\n\n"

       << "[-s|--secure-admin]\n"
       << "  When key (-k|--server-key) and crt (-c|--server-crt) are provided, only the traffic\n"
       << "   interface is secured by default. To include management interface, this option must\n"
       << "   be also provided.\n\n"

       << "[--server-request-schema <path file>]\n"
       << "  Path file for the server schema to validate requests received.\n\n"

       << "[--server-matching <path file>]\n"
       << "  Path file for optional startup server matching configuration.\n\n"

       << "[--server-provision <path file>]\n"
       << "  Path file for optional startup server provision configuration.\n\n"

       << "[--discard-server-data]\n"
       << "  Disables server data storage for events received (enabled by default).\n"
       << "  This invalidates some features like FSM related ones (in-state, out-state)\n"
       << "   or event-source transformations.\n\n"

       << "[--discard-server-data-requests-history]\n"
       << "  Disables server data requests history storage (enabled by default).\n"
       << "  Only latest request (for each key 'method/uri') will be stored and will\n"
       << "   be accessible for further analysis.\n"
       << "  This limits some features like FSM related ones (in-state, out-state)\n"
       << "   or event-source transformations.\n"
       << "  Implicitly disabled by option '--discard-server-data'.\n"
       << "  Ignored for unprovisioned events (for troubleshooting purposes).\n\n"

       << "[--prometheus-port <port>]\n"
       << "  Prometheus <port>; defaults to 8080 (-1 to disable metrics).\n\n"

       << "[--prometheus-response-delay-seconds-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.\n\n"

       << "[--prometheus-message-size-bytes-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for message size bytes histogram; no boundaries are defined by default.\n\n"

       << "[-v|--version]\n"
       << "  Program version.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

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
    srand(time(nullptr));

    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    bool ipv6 = false; // ipv4 by default
    std::string admin_port = "8074";
    std::string server_port = "8000";
    std::string server_api_name = "";
    std::string server_api_version = "";
    int worker_threads = -1;
    int server_threads = 1;
    std::string server_key_file = "";
    std::string server_key_password = "";
    std::string server_crt_file = "";
    bool admin_secured = false;
    std::string server_req_schema_file = "";
    bool discard_server_data = false;
    bool discard_server_data_requests_history = false;
    bool verbose = false;
    std::string server_matching_file = "";
    std::string server_provision_file = "";
    std::string prometheus_port = "8080";
    std::string prometheus_response_delay_seconds_histogram_boundaries = "";
    std::string prometheus_message_size_bytes_histogram_boundaries = "";
    ert::metrics::bucket_boundaries_t responseDelaySecondsHistogramBucketBoundaries;
    ert::metrics::bucket_boundaries_t messageSizeBytesHistogramBucketBoundaries;

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-l", value)
            || cmdOptionExists(argv, argv + argc, "--log-level", value))
    {
        if (!ert::tracing::Logger::setLevel(value))
        {
            usage(EXIT_FAILURE);
        }
    }

    if (cmdOptionExists(argv, argv + argc, "--verbose", value))
    {
        verbose = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--ipv6", value))
    {
        ipv6 = true;
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
            || cmdOptionExists(argv, argv + argc, "--worker-threads", value))
    {
        worker_threads = toNumber(value);
    }

    // Probably, this parameter is not useful as we release the server thread using our workers, so
    //  no matter if you launch more server threads here, no difference should be detected ...
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

    if (cmdOptionExists(argv, argv + argc, "--server-key-password", value))
    {
        server_key_password = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-c", value)
            || cmdOptionExists(argv, argv + argc, "--server-crt", value))
    {
        server_crt_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-s", value)
            || cmdOptionExists(argv, argv + argc, "--secure-admin", value))
    {
        admin_secured = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--server-request-schema", value))
    {
        server_req_schema_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--server-matching", value))
    {
        server_matching_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--server-provision", value))
    {
        server_provision_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--discard-server-data", value))
    {
        discard_server_data = true;
        discard_server_data_requests_history = true; // implicitly
    }

    if (cmdOptionExists(argv, argv + argc, "--discard-server-data-requests-history", value))
    {
        discard_server_data_requests_history = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-port", value))
    {
        prometheus_port = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-response-delay-seconds-histogram-boundaries", value))
    {
        prometheus_response_delay_seconds_histogram_boundaries = loadHistogramBoundaries(value, responseDelaySecondsHistogramBucketBoundaries);
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-message-size-bytes-histogram-boundaries", value))
    {
        prometheus_message_size_bytes_histogram_boundaries = loadHistogramBoundaries(value, messageSizeBytesHistogramBucketBoundaries);
    }

    if (cmdOptionExists(argv, argv + argc, "-v", value)
            || cmdOptionExists(argv, argv + argc, "--version", value))
    {
        std::cout << h2agent::GIT_VERSION << '\n';
        _exit(EXIT_SUCCESS);
    }

    // Secure options
    bool traffic_secured = (!server_key_file.empty() && !server_crt_file.empty());
    bool hasPEMpasswordPrompt = (admin_secured && traffic_secured && server_key_password.empty());

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[" << getLocaltime().c_str() << "] Starting " << progname <<
              " (version " << h2agent::GIT_VERSION << ") ..." << '\n';
    std::cout << "Log level: " << ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel()) << '\n';
    std::cout << "Verbose (stdout): " << (verbose ? "true":"false") << '\n';
    std::cout << "IP stack: " << (ipv6 ? "IPv6":"IPv4") << '\n';
    std::cout << "Admin port: " << admin_port << '\n';
    std::cout << "Server port: " << server_port << '\n';
    std::cout << "Server api name: " << ((server_api_name != "") ?
                                         server_api_name :
                                         "<none>") << '\n';
    std::cout << "Server api version: " << ((server_api_version != "") ?
                                            server_api_version :
                                            "<none>") << '\n';

    unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
    std::cout << "Hardware concurrency: " << hardwareConcurrency << '\n';

    if (worker_threads < 1) {
        // Calculate traffic server threads:
        // * Miminum assignment = 2 threads
        // * Maximum assignment = CPUs - rest of threads count
        int maxWorkerThreadsAssignment = hardwareConcurrency - 7 /* main(1) + admin server(workers=1) + timers io(tt=1) + admin(t1->2) + traffic (t2->2) */;
        worker_threads = (maxWorkerThreadsAssignment > 2) ? maxWorkerThreadsAssignment : 2;
    }
    std::cout << "Traffic server worker threads: " << worker_threads << '\n';
    std::cout << "Server threads (exploited by multiple clients): " << server_threads << '\n';
    std::cout << "Server key password: " << ((server_key_password != "") ? "***" :
              "<not provided>") << '\n';
    std::cout << "Server key file: " << ((server_key_file != "") ? server_key_file :
                                         "<not provided>") << '\n';
    std::cout << "Server crt file: " << ((server_crt_file != "") ? server_crt_file :
                                         "<not provided>") << '\n';
    if (!traffic_secured) {
        std::cout << "SSL/TLS disabled: both key & certificate must be provided" << '\n';
        if (!server_key_password.empty()) {
            std::cout << "Server key password was provided but will be ignored !" << '\n';
        }
    }

    std::cout << "Traffic secured: " << (traffic_secured ? "yes":"no") << '\n';
    std::cout << "Admin secured: " << (traffic_secured ? (admin_secured ? "yes":"no"):(admin_secured ? "ignored":"no")) << '\n';

    std::cout << "Server request schema: " << ((server_req_schema_file != "") ? server_req_schema_file :
              "<not provided>") << '\n';
    std::cout << "Server matching configuration file: " << ((server_matching_file != "") ? server_matching_file :
              "<not provided>") << '\n';
    std::cout << "Server provision configuration file: " << ((server_provision_file != "") ? server_provision_file :
              "<not provided>") << '\n';
    std::cout << "Server data storage: " << (!discard_server_data ? "enabled":"disabled") << '\n';
    std::cout << "Server data requests history storage: " << (!discard_server_data_requests_history ? "enabled":"disabled") << '\n';
    if (prometheus_port != "-1") {

        std::cout << "Prometheus port: " << prometheus_port << '\n';
        if (!responseDelaySecondsHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'response delay seconds' histogram boundaries: " << prometheus_response_delay_seconds_histogram_boundaries << '\n';
        }
        if (!messageSizeBytesHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'message size bytes' histogram boundaries: " << prometheus_message_size_bytes_histogram_boundaries << '\n';
        }
    }
    else {
        std::cout << "Metrics disabled" << '\n';
    }

    // Flush:
    std::cout << std::endl;

    ert::tracing::Logger::initialize(progname);
    ert::tracing::Logger::verbose(verbose);


    // Process bin address for servers
    std::string bind_address = (ipv6 ? "::" : "0.0.0.0");
    std::string bind_address_prometheus_exposer = (ipv6 ? "[::]" : "0.0.0.0");

    // Prometheus
    ert::metrics::Metrics *p_metrics = ((prometheus_port != "-1") ? new ert::metrics::Metrics : nullptr);
    if (p_metrics) {
        std::string bind_address_port_prometheus_exposer = bind_address_prometheus_exposer + std::string(":") + prometheus_port;
        if(!p_metrics->serve(bind_address_port_prometheus_exposer)) {
            std::cerr << "Initialization error in prometheus interface (" << bind_address_port_prometheus_exposer << "). Exiting ..." << '\n';
            _exit(EXIT_FAILURE);
        }
    }

    myAdminHttp2Server = new h2agent::http2server::MyAdminHttp2Server(1);
    myAdminHttp2Server->enableMetrics(p_metrics);
    myAdminHttp2Server->setApiName(AdminApiName);
    myAdminHttp2Server->setApiVersion(AdminApiVersion);

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    timersIoService = new boost::asio::io_service();
    std::thread tt([&] {
        boost::asio::io_service::work work(*timersIoService);
        timersIoService->run();
    });

    myHttp2Server = new h2agent::http2server::MyHttp2Server(worker_threads, timersIoService);
    myHttp2Server->enableMetrics(p_metrics, responseDelaySecondsHistogramBucketBoundaries, messageSizeBytesHistogramBucketBoundaries);
    myHttp2Server->enableMyMetrics(p_metrics);
    myHttp2Server->setApiName(server_api_name);
    myHttp2Server->setApiVersion(server_api_version);

    std::string fileContent;
    nlohmann::json jsonObject;
    bool success;

    if (server_req_schema_file != "") {
        success = h2agent::http2server::getFileContent(server_req_schema_file, fileContent);
        if (success)
            success = myHttp2Server->setRequestsSchema(fileContent);

        if (!success) {
            std::cerr << "Requests schema load failed: will be ignored" << std::endl;
        }
    }

    if (server_matching_file != "") {
        success = h2agent::http2server::getFileContent(server_matching_file, fileContent);
        std::string log = "Server matching configuration load failed and will be ignored";
        if (success)
            success = h2agent::http2server::parseJsonContent(fileContent, jsonObject);

        if (success) {
            log += ": ";
            success = myAdminHttp2Server->serverMatching(jsonObject, log);
        }

        if (!success) {
            std::cerr << log << std::endl;
        }
    }

    if (server_provision_file != "") {
        success = h2agent::http2server::getFileContent(server_provision_file, fileContent);
        std::string log = "Server provision configuration load failed and will be ignored";
        if (success)
            success = h2agent::http2server::parseJsonContent(fileContent, jsonObject);

        if (success) {
            log += ": ";
            success = myAdminHttp2Server->serverProvision(jsonObject, log);
        }

        if (!success) {
            std::cerr << log << std::endl;
        }
    }

    // Server data configuration:
    myHttp2Server->discardServerData(discard_server_data);
    myHttp2Server->discardServerDataRequestsHistory(discard_server_data_requests_history);
    myAdminHttp2Server->setHttp2Server(myHttp2Server);

    // Associate data containers:
    myHttp2Server->setAdminData(myAdminHttp2Server->getAdminData()); // to retrieve mock behaviour configuration

    // Server key password:
    if (!server_key_password.empty()) {
        if (traffic_secured) myHttp2Server->setServerKeyPassword(server_key_password);
        if (admin_secured) myAdminHttp2Server->setServerKeyPassword(server_key_password);
    }

    if (hasPEMpasswordPrompt) {
        std::cout << "You MUST wait for both 'PEM pass phrase' prompts (10 seconds between them) ..." << '\n';
        std::cout << "To avoid these prompts, you may provide '--server-key-password':" << '\n';
    }

    int rc1 = EXIT_SUCCESS;
    int rc2 = EXIT_SUCCESS;
    std::thread t1([&] { rc1 = myAdminHttp2Server->serve(bind_address, admin_port, admin_secured ? server_key_file:"", admin_secured ? server_crt_file:"", server_threads);});

    if (hasPEMpasswordPrompt) std::this_thread::sleep_for(std::chrono::milliseconds(10000)); // This sleep is to separate prompts and allow cin to get both of them.
    // This is weird ! So, --server-key-password SHOULD BE PROVIDED for TLS/SSL

    std::thread t2([&] { rc2 = myHttp2Server->serve(bind_address, server_port, server_key_file, server_crt_file, server_threads);});

    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait for rc1/rc2 update from threads
    if (rc1 != EXIT_SUCCESS) _exit(rc1);
    if (rc2 != EXIT_SUCCESS) _exit(rc2);

    // Join threads
    tt.join();
    t1.join();
    t2.join();

    _exit(EXIT_SUCCESS);
}

