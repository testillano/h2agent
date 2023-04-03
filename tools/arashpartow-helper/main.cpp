/*
 _____________________________________________________________________________________________________
|                       _                      _                       _          _                   |
|                      | |                    | |                     | |        | |                  |
|    __ _ _ __ __ _ ___| |__  _ __   __ _ _ __| |_ _____      __  __  | |__   ___| |_ __   ___ _ __   |
|   / _` | '__/ _` / __| '_ \| '_ \ / _` | '__| __/ _ \ \ /\ / / |__| | '_ \ / _ \ | '_ \ / _ \ '__|  |  HELPER UTILITY TO RUN ARASH PARTOW'S MATHEMATICAL OPERATIONS
|  | (_| | | | (_| \__ \ | | | |_) | (_| | |  | || (_) \ V  V /       | | | |  __/ | |_) |  __/ |     |  Version 0.0.z
|   \__,_|_|  \__,_|___/_| |_| .__/ \__,_|_|   \__\___/ \_/\_/        |_| |_|\___|_| .__/ \___|_|     |  https://github.com/testillano/h2agent (tools/arashpartow-helper)
|                            | |                                                   | |                |
|                            |_|                                                   |_|                |
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

#include <libgen.h> // basename

// Standard
#include <iostream>
#include <string>

// 3rd party
#include <arashpartow/exprtk.hpp>


const char* progname;

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc)
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "--expression <value>\n"
       << "  Expression to be calculated.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --expression \"(1+sqrt(5))/2\"" << '\n'
       << "   " << progname << " --expression \"404 == 404\"" << '\n'
       << "   " << progname << " --expression \"cos(3.141592)\"" << '\n'

       << "\nArash Partow help: https://raw.githubusercontent.com/ArashPartow/exprtk/master/readme.txt"

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
    std::string expression_string;

    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "--expression", value))
    {
        expression_string = value;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    if (expression_string.empty()) usage(EXIT_FAILURE);

    std::cout << "Expression: " << expression_string << '\n';

    // Flush:
    std::cout << std::endl;

    // Calculate:
    typedef exprtk::expression<double>   expression_t;
    typedef exprtk::parser<double>       parser_t;

    expression_t   expression;
    parser_t       parser;

    parser.compile(expression_string, expression);
    std::cout << "Result: " << expression.value() << std::endl; // string stream formatter involved

    //exit(match ? EXIT_SUCCESS:EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}

