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

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace h2agent {
namespace migrate {

/** Insertion-ordered json: provisions keep their field order when rewritten. */
using Json = nlohmann::ordered_json;

/**
 * Raised when a rule cannot be converted. The message starts with the
 * location of the offending field (json pointer within the input document),
 * e.g. "/0/transform/3/target: cannot identify v1 target 'foo.bar'".
 */
class MigrationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * Transformation grammar v1 <-> v2 converter (docs/developers/GrammarV2.md).
 *
 * This is a SYNTAX-ONLY step: every v2 rule produced behaves exactly as its v1
 * origin. The v1 parsing mirrors the former v1 runtime parser (same prefixes,
 * same precedence), but with strict literal dots: inputs rejected here are
 * inputs that the v1 json schema already rejected.
 *
 * The reverse direction (v2 -> v1) exists as a test oracle (v1 -> v2 -> v1 is
 * the identity on canonical v1) and as a diagnostic aid. It only accepts the
 * subset of v2 that has a v1 equivalent.
 *
 * Mode-agnostic on purpose: every v1 string maps to a single v2 kind whatever
 * the provision type (server, client request, client response); whether a kind
 * is applicable in a context is checked by the schema/parser, not here. The
 * only exception is event addressing ('serverEvent'/'clientEvent'), whose
 * accepted fields depend on the role (source or purge target).
 */
class GrammarMigrator {
public:
    /** Grammar found in a document (provision, provision array or rule). */
    enum class Grammar { None, V1, V2, Mixed };

    /** Converts one v1 source string into its v2 'from' object ({"source": <kind>, ...}). */
    static Json sourceToV2(const std::string &v1, const std::string &where = "");

    /** Converts one v2 'from' value into its v1 source string. */
    static std::string sourceToV1(const Json &v2, const std::string &where = "");

    /**
     * Converts one v1 target string into its v2 'to' value.
     * @param castType output: the cast the v1 target implied ("" when none),
     *        to be appended as {"op": "cast", "type": <castType>} at the end of
     *        the filter list.
     */
    static Json targetToV2(const std::string &v1, std::string &castType, const std::string &where = "");

    /**
     * Converts one v2 'to' object (plus the trailing cast type, "" if none) into
     * its v1 target string.
     */
    static std::string targetToV1(const Json &v2, const std::string &castType, const std::string &where = "");

    /** Converts one v1 filter ("Size" or a one-key object) into its v2 item ({"op": ...}). */
    static Json filterToV2(const Json &v1, const std::string &where = "");

    /** Converts one v2 filter item (except the cast) into its v1 filter. */
    static Json filterToV1(const Json &v2, const std::string &where = "");

    /** Converts one v1 rule (source/target/filter/onFilterFail), recursively. */
    static Json ruleToV2(const Json &v1Rule, const std::string &where = "");

    /** Converts one v2 rule (from/filter/to/onFilterFail), recursively. */
    static Json ruleToV1(const Json &v2Rule, const std::string &where = "");

    /**
     * Converts a provisions document: a single provision object or an array of
     * them (server or client). Only the 'transform' and 'onResponseTransform'
     * arrays are rewritten; every other field is kept untouched and in order.
     * A document already (fully or partially) in the target grammar is an error.
     */
    static Json provisionsToV2(const Json &doc);
    static Json provisionsToV1(const Json &doc);

    /** Grammar used by the rules found in a provisions document. */
    static Grammar detect(const Json &doc);

    /** One finding of scan(). Line is 1-based (0: whole file). */
    struct ScanHit {
        std::string file;
        std::size_t line;
        std::string kind;  // "admin-v1-path", "v1-provision", "v1-rule-literal", "v5-stop-target", "v5-vault-wait-path", "v5-removed-counter", "v53-failed-branch", "v53-colon-state", "v53-encoded-uri", "v53-raw-illegal-uri"
        std::string text;
    };

    /**
     * Recursively scans a directory (or a single file) for v1 leftovers:
     * '/admin/v1' references, json files holding v1 provisions and v1 rule
     * literals embedded in other files (scripts, tests); and for the h2agent
     * 5.0-5.2 items removed in 5.3.0: 'stop' targets, the vault wait path form
     * and the former unexpected response status code counter; and the 5.3.0
     * items changed in 5.4.0: client error branches '__core:failed:<x>' (now a
     * stage), user states with ':', and server uris written one decoding level
     * below the wire ('%XX' in a path) or with a character not allowed raw in a
     * path (most likely sent encoded). Binary files and '.git', 'build' and
     * 'node_modules' directories are skipped.
     */
    static std::vector<ScanHit> scan(const std::string &path);

    /**
     * Percent-encodes the given characters in the path part of the server uris
     * of a provisions document (5.4.0: the server uri is the received one, not
     * decoded): server provision 'requestUri', server matching 'rgx', the
     * 'serverEvent' 'requestUri' / 'requestUriStartsWith' (server and client
     * provisions), the foreign 'outState' 'uri', and the 'rgx' of 'regexCapture'
     * / 'regexReplace' over 'requestUri' / 'requestUriPath'. The query part (from
     * the first '?', or '\?' in a regular expression) and '@{...}' / '#{...}'
     * are kept. In a regular expression an escaped character ('\|') is always
     * encoded, an unescaped regular expression metacharacter never (it is
     * syntax); a 'requestUri' is a regular expression when it holds '(', '[',
     * '\', '*', '+', '^' or '$'. Client provision uris are not touched (they are
     * sent as written). Encoding '%' turns one level ('%3B') into the wire form
     * of a double encoding ('%253B').
     *
     * @param changes number of values changed (output)
     */
    static Json encodeUris(const Json &doc, const std::string &chars, std::size_t &changes);
};

}  // namespace migrate
}  // namespace h2agent
