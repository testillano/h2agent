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
#include <chrono>

#include <boost/bind.hpp>

// Project
#include "version.hpp"
#include <functions.hpp>
#include "MyAdminHttp2Server.hpp"
#include "MyTrafficHttp2Server.hpp"
#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/metrics/Metrics.hpp>


const char* progname;

namespace
{
h2agent::http2::MyAdminHttp2Server* myAdminHttp2Server = nullptr;
h2agent::http2::MyTrafficHttp2Server* myTrafficHttp2Server = nullptr; // incoming traffic
boost::asio::io_service *timersIoService = nullptr;

const char* AdminApiName = "admin";
const char* AdminApiVersion = "v1";
}

/////////////////////////
// Auxiliary functions //
/////////////////////////

// Transform input in the form "<double> <double> .. <double>" to bucket boundaries vector
// Cientific notation is allowed, for example boundaries for 150us would be 150e-6
// Returns the final string ignoring non-double values scanned. Also sort is applied.
std::string loadHistogramBoundaries(const std::string &input, ert::metrics::bucket_boundaries_t &boundaries) {
    std::string result;

    std::stringstream ss(input);
    double value = 0;
    while (ss >> value) {
        boundaries.push_back(value);
    }

    std::sort(boundaries.begin(), boundaries.end());
    for (const auto &i: boundaries) {
        result += (std::to_string(i) + " ");
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
    time_t rawtime = 0;
    struct tm* timeinfo(nullptr);

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

    if (myTrafficHttp2Server)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString(
                       "Stopping h2agent traffic service at %s", getLocaltime().c_str()), ERT_FILE_LOCATION));
        myTrafficHttp2Server->stop();
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

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << progname << " - HTTP/2 Agent service\n\n"

       << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[--ipv6]\n"
       << "  IP stack configured for IPv6. Defaults to IPv4.\n\n"

       << "[-b|--bind-address <address>]\n"
       << "  Servers bind <address> (admin/traffic/prometheus); defaults to '0.0.0.0' (ipv4) or '::' (ipv6).\n\n"

       << "[-a|--admin-port <port>]\n"
       << "  Admin <port>; defaults to 8074.\n\n"

       << "[-p|--traffic-server-port <port>]\n"
       << "  Traffic server <port>; defaults to 8000. Set '-1' to disable\n"
       << "  (mock server service is enabled by default).\n\n"

       << "[-m|--traffic-server-api-name <name>]\n"
       << "  Traffic server API name; defaults to empty.\n\n"

       << "[-n|--traffic-server-api-version <version>]\n"
       << "  Traffic server API version; defaults to empty.\n\n"

       << "[-w|--traffic-server-worker-threads <threads>]\n"
       << "  Number of traffic server worker threads; defaults to 1, which should be enough\n"
       << "  even for complex logic provisioned (admin server always uses 1 worker thread).\n"
       << "  It could be increased if hardware concurrency permits a greater margin taking\n"
       << "  into account other process threads considered busy.\n\n"

       << "[-t|--traffic-server-threads <threads>]\n"
       << "  Number of nghttp2 traffic server threads; defaults to 1 (1 connection)\n"
       << "  (admin server always uses 1 nghttp2 thread). This option is exploited\n"
       << "  by multiple clients.\n\n"
       // Note: test if 2 nghttp2 threads for admin interface is needed for intensive provision applications

       << "[-k|--traffic-server-key <path file>]\n"
       << "  Path file for traffic server key to enable SSL/TLS; unsecured by default.\n\n"

       << "[-d|--traffic-server-key-password <password>]\n"
       << "  When using SSL/TLS this may provided to avoid 'PEM pass phrase' prompt at process\n"
       << "  start.\n\n"

       << "[-c|--traffic-server-crt <path file>]\n"
       << "  Path file for traffic server crt to enable SSL/TLS; unsecured by default.\n\n"

       << "[-s|--secure-admin]\n"
       << "  When key (-k|--traffic-server-key) and crt (-c|--traffic-server-crt) are provided,\n"
       << "  only traffic interface is secured by default. This option secures admin interface\n"
       << "  reusing traffic configuration (key/crt/password).\n\n"

       << "[--schema <path file>]\n"
       << "  Path file for optional startup schema configuration.\n\n"

       << "[--global-variable <path file>]\n"
       << "  Path file for optional startup global variable(s) configuration.\n\n"

       << "[--traffic-server-matching <path file>]\n"
       << "  Path file for optional startup traffic server matching configuration.\n\n"

       << "[--traffic-server-provision <path file>]\n"
       << "  Path file for optional startup traffic server provision configuration.\n\n"

       << "[--discard-data]\n"
       << "  Disables data storage for events processed (enabled by default).\n"
       << "  This invalidates some features like FSM related ones (in-state, out-state)\n"
       << "  or event-source transformations.\n\n"
       //<< "  This affects to both mock server-data and client-data storages,\n"
       //<< "  but normally both containers will not be used together in the same process instance.\n\n"

       << "[--discard-data-key-history]\n"
       << "  Disables data key history storage (enabled by default).\n"
       << "  Only latest event (for each key 'method/uri') will be stored and will\n"
       //<< "  Only latest event (for each key 'method/uri'/'endpoint') will be stored and will\n"
       << "  be accessible for further analysis.\n"
       << "  This limits some features like FSM related ones (in-state, out-state)\n"
       << "  or event-source transformations.\n"
       //<< "  , event-source transformations or client triggers.\n"
       << "  Implicitly disabled by option '--discard-data'.\n"
       << "  Ignored for server-unprovisioned events (for troubleshooting purposes).\n\n"
       //<< "  This affects to both mock server-data and client-data storages,\n"
       //<< "  but normally both containers will not be used together in the same process instance.\n\n"

       << "[--disable-purge]\n"
       << "  Skips events post-removal when a provision on 'purge' state is reached (enabled by default).\n\n"
       //<< "  This affects to both mock 'server internal/client external' purge procedures,\n"
       //<< "  but normally both flows will not be used together in the same process instance.\n\n"

       << "[--prometheus-port <port>]\n"
       << "  Prometheus <port>; defaults to 8080.\n\n"

       << "[--prometheus-response-delay-seconds-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.\n"
       << "  Scientific notation is allowed, so in terms of microseconds (e-6) and milliseconds (e-3) we\n"
       << "  could provide, for example: \"100e-6 200e-6 300e-6 400e-6 500e-6 1e-3 5e-3 10e-3 20e-3\".\n\n"
       //<< "  This affects to both mock 'server internal/client external' processing time values,\n"
       //<< "  but normally both flows will not be used together in the same process instance.\n\n"

       << "[--prometheus-message-size-bytes-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for Rx/Tx message size bytes histogram; no boundaries are defined by default.\n\n"
       //<< "  This affects to both mock 'server internal/client external' message size values,\n"
       //<< "  but normally both flows will not be used together in the same process instance.\n\n"

       << "[--disable-metrics]\n"
       << "  Disables prometheus scrape port (enabled by default).\n\n"

       << "[-v|--version]\n"
       << "  Program version.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << '\n';


    if (rc != 0 && !errorMessage.empty())
    {
        ss << errorMessage << '\n';
    }

    _exit(rc);
}

