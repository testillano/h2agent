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

// Standard
#include <string>
#include <vector>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace adminSchemas
{

//////////////////////////////////////////////////////////////////////////////
// Transformation grammar v2 (docs/developers/GrammarV2.md)
//
// Rule: {"from": <source>, "to": <target>, "filter": [<filter>...], "onFilterFail": [<rule>...]}
// A kind without arguments is a plain string ("requestHeaders"); a kind with
// arguments is an object with exactly one kind key plus its sibling fields
// ({"vault": "K", "path": "/a"}). Each kind has exactly one valid form.
//
// The schema is built from per-kind tables so that each provision context
// (server transform, client transform, client onResponseTransform) lists the
// kinds it accepts, exactly as the v1 schema patterns did.
//////////////////////////////////////////////////////////////////////////////
namespace grammar
{

/** Json-pointer argument: "" (whole document) or "/<node>...". */
inline nlohmann::json pointerArg() {
    return {{"type", "string"}, {"pattern", "^(/.*)?$"}};
}
inline nlohmann::json textArg() {
    return {{"type", "string"}, {"minLength", 1}};
}
inline nlohmann::json anyTextArg() {
    return {{"type", "string"}};
}
inline nlohmann::json vaultKeyArg() {
    return {{"type", "string"}, {"pattern", "^[^.]+$"}};
}
inline nlohmann::json nodePathArg() {
    return {{"type", "string"}, {"pattern", "^/.+$"}};
}

/** Object kind: exactly the kind key (mandatory) plus optional/mandatory siblings. */
inline nlohmann::json objectKind(const std::string &kind, const nlohmann::json &arg,
                                 const nlohmann::json &siblings = nlohmann::json::object(),
                                 const std::vector<std::string> &requiredSiblings = {}) {
    nlohmann::json properties = siblings;
    properties[kind] = arg;
    nlohmann::json required = nlohmann::json::array({kind});
    for (const auto &s : requiredSiblings) required.push_back(s);
    return {{"type", "object"}, {"additionalProperties", false}, {"required", required}, {"properties", properties}};
}

inline const nlohmann::json &objectSources() {
    static const nlohmann::json kinds = {
        {"requestUriParam", objectKind("requestUriParam", textArg())},
        {"requestBody", objectKind("requestBody", pointerArg())},
        {"responseBody", objectKind("responseBody", pointerArg())},
        {"requestHeader", objectKind("requestHeader", textArg())},
        {"responseHeader", objectKind("responseHeader", textArg())},
        {"math", objectKind("math", anyTextArg())},
        {"random", objectKind("random", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"minItems", 2}, {"maxItems", 2}})},
        {"randomset", objectKind("randomset", {{"type", "array"}, {"items", {{"type", "string"}}}, {"minItems", 1}})},
        {"timestamp", objectKind("timestamp", {{"type", "string"}, {"enum", {"s", "ms", "us", "ns"}}})},
        {"strftime", objectKind("strftime", textArg())},
        {"var", objectKind("var", textArg())},
        {"vault", objectKind("vault", vaultKeyArg(), {{"path", nodePathArg()}})},
        {"value", objectKind("value", anyTextArg())},
        {"serverEvent", objectKind("serverEvent", textArg())},
        {"clientEvent", objectKind("clientEvent", textArg())},
        {"txtFile", objectKind("txtFile", textArg())},
        {"binFile", objectKind("binFile", textArg())},
        {"command", objectKind("command", textArg())},
    };
    return kinds;
}

inline const nlohmann::json &objectTargets() {
    static const nlohmann::json kinds = {
        {"responseBody", objectKind("responseBody", pointerArg())},
        {"requestBody", objectKind("requestBody", pointerArg())},
        {"responseHeader", objectKind("responseHeader", textArg())},
        {"requestHeader", objectKind("requestHeader", textArg())},
        {"var", objectKind("var", textArg())},
        {"vault", objectKind("vault", vaultKeyArg(), {{"path", nodePathArg()}})},
        {   "foreignOutState", objectKind("foreignOutState", {{"type", "string"}, {"enum", {"POST", "GET", "PUT", "DELETE", "HEAD"}}},
            {{"uri", textArg()}})
        },
        {"txtFile", objectKind("txtFile", textArg())},
        {"binFile", objectKind("binFile", textArg())},
        {   "udpSocket", objectKind("udpSocket", textArg(),
            {{"delayMs", {{"oneOf", {{{"type", "integer"}, {"minimum", 0}}, textArg()}}}}})
        },
        {"serverEvent", objectKind("serverEvent", textArg())},
        {"clientEvent", objectKind("clientEvent", textArg())},
        {"clientProvision", objectKind("clientProvision", vaultKeyArg(), {{"inState", textArg()}}, {"inState"})},
    };
    return kinds;
}

