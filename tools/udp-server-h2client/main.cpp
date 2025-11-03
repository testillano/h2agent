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
#include <ctime>
#include <iomanip>
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
#define COL1_WIDTH 36 // <timestamp>: date time and microseconds
#define COL2_WIDTH 16 // <sequence>
#define COL3_WIDTH 64 // <udp datagram> 256 is too much, but we could accept UDP datagrams with that size ...
#define COL4_WIDTH 100 // <accumulated status codes>


const char* progname;

// Status codes statistics, like h2load:
// status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx, 3 timeouts, 0 connection errors
std::atomic<unsigned int> STATUS_CODES_2xx{};
std::atomic<unsigned int> STATUS_CODES_3xx{};
std::atomic<unsigned int> STATUS_CODES_4xx{};
std::atomic<unsigned int> STATUS_CODES_5xx{};
std::atomic<unsigned int> TIMEOUTS{};
std::atomic<unsigned int> CONNECTION_ERRORS{};

// Globals
std::map<std::string, std::string> UdpOutputValuePatterns{};
std::map<std::string, std::string> MethodPatterns{};
std::map<std::string, std::string> PathPatterns{};
std::map<std::string, std::string> BodyPatterns{};
std::map<std::string, std::string> HeadersPatterns{};
ert::metrics::Metrics *myMetrics{}; // global to allow signal wrapup
int Sockfd{}; // global to allow signal wrapup
int OutputSockfd{}; // global to allow signal wrapup
std::string UdpSocketPath{}; // global to allow signal wrapup
std::string UdpOutputSocketPath{}; // global to allow signal wrapup
struct sockaddr_un TargetAddr {};

int HardwareConcurrency = std::thread::hardware_concurrency();
int Workers = HardwareConcurrency; // default

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "UDP server will trigger one HTTP/2 request for every reception, replacing optionally\n"
       << "certain patterns on method, uri, headers and/or body provided. Implemented patterns:\n"
       << '\n'
       << "   @{udp}:      replaced by the whole UDP datagram received.\n"
       << "   @{udp8}:     selects the 8 least significant digits in the UDP datagram, and may\n"
       << "                be used to build valid IPv4 addresses for a given sequence.\n"
       << "   @{udp.<n>}:  UDP datagram received may contain a pipe-separated list of tokens\n"
       << "                and this pattern will be replaced by the nth one.\n"
       << "   @{udp8.<n>}: selects the 8 least significant digits in each part if exists.\n"
       << "\n"
       << "To stop the process you can send UDP message 'EOF'.\n"
       << "To print accumulated statistics you can send UDP message 'STATS' or stop/interrupt the process.\n\n"

       << "[--name <name>]\n"
       << "  Application process name. Used in prometheus metrics 'source' label. Defaults to '" << progname << "'.\n\n"

       << "-k|--udp-socket-path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "[-o|--udp-output-socket-path <value>]\n"
       << "  UDP unix output socket path. Written for every response received. This socket must be previously created by UDP server (bind()).\n"
       << "  Try this bash recipe to create an UDP server socket (or use another " << progname << " instance for that):\n"
       << "     $ path=\"/tmp/udp2.sock\"\n"
       << "     $ rm -f ${path}\n"
       << "     $ socat -lm -ly UNIX-RECV:\"${path}\" STDOUT\n\n"

       << "[--udp-output-value <value>]\n"
       << "  UDP datagram to be written on output socket, for every response received. By default,\n"
       << "  original received datagram is used (@{udp}). Same patterns described above are valid for this parameter.\n\n"

       << "[-w|--workers <value>]\n"
       << "  Number of worker threads to post outgoing requests and manage asynchronous timers (timeout, pre-delay).\n"
       << "  Defaults to system hardware concurrency (" << HardwareConcurrency << "), however 2 could be enough.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print UDP receptions each specific amount (must be positive). Defaults to 1.\n"
       << "  Setting datagrams estimated rate should take 1 second/printout and output\n"
       << "  frequency gives an idea about UDP receptions rhythm.\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[-v|--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[-t|--timeout-milliseconds <value>]\n"
       << "  Time in milliseconds to wait for requests response. Defaults to 5000.\n\n"

       << "[-d|--send-delay-milliseconds <value>]\n"
       << "  Time in seconds to delay before sending the request. Defaults to 0.\n"
       << "  It also supports negative values which turns into random number in\n"
       << "  the range [0,abs(value)].\n\n"

       << "[-m|--method <value>]\n"
       << "  Request method. Defaults to 'GET'. After optional parsing, should be one of:\n"
       << "  POST|GET|PUT|DELETE|HEAD.\n\n"

       << "-u|--uri <value>\n"
       << " URI to access.\n\n"

       << "[--header <value>]\n"
       << "  Header in the form 'name:value'. This parameter can occur multiple times.\n\n"

       << "[-b|--body <value>]\n"
       << "  Plain text for request body content. Ignored by underlying library for GET, DELETE and HEAD methods.\n\n"

       << "[--secure]\n"
       << " Use secure connection.\n\n"

       << "[--prometheus-bind-address <address>]\n"
       << "  Prometheus local bind <address>; defaults to 0.0.0.0.\n\n"

       << "[--prometheus-port <port>]\n"
       << "  Prometheus local <port>; defaults to 8081. Value of -1 disables metrics.\n\n"

       << "[--prometheus-response-delay-seconds-histogram-boundaries <comma-separated list of doubles>]\n"
       << "  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.\n"
       << "  Scientific notation is allowed, i.e.: \"100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3\".\n\n"

       << "[--prometheus-message-size-bytes-histogram-boundaries <comma-separated list of doubles>]\n"
       << "  Bucket boundaries for Tx/Rx message size bytes histogram; no boundaries are defined by default.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path /tmp/udp.sock --print-each 1000 --timeout-milliseconds 1000 --uri http://0.0.0.0:8000/book/@{udp} --body \"ipv4 is @{udp8}\"" << '\n'
       << "   " << progname << " --udp-socket-path /tmp/udp.sock --print-each 1000 --method POST --uri http://0.0.0.0:8000/data --header \"content-type:application/json\" --body '{\"book\":\"@{udp}\"}'" << '\n'
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

