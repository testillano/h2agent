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

#include <libgen.h> // basename

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <GrammarMigrator.hpp>

using h2agent::migrate::GrammarMigrator;
using h2agent::migrate::Json;
using h2agent::migrate::MigrationError;

namespace {

// Exit codes: scan findings are not an error, but must be detectable by CI.
constexpr int ExitOk = 0;
constexpr int ExitFindings = 1;
constexpr int ExitError = 2;

const char *progname;

void usage(int rc) {
    auto &ss = (rc == ExitOk) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "-i|--in <file|->\n"
       << "  Provisions file to convert (server or client; a single provision or an array).\n"
       << "  Use '-' to read from standard input. Only 'transform' and 'onResponseTransform'\n"
       << "  rules are rewritten; any other field is kept untouched and in order.\n\n"

       << "[-o|--out <file|->]\n"
       << "  Output file; defaults to standard output ('-'). It may be the input file.\n\n"

       << "[-r|--reverse]\n"
       << "  Convert v2 into v1 instead (diagnostics). Only the v2 subset with a v1\n"
       << "  equivalent is accepted.\n\n"

       << "[-e|--encode-uri <characters>]\n"
       << "  With '--in': percent-encode the given characters in the path part of the\n"
       << "  server uris (h2agent 5.4.0 compares the uri as received, not decoded):\n"
       << "  server provision 'requestUri', server matching 'rgx', 'serverEvent'\n"
       << "  'requestUri'/'requestUriStartsWith', foreign 'outState' 'uri', and the 'rgx'\n"
       << "  of 'regexCapture'/'regexReplace' over 'requestUri'/'requestUriPath'. The\n"
       << "  query part, '@{...}' and '#{...}' are kept; in a regular expression an\n"
       << "  escaped character ('\\|') is encoded, a metacharacter is not. Client\n"
       << "  provision uris are not touched. '%' turns '%3B' into '%253B' (the wire form\n"
       << "  of a double encoding). v1 rules are converted first; v2 ones are kept.\n\n"

       << "-s|--scan <directory|file>\n"
       << "  Report v1 leftovers: '/admin/v1' references, json files holding v1 provisions\n"
       << "  and v1 rule literals embedded in scripts or tests; and h2agent 5.0-5.2 items\n"
       << "  removed in 5.3.0: 'stop' targets (use 'break', or 'fail' for a failure), the\n"
       << "  'vault/<key>/wait' path form and the former unexpected response status code\n"
       << "  counter; and h2agent 5.3.0 items changed in 5.4.0: client error branches\n"
       << "  '__core:failed:<x>' (a reason in 5.3.0, a stage now), user states with ':',\n"
       << "  server uris with '%XX' in the path (one decoding level below the wire in\n"
       << "  5.3.0, the wire in 5.4.0) and with characters not allowed raw in a path\n"
       << "  (most likely sent encoded: see '--encode-uri')\n"
       << "  ('.git', 'build' and 'node_modules' are skipped). Exit code is 1 when\n"
       << "  something is found.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Grammar v1 -> v2 is a syntax-only step: each v2 rule behaves as its v1 origin,\n"
       << "except that stored values keep their json type (mapping and store values in\n"
       << "docs/developers/GrammarV2.md).\n\n"

       << "Exit codes: 0 success, 1 scan findings, 2 error.\n\n"

       << "Examples:\n"
       << "   " << progname << " --in server-provision.json --out server-provision.v2.json\n"
       << "   " << progname << " --in client-provision.json --out client-provision.json\n"
       << "   cat provision.json | " << progname << " --in -\n"
       << "   " << progname << " --scan ./ct\n"
       << "   " << progname << " --in server-provision.json --out server-provision.json --encode-uri '|'\n"

       << '\n';

    exit(rc);
}

bool readAll(const std::string &path, std::string &content) {
    std::ostringstream ss;
    if (path == "-") {
        ss << std::cin.rdbuf();
    } else {
        std::ifstream in(path);
        if (!in) return false;
        ss << in.rdbuf();
    }
    content = ss.str();
    return true;
}

bool writeAll(const std::string &path, const std::string &content) {
    if (path == "-") {
        std::cout << content;
        return static_cast<bool>(std::cout);
    }
    std::ofstream out(path, std::ios::trunc);
    out << content;
    return static_cast<bool>(out);
}

int convert(const std::string &in, const std::string &out, bool reverse, const std::string &encodeChars) {
    std::string content;
    if (!readAll(in, content)) {
        std::cerr << "Cannot read '" << in << "'" << std::endl;
        return ExitError;
    }

    Json doc;
    try {
        doc = Json::parse(content);
    } catch (const std::exception &e) {
        std::cerr << "Invalid json in '" << in << "': " << e.what() << std::endl;
        return ExitError;
    }

    const auto grammar = GrammarMigrator::detect(doc);
    if (encodeChars.empty() && grammar == GrammarMigrator::Grammar::None) {
        std::cerr << "Note: no transformation rules found in '" << in << "' (copied unchanged)" << std::endl;
    }

    Json result;
    try {
        if (!encodeChars.empty()) {
            // grammar step only for v1 rules: a v2 (or rule-less) document is kept as it is
            result = (grammar == GrammarMigrator::Grammar::V1 || grammar == GrammarMigrator::Grammar::Mixed) ? GrammarMigrator::provisionsToV2(doc) : doc;
            std::size_t changes = 0;
            result = GrammarMigrator::encodeUris(result, encodeChars, changes);
            std::cerr << changes << " uri value(s) encoded in '" << in << "'" << std::endl;
        }
        else {
            result = reverse ? GrammarMigrator::provisionsToV1(doc) : GrammarMigrator::provisionsToV2(doc);
        }
    } catch (const MigrationError &e) {
        std::cerr << in << ": " << e.what() << std::endl;
        return ExitError;
    }

    if (!writeAll(out, result.dump(2) + "\n")) {
        std::cerr << "Cannot write '" << out << "'" << std::endl;
        return ExitError;
    }
    return ExitOk;
}

int scan(const std::string &path) {
    std::vector<GrammarMigrator::ScanHit> hits;
    try {
        hits = GrammarMigrator::scan(path);
    } catch (const MigrationError &e) {
        std::cerr << e.what() << std::endl;
        return ExitError;
    }
    for (const auto &hit : hits) {
        std::cout << hit.file << ":" << hit.line << ": " << hit.kind << ": " << hit.text << '\n';
    }
    std::cout << hits.size() << " finding(s)" << std::endl;
    return hits.empty() ? ExitOk : ExitFindings;
}

}  // namespace