/**
 * One kind slot ('from' or 'to'): plain-string kinds as an enum, object kinds
 * as one exclusive branch each. 'kinds' lists every kind accepted in the context.
 */
inline nlohmann::json slot(const std::vector<std::string> &kinds, const nlohmann::json &objectKinds) {
    nlohmann::json plain = nlohmann::json::array();
    nlohmann::json branches = nlohmann::json::array();
    for (const auto &kind : kinds) {
        auto it = objectKinds.find(kind);
        if (it != objectKinds.end()) branches.push_back(*it);
        else plain.push_back(kind);
    }
    if (!plain.empty()) branches.insert(branches.begin(), nlohmann::json{{"type", "string"}, {"enum", plain}});
    return {{"oneOf", branches}};
}

/** Rule definition for one context; onFilterFail rules belong to the same context. */
inline nlohmann::json rule(const std::string &fromRef, const std::string &toRef, const std::string &selfRef) {
    return {
        {"type", "object"},
        {"additionalProperties", false},
        {"required", {"from", "to"}},
        {   "properties", {
                {"from", {{"$ref", fromRef}}},
                {"to", {{"$ref", toRef}}},
                {"filter", {{"type", "array"}, {"minItems", 1}, {"items", {{"$ref", "#/definitions/filter"}}}}},
                {"onFilterFail", {{"type", "array"}, {"minItems", 1}, {"items", {{"$ref", selfRef}}}}}
            }
        }
    };
}

/** Rules array (provision 'transform' / 'onResponseTransform' value). */
inline nlohmann::json rules(const std::string &ruleRef, bool nonEmpty) {
    nlohmann::json r = {{"type", "array"}, {"items", {{"$ref", ruleRef}}}};
    if (nonEmpty) r["minItems"] = 1;
    return r;
}

/** Adds the 'as' cast to the filter definition (last element of a filter list). */
inline void addCastFilter(nlohmann::json &filterDefinition) {
    nlohmann::json &object = filterDefinition["oneOf"][1];
    object["oneOf"].push_back({{"required", {"as"}}});
    object["properties"]["as"] = {{"type", "string"}, {"enum", {"string", "integer", "unsigned", "float", "boolean", "object", "json"}}};
}

// Kinds accepted per context (same scope as the v1 schema patterns):
inline const std::vector<std::string> &serverFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestUriPath", "requestUriParam", "requestBody", "requestHeader", "requestHeaders",
        "responseBody", "responseHeaders", "eraser", "math", "random", "randomset", "timestamp", "strftime",
        "recvseq", "var", "vault", "serverEvent", "clientEvent", "value", "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &serverTo() {
    static const std::vector<std::string> k = {
        "responseBodyText", "responseBodyHex", "responseBody", "responseHeader", "responseStatusCode",
        "responseDelayMs", "var", "vault", "serverEvent", "outState", "foreignOutState", "txtFile", "binFile",
        "udpSocket", "clientProvision", "break"
    };
    return k;
}
inline const std::vector<std::string> &clientFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestBody", "requestHeader", "requestHeaders", "responseHeaders", "eraser", "math",
        "random", "randomset", "timestamp", "strftime", "sendseq", "var", "vault", "clientEvent", "serverEvent",
        "value", "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &clientTo() {
    static const std::vector<std::string> k = {
        "requestBodyText", "requestBodyHex", "requestBody", "requestHeader", "requestDelayMs", "requestTimeoutMs",
        "requestUri", "requestMethod", "var", "vault", "clientEvent", "outState", "txtFile", "binFile", "udpSocket"
    };
    return k;
}
inline const std::vector<std::string> &clientResponseFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestUriPath", "requestUriParam", "requestBody", "requestHeader", "requestHeaders",
        "responseBody", "responseHeader", "responseHeaders", "responseStatusCode", "eraser", "math", "random",
        "randomset", "timestamp", "strftime", "sendseq", "var", "vault", "clientEvent", "serverEvent", "value",
        "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &clientResponseTo() {
    static const std::vector<std::string> k = {"var", "vault", "clientEvent", "outState", "txtFile", "binFile", "udpSocket", "break"};
    return k;
}