// status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx, 3 timeouts, 0 connection errors
std::string statsAsString() {
    std::stringstream ss;

    ss << STATUS_CODES_2xx << " 2xx, " << STATUS_CODES_3xx << " 3xx, " << STATUS_CODES_4xx << " 4xx, " << STATUS_CODES_5xx << " 5xx, " << TIMEOUTS << " timeouts, " << CONNECTION_ERRORS << " connection errors";

    return ss.str();
}

void wrapup() {
    close(Sockfd);
    unlink(UdpSocketPath.c_str());
    if (!UdpOutputSocketPath.empty()) {
        unlink(UdpOutputSocketPath.c_str());
        close(OutputSockfd);
    }
    delete (myMetrics);
    LOGWARNING(ert::tracing::Logger::warning("Stopping logger", ERT_FILE_LOCATION));
    ert::tracing::Logger::terminate();

    // Print status codes statistics:
    std::cout << '\n' << "status codes: " << statsAsString() << '\n';
}

void sighndl(int signal)
{
    std::cout << "Signal received: " << signal << '\n';
    std::cout << "Wrap-up and exit ..." << '\n';

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

// Extract least N significant characters from string:
std::string extractLastNChars(const std::string &input, int n) {
    if(input.size() < n) return input;
    return input.substr(input.size() - n);
}


// Transform input in the form "<double>,<double>,..,<double>" to bucket boundaries vector
// Cientific notation is allowed, for example boundary for 150us would be 150e-6
// Returns the final string ignoring non-double values scanned. Also sort is applied.
std::string loadHistogramBoundaries(const std::string &input, ert::metrics::bucket_boundaries_t &boundaries) {
    std::string result;

    std::istringstream ss(input);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            double value = std::stod(item);
            if (value >= 0) {
                boundaries.push_back(value);
                result += (std::to_string(value) + ",");
            }
            else {
                std::cerr << "Ignoring negative double: " << item << "\n";
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "Ignoring invalid double: " << item << "\n";
        } catch (const std::out_of_range& e) {
            std::cerr << "Ignoring out-of-range double: " << item << "\n";
        }
    }

    // Sort surviving numbers:
    std::sort(boundaries.begin(), boundaries.end());

    result.pop_back(); // remove last comma

    return result;
}

class Stream {
    std::string data_{}; // UDP

    std::map<std::string, std::string> variables_;

