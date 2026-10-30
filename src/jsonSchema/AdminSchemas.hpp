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
// Rule: {"from": {"source": <kind>, ...}, "filter": [{"op": <op>, ...}], "to": {"target": <kind>, ...},
//        "onFilterFail": [<rule>...]}
// 'source', 'target' and 'op' are discriminators; every other argument is a
// named field. The schema is built from per-kind tables so that each provision
// context (server transform, client transform, client onResponseTransform)
// lists the kinds it accepts, with the same scope as the former v1 patterns.
//////////////////////////////////////////////////////////////////////////////
namespace grammar
{

inline nlohmann::json text() {
    return {{"type", "string"}, {"minLength", 1}};
}
inline nlohmann::json anyText() {
    return {{"type", "string"}};
}
/** Json pointer inside a document (absent field = whole document). */
inline nlohmann::json pointer() {
    return {{"type", "string"}, {"pattern", "^/"}};
}
/** Vault node path: "/<node>...". */
inline nlohmann::json nodePath() {
    return {{"type", "string"}, {"pattern", "^/.+$"}};
}
/** Key or identifier without dots. */
inline nlohmann::json dotless() {
    return {{"type", "string"}, {"pattern", "^[^.]+$"}};
}
/** Number field which also accepts a text holding a variable ("@{seq}"). */
inline nlohmann::json numberOrVariable(bool allowNegative) {
    nlohmann::json number = {{"type", "integer"}};
    if (!allowNegative) number["minimum"] = 0;
    return {{"oneOf", {number, {{"type", "string"}, {"pattern", "@\\{"}}}}};
}
inline nlohmann::json enumOf(const std::vector<std::string> &values) {
    return {{"type", "string"}, {"enum", values}};
}

/** One kind: the discriminator fixed to 'kind' plus its fields (no others allowed). */
inline nlohmann::json kind(const std::string &discriminator, const std::string &name,
                           const nlohmann::json &fields = nlohmann::json::object(),
                           const std::vector<std::string> &required = {}) {
    nlohmann::json properties = fields;
    properties[discriminator] = {{"type", "string"}, {"enum", {name}}};
    nlohmann::json req = nlohmann::json::array({discriminator});
    for (const auto &r : required) req.push_back(r);
    return {{"type", "object"}, {"additionalProperties", false}, {"required", req}, {"properties", properties}};
}

/** Event address fields, per domain and role (source or purge target). */
inline nlohmann::json eventFields(bool client, bool source) {
    nlohmann::json f = {
        {"requestMethod", text()}, {"requestUri", anyText()},
        {"eventNumber", numberOrVariable(true)}
    };
    if (client) {
        f["clientEndpointId"] = text();
        f["sendseq"] = numberOrVariable(false);
    }
    else {
        f["recvseq"] = numberOrVariable(false);
    }
    if (source) {
        f["requestUriStartsWith"] = anyText();
        f["eventPath"] = anyText();
    }
    return f;
}

inline nlohmann::json sourceKind(const std::string &name) {
    static const nlohmann::json kinds = {
        {"requestUri", kind("source", "requestUri")},
        {"requestUriPath", kind("source", "requestUriPath")},
        {"requestUriParam", kind("source", "requestUriParam", {{"name", text()}}, {"name"})},
        {"requestBody", kind("source", "requestBody", {{"path", pointer()}})},
        {"responseBody", kind("source", "responseBody", {{"path", pointer()}})},
        {"requestHeader", kind("source", "requestHeader", {{"name", text()}}, {"name"})},
        {"requestHeaders", kind("source", "requestHeaders")},
        {"responseHeader", kind("source", "responseHeader", {{"name", text()}}, {"name"})},
        {"responseHeaders", kind("source", "responseHeaders")},
        {"responseStatusCode", kind("source", "responseStatusCode")},
        {"erase", kind("source", "erase")},
        {"math", kind("source", "math", {{"expression", anyText()}}, {"expression"})},
        {"random", kind("source", "random", {{"min", {{"type", "integer"}}}, {"max", {{"type", "integer"}}}}, {"min", "max"})},
        {"randomset", kind("source", "randomset", {{"values", {{"type", "array"}, {"items", {{"type", "string"}}}, {"minItems", 1}}}}, {"values"})},
        {"timestamp", kind("source", "timestamp", {{"unit", enumOf({"s", "ms", "us", "ns"})}}, {"unit"})},
        {"strftime", kind("source", "strftime", {{"fmt", text()}}, {"fmt"})},
        {"recvseq", kind("source", "recvseq")},
        {"sendseq", kind("source", "sendseq")},
        {"var", kind("source", "var", {{"name", text()}}, {"name"})},
        {"vault", kind("source", "vault", {{"key", dotless()}, {"path", nodePath()}}, {"key"})},
        {"literal", kind("source", "literal", {{"value", anyText()}}, {"value"})},
        {"inState", kind("source", "inState")},
        {"serverEvent", kind("source", "serverEvent", eventFields(false, true))},
        {"clientEvent", kind("source", "clientEvent", eventFields(true, true))},
        {"txtFile", kind("source", "txtFile", {{"file", text()}}, {"file"})},
        {"binFile", kind("source", "binFile", {{"file", text()}}, {"file"})},
        {"command", kind("source", "command", {{"command", text()}}, {"command"})},
    };
    return kinds.at(name);
}

inline nlohmann::json targetKind(const std::string &name) {
    static const nlohmann::json kinds = {
        {"responseBodyText", kind("target", "responseBodyText")},
        {"responseBodyHex", kind("target", "responseBodyHex")},
        {"responseBody", kind("target", "responseBody", {{"path", pointer()}})},
        {"requestBodyText", kind("target", "requestBodyText")},
        {"requestBodyHex", kind("target", "requestBodyHex")},
        {"requestBody", kind("target", "requestBody", {{"path", pointer()}})},
        {"responseHeader", kind("target", "responseHeader", {{"name", text()}}, {"name"})},
        {"responseStatusCode", kind("target", "responseStatusCode")},
        {"responseDelayMs", kind("target", "responseDelayMs")},
        {"requestHeader", kind("target", "requestHeader", {{"name", text()}}, {"name"})},
        {"requestUri", kind("target", "requestUri")},
        {"requestMethod", kind("target", "requestMethod")},
        {"requestDelayMs", kind("target", "requestDelayMs")},
        {"requestTimeoutMs", kind("target", "requestTimeoutMs")},
        {"var", kind("target", "var", {{"name", text()}}, {"name"})},
        {"vault", kind("target", "vault", {{"key", dotless()}, {"path", nodePath()}}, {"key"})},
        // server outState: optional foreign method/uri ('uri' requires 'method'):
        {   "outState", [] {
                nlohmann::json k = kind("target", "outState", {{"method", enumOf({"POST", "GET", "PUT", "DELETE", "HEAD"})}, {"uri", text()}});
                k["dependencies"] = {{"uri", {"method"}}};
                return k;
            }()
        },
        // client outState: next step state only
        {"outStatePlain", kind("target", "outState")},
        {"txtFile", kind("target", "txtFile", {{"file", text()}}, {"file"})},
        {"binFile", kind("target", "binFile", {{"file", text()}}, {"file"})},
        {"udpSocket", kind("target", "udpSocket", {{"socket", text()}, {"delayMs", {{"oneOf", {{{"type", "integer"}, {"minimum", 0}}, text()}}}}}, {"socket"})},
        {"serverEvent", kind("target", "serverEvent", eventFields(false, false))},
        {"clientEvent", kind("target", "clientEvent", eventFields(true, false))},
        {"clientProvision", kind("target", "clientProvision", {{"id", dotless()}, {"inState", text()}}, {"id", "inState"})},
        {"stop", kind("target", "stop")},
    };
    return kinds.at(name);
}

/** Filter items: one exclusive branch per op ('cast' included). */
inline nlohmann::json filter(const nlohmann::json &conditionVarName) {
    const nlohmann::json unit = enumOf({"s", "ms", "us", "ns"});
    const nlohmann::json ops = nlohmann::json::array({
        kind("op", "regexCapture", {{"rgx", anyText()}}, {"rgx"}),
        kind("op", "regexReplace", {{"rgx", anyText()}, {"fmt", anyText()}}, {"rgx", "fmt"}),
        kind("op", "regexKey", {{"rgx", anyText()}}, {"rgx"}),
        kind("op", "append", {{"value", anyText()}}, {"value"}),
        kind("op", "prepend", {{"value", anyText()}}, {"value"}),
        kind("op", "sum", {{"value", {{"type", "number"}}}}, {"value"}),
        kind("op", "multiply", {{"value", {{"type", "number"}}}}, {"value"}),
        kind("op", "conditionVar", {{"name", conditionVarName}, {"not", {{"type", "boolean"}}}}, {"name"}),
        kind("op", "equalTo", {{"value", anyText()}}, {"value"}),
        kind("op", "differentFrom", {{"value", anyText()}}, {"value"}),
        kind("op", "jsonConstraint", {{"value", {{"type", {"object", "array"}}}}}, {"value"}),
        kind("op", "schemaId", {{"id", anyText()}}, {"id"}),
        kind("op", "split", {
            {"size", {{"type", "integer"}, {"minimum", 1}}}, {"count", {{"type", "integer"}, {"minimum", 1}}},
            {"sep", anyText()}, {"filler", anyText()}, {"numeric", {{"type", "boolean"}}}
        }),
        kind("op", "baseConvert", {
            {"in", {{"type", "integer"}, {"minimum", 2}, {"maximum", 36}}}, {"out", {{"type", "integer"}, {"minimum", 2}, {"maximum", 36}}},
            {"capital", {{"type", "boolean"}}}
        }, {"in", "out"}),
        kind("op", "strptime", {{"fmt", anyText()}, {"unit", unit}}, {"fmt"}),
        kind("op", "strftime", {{"fmt", anyText()}, {"unit", unit}}, {"fmt"}),
        kind("op", "size"),
        kind("op", "cast", {{"type", enumOf({"string", "integer", "unsigned", "float", "boolean", "object", "json"})}}, {"type"}),
    });
    return {{"oneOf", ops}};
}

/** conditionVar variable name: negation is the 'not' field, never a leading '!'. */
inline nlohmann::json conditionVarName() {
    return {{"type", "string"}, {"minLength", 1}, {"pattern", "^[^!]"}};
}

/** One stage slot ('from' or 'to'): one exclusive branch per accepted kind. */
inline nlohmann::json slot(const std::vector<std::string> &kinds, nlohmann::json (*kindOf)(const std::string &)) {
    nlohmann::json branches = nlohmann::json::array();
    for (const auto &k : kinds) branches.push_back(kindOf(k));
    return {{"oneOf", branches}};
}

/** Condition "rule.<stage>.<discriminator> is one of <kinds>" (for if/then constraints). */
inline nlohmann::json stage(const std::string &name, const std::string &discriminator, const std::vector<std::string> &kinds) {
    return {{"required", {name}}, {"properties", {{name, {{"required", {discriminator}}, {"properties", {{discriminator, {{"enum", kinds}}}}}}}}}};
}

/**
 * Rule definition for one context; onFilterFail rules belong to the same
 * context. An 'erase' source provides no value (only 'conditionVar' filters and
 * the cast), and it is the only source an event purge target accepts.
 */
inline nlohmann::json rule(const std::string &fromRef, const std::string &toRef, const std::string &selfRef) {
    return {
        {"type", "object"},
        {"additionalProperties", false},
        {"required", {"from", "to"}},
        {   "properties", {
                {"from", {{"$ref", fromRef}}},
                {"filter", {{"type", "array"}, {"minItems", 1}, {"items", {{"$ref", "#/definitions/filter"}}}}},
                {"to", {{"$ref", toRef}}},
                {"onFilterFail", {{"type", "array"}, {"minItems", 1}, {"items", {{"$ref", selfRef}}}}}
            }
        },
        {   "allOf", {
                // 'erase' provides no value: only conditionVar filters, plus the cast which keeps the
                // v1 erase flavour on a body target (object: remove the node, string: empty string)
                {   {"if", stage("from", "source", {"erase"})},
                    {"then", {{"properties", {{"filter", {{"items", {{"required", {"op"}}, {"properties", {{"op", {{"enum", {"conditionVar", "cast"}}}}}}}}}}}}}}
                },
                // event purge (serverEvent/clientEvent target) is only fed by 'erase'
                {   {"if", stage("to", "target", {"serverEvent", "clientEvent"})},
                    {"then", stage("from", "source", {"erase"})}
                }
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

// Kinds accepted per context (same scope as the v1 schema patterns):
inline const std::vector<std::string> &serverFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestUriPath", "requestUriParam", "requestBody", "requestHeader", "requestHeaders",
        "responseBody", "responseHeaders", "erase", "math", "random", "randomset", "timestamp", "strftime",
        "recvseq", "var", "vault", "serverEvent", "clientEvent", "literal", "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &serverTo() {
    static const std::vector<std::string> k = {
        "responseBodyText", "responseBodyHex", "responseBody", "responseHeader", "responseStatusCode",
        "responseDelayMs", "var", "vault", "serverEvent", "outState", "txtFile", "binFile", "udpSocket",
        "clientProvision", "stop"
    };
    return k;
}
inline const std::vector<std::string> &clientFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestBody", "requestHeader", "requestHeaders", "responseHeaders", "erase", "math",
        "random", "randomset", "timestamp", "strftime", "sendseq", "var", "vault", "clientEvent", "serverEvent",
        "literal", "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &clientTo() {
    static const std::vector<std::string> k = {
        "requestBodyText", "requestBodyHex", "requestBody", "requestHeader", "requestDelayMs", "requestTimeoutMs",
        "requestUri", "requestMethod", "var", "vault", "clientEvent", "outStatePlain", "txtFile", "binFile", "udpSocket"
    };
    return k;
}
inline const std::vector<std::string> &clientResponseFrom() {
    static const std::vector<std::string> k = {
        "requestUri", "requestUriPath", "requestUriParam", "requestBody", "requestHeader", "requestHeaders",
        "responseBody", "responseHeader", "responseHeaders", "responseStatusCode", "erase", "math", "random",
        "randomset", "timestamp", "strftime", "sendseq", "var", "vault", "clientEvent", "serverEvent", "literal",
        "inState", "txtFile", "binFile", "command"
    };
    return k;
}
inline const std::vector<std::string> &clientResponseTo() {
    static const std::vector<std::string> k = {"var", "vault", "clientEvent", "outStatePlain", "txtFile", "binFile", "udpSocket", "stop"};
    return k;
}

/** Completes a server provision schema with the v2 transformation grammar. */
inline nlohmann::json withServerGrammar(nlohmann::json schema) {
    auto &d = schema["definitions"];
    d["filter"] = filter(conditionVarName());
    d["from"] = slot(serverFrom(), sourceKind);
    d["to"] = slot(serverTo(), targetKind);
    d["rule"] = rule("#/definitions/from", "#/definitions/to", "#/definitions/rule");
    schema["properties"]["transform"] = rules("#/definitions/rule", true);
    return schema;
}

/** Completes a client provision schema with the v2 transformation grammar. */
inline nlohmann::json withClientGrammar(nlohmann::json schema) {
    auto &d = schema["definitions"];
    d["filter"] = filter(conditionVarName());
    d["from"] = slot(clientFrom(), sourceKind);
    d["to"] = slot(clientTo(), targetKind);
    d["rule"] = rule("#/definitions/from", "#/definitions/to", "#/definitions/rule");
    d["responseFrom"] = slot(clientResponseFrom(), sourceKind);
    d["responseTo"] = slot(clientResponseTo(), targetKind);
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
const nlohmann::json server_provision = grammar::withServerGrammar(R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {},
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

const nlohmann::json client_provision = grammar::withClientGrammar(R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {},
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