/** Completes a server provision schema with the v2 transformation grammar. */
inline nlohmann::json withServerGrammar(nlohmann::json schema) {
    auto &d = schema["definitions"];
    addCastFilter(d["filter"]);
    d["from"] = slot(serverFrom(), objectSources());
    d["to"] = slot(serverTo(), objectTargets());
    d["rule"] = rule("#/definitions/from", "#/definitions/to", "#/definitions/rule");
    schema["properties"]["transform"] = rules("#/definitions/rule", true);
    return schema;
}

/** Completes a client provision schema with the v2 transformation grammar. */
inline nlohmann::json withClientGrammar(nlohmann::json schema) {
    auto &d = schema["definitions"];
    addCastFilter(d["filter"]);
    d["from"] = slot(clientFrom(), objectSources());
    d["to"] = slot(clientTo(), objectTargets());
    d["rule"] = rule("#/definitions/from", "#/definitions/to", "#/definitions/rule");
    d["responseFrom"] = slot(clientResponseFrom(), objectSources());
    d["responseTo"] = slot(clientResponseTo(), objectTargets());
    d["responseRule"] = rule("#/definitions/responseFrom", "#/definitions/responseTo", "#/definitions/responseRule");
    schema["properties"]["transform"] = rules("#/definitions/rule", true);
    schema["properties"]["onResponseTransform"] = rules("#/definitions/responseRule", false);
    return schema;
}

} // namespace grammar

const nlohmann::json schema = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "id": {
      "type": "string"
    },
    "schema": {
      "type": "object"
    }
  },
  "required": [ "id", "schema" ]
}
)"_json;

const nlohmann::json vault = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "patternProperties": {
    "^[^.]+$": {}
  }
}
)"_json;

const nlohmann::json server_matching = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "algorithm": {
      "type": "string",
        "enum": ["FullMatching", "FullMatchingRegexReplace", "RegexMatching"]
    },
    "rgx": {
      "type": "string"
    },
    "fmt": {
      "type": "string"
    },
    "uriPathQueryParameters": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "filter": {
          "type": "string",
            "enum": ["Sort", "PassBy", "Ignore"]
        },
        "separator": {
          "type": "string",
            "enum": ["Ampersand", "Semicolon"]
        }
      },
      "required": [ "filter" ]
    }
  },
  "required": [ "algorithm" ]
}
)"_json;

