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
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <JsonDump.hpp>


namespace h2agent
{
namespace model
{

/**
 * Reserved states (docs/api/README.md, "Reserved states and failures").
 *
 * '__core:' is the h2agent reserved prefix, in any context: the user reads it,
 * never writes it as an output. ':' is the level separator of the reserved
 * states, so it is not allowed in a user state. A failure is a state of the
 * event: '__core:failed:<state>:<reason>', where <state> is the inState of the
 * provision which failed (the stage). A client provision may react to it with
 * that inState (exact), '__core:failed:<state>' (any failure of that stage) or
 * '__core:failed' (any failure): the error branch.
 */
constexpr const char *RESERVED_STATE_PREFIX = "__core:";
constexpr const char *FAILED_STATE = "__core:failed";
constexpr const char *FAILED_STATE_PREFIX = "__core:failed:";
constexpr char STATE_LEVEL_SEPARATOR = ':';

// Built-in failure reasons:
constexpr const char *FAILURE_TRANSPORT = "transport";
constexpr const char *FAILURE_EXPECTED_RESPONSE_STATUS_CODE = "expectedResponseStatusCode";
constexpr const char *FAILURE_RESPONSE_SCHEMA_ID = "responseSchemaId";
constexpr const char *FAILURE_REQUEST_SCHEMA_ID = "requestSchemaId";
constexpr const char *FAILURE_UNSPECIFIED = "unspecified";  // target 'fail' without reason

inline bool hasStatePrefix(const std::string &s, const char *prefix) {
    return s.rfind(prefix, 0) == 0;
}

/** True for any state with the reserved prefix */
inline bool isReservedState(const std::string &state) {
    return hasStatePrefix(state, RESERVED_STATE_PREFIX);
}

/** True when a user state is valid: no level separator (':') */
inline bool isUserState(const std::string &state) {
    return state.find(STATE_LEVEL_SEPARATOR) == std::string::npos;
}

/**
 * True for a failure state accepted as a client inState: '__core:failed',
 * '__core:failed:<state>' or '__core:failed:<state>:<reason>' (non-empty
 * levels, no ':' inside them)
 */
inline bool isFailedState(const std::string &state) {
    if (state == FAILED_STATE) return true;
    if (!hasStatePrefix(state, FAILED_STATE_PREFIX)) return false;
    const std::string levels = state.substr(std::char_traits<char>::length(FAILED_STATE_PREFIX));
    const std::size_t sep = levels.find(STATE_LEVEL_SEPARATOR);
    if (sep == std::string::npos) return !levels.empty();
    const std::string stage = levels.substr(0, sep);
    const std::string reason = levels.substr(sep + 1);
    return !stage.empty() && !reason.empty() && isUserState(reason);
}

/**
 * Stage level of a failure state: the inState of the provision which failed. A
 * user state has no ':'; an error branch inState ('__core:failed[:...]') has it
 * replaced by '_', so the failure state keeps its three levels (a failure within
 * an error branch ends the chain, see MyAdminHttp2Server::sendClientRequest).
 */
inline std::string failureStage(std::string inState) {
    std::replace(inState.begin(), inState.end(), STATE_LEVEL_SEPARATOR, '_');
    return inState;
}

/** Failure state of a stage (the inState of the provision which failed) for a reason */
inline std::string failedState(const std::string &stage, const std::string &reason) {
    return FAILED_STATE_PREFIX + failureStage(stage) + STATE_LEVEL_SEPARATOR + reason;
}

/**
 * Event state for a list of failures: the first one (evaluation order) at the
 * given stage, or the given state when the list is empty.
 */
inline std::string stateAfterFailures(const nlohmann::json &failures, const std::string &stage, const std::string &state) {
    if (!failures.is_array() || failures.empty()) return state;
    return failedState(stage, failures[0].value("reason", std::string("")));
}

/**
 * Client error branch lookup order for a failure state
 * ('__core:failed:<state>:<reason>'): the exact state, the stage
 * ('__core:failed:<state>') and the generic one ('__core:failed').
 */
inline std::vector<std::string> errorBranchStates(const std::string &failed) {
    std::vector<std::string> result{failed};
    const std::size_t sep = failed.rfind(STATE_LEVEL_SEPARATOR);
    if (sep != std::string::npos && sep >= std::char_traits<char>::length(FAILED_STATE_PREFIX)) result.push_back(failed.substr(0, sep));
    result.push_back(FAILED_STATE);
    return result;
}

/**
 * Reason of a 'fail' target once its variables are substituted: ':' (a state
 * level separator) becomes '_', and an empty one is 'unspecified'.
 */
inline std::string sanitizedReason(std::string reason) {
    if (reason.empty()) return FAILURE_UNSPECIFIED;
    std::replace(reason.begin(), reason.end(), ':', '_');
    return reason;
}

/** 'failures' entry of a 'fail' target: the detail is the value which reached it */
inline nlohmann::json targetFailure(const std::string &reason, const nlohmann::json &detail) {
    return {{"reason", sanitizedReason(reason)}, {"detail", detail}};
}

/**
 * Metric label of a failed event: the reason of its state (first failure) as
 * written in the provision, so that the cardinality is bounded by the loaded
 * provisions and not by the traffic.
 *
 * @param failures Failures of the event (non-empty)
 * @param failReasonTemplate Unsubstituted reason of the 'fail' target, when it is the first failure
 */
inline std::string metricReason(const nlohmann::json &failures, const std::optional<std::string> &failReasonTemplate) {
    if (failReasonTemplate) return failReasonTemplate->empty() ? std::string(FAILURE_UNSPECIFIED) : *failReasonTemplate;
    return failures[0].value("reason", std::string(""));
}

/** Human readable detail of a 'failures' entry (log) */
inline std::string failureDetail(const nlohmann::json &failure) {
    if (failure.contains("expected") && failure.contains("got")) {
        return "expected " + h2agent::model::dumpJson(failure["expected"]) + ", got " + h2agent::model::dumpJson(failure["got"]);
    }
    if (failure.contains("id")) {
        const nlohmann::json error = failure.value("error", nlohmann::json(""));
        return "schema '" + failure.value("id", std::string("")) + "': " + (error.is_string() ? error.get<std::string>() : h2agent::model::dumpJson(error));
    }
    if (failure.contains("statusCode")) {
        return "statusCode " + h2agent::model::dumpJson(failure["statusCode"]);
    }
    if (failure.contains("detail")) {
        const nlohmann::json &detail = failure["detail"];
        return detail.is_string() ? detail.get<std::string>() : h2agent::model::dumpJson(detail);
    }
    return "";
}

}
}
