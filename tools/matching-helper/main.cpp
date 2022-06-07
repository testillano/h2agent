/*
 _____________________________________________________________________________________
|                   _       _     _                    _          _                   |
|                  | |     | |   (_)                  | |        | |                  |
|   _ __ ___   __ _| |_ ___| |__  _ _ __   __ _   __  | |__   ___| |_ __   ___ _ __   |
|  | '_ ` _ \ / _` | __/ __| '_ \| | '_ \ / _` | |__| | '_ \ / _ \ | '_ \ / _ \ '__|  |  HELPER UTILITY TO TEST std::regex REGULAR EXPRESSIONS
|  | | | | | | (_| | || (__| | | | | | | | (_| |      | | | |  __/ | |_) |  __/ |     |  Version 0.0.z
|  |_| |_| |_|\__,_|\__\___|_| |_|_|_| |_|\__, |      |_| |_|\___|_| .__/ \___|_|     |  https://github.com/testillano/h2agent (matching-helper)
|                                          __/ |                   | |                |
|                                         |___/                    |_|                |
|_____________________________________________________________________________________|

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
#include <regex>


const char* progname;

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc)
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "--regex <value>\n"
       << "  Regex pattern value to match against.\n\n"

       << "--test <value>\n"
       << "  Test string value to be matched.\n\n"

       << "[--fmt <value>]\n"
       << "  Optional regex-replace output format.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --regex \"https://(\\w+).(com|es)/(\\w+)/(\\w+)\" --test \"https://github.com/testillano/h2agent\" --fmt 'User: $3; Project: $4'" << '\n'
       << "   " << progname << " --regex \"(a\\|b\\|)([0-9]{10})\" --test \"a|b|0123456789\" --fmt '$2'" << '\n'
       << "   " << progname << " --regex \"1|3|5|9\" --test 2" << '\n'

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


///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    progname = basename(argv[0]);

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string regex;
    std::string test;
    std::string fmt;

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "--regex", value))
    {
        regex = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--test", value))
    {
        test = value;
    }

    if (cmdOptionExists(argv, argv + argc, "--fmt", value))
    {
        fmt = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (regex.empty()) usage(EXIT_FAILURE);
    if (test.empty()) usage(EXIT_FAILURE);

    std::cout << "Regex: " << regex << '\n';
    std::cout << "Test:  " << test << '\n';
    if (!fmt.empty()) std::cout << "Fmt:   " << fmt << '\n';

    // Flush:
    std::cout << std::endl;

    // Build regular expression:
    std::regex re;
    bool match = false;

    try {
        re.assign(regex, std::regex::optimize);
        match = std::regex_match(test, re);
    }
    catch (std::regex_error &e) {
        std::cerr << "Invalid regular expression: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Match result: " << (match ? "true":"false") << std::endl;

    if (!fmt.empty()) {
        std::cout << "Fmt result  : " << std::regex_replace (test, re, fmt) << std::endl;
        if (!match) std::cout << "              (output regex-replace format is a fallback for non-matched regular expression)" << std::endl;
    }

    exit(match ? EXIT_SUCCESS:EXIT_FAILURE);
}