    std::shared_ptr<ert::http2comm::Http2Client> client_{};
    std::string udp_output_value_{};
    std::string method_{};
    std::string path_{};
    std::string body_{};
    nghttp2::asio_http2::header_map headers_{};
    int timeout_ms_{};
    int delay_ms_{};

public:
    Stream(const std::string &data) : data_(data) {;}

    // Set HTTP/2 components
    void setRequest(std::shared_ptr<ert::http2comm::Http2Client> client, const std::string &method, const std::string &path, const std::string &body, nghttp2::asio_http2::header_map &headers, int millisecondsTimeout, int millisecondsDelay, const std::string &udpOutputValue) {
        client_ = client;
        udp_output_value_ = udpOutputValue;
        method_ = method;
        path_ = path;
        body_ = body;
        headers_ = headers;
        timeout_ms_ = millisecondsTimeout;
        delay_ms_ = millisecondsDelay;

        // Main variable @{udp}
        variables_["udp"] = data_;

        // Reserved variables:
        // @{udp8}
        variables_["udp8"] = extractLastNChars(data_, 8);

        // Variable parts: @{udp[8].1}, @{udp[8].2}, etc.
        char delimiter = '|';
        if (data_.find(delimiter) == std::string::npos)
            return;

        const std::string patternPrefixes[] = {"udp", "udp8"};
        for (const std::string& patternPrefix : patternPrefixes) {
            std::size_t start = 0;
            std::size_t pos = data_.find(delimiter);
            std::string var, val;
            int count = 1;
            while (pos != std::string::npos) {
                var = patternPrefix;
                var += ".";
                var += std::to_string(count);
                count++;
                val = data_.substr(start, pos - start);
                if (patternPrefix == "udp8") val = extractLastNChars(val, 8);
                variables_[var] = val;
                start = pos + 1;
                pos = data_.find(delimiter, start);
            }
            // add latest
            var = patternPrefix;
            var += ".";
            var += std::to_string(count);
            val = data_.substr(start);
            if (patternPrefix == "udp8") val = extractLastNChars(val, 8);
            variables_[var] = val;
        }
    }

    std::string getUdpOutputValue() const { // variables substitution
        std::string result = udp_output_value_;
        replaceVariables(result, UdpOutputValuePatterns, variables_);
        return result;
    }

    std::string getMethod() const { // variables substitution
        std::string result = method_;
        replaceVariables(result, MethodPatterns, variables_);
        return result;
    }

    std::string getPath() const { // variables substitution
        std::string result = path_;
        replaceVariables(result, PathPatterns, variables_);
        return result;
    }

    std::string getBody() const { // variables substitution
        std::string result = body_;
        replaceVariables(result, BodyPatterns, variables_);
        return result;
    }

    nghttp2::asio_http2::header_map getHeaders() const { // variables substitution
        nghttp2::asio_http2::header_map result;
        std::string aux;

        for(auto it = headers_.begin(); it != headers_.end(); it ++) {
            aux = it->second.value;
            replaceVariables(aux, HeadersPatterns, variables_);
            result.emplace(it->first, nghttp2::asio_http2::header_value{aux});
        }

        return result;
    }

    int getMillisecondsTimeout() const { // No sense to do substitutions
        return timeout_ms_;
    }

    int getMillisecondsDelay() const { // No sense to do substitutions
        return delay_ms_;
    }

    // Process reception
    void process() {

        // Callback:
        ert::http2comm::Http2Client::ResponseCallback response_handler = [this](ert::http2comm::Http2Client::response res) -> void {
            // Log debug the response, and store status codes statistics:
            int status = res.statusCode;

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
            else if (status == -2) {
                TIMEOUTS++;
            }
            else if (status == -1 || status == -3 || status == -4) {
                CONNECTION_ERRORS++;
            }

            LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("[process] Data processed: %s", data_.c_str()), ERT_FILE_LOCATION));