// Regular expressions within json schemas:
//
// Any regex syntax that need to be a literal needs to be double escaped.
// Let’s take your example, we would like specify a literal $ character, which is a regex syntax for boundary-type assertions indicating the end of an input.
// We would need escape the character with a single \ to be literal, however in jsonSchema, the \ would also needed to be escaped with another slash \.
//
// About pattern (schema regex): https://json-schema.org/understanding-json-schema/reference/regular_expressions.html
//
// ConditionVar pattern: ^(?!$)(?!!$).*$
// 1) non empty string:     "negative lookahead" (?!$)
// 2) non valid string "!": "negative lookahead" (?!!$)
const nlohmann::json server_provision = grammar::withServerGrammar(R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {
    "filter": {
      "oneOf": [
        { "type": "string", "enum": ["Size"] },
        {
          "type": "object",
          "additionalProperties": false,
          "oneOf": [
        {"required": ["RegexCapture"]},
        {"required": ["RegexReplace"]},
        {"required": ["Append"]},
        {"required": ["Prepend"]},
        {"required": ["Sum"]},
        {"required": ["Multiply"]},
        {"required": ["ConditionVar"]},
        {"required": ["EqualTo"]},
        {"required": ["DifferentFrom"]},
        {"required": ["JsonConstraint"]},
        {"required": ["SchemaId"]},
        {"required": ["Split"]},
        {"required": ["BaseConvert"]},
        {"required": ["Strptime"]},
        {"required": ["Strftime"]},
        {"required": ["RegexKey"]}
      ],
      "properties": {
        "RegexCapture": { "type": "string" },
        "RegexReplace": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "rgx": {
              "type": "string"
            },
            "fmt": {
              "type": "string"
            }
          },
          "required": [ "rgx", "fmt" ]
        },
        "Append": { "type": "string" },
        "Prepend": { "type": "string" },
        "Sum": { "type": "number" },
        "Multiply": { "type": "number" },
        "ConditionVar": { "type": "string", "pattern": "^(?!$)(?!!$).*$" },
        "EqualTo": { "type": "string" },
        "DifferentFrom": { "type": "string" },
        "JsonConstraint": { "type": ["object", "array"] },
        "SchemaId": { "type": "string" },
        "Split": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "size": { "type": "integer", "minimum": 1 },
            "count": { "type": "integer", "minimum": 1 },
            "sep": { "type": "string" },
            "filler": { "type": "string" },
            "numeric": { "type": "boolean" }
          }
        },
        "BaseConvert": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "in", "out" ],
          "properties": {
            "in": { "type": "integer", "minimum": 2, "maximum": 36 },
            "out": { "type": "integer", "minimum": 2, "maximum": 36 },
            "capital": { "type": "boolean" }
          }
        },
        "Strptime": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "fmt" ],
          "properties": {
            "fmt": { "type": "string" },
            "unit": { "type": "string", "enum": ["s", "ms", "us", "ns"] }
          }
        },
        "Strftime": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "fmt" ],
          "properties": {
            "fmt": { "type": "string" },
            "unit": { "type": "string", "enum": ["s", "ms", "us", "ns"] }
          }
        },
        "RegexKey": { "type": "string" }
      }
        }
      ]
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "description":{
      "type": "string"
    },
    "inState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "outState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE", "HEAD" ]
    },
    "requestUri": {
      "type": "string"
    },
    "requestSchemaId": {
      "type": "string"
    },
    "responseHeaders": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "responseCode": {
      "type": "integer"
    },
    "responseBody": {
      "anyOf": [
        {"type": "object"},
        {"type": "array"},
        {"type": "string"},
        {"type": "integer"},
        {"type": "number"},
        {"type": "boolean"},
        {"type": "null"}
      ]
    },
    "responseDelayMs": {
      "type": "integer"
    },
    "responseSchemaId": {
      "type": "string"
    }
  },
  "required": [ "requestMethod", "responseCode" ]
}
)"_json);

// Valid HTTP schemes:
//
// URI (Uniform Resource Identifier) schemes for HTTP (Hypertext Transfer Protocol) are used to identify resources
// on the web that can be accessed using HTTP. Here are some commonly used URI schemes for HTTP:
//
// http:// - This is the most common URI scheme for HTTP. It identifies a resource that is accessible using HTTP.
//           For example, http://www.example.com/page.html.
// https:// - This is similar to http://, but the communication between the client and server is encrypted using SSL/TLS.
//            For example, https://www.example.com/page.html.
// ftp:// - This URI scheme is used to identify resources that are accessible using FTP (File Transfer Protocol) over the internet.
//          For example, ftp://ftp.example.com/file.txt.
// sftp:// - This is similar to ftp://, but the communication between the client and server is encrypted using SSH (Secure Shell).
//           For example, sftp://ftp.example.com/file.txt.
// scp:// - This URI scheme is used to identify resources that are accessible using SCP (Secure Copy Protocol) over the internet.
//          For example, scp://user@example.com:/path/to/file.txt.
// smb:// - This URI scheme is used to identify resources that are accessible using SMB (Server Message Block) protocol over a network.
//          For example, smb://server/share/file.txt.
// nfs:// - This URI scheme is used to identify resources that are accessible using NFS (Network File System) protocol over a network.
//          For example, nfs://server/path/to/file.txt.
//
// Note that HTTP and HTTPS are the most commonly used URI schemes for accessing resources on the web.
// H2AGENT WILL ONLY IMPLEMENT HTTP/HTTPS
//
// Valid HTTP ports:
//
// 0 - 1024 reserved for privileged services, 1025 - 65536 for applications
// We could restrict to application scope, but there is no reason for that as we would deny mocking of privileged services.

