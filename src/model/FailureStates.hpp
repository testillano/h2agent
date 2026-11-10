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

#include <algorithm>
#include <string>

#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{

/**
 * Reserved states (docs/api/README.md, "Reserved states").
 *
 * '__core:' is the h2agent reserved prefix, in any context: the user reads it,
 * never writes it as an output. A failure is a state of the event:
 * '__core:failed:<reason>'. A client provision may react to it with that inState
 * (exact) or with '__core:failed' (any failure).
 */
constexpr const char *RESERVED_STATE_PREFIX = "__core:";
constexpr const char *FAILED_STATE = "__core:failed";
constexpr const char *FAILED_STATE_PREFIX = "__core:failed:";

// Built-in failure reasons:
constexpr const char *FAILURE_TRANSPORT = "transport";
constexpr const char *FAILURE_EXPECTED_RESPONSE_STATUS_CODE = "expectedResponseStatusCode";
constexpr const char *FAILURE_RESPONSE_SCHEMA_ID = "responseSchemaId";
constexpr const char *FAILURE_REQUEST_SCHEMA_ID = "requestSchemaId";
constexpr const char *FAILURE_TRANSFORM = "transform";  // target 'fail' without reason

inline bool hasStatePrefix(const std::string &s, const char *prefix) {
    return s.rfind(prefix, 0) == 0;
}

/** True for any state with the reserved prefix */
inline bool isReservedState(const std::string &state) {
    return hasStatePrefix(state, RESERVED_STATE_PREFIX);
}

/** True for a failure state: the generic one or '__core:failed:<reason>' (non-empty reason, no ':') */
inline bool isFailedState(const std::string &state) {
    if (state == FAILED_STATE) return true;
    if (!hasStatePrefix(state, FAILED_STATE_PREFIX)) return false;
    const std::string reason = state.substr(std::char_traits<char>::length(FAILED_STATE_PREFIX));
    return !reason.empty() && reason.find(':') == std::string::npos;
}

/** Failure state for a reason */
inline std::string failedState(const std::string &reason) {
    return FAILED_STATE_PREFIX + reason;
}

/**
 * Event state for a list of failures: the first one (evaluation order), or the
 * given state when the list is empty.
 */
inline std::string stateAfterFailures(const nlohmann::json &failures, const std::string &state) {
    if (!failures.is_array() || failures.empty()) return state;
    return failedState(failures[0].value("reason", std::string("")));
}

/**
 * Reason of a 'fail' target once its variables are substituted: ':' (a state
 * level separator) becomes '_', and an empty one is 'transform'.
 */
inline std::string sanitizedReason(std::string reason) {
    if (reason.empty()) return FAILURE_TRANSFORM;
    std::replace(reason.begin(), reason.end(), ':', '_');
    return reason;
}

/** 'failures' entry of a 'fail' target: the detail is the value which reached it */
inline nlohmann::json targetFailure(const std::string &reason, const nlohmann::json &detail) {
    return {{"reason", sanitizedReason(reason)}, {"detail", detail}};
}

/** Human readable detail of a 'failures' entry (log) */
inline std::string failureDetail(const nlohmann::json &failure) {
    if (failure.contains("expected") && failure.contains("got")) {
        return "expected " + failure["expected"].dump() + ", got " + failure["got"].dump();
    }
    if (failure.contains("id")) {
        const nlohmann::json error = failure.value("error", nlohmann::json(""));
        return "schema '" + failure.value("id", std::string("")) + "': " + (error.is_string() ? error.get<std::string>() : error.dump());
    }
    if (failure.contains("statusCode")) {
        return "statusCode " + failure["statusCode"].dump();
    }
    if (failure.contains("detail")) {
        const nlohmann::json &detail = failure["detail"];
        return detail.is_string() ? detail.get<std::string>() : detail.dump();
    }
    return "";
}

}
}