            if (!UdpOutputSocketPath.empty()) {
                // sendto is thread-safe:
                const char *dataToSend = getUdpOutputValue().c_str();
                ssize_t bytes_sent = sendto(OutputSockfd, dataToSend, strlen(dataToSend), 0, (struct sockaddr*)&TargetAddr, sizeof(struct sockaddr_un));
                if (bytes_sent == -1) {
                    ert::tracing::Logger::error(ert::tracing::Logger::asString("Error sending datagram to %s (check if socket was created by an UDP server)", UdpOutputSocketPath.c_str()), ERT_FILE_LOCATION);
                } else {
                    LOGDEBUG(
                        std::string msg = ert::tracing::Logger::asString("Successful sent (%zd bytes) to: %s", bytes_sent, UdpOutputSocketPath.c_str());
                        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                    );
                }
            }
        };

        // Asynchronous send:
        client_->asyncSend(getMethod(), getPath(), getBody(), getHeaders(), response_handler, std::chrono::milliseconds(getMillisecondsTimeout()), std::chrono::milliseconds(getMillisecondsDelay()));
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
    std::string applicationName = progname;
    int i_printEach = 1; // default
    int millisecondsTimeout = 5000; // default
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
    std::string udpOutputValue = "@{udp}";


    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "--name", value))
    {
        applicationName = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--udp-socket-path", value))
    {
        UdpSocketPath = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-o", value)
            || cmdOptionExists(argv, argv + argc, "--udp-output-socket-path", value))
    {
        UdpOutputSocketPath = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--udp-output-value", value))
    {
        udpOutputValue = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-w", value)
            || cmdOptionExists(argv, argv + argc, "--workers", value))
    {
        Workers = toNumber(value);
        if (Workers <= 0)
        {
            usage(EXIT_FAILURE, "Invalid '-w|--workers' value. Must be positive.");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-e", value)
            || cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        i_printEach = atoi(value.c_str());
        if (i_printEach <= 0) usage(EXIT_FAILURE);
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
            || cmdOptionExists(argv, argv + argc, "--timeout-milliseconds", value))
    {
        millisecondsTimeout = toNumber(value);
        if (millisecondsTimeout < 0)
        {
            usage(EXIT_FAILURE, "Invalid '--timeout-milliseconds' value. Must be greater than 0.");
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

    if (cmdOptionExists(argv, argv + argc, "-m", value)
            || cmdOptionExists(argv, argv + argc, "--method", value))
    {
        method = value;
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
    if (uri.empty()) usage(EXIT_FAILURE);

    std::cout << "Application/process name: " << applicationName << '\n';
    std::cout << "UDP socket path: " << UdpSocketPath << '\n';
    if (!UdpOutputSocketPath.empty()) std::cout << "UDP output socket path: " << UdpOutputSocketPath << '\n';
    if (!udpOutputValue.empty()) std::cout << "UDP output value: " << udpOutputValue << '\n';
    std::cout << "Workers: " << Workers << '\n';
    std::cout << "Log level: " << ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel()) << '\n';
    std::cout << "Verbose (stdout): " << (verbose ? "true":"false") << '\n';
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
        std::cerr << "Invalid URI !" << '\n';
        exit(EXIT_FAILURE);
    }

    // Collect variable patterns:
    collectVariablePatterns(udpOutputValue, UdpOutputValuePatterns);
    collectVariablePatterns(method, MethodPatterns);
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

    iterateMap(UdpOutputValuePatterns);
    iterateMap(MethodPatterns);
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
    std::cout << "   Timeout for responses (ms): " << millisecondsTimeout << '\n';
    std::cout << "   Send delay for requests (ms): ";
    if (randomSendDelay) {
        std::cout << "random in [0," << millisecondsSendDelay << "]" << '\n';
    }
    else {
        std::cout << millisecondsSendDelay << '\n';
    }
    std::cout << "   Builtin patterns used:" << (s_builtinPatternsUsed.empty() ? " not detected":s_builtinPatternsUsed) << '\n';
    std::cout << '\n';

    auto client = std::make_shared<ert::http2comm::Http2Client>("udp_server_h2client", host, port, secure);
    if (!client->isConnected()) {
        std::cerr << "WARNING: failed to connect the server. This will be done later in lazy mode ..." << '\n' << '\n';
    }

    // Create metrics server
    std::string bindAddressPortPrometheusExposer{};
    if (!disableMetrics) {
        myMetrics = new ert::metrics::Metrics;

        bindAddressPortPrometheusExposer = prometheusBindAddress + std::string(":") + prometheusPort;
        if(!myMetrics->serve(bindAddressPortPrometheusExposer)) {
            std::cerr << "Initialization error in prometheus interface (" << bindAddressPortPrometheusExposer << "). Exiting ..." << '\n';
            delete (myMetrics);
            exit(EXIT_FAILURE);
        }

        // Enable client metrics:
        client->enableMetrics(myMetrics, responseDelaySecondsHistogramBucketBoundaries, messageSizeBytesHistogramBucketBoundaries, applicationName/*source label*/);
    }

    // Creating UDP server ////////////////////////////////////////////////////
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


    // Creating UDP client ////////////////////////////////////////////////////
    if (!UdpOutputSocketPath.empty()) {

        OutputSockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (OutputSockfd < 0) {
            perror("Error creating UDP output socket");
            exit(EXIT_FAILURE);
        }

        memset(&TargetAddr, 0, sizeof(struct sockaddr_un));
        TargetAddr.sun_family = AF_UNIX;
        strncpy(TargetAddr.sun_path, UdpOutputSocketPath.c_str(), sizeof(TargetAddr.sun_path) - 1);
    }


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
    if (!disableMetrics) std::cout << " To get prometheus metrics:       curl http://" << bindAddressPortPrometheusExposer << "/metrics" << '\n';
    std::cout << " To send ad-hoc UDP message:      echo -n <data> | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';
    std::cout << " To print accumulated statistics: echo -n STATS  | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';
    std::cout << " To stop process:                 echo -n EOF    | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';

    std::cout << '\n';
    std::cout << '\n';
    std::cout << "Waiting for UDP messages..." << '\n' << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << "<timestamp>"
              << std::setw(COL2_WIDTH) << std::left << "<sequence>"
              << std::setw(COL3_WIDTH) << std::left << "<udp datagram>"
              << std::setw(COL4_WIDTH) << std::left << "<accumulated status codes>" << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << std::string(COL1_WIDTH-1, '_')
              << std::setw(COL2_WIDTH) << std::left << std::string(COL2_WIDTH-1, '_')
              << std::setw(COL3_WIDTH) << std::left << std::string(COL3_WIDTH-1, '_')
              << std::setw(COL4_WIDTH) << std::left << std::string(COL4_WIDTH-1, '_') << '\n';

    std::string udpData;

    // Worker threads:
    boost::asio::io_context io_ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> io_ctx_guard = boost::asio::make_work_guard(io_ctx); // this is to avoid terminating io_ctx.run() if no more work is pending
    std::vector<std::thread> myWorkers;
    for (auto i = 0; i < Workers; ++i)
    {
        myWorkers.emplace_back([&io_ctx]() {
            io_ctx.run();
        });
    }

    // While loop to read UDP datagrams:
    unsigned int sequence{};
    while ((bytesRead = recvfrom(Sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) > 0) {

        buffer[bytesRead] = '\0'; // Agregar terminador nulo al final del texto leído
        udpData.assign(buffer);

        // exit condition:
        if (udpData == "EOF") {
            std::cout<<  '\n' << "Exiting (EOF received) !" << '\n';
            break;
        }
        else if (udpData == "STATS") {
            std::cout << std::setw(COL1_WIDTH) << std::left << "-"
                      << std::setw(COL2_WIDTH) << std::left << "-"
                      << std::setw(COL3_WIDTH) << std::left << "STATS"
                      << std::setw(COL4_WIDTH) << std::left << statsAsString() << '\n';
        }
        else {
            if (sequence % i_printEach == 0 || (sequence == 0) /* first one always shown :-)*/) {
                std::cout << std::setw(COL1_WIDTH) << std::left << ert::tracing::getLocaltime()
                          << std::setw(COL2_WIDTH) << std::left << sequence
                          << std::setw(COL3_WIDTH) << std::left << udpData
                          << std::setw(COL4_WIDTH) << std::left << statsAsString() << '\n';
            }
            sequence++;

            boost::asio::post(io_ctx, [&]() {
                auto stream = std::make_shared<Stream>(udpData);
                int delayMs = randomSendDelay ? (rand() % (millisecondsSendDelay + 1)):millisecondsSendDelay;
                stream->setRequest(client, method, path, body, headers, millisecondsTimeout, delayMs, udpOutputValue);
                stream->process();
            });
        }
    }

    // Join workers:
    for (auto &w: myWorkers) w.join();

    // wrap up
    wrapup();

    exit(EXIT_SUCCESS);
}

