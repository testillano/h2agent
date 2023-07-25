/*
 _____________________________________________________________________________________________________
|             _                                                 _     ___        _ _            _     |
|            | |                                               | |   |__ \      | (_)          | |    |
|   _   _  __| |_ __   __   ___  ___ _ ____   _____ _ __   __  | |__    ) |  ___| |_  ___ _ __ | |_   |
|  | | | |/ _` | '_ \_|__| / __|/ _ \ '__\ \ / / _ \ '__| |__| | '_ \  / /  / __| | |/ _ \ '_ \| __|  |  SERVER UDP H2CLIENT UTILITY TO trigger http/2 requests on UDP events
|  | |_| | (_| | |_) |     \__ \  __/ |   \ V /  __/ |         | | | |/ /_ | (__| | |  __/ | | | |_   |  Version 0.0.z
|   \__,_|\__,_| .__/      |___/\___|_|    \_/ \___|_|         |_| |_|____| \___|_|_|\___|_| |_|\__|  |  ttps://github.com/testillano/h2agent (tools/udp-server-h2client)
|              | |                                                                                    |
|              |_|                                                                                    |
|_____________________________________________________________________________________________________|

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

// Standard
#include <libgen.h> // basename
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <memory>
#include <atomic>
#include <regex>
#include <chrono>
#include <algorithm>

#include <cctype> // parse uri
#include <functional> // parse uri

#include <ert/tracing/Logger.hpp>
#include <ert/metrics/Metrics.hpp>

#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/Http2Client.hpp>


#define BUFFER_SIZE 256


const char* progname;

// Status codes statistics, like h2load:
// status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
std::atomic<unsigned int> CONNECTION_ERRORS{};
std::atomic<unsigned int> STATUS_CODES_2xx{};
std::atomic<unsigned int> STATUS_CODES_3xx{};
std::atomic<unsigned int> STATUS_CODES_4xx{};
std::atomic<unsigned int> STATUS_CODES_5xx{};

// Globals
std::map<std::string, std::string> PathPatterns{};
std::map<std::string, std::string> BodyPatterns{};
std::map<std::string, std::string> HeadersPatterns{};
ert::metrics::Metrics *myMetrics{}; // global to allow signal wrapup
int Sockfd{}; // global to allow signal wrapup
std::string UdpSocketPath{}; // global to allow signal wrapup

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "UDP server will trigger one HTTP/2 request for every reception, replacing optionally\n"
       << "the '@{udp}' pattern on uri, headers and/or body provided, with the UDP data read.\n"
       << "If data received contains pipes (|), it is also possible to access each part during\n"
       << "parsing procedure through the use of pattern '@{udp.<n>}'.\n"
       << "To stop the process you can send UDP message 'EOF'.\n\n"

       << "-k|--udp-socket-path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print UDP receptions each specific amount (must be positive). Defaults to 1.\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[-v|--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[-t|--timeout-seconds <value>]\n"
       << "  Time in seconds to wait for requests response. Defaults to 5.\n\n"

       << "[-d|--send-delay-milliseconds <value>]\n"
       << "  Time in seconds to delay before sending the request. Defaults to 0.\n"
       << "  It also supports negative values which turns into random number in\n"
       << "  the range [0,abs(value)].\n\n"

       << "[-m|--method <POST|GET|PUT|DELETE|HEAD>]\n"
       << "  Request method. Defaults to 'GET'.\n\n"

       << "-u|--uri <value>\n"
       << " URI to access.\n\n"

       << "[--header <value>]\n"
       << "  Header in the form 'name:value'. This parameter can occur multiple times.\n\n"

       << "[-b|--body <value>]\n"
       << "  Plain text for request body content.\n\n"

       << "[--secure]\n"
       << " Use secure connection.\n\n"

       << "[--prometheus-bind-address <address>]\n"
       << "  Prometheus local bind <address>; defaults to 0.0.0.0.\n\n"

       << "[--prometheus-port <port>]\n"
       << "  Prometheus local <port>; defaults to 8081. Value of -1 disables metrics.\n\n"

       << "[--prometheus-response-delay-seconds-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.\n"
       << "  Scientific notation is allowed, so in terms of microseconds (e-6) and milliseconds (e-3) we\n"
       << "  could provide, for example: \"100e-6 200e-6 300e-6 400e-6 500e-6 1e-3 5e-3 10e-3 20e-3\".\n\n"

       << "[--prometheus-message-size-bytes-histogram-boundaries <space-separated list of doubles>]\n"
       << "  Bucket boundaries for Tx/Rx message size bytes histogram; no boundaries are defined by default.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path \"/tmp/my_unix_socket\" -print-each 1000 --timeout-seconds 1 --uri http://0.0.0.0:8000/book/@{udp}" << '\n'
       << "   " << progname << " --udp-socket-path \"/tmp/my_unix_socket\" --print-each 1000 --method POST --uri http://0.0.0.0:8000/data --header \"content-type:application/json\" --body '{\"book\":\"@{udp}\"}'" << '\n'
       << '\n'
       << "   To provide body from file, use this trick: --body \"$(jq -c '.' long-body.json)\"" << '\n'

       << '\n';

    if (rc != 0 && !errorMessage.empty())
    {
        ss << errorMessage << '\n';
    }

    exit(rc);
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

std::string statusCodesAsString() {
    std::stringstream ss;

    ss << STATUS_CODES_2xx << " 2xx, " << STATUS_CODES_3xx << " 3xx, " << STATUS_CODES_4xx << " 4xx, " << STATUS_CODES_5xx << " 5xx, " << CONNECTION_ERRORS << " connection errors";

    return ss.str();
}

void wrapup() {
    close(Sockfd);
    unlink(UdpSocketPath.c_str());
    delete (myMetrics);
    LOGWARNING(ert::tracing::Logger::warning("Stopping logger", ERT_FILE_LOCATION));
    ert::tracing::Logger::terminate();

    // Print status codes statistics:
    std::cout << std::endl << "status codes: " << statusCodesAsString() << std::endl;
}

void sighndl(int signal)
{
    std::cout << "Signal received: " << signal << std::endl;
    std::cout << "Wrap-up and exit ..." << std::endl;

    // wrap up
    wrapup();

    exit(EXIT_FAILURE);
}

void collectVariablePatterns(const std::string &str, std::map<std::string, std::string> &patterns) {

    static std::regex re("@\\{[^\\{\\}]*\\}", std::regex::optimize); // @{[^{}]*} with curly braces escaped
    // or: R"(@\{[^\{\}]*\})"

    std::string::const_iterator it(str.cbegin());
    std::smatch matches;
    std::string pattern;
    patterns.clear();
    while (std::regex_search(it, str.cend(), matches, re)) {
        it = matches.suffix().first;
        pattern = matches[0];
        patterns[pattern] = pattern.substr(2, pattern.size()-3); // @{foo} -> foo
    }
}

void searchReplaceAll(std::string& str,
                      const std::string& from,
                      const std::string& to)
{
    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String source to 'search/replace all': %s | from: %s | to: %s", str.c_str(), from.c_str(), to.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
    std::string::size_type pos = 0u;
    while((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String result of 'search/replace all': %s", str.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
}

void replaceVariables(std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars) {

    if (patterns.empty()) return;
    if (vars.empty()) return;

    std::map<std::string,std::string>::const_iterator it;
    std::unordered_map<std::string,std::string>::const_iterator git;

    for (auto pit = patterns.begin(); pit != patterns.end(); pit++) {

        // local var has priority over a global var with the same name
        if (!vars.empty()) {
            it = vars.find(pit->second);
            if (it != vars.end()) {
                searchReplaceAll(str, pit->first, it->second);
                continue; // all is done
            }
        }
    }
}

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

class Stream {
    std::string data_{}; // UDP

    std::map<std::string, std::string> variables_;

    std::shared_ptr<ert::http2comm::Http2Client> client_{};
    std::string method_{};
    std::string path_{};
    std::string body_{};
    nghttp2::asio_http2::header_map headers_{};
    int timeout_s_{};

public:
    Stream(const std::string &data) : data_(data) {;}

    // Set HTTP/2 components
    void setRequest(std::shared_ptr<ert::http2comm::Http2Client> client, const std::string &method, const std::string &path, const std::string &body, nghttp2::asio_http2::header_map &headers, int secondsTimeout) {
        client_ = client;
        method_ = method;
        path_ = path;
        body_ = body;
        headers_ = headers;
        timeout_s_ = secondsTimeout;

        // Main variable @{udp}, and possible parts:
        std::string mainVar = "udp";
        variables_[mainVar] = data_;

        // check pipes:
        char delimiter = '|';
        if (data_.find(delimiter) == std::string::npos)
            return;

        std::size_t start = 0;
        std::size_t pos = data_.find(delimiter);
        std::string aux;
        int count = 1;
        while (pos != std::string::npos) {
            aux = mainVar;
            aux += ".";
            aux += std::to_string(count);
            count++;
            variables_[aux] = data_.substr(start, pos - start);
            start = pos + 1;
            pos = data_.find(delimiter, start);
        }
        // add latest
        aux = mainVar;
        aux += ".";
        aux += std::to_string(count);
        variables_[aux] = data_.substr(start);
    }

    const std::string &getMethod() const { // TODO: method substitution with message patterns (probably not useful)
        return method_;
    }

    std::string getPath() const { // @{udp[.part]} substitution
        std::string result = path_;
        replaceVariables(result, PathPatterns, variables_);
        return result;
    }

    std::string getBody() const { // @{udp[.part]} substitution
        std::string result = body_;
        replaceVariables(result, BodyPatterns, variables_);
        return result;
    }

    nghttp2::asio_http2::header_map getHeaders() const { // @{udp[.part]} substitution
        nghttp2::asio_http2::header_map result;
        std::string aux;

        for(auto it = headers_.begin(); it != headers_.end(); it ++) {
            aux = it->second.value;
            replaceVariables(aux, HeadersPatterns, variables_);
            result.emplace(it->first, nghttp2::asio_http2::header_value{aux});
        }

        return result;
    }

    int getSecondsTimeout() const { // No sense to do substitutions
        return timeout_s_;
    }

    // Process reception
    void process() {
        // Send the request:
        ert::http2comm::Http2Client::response response = client_->send(getMethod(), getPath(), getBody(), getHeaders(), std::chrono::milliseconds(getSecondsTimeout() * 1000));

        // Log debug the response, and store status codes statistics:
        int status = response.statusCode;
        if (status >= 200 && status < 300) {
            STATUS_CODES_2xx++;
        }
        else if (status >= 300 && status < 400) {
            STATUS_CODES_3xx++;
        }
        else if (status >= 400 && status < 500) {
            STATUS_CODES_4xx++;
        }
        else if (status >= 500 && status < 600) {
            STATUS_CODES_5xx++;
        }
        else if (status == -1) {
            CONNECTION_ERRORS++;
        }

        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("[process] Data processed: %s", data_.c_str()), ERT_FILE_LOCATION));
    };
};

///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    srand(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    progname = basename(argv[0]);

    // Traces
    ert::tracing::Logger::initialize(progname); // initialize logger (before possible myExit() execution):

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string printEach{};
    int secondsTimeout = 5; // default
    int millisecondsSendDelay = 0; // default
    bool randomSendDelay = false; // default
    std::string method = "GET";
    nghttp2::asio_http2::header_map headers;
    std::string body;
    std::string uri;
    bool secure = false;
    bool verbose = false;
    std::string prometheusBindAddress = "0.0.0.0";
    std::string prometheusPort = "8081";
    std::string s_responseDelaySecondsHistogramBucketBoundaries = "";
    std::string s_messageSizeBytesHistogramBucketBoundaries = "";
    ert::metrics::bucket_boundaries_t responseDelaySecondsHistogramBucketBoundaries{};
    ert::metrics::bucket_boundaries_t messageSizeBytesHistogramBucketBoundaries{};


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

    if (cmdOptionExists(argv, argv + argc, "-l", value)
            || cmdOptionExists(argv, argv + argc, "--log-level", value))
    {
        if (!ert::tracing::Logger::setLevel(value))
        {
            usage(EXIT_FAILURE, "Invalid log level provided !");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-v", value)
            || cmdOptionExists(argv, argv + argc, "--verbose", value))
    {
        verbose = true;
    }

    if (cmdOptionExists(argv, argv + argc, "-t", value)
            || cmdOptionExists(argv, argv + argc, "--timeout-seconds", value))
    {
        secondsTimeout = toNumber(value);
        if (secondsTimeout < 1)
        {
            usage(EXIT_FAILURE, "Invalid '--timeout-seconds' value. Must be greater than 0.");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-d", value)
            || cmdOptionExists(argv, argv + argc, "--send-delay-milliseconds", value))
    {
        millisecondsSendDelay = toNumber(value);
        if (millisecondsSendDelay < 0)
        {
            randomSendDelay = true;
            millisecondsSendDelay *= -1;
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-m", method)
            || cmdOptionExists(argv, argv + argc, "--method", value))
    {
        if (method != "POST" && method != "GET" && method != "PUT" && method != "DELETE" && method != "HEAD")
        {
            usage(EXIT_FAILURE, "Invalid '--method' value. Allowed: POST, GET, PUT, DELETE, HEAD.");
        }
    }

    char **next = argv;
    while ((next = cmdOptionExists(next, argv + argc, "--header", value)))
    {
        size_t pos = value.find(":");
        if (pos == 0) pos = value.find(":", 1); // case of :method
        std::string hname, hvalue;
        if (pos != std::string::npos) {
            hname = value.substr(0, pos);
        }
        hvalue = value.substr(pos + 1, value.size());
        headers.emplace(hname, nghttp2::asio_http2::header_value{hvalue});
    }

    if (cmdOptionExists(argv, argv + argc, "-b", value)
            || cmdOptionExists(argv, argv + argc, "--body", value))
    {
        body = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-u", value)
            || cmdOptionExists(argv, argv + argc, "--uri", value))
    {
        uri = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--secure", value))
    {
        secure = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-bind-address", value))
    {
        prometheusBindAddress = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-port", value))
    {
        prometheusPort = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-response-delay-seconds-histogram-boundaries", value))
    {
        s_responseDelaySecondsHistogramBucketBoundaries = loadHistogramBoundaries(value, responseDelaySecondsHistogramBucketBoundaries);
    }

    if (cmdOptionExists(argv, argv + argc, "--prometheus-message-size-bytes-histogram-boundaries", value))
    {
        s_messageSizeBytesHistogramBucketBoundaries = loadHistogramBoundaries(value, messageSizeBytesHistogramBucketBoundaries);
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    // Logger verbosity
    ert::tracing::Logger::verbose(verbose);

    if (UdpSocketPath.empty()) usage(EXIT_FAILURE);
    int i_printEach = (printEach.empty() ? 1:atoi(printEach.c_str()));
    if (i_printEach <= 0) usage(EXIT_FAILURE);
    if (uri.empty()) usage(EXIT_FAILURE);

    std::cout << "UDP socket path: " << UdpSocketPath << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";
    bool disableMetrics = (prometheusPort == "-1");

    if (disableMetrics) {
        std::cout << "Metrics (prometheus): disabled" << '\n';
    }
    else {
        std::cout << "Prometheus local bind address: " << prometheusBindAddress << '\n';
        std::cout << "Prometheus local port: " << prometheusPort << '\n';
        if (!responseDelaySecondsHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'response delay seconds' histogram boundaries: " << s_responseDelaySecondsHistogramBucketBoundaries << '\n';
        }
        if (!messageSizeBytesHistogramBucketBoundaries.empty()) {
            std::cout << "Prometheus 'message size bytes' histogram boundaries: " << s_messageSizeBytesHistogramBucketBoundaries << '\n';
        }
    }

    // Tokenize URI
    std::string authority_path, path;
    std::string host_port, host, port;
    size_t pos = 0;

    pos = uri.find("://");
    authority_path = uri;
    host = uri;
    if (pos != std::string::npos) {
        authority_path = uri.substr(pos + 3);
    }

    if (!authority_path.empty()) {
        pos = authority_path.find("/");
        host_port = authority_path;
        if (pos != std::string::npos) {
            host_port = authority_path.substr(0, pos);
            path = authority_path.substr(pos + 1, authority_path.size());
        }

        if (!host_port.empty()) {
            pos = host_port.find(":");
            host = host_port;
            if (pos != std::string::npos) {
                host = host_port.substr(0, pos);
                port = host_port.substr(pos + 1, host_port.size());
            }
        }
    }

    if (host.empty()) {
        std::cerr << "Invalid URI !" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Collect variable patterns:
    collectVariablePatterns(path, PathPatterns);
    collectVariablePatterns(body, BodyPatterns);
    collectVariablePatterns(ert::http2comm::headersAsString(headers), HeadersPatterns);

    // Detect builtin patterns:
    std::map<std::string, std::string> AllPatterns;
    auto iterateMap = [&AllPatterns](const std::map<std::string, std::string>& map) {
        static std::regex builtinPatternRegex("^udp(?:\\.\\d+)?$");
        for (const auto& it : map) {
            const std::string& value = it.second;
            if (std::regex_match(value, builtinPatternRegex)) AllPatterns[value] = std::string("@{") + value + std::string("}");
        }
    };

    iterateMap(PathPatterns);
    iterateMap(BodyPatterns);
    iterateMap(HeadersPatterns);
    bool builtinPatternsUsed = (!AllPatterns.empty());
    std::string s_builtinPatternsUsed{};
    if (builtinPatternsUsed) for (auto it: AllPatterns) {
            s_builtinPatternsUsed += " " ;
            s_builtinPatternsUsed += it.second;
        }

    std::cout << "Client endpoint:" << '\n';
    std::cout << "   Secure connection: " << (secure ? "true":"false") << '\n';
    std::cout << "   Host:   " << host << '\n';
    if (!port.empty()) std::cout << "   Port:   " << port << '\n';
    std::cout << "   Method: " << method << '\n';
    std::cout << "   Uri: " << uri << '\n';
    if (!path.empty()) std::cout << "   Path:   " << path << '\n';
    if (headers.size() != 0) std::cout << "   Headers: " << ert::http2comm::headersAsString(headers) << '\n';
    if (!body.empty()) std::cout << "   Body: " << body << '\n';
    std::cout << "   Timeout for responses (s): " << secondsTimeout << '\n';
    std::cout << "   Send delay for requests (ms): ";
    if (randomSendDelay) {
        std::cout << "random in [0," << millisecondsSendDelay << "]" << '\n';
    }
    else {
        std::cout << millisecondsSendDelay << '\n';
    }
    std::cout << "   Builtin patterns used:" << (s_builtinPatternsUsed.empty() ? " not detected":s_builtinPatternsUsed) << '\n';


    // Flush:
    std::cout << std::endl;

    // Create client class
    auto client = std::make_shared<ert::http2comm::Http2Client>("myClient", host, port, secure);
    if (!client->isConnected()) {
        std::cerr << "Failed to connect the server. Exiting ..." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create metrics server
    if (!disableMetrics) {
        myMetrics = new ert::metrics::Metrics;

        std::string bindAddressPortPrometheusExposer = prometheusBindAddress + std::string(":") + prometheusPort;
        if(!myMetrics->serve(bindAddressPortPrometheusExposer)) {
            std::cerr << "Initialization error in prometheus interface (" << bindAddressPortPrometheusExposer << "). Exiting ..." << '\n';
            delete (myMetrics);
            exit(EXIT_FAILURE);
        }

        // Enable client metrics:
        client->enableMetrics(myMetrics, responseDelaySecondsHistogramBucketBoundaries, messageSizeBytesHistogramBucketBoundaries);
    }

    // Creating UDP server:
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

    std::cout << std::endl << "Waiting for UDP messages ([sequence] <udp data> (<status codes current statistics>)) ..." << '\n';
    int sequence = 1;
    std::string udpData;

    // Worker threads:
    boost::asio::io_context io_ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> io_ctx_guard = boost::asio::make_work_guard(io_ctx); // this is to avoid terminating io_ctx.run() if no more work is pending
    std::vector<std::thread> myWorkers;
    for (auto i = 0; i < 2 /* two by superstition */; ++i)
    {
        myWorkers.emplace_back([&io_ctx]() {
            io_ctx.run();
        });
    }

    // While loop to read UDP datagrams:
    while ((bytesRead = recvfrom(Sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) > 0) {

        buffer[bytesRead] = '\0'; // Agregar terminador nulo al final del texto leído
        udpData.assign(buffer);

        if (sequence % i_printEach == 0) {
            std::cout << "[" << sequence << "] " << udpData << " (" << statusCodesAsString() << ")" << std::endl;
        }
        sequence++;

        /*
        // WITHOUT DELAY FEATURE:
        boost::asio::post(io_ctx, [&]() {
            auto stream = std::make_shared<Stream>(udpData);
            stream->setRequest(client, method, path, body, headers, secondsTimeout);
            stream->process(false, 0);
        });
        */

        // WITH DELAY FEATURE:
        int delayMs = randomSendDelay ? (rand() % (millisecondsSendDelay + 1)):millisecondsSendDelay;
        auto timer = std::make_shared<boost::asio::steady_timer>(io_ctx, std::chrono::milliseconds(delayMs));
        timer->async_wait([&, timer] (const boost::system::error_code& e) {
            auto stream = std::make_shared<Stream>(udpData);
            stream->setRequest(client, method, path, body, headers, secondsTimeout);
            stream->process();
        });

        // exit condition:
        if (udpData == "EOF") {
            std::cout<<  std::endl << "Existing (EOF received) !" << '\n';
            break;
        }
    }

    // wrap up
    wrapup();

    exit(EXIT_SUCCESS);
}