int main(int argc, char *argv[]) {
    progname = basename(argv[0]);

    std::string in, out = "-", scanPath, encodeChars;
    bool reverse = false;

    for (int i = 1; i < argc; i++) {
        const std::string opt = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for '" << opt << "'\n\n";
                usage(ExitError);
            }
            return argv[++i];
        };

        if (opt == "-h" || opt == "--help") usage(ExitOk);
        else if (opt == "-i" || opt == "--in") in = value();
        else if (opt == "-o" || opt == "--out") out = value();
        else if (opt == "-r" || opt == "--reverse") reverse = true;
        else if (opt == "-s" || opt == "--scan") scanPath = value();
        else if (opt == "-e" || opt == "--encode-uri") {
            encodeChars = value();
            if (encodeChars.empty()) {
                std::cerr << "Empty value for '" << opt << "'\n\n";
                usage(ExitError);
            }
        }
        else {
            std::cerr << "Unknown option '" << opt << "'\n\n";
            usage(ExitError);
        }
    }

    if (in.empty() == scanPath.empty()) {
        std::cerr << "Provide exactly one of '--in' or '--scan'\n\n";
        usage(ExitError);
    }

    if (!encodeChars.empty() && (in.empty() || reverse)) {
        std::cerr << "'--encode-uri' goes with '--in' (and not with '--reverse')\n\n";
        usage(ExitError);
    }

    return scanPath.empty() ? convert(in, out, reverse, encodeChars) : scan(scanPath);
}