const nlohmann::json client_endpoint = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "id": {
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "host": {
      "type": "string"
    },
    "port": {
      "type": "integer",
      "minimum": 0,
      "maximum": 65536
    },
    "secure": {
      "type": "boolean"
    },
    "permit": {
      "type": "boolean"
    }
  },
  "required": [ "id", "host", "port" ]
}
)"_json;

// ConditionVar pattern: ^(?!$)(?!!$).*$
// 1) non empty string:     "negative lookahead" (?!$)
// 2) non valid string "!": "negative lookahead" (?!!$)
const nlohmann::json client_provision = grammar::withClientGrammar(R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {
    "filter": {
      "oneOf": [
        { "type": "string", "enum": ["Size"] },
        {
          "type": "object",
          "additionalProperties": false,
          "oneOf": [
        {"required": ["RegexCapture"]},
        {"required": ["RegexReplace"]},
        {"required": ["Append"]},
        {"required": ["Prepend"]},
        {"required": ["Sum"]},
        {"required": ["Multiply"]},
        {"required": ["ConditionVar"]},
        {"required": ["EqualTo"]},
        {"required": ["DifferentFrom"]},
        {"required": ["JsonConstraint"]},
        {"required": ["SchemaId"]},
        {"required": ["Split"]},
        {"required": ["BaseConvert"]},
        {"required": ["Strptime"]},
        {"required": ["Strftime"]},
        {"required": ["RegexKey"]}
      ],
      "properties": {
        "RegexCapture": { "type": "string" },
        "RegexReplace": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "rgx": {
              "type": "string"
            },
            "fmt": {
              "type": "string"
            }
          },
          "required": [ "rgx", "fmt" ]
        },
        "Append": { "type": "string" },
        "Prepend": { "type": "string" },
        "Sum": { "type": "number" },
        "Multiply": { "type": "number" },
        "ConditionVar": { "type": "string", "pattern": "^!?.*$" },
        "EqualTo": { "type": "string" },
        "DifferentFrom": { "type": "string" },
        "JsonConstraint": { "type": ["object", "array"] },
        "SchemaId": { "type": "string" },
        "Split": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "size": { "type": "integer", "minimum": 1 },
            "count": { "type": "integer", "minimum": 1 },
            "sep": { "type": "string" },
            "filler": { "type": "string" },
            "numeric": { "type": "boolean" }
          }
        },
        "BaseConvert": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "in", "out" ],
          "properties": {
            "in": { "type": "integer", "minimum": 2, "maximum": 36 },
            "out": { "type": "integer", "minimum": 2, "maximum": 36 },
            "capital": { "type": "boolean" }
          }
        },
        "Strptime": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "fmt" ],
          "properties": {
            "fmt": { "type": "string" },
            "unit": { "type": "string", "enum": ["s", "ms", "us", "ns"] }
          }
        },
        "Strftime": {
          "type": "object",
          "additionalProperties": false,
          "required": [ "fmt" ],
          "properties": {
            "fmt": { "type": "string" },
            "unit": { "type": "string", "enum": ["s", "ms", "us", "ns"] }
          }
        },
        "RegexKey": { "type": "string" }
      }
        }
      ]
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "description":{
      "type": "string"
    },
    "id":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "inState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "outState":{
      "type": "string",
      "pattern": "^[^#]*$",
      "not": { "enum": ["initial"] }
    },
    "endpoint":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE", "HEAD" ]
    },
    "requestUri": {
      "type": "string"
    },
    "requestSchemaId": {
      "type": "string"
    },
    "requestHeaders": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "requestBody": {
      "anyOf": [
        {"type": "object"},
        {"type": "array"},
        {"type": "string"},
        {"type": "integer"},
        {"type": "number"},
        {"type": "boolean"},
        {"type": "null"}
      ]
    },
    "requestDelayMs": {
      "type": "integer"
    },
    "timeoutMs": {
      "type": "integer"
    },
    "responseSchemaId": {
      "type": "string"
    },
    "expectedResponseStatusCode": {
      "type": "integer",
      "minimum": 100,
      "maximum": 599
    }
  },
  "required": [ "id" ]
}
)"_json);

}
}

