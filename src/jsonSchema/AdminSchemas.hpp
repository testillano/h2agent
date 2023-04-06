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

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace adminSchemas
{

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

const nlohmann::json server_data_global = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "patternProperties": {
    "^.*$": {
      "anyOf": [
        {
          "type": "string"
        }
      ]
    }
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
        "enum": ["FullMatching", "FullMatchingRegexReplace", "PriorityMatchingRegex", "RegexMatching"]
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
const nlohmann::json server_provision = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {
    "filter": {
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
        {"required": ["JsonConstraint"]}
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
        "JsonConstraint": { "type": "object" }
      }
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "inState":{
      "type": "string"
    },
    "outState":{
      "type": "string"
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
      "oneOf": [
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
    "transform" : {
      "type" : "array",
      "minItems": 1,
      "items" : {
        "type" : "object",
        "minProperties": 2,
        "maxProperties": 3,
        "properties": {
          "source": {
            "type": "string",
            "pattern": "^request\\.(uri(\\.(path$|param\\..+))?|body(\\..+)?|header\\..+)$|^response\\.body(\\..+)?$|^eraser$|^math\\..*|^random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^randomset\\..+|^timestamp\\.[m|u|n]{0,1}s$|^strftime\\..+|^recvseq$|^(var|globalVar|serverEvent)\\..+|^(value)\\..*|^inState$|^txtFile\\..+|^binFile\\..+|^command\\..+"
          },
          "target": {
            "type": "string",
            "pattern": "^response\\.body\\.(string$|hexstring$)|^response\\.body\\.json\\.(object$|object\\..+|jsonstring$|jsonstring\\..+|string$|string\\..+|integer$|integer\\..+|unsigned$|unsigned\\..+|float$|float\\..+|boolean$|boolean\\..+)|^response\\.(header\\..+|statusCode|delayMs)$|^(var|globalVar|serverEvent)\\..+|^outState(\\.(POST|GET|PUT|DELETE|HEAD)(\\..+)?)?$|^txtFile\\..+|^binFile\\..+|^break$"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    },
    "responseSchemaId": {
      "type": "string"
    }
  },
  "required": [ "requestMethod", "responseCode" ]
}
)"_json;

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

const nlohmann::json client_endpoint = R"(
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "id": {
      "type": "string"
    },
    "host": {
      "type": "string"
    },
    "port": {
      "type": "integer",
      "minimum": 1025,
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

}
}

