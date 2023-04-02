/*
 ______________________________________
|   _     ___      _ _            _    |
|  | |   |__ \    | (_)          | |   |
|  | |__    ) |___| |_  ___ _ __ | |_  |
|  | '_ \  / // __| | |/ _ \ '_ \| __| |  HTTP2 SIMPLE CLIENT HELPER UTILITY
|  | | | |/ /| (__| | |  __/ | | | |_  |  Version 0.0.z
|  |_| |_|____\___|_|_|\___|_| |_|\__| |  https://github.com/testillano/http2comm (tools/h2client)
|______________________________________|

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

// Standard
#include <iostream>
#include <string>
#include <memory>
#include <algorithm>

#include <cctype> // parse uri
#include <functional> // parse uri

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/Http2Client.hpp>


const char* progname;

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[--timeout <value>]\n"
       << "  Time in seconds to wait for request response. Defaults to 5.\n\n"

       << "[--method <POST|GET|PUT|DELETE|HEAD>]\n"
       << "  Request method. Defaults to 'GET'.\n\n"

       << "[--header <value>]\n"
       << "  Header in the form 'name:value'. This parameter can occur multiple times.\n\n"

       << "[--body <value>]\n"
       << "  Plain text for request body content.\n\n"

       << "--uri <value>\n"
       << " URI to access.\n\n"

       << "--secure\n"
       << " Use secure connection.\n\n"

       << "--rc-probe\n"
       << "  Forwards HTTP status code into equivalent program return code.\n"
       << "  So, any code greater than or equal to 200 and less than 400\n"
       << "  indicates success and will return 0 (1 in other case).\n"
       << "  This allows to use the client as HTTP/2 command probe in\n"
       << "  kubernetes where native probe is only supported for HTTP/1.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --timeout 1 --uri http://localhost:8000/book/8472098362" << '\n'
       << "   " << progname << " --method POST --header \"content-type:application/json\" --body '{\"foo\":\"bar\"}' --uri http://localhost:8000/data" << '\n'

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


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Traces
    ert::tracing::Logger::initialize(progname); // initialize logger (before possible myExit() execution):

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    int timeout = 5; // default
    std::string method = "GET";
    nghttp2::asio_http2::header_map headers;
    std::string body;
    std::string uri;
    bool secure = false;
    bool verbose = false;
    bool rcProbe = false;

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

    if (cmdOptionExists(argv, argv + argc, "--timeout", value))
    {
        timeout = toNumber(value);
        if (timeout < 1)
        {
            usage(EXIT_FAILURE, "Invalid '--timeout' value. Must be greater than 0.");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "--method", method))
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

    if (cmdOptionExists(argv, argv + argc, "--body", value))
    {
        body = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--uri", value))
    {
        uri = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--secure", value))
    {
        secure = true;
    }

    if (cmdOptionExists(argv, argv + argc, "--rc-probe", value))
    {
        rcProbe = true;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    // Logger verbosity
    ert::tracing::Logger::verbose(verbose);

    if (uri.empty()) usage(EXIT_FAILURE);

    std::cout << "Timeout: " << timeout << '\n';
    if (headers.size() != 0) std::cout << "Headers: " << ert::http2comm::headersAsString(headers) << '\n';
    if (!body.empty()) std::cout << "Body: " << body << '\n';
    std::cout << "Secure connection: " << (secure ? "true":"false") << '\n';
    std::cout << "Uri: " << uri << '\n';

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

    std::cout << "   Host: " << host << '\n';
    if (!port.empty()) std::cout << "   Port: " << port << '\n';
    if (!path.empty()) std::cout << "   Path: " << path << '\n';

    // Flush:
    std::cout << std::endl;

    // Create client class
    ert::http2comm::Http2Client client("myClient", host, port, secure);

    ert::http2comm::Http2Client::response response = client.send(method, path, body, headers, std::chrono::milliseconds(timeout * 1000));

    int status = response.statusCode;
    std::cout << "Response status code: " << status << ((status == -1) ? " (connection error)":"") <<std::endl;
    if (!response.body.empty()) std::cout << "Response body: " << response.body << std::endl;
    if (response.headers.size() != 0) std::cout << "Response headers: " << ert::http2comm::headersAsString(response.headers) << '\n';

    int rc = (status == -1) ? EXIT_FAILURE:EXIT_SUCCESS;
    if (rcProbe) rc = !(status >= 200 && status < 400);

    LOGWARNING(ert::tracing::Logger::warning("Stopping logger", ERT_FILE_LOCATION));
    ert::tracing::Logger::terminate();

    exit(rc);
}