int toNumber(const std::string& value)
{
    int result = 0;

    try
    {
        result = std::stoul(value, nullptr, 10);
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


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    srand(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    bool ipv6 = false; // ipv4 by default
    std::string bind_address = "";
    std::string admin_port = "8074";
    std::string traffic_server_port = "8000";
    std::string traffic_server_api_name = "";
    std::string traffic_server_api_version = "";
    int traffic_server_worker_threads = 1;
    int traffic_server_threads = 1;
    std::string traffic_server_key_file = "";
    std::string traffic_server_key_password = "";
    std::string traffic_server_crt_file = "";
    bool admin_secured = false;
    bool discard_data = false;
    bool discard_data_key_history = false;
    bool disable_purge = false;
    bool verbose = false;
    std::string schema_file = "";
    std::string traffic_server_matching_file = "";
    std::string traffic_server_provision_file = "";
    std::string global_variable_file = "";
    std::string prometheus_port = "8080";
    std::string prometheus_response_delay_seconds_histogram_boundaries = "";
    std::string prometheus_message_size_bytes_histogram_boundaries = "";
    bool disable_metrics = false;
    ert::metrics::bucket_boundaries_t responseDelaySecondsHistogramBucketBoundaries{};
    ert::metrics::bucket_boundaries_t messageSizeBytesHistogramBucketBoundaries{};

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
            usage(EXIT_FAILURE, "Invalid log level provided !");
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

    if (cmdOptionExists(argv, argv + argc, "-b", value)
            || cmdOptionExists(argv, argv + argc, "--bind-address", value))
    {
        bind_address = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-a", value)
            || cmdOptionExists(argv, argv + argc, "--admin-port", value))
    {
        admin_port = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-p", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-port", value))
    {
        traffic_server_port = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-m", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-api-name", value))
    {
        traffic_server_api_name = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-n", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-api-version", value))
    {
        traffic_server_api_version = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-w", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-worker-threads", value))
    {
        traffic_server_worker_threads = toNumber(value);
        if (traffic_server_worker_threads < 1)
        {
            usage(EXIT_FAILURE, "Invalid '--traffic-server-worker-threads' value. Must be greater than 0.");
        }
    }

    // Probably, this parameter is not useful as we release the server thread using our workers, so
    //  no matter if you launch more server threads here, no difference should be detected ...
    if (cmdOptionExists(argv, argv + argc, "-t", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-threads", value))
    {
        traffic_server_threads = toNumber(value);
        if (traffic_server_threads < 1)
        {
            usage(EXIT_FAILURE, "Invalid '--traffic-server-threads' value. Must be greater than 0.");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-key", value))
    {
        traffic_server_key_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-d", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-key-password", value))
    {
        traffic_server_key_password = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-c", value)
            || cmdOptionExists(argv, argv + argc, "--traffic-server-crt", value))
    {
        traffic_server_crt_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-s", value)
            || cmdOptionExists(argv, argv + argc, "--secure-admin", value))
    {
        admin_secured = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--schema", value))
    {
        schema_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--traffic-server-matching", value))
    {
        traffic_server_matching_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--traffic-server-provision", value))
    {
        traffic_server_provision_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--global-variable", value))
    {
        global_variable_file = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--discard-data", value))
    {
        discard_data = true;
        discard_data_key_history = true; // implicitly
    }

    if (cmdOptionExists(argv, argv + argc, "--discard-data-key-history", value))
    {
        discard_data_key_history = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--disable-purge", value))
    {
        disable_purge = true;
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

    if (cmdOptionExists(argv, argv + argc, "--disable-metrics", value))
    {
        disable_metrics = true;
    }

    std::string gitVersion = h2agent::GIT_VERSION;
    if (cmdOptionExists(argv, argv + argc, "-v", value)
            || cmdOptionExists(argv, argv + argc, "--version", value))
    {
        std::cout << (gitVersion.empty() ? "unknown: not built on git repository, may be forked":gitVersion) << '\n';
        _exit(EXIT_SUCCESS);
    }

    // Secure options
    bool traffic_server_enabled = (traffic_server_port != "-1");
    bool traffic_secured = (!traffic_server_key_file.empty() && !traffic_server_crt_file.empty());
    bool hasPEMpasswordPrompt = (admin_secured && traffic_secured && traffic_server_key_password.empty());

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << getLocaltime().c_str() << ": Starting " << progname << " " << (gitVersion.empty() ? "":gitVersion) << '\n';
    std::cout << "Log level: " << ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel()) << '\n';
    std::cout << "Verbose (stdout): " << (verbose ? "true":"false") << '\n';
    std::cout << "IP stack: " << (ipv6 ? "IPv6":"IPv4") << '\n';
    std::cout << "Admin port: " << admin_port << '\n';

    // Server bind address for servers
    std::string bind_address_prometheus_exposer = bind_address;
    if (bind_address.empty()) {
        bind_address = (ipv6 ? "::" : "0.0.0.0"); // default bind address
        bind_address_prometheus_exposer = (ipv6 ? "[::]" : "0.0.0.0"); // default bind address prometheus exposer
    }

    std::cout << "Traffic server (mock server service): " << (traffic_server_enabled ? "enabled":"disabled") << '\n';

    if (traffic_server_enabled) {
        std::cout << "Traffic server bind address: " << bind_address << '\n';
        std::cout << "Traffic server port: " << traffic_server_port << '\n';
        std::cout << "Traffic server api name: " << ((traffic_server_api_name != "") ?  traffic_server_api_name : "<none>") << '\n';
        std::cout << "Traffic server api version: " << ((traffic_server_api_version != "") ?  traffic_server_api_version : "<none>") << '\n';

        std::cout << "Traffic server threads (nghttp2): " << traffic_server_threads << '\n';
        std::cout << "Traffic server worker threads: " << traffic_server_worker_threads << '\n';

        // h2agent threads may not be 100% busy. So there is not significant time stolen when there are i/o waits (timers for example)
        // even if planned threads (main(1) + admin server workers(hardcoded to 1) + admin nghttp2(1) + io timers(1) + traffic_server_threads + traffic_server_worker_threads)
        // are over hardware concurrency (unsigned int hardwareConcurrency = std::thread::hardware_concurrency();)

        std::cout << "Traffic server key password: " << ((traffic_server_key_password != "") ? "***" : "<not provided>") << '\n';
        std::cout << "Traffic server key file: " << ((traffic_server_key_file != "") ? traffic_server_key_file : "<not provided>") << '\n';
        std::cout << "Traffic server crt file: " << ((traffic_server_crt_file != "") ? traffic_server_crt_file : "<not provided>") << '\n';
        if (!traffic_secured) {
            std::cout << "SSL/TLS disabled: both key & certificate must be provided" << '\n';
            if (!traffic_server_key_password.empty()) {
                std::cout << "Traffic server key password was provided but will be ignored !" << '\n';
            }
        }

        std::cout << "Traffic secured: " << (traffic_secured ? "yes":"no") << '\n';
    }

    std::cout << "Admin secured: " << (traffic_secured ? (admin_secured ? "yes":"no"):(admin_secured ? "ignored":"no")) << '\n';

    std::cout << "Schema configuration file: " << ((schema_file != "") ? schema_file :
              "<not provided>") << '\n';
    std::cout << "Global variables configuration file: " << ((global_variable_file != "") ? global_variable_file :
              "<not provided>") << '\n';

    std::cout << "Data storage: " << (!discard_data ? "enabled":"disabled") << '\n';
    std::cout << "Data key history storage: " << (!discard_data_key_history ? "enabled":"disabled") << '\n';
    std::cout << "Purge execution: " << (disable_purge ? "disabled":"enabled") << '\n';

    if (traffic_server_enabled) {
        std::cout << "Traffic server matching configuration file: " << ((traffic_server_matching_file != "") ? traffic_server_matching_file : "<not provided>") << '\n';
        std::cout << "Traffic server provision configuration file: " << ((traffic_server_provision_file != "") ? traffic_server_provision_file : "<not provided>") << '\n';
    }

    if (disable_metrics) {
        std::cout << "Metrics (prometheus): disabled" << '\n';
    }
    else {
        std::cout << "Prometheus bind address: " << bind_address_prometheus_exposer << '\n';
        std::cout << "Prometheus port: " << prometheus_port << '\n';
        if (!responseDelaySecondsHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'response delay seconds' histogram boundaries: " << prometheus_response_delay_seconds_histogram_boundaries << '\n';
        }
        if (!messageSizeBytesHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'message size bytes' histogram boundaries: " << prometheus_message_size_bytes_histogram_boundaries << '\n';
        }
    }

    // Flush:
    std::cout << std::endl;

    ert::tracing::Logger::initialize(progname);
    ert::tracing::Logger::verbose(verbose);

    // Prometheus
    ert::metrics::Metrics *p_metrics = (disable_metrics ? nullptr:new ert::metrics::Metrics);
    if (p_metrics) {
        std::string bind_address_port_prometheus_exposer = bind_address_prometheus_exposer + std::string(":") + prometheus_port;
        if(!p_metrics->serve(bind_address_port_prometheus_exposer)) {
            std::cerr << getLocaltime().c_str() << ": Initialization error in prometheus interface (" << bind_address_port_prometheus_exposer << "). Exiting ..." << '\n';
            _exit(EXIT_FAILURE);
        }
    }

    myAdminHttp2Server = new h2agent::http2::MyAdminHttp2Server(1); // 1 nghttp2 server thread
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

    if (traffic_server_enabled) {
        myTrafficHttp2Server = new h2agent::http2::MyTrafficHttp2Server(traffic_server_worker_threads, timersIoService);
        myTrafficHttp2Server->enableMetrics(p_metrics, responseDelaySecondsHistogramBucketBoundaries, messageSizeBytesHistogramBucketBoundaries);
        myTrafficHttp2Server->enableMyMetrics(p_metrics);
        myTrafficHttp2Server->setApiName(traffic_server_api_name);
        myTrafficHttp2Server->setApiVersion(traffic_server_api_version);
    }

    std::string fileContent;
    nlohmann::json jsonObject;
    bool success = false;

    if (schema_file != "") {
        success = h2agent::http2::getFileContent(schema_file, fileContent);
        std::string log = "Schema configuration load failed and will be ignored";
        if (success)
            success = h2agent::http2::parseJsonContent(fileContent, jsonObject);

        if (success) {
            log += ": ";
            success = myAdminHttp2Server->schema(jsonObject, log);
        }

        if (!success) {
            std::cerr << getLocaltime().c_str() << ": " << log << std::endl;
        }
    }

    if (traffic_server_enabled) {
        if (traffic_server_matching_file != "") {
            success = h2agent::http2::getFileContent(traffic_server_matching_file, fileContent);
            std::string log = "Server matching configuration load failed and will be ignored";
            if (success)
                success = h2agent::http2::parseJsonContent(fileContent, jsonObject);

            if (success) {
                log += ": ";
                success = myAdminHttp2Server->serverMatching(jsonObject, log);
            }

            if (!success) {
                std::cerr << getLocaltime().c_str() << ": " << log << std::endl;
            }
        }

        if (traffic_server_provision_file != "") {
            success = h2agent::http2::getFileContent(traffic_server_provision_file, fileContent);
            std::string log = "Server provision configuration load failed and will be ignored";
            if (success)
                success = h2agent::http2::parseJsonContent(fileContent, jsonObject);

            if (success) {
                log += ": ";
                success = myAdminHttp2Server->serverProvision(jsonObject, log);
            }

            if (!success) {
                std::cerr << getLocaltime().c_str() << ": " << log << std::endl;
            }
        }

        // Server data configuration:
        myTrafficHttp2Server->discardData(discard_data);
        myTrafficHttp2Server->discardDataKeyHistory(discard_data_key_history);
        myTrafficHttp2Server->disablePurge(disable_purge);
    }

    myAdminHttp2Server->setHttp2Server(myTrafficHttp2Server);

    // Now that myTrafficHttp2Server is referenced, I wil have access to global variables:
    if (global_variable_file != "") {
        success = h2agent::http2::getFileContent(global_variable_file, fileContent);
        std::string log = "Global variables configuration load failed and will be ignored";
        if (success)
            success = h2agent::http2::parseJsonContent(fileContent, jsonObject);

        if (success) {
            log += ": ";
            success = myAdminHttp2Server->globalVariable(jsonObject, log);
        }

        if (!success) {
            std::cerr << getLocaltime().c_str() << ": " << log << std::endl;
        }
    }

    // Associate data containers:
    if (traffic_server_enabled) {
        myTrafficHttp2Server->setAdminData(myAdminHttp2Server->getAdminData()); // to retrieve mock behaviour configuration
    }

    // Server key password:
    if (!traffic_server_key_password.empty()) {
        if (traffic_server_enabled && traffic_secured) myTrafficHttp2Server->setServerKeyPassword(traffic_server_key_password);
        if (admin_secured) myAdminHttp2Server->setServerKeyPassword(traffic_server_key_password);
    }

    if (hasPEMpasswordPrompt) {
        std::cout << "You MUST wait for both 'PEM pass phrase' prompts (10 seconds between them) ..." << '\n';
        std::cout << "To avoid these prompts, you may provide '--server-key-password':" << '\n';
    }

    int rc1 = EXIT_SUCCESS;
    int rc2 = EXIT_SUCCESS;
    std::thread t1([&] { rc1 = myAdminHttp2Server->serve(bind_address, admin_port, admin_secured ? traffic_server_key_file:"", admin_secured ? traffic_server_crt_file:"", 1);});

    if (hasPEMpasswordPrompt) std::this_thread::sleep_for(std::chrono::milliseconds(10000)); // This sleep is to separate prompts and allow cin to get both of them.
    // This is weird ! So, --server-key-password SHOULD BE PROVIDED for TLS/SSL

    std::thread t2([&] {
        if (myTrafficHttp2Server) {
            rc2 = myTrafficHttp2Server->serve(bind_address, traffic_server_port, traffic_server_key_file, traffic_server_crt_file, traffic_server_threads);
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait for rc1/rc2 update from threads
    if (rc1 != EXIT_SUCCESS) _exit(rc1);
    if (rc2 != EXIT_SUCCESS) _exit(rc2);

    // Join threads
    tt.join();
    t1.join();
    t2.join();

    _exit(EXIT_SUCCESS);
}

