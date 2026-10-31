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

#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Ports.hpp"

namespace h2agent {
namespace engine {

/**
 * Classified source kind of a rule.
 *
 * Mirrors h2agent's Transformation::SourceType for behavior parity. Only the
 * kinds already implemented are listed; anything not yet ported is parsed as
 * `Unsupported` (the rule is skipped at evaluation and noted in the trace),
 * so a partial engine never silently misbehaves.
 */
enum class SourceType {
    Unsupported,
    Value,        // value.<literal> with @{var} substitution
    Sequence,     // sequence -> EvaluationContext::sequence (unsigned); host maps its
    // recvseq/sendseq here
    Timestamp,    // timestamp.<s|ms|us|ns> -> epoch integer (via Clock)
    Strftime,     // strftime.<format> -> current UTC time formatted via strftime
    // (admits @{var} substitution on the formatted result; parity
    // with h2agent Strftime source). Uses the real wall clock
    // (gmtime_r) independently of the Clock port.
    Random,       // random.<min>.<max> -> integer in [min,max]
    RandomSet,    // randomset.<v1|v2|..|vN> -> one pipe-separated token chosen at
    // random (the chosen token admits @{var} substitution). Result is
    // a string. Leading/trailing/empty parts count as empty-string
    // tokens (parity with h2agent: 'a||b' => 3 tokens).
    Eraser,       // eraser -> special "reset/remove the target" source. Carries no
    // value; it sets an eraser flag so the target application erases
    // (store key removal, json node removal, body clear, or empty
    // string) instead of assigning. Parity with h2agent Eraser.
    Math,         // math.<expression> -> arithmetic result (admits @{var}
    // substitution before evaluation). See Engine.cpp note for the
    // evaluator approach taken in this batch.
    StoreLocal,   // local.<key> -> Store(Scope::Local); host maps v1 'var.'
    StoreGlobal,  // global.<key> -> Store(Scope::Global); host maps v1 'vault.'
    InState,      // inState -> EvaluationContext::inState (data only; FSM lives in the
    // host)
    Input,        // input[./json-pointer] -> EvaluationContext::input; host maps v1
    // request.* (method/uri/headers/body) into neutral json
    Meta,         // meta[./<field>] -> EvaluationContext::meta (typed live metadata
    // of the message being built, e.g. a client request uri)
    Output,       // output[./json-pointer] -> EvaluationContext::output; host maps v1
    // response.* (the response body being built) into neutral json
    ServerEvent,  // serverEvent.<query-params> -> EventSource port resolve (skip if
    // not found / port null). Address admits @{var}.
    ClientEvent,  // clientEvent.<query-params> -> EventSource port resolve.
    Sendseq,      // sendseq -> EvaluationContext::sequence (client alias of sequence)
    TxtFile,      // txtFile.<path> -> FileAccess port read (text). Missing file => ""
    // (h2agent parity: the rule is NOT skipped). Path admits @{var}.
    BinFile,      // binFile.<path> -> FileAccess port read (binary). Same semantics.
    Command,      // command.<cmd> -> Shell port; value = stdout, local var "rc" = exit
    // code (h2agent parity). Command admits @{var}.
};

/**
 * Classified target kind of a rule.
 *
 * Mirrors h2agent's Transformation::TargetType for behavior parity.
 */
enum class TargetType {
    Unsupported,
    OutputString,       // output.string (store processed string as body)
    OutputHexString,    // output.hexstring (hex string -> raw bytes body)
    OutputJsonString,   // output.json.string[./path] (string node in
    // output json)
    OutputJsonInteger,  // output.json.integer[./path] (integer node)
    OutputJsonUnsigned,  // output.json.unsigned[./path] (unsigned node)
    OutputJsonFloat,    // output.json.float[./path] (float/double node)
    OutputJsonBoolean,  // output.json.boolean[./path] (boolean node)
    OutputJsonObject,   // output.json.object[./path] (object node, with
    // best-effort scalar fallback)
    OutputJsonJsonString,  // output.json.jsonstring[./path] (parse a JSON
    // string source and set the resulting node)
    OutputJson,         // output.json[./path] (set the value as it is, keeping the
    // type the source yields: grammar v2 json body target without cast)
    StoreLocal,               // local.<key> -> Store(Scope::Local)
    StoreGlobal,              // global.<key> -> Store(Scope::Global)
    StoreLocalJsonObject,     // local.<key>.json.object[./ptr] (require object source)
    StoreLocalJsonString,     // local.<key>.json.jsonstring[./ptr] (parse json string)
    StoreGlobalJsonObject,    // global.<key>.json.object[./ptr]
    StoreGlobalJsonString,    // global.<key>.json.jsonstring[./ptr]
    OutState,                 // outState -> EvaluationContext::outState (data only; no FSM logic)
    OutStateForeign,          // outState.<METHOD>[.<uri>] -> outState + method/uri for a
    // host-created virtual event (data only; no FSM logic)
    MetaHeader,           // meta.header.<name> -> appended to Meta::headers
    MetaStatusCode,       // meta.statusCode -> *Meta::statusCode (unsigned)
    MetaDelayMs,          // meta.delayMs -> *Meta::delayMs (unsigned)
    TxtFile,                  // txtFile.<path> -> FileAccess port (text)
    BinFile,                  // binFile.<path> -> FileAccess port (binary)
    UdpSocket,                // udpSocket.<path>[|<ms>] -> SocketWriter port
    EventPurge,               // serverEvent/clientEvent (with eraser source) -> EventSource purge
    ClientProvisionTrigger,   // clientProvision.<id>.<inState> -> Trigger port (fires on a true value)
    MetaUri,                  // meta.uri -> *Meta::uri (string; e.g. client request uri)
    MetaMethod,               // meta.method -> *Meta::method (string; e.g. client request method)
    MetaTimeoutMs,            // meta.timeoutMs -> *Meta::timeoutMs (unsigned)
    Break,                    // break -> stop the pipeline on a true value
};

/** Optional filter applied over the source value before it reaches the target.
 */
enum class FilterType {
    None,
    Append,        // Append: concatenate a (variable-substituted) suffix to the source
    // string
    Prepend,       // Prepend: concatenate a (variable-substituted) prefix to the source
    // string
    Sum,           // Sum: numeric source + operand
    Multiply,      // Multiply: numeric source * operand
    RegexReplace,  // RegexReplace: std::regex_replace(source, rgx, fmt) -> string
    // (no match => source unchanged). Regex precompiled at parse.
    RegexCapture,  // RegexCapture: std::regex_match(source, rgx). No full match =>
    // skip the rule. Full match => value is the whole match, and
    // capture groups are exposed as a single JSON object with
    // numeric string keys {"0": whole, "1": g1, ...} when the target
    // is a store. Regex precompiled at parse.
    ConditionVar,  // ConditionVar: {"ConditionVar":"[!]varname"}. Transfer source->
    // target only if the named store var is truthy (defined and
    // non-empty string / non-zero number); '!' inverts. Local store
    // is consulted before global. When the condition is false, the
    // whole rule is SKIPPED (no target assignment). Parity w/ h2agent.
    EqualTo,       // EqualTo: {"EqualTo":"<val>"} (admits @{var}). Transfer only if
    // source string == val, else skip the rule.
    DifferentFrom,  // DifferentFrom: inverse of EqualTo (transfer if source != val).
    JsonConstraint,  // JsonConstraint: {"JsonConstraint":<obj|array>}. Validate that
    // the (json) source contains the given subset; on success the
    // value becomes "1", otherwise it becomes a fail-report string
    // (parity with h2agent jsonConstraint; the rule still applies).
    SchemaId,      // SchemaId: {"SchemaId":"<id>"}. Validate the (json) source via the
    // SchemaValidator port; value becomes "1" on success else "".
    // When the port is null the rule is skipped (validation ignored).
    Split,         // Split: {"Split":{size,count,sep,filler,numeric}}. Take the last
    // size*count chars (left-pad with filler when shorter), split into
    // count groups of 'size', joined by 'sep'; when numeric, strip
    // leading zeros per group. Defaults 2/4/'.'/'0'/false.
    BaseConvert,   // BaseConvert: {"BaseConvert":{in,out,capital}}. Reinterpret the
    // source as a number in base 'in' (2-36) and render it in base
    // 'out'; 'capital' selects upper-case digits. On any error the
    // source is kept unchanged.
    Strptime,      // Strptime: {"Strptime":{fmt,unit}}. Parse the source date string
    // into a UNIX epoch via strptime+timegm, scaled to the unit
    // (s/ms/us/ns). Parse failure skips the rule. Result is integer.
    Strftime,      // Strftime (filter): {"Strftime":{fmt,unit}}. Format a numeric epoch
    // source (interpreted in the given unit s/ms/us/ns) into a string
    // via gmtime_r+strftime. Non-numeric source or format failure
    // skips the rule.
    RegexKey,      // RegexKey: {"RegexKey":"<pattern>"} (precompiled, NO @{} subst).
    // The source must be a json object; match its keys and resolve to
    // the value of the FIRST matching key. No match skips the rule.
    Size,          // Size: plain string "Size". Resolve to the number of elements/keys
    // (object/array) or characters (string) as a numeric string.
    Cast,          // Cast: {"Cast":"string|integer|unsigned|float|boolean|object|json"}.
    // Convert the value to the type (object: object or array required; json:
    // parse the value text). A failed conversion fails the filter.
};

/**
 * One parsed filter of a rule chain (filters run in list order, each one on the
 * output of the previous one).
 */
struct Filter {
    FilterType type{FilterType::None};
    std::string raw{};         // filter label for trace, e.g. "Append"

    std::string arg{};         // Append/Prepend affix, EqualTo/DifferentFrom value,
    // ConditionVar varname, SchemaId id, Split sep,
    // Strptime/Strftime fmt, Cast type (admits @{var} where noted)
    double number{0.0};        // Sum/Multiply operand
    long long i1{0};           // Split size / BaseConvert in-base
    long long i2{0};           // Split count / BaseConvert out-base
    bool flag{false};          // Split numeric / BaseConvert capital
    std::string filler{};      // Split filler
    int unit{0};               // Strptime/Strftime unit (0:s, 1:ms, 2:us, 3:ns)
    nlohmann::json object{};   // JsonConstraint expected subset

    // Precompiled filter regex (RegexReplace/RegexCapture/RegexKey). Compiled
    // ONCE at parse time (never in evaluate, per the performance rule).
    // `arg` holds the replacement format for RegexReplace; it is unused by
    // RegexCapture/RegexKey. An invalid pattern at parse time drops the filter
    // and marks the rule Unsupported (so it is skipped + traced).
    std::regex regex{};
};

/**
 * A single parsed `source -> [filter] -> target` rule.
 *
 * Holds the classified kinds plus the parsed arguments, and the optional
 * filter chain.
 */
struct MathProgram;  // defined in Engine.cpp (keeps exprtk out of this header)

struct Rule {
    SourceType sourceType{SourceType::Unsupported};
    TargetType targetType{TargetType::Unsupported};

    std::string rawSource{};  // original source expression, e.g. "value.hello"
    std::string rawTarget{};  // original target expression, e.g. "output.string"
    std::string rawFilter{};  // filter labels for trace, e.g. "Append,Cast"

    // Parsed source arguments:
    std::string sourceArg{};  // value literal / timestamp unit / strftime fmt / math expression
    long long sourceI1{0};    // random min
    long long sourceI2{0};    // random max
    std::string sourceKey{};  // store key for local./global. sources (admits @{var})
    std::string sourcePtr{};  // json-pointer into a stored value ("" = whole value)
    // sourcePtr of input/output sources compiled ONCE at parse when static (no @{}).
    CompiledPointer sourceCompiled{};
    bool sourceCompiledStatic{false};
    std::vector<std::string> sourceTokens{};  // randomset pipe-separated tokens (each
    // admits @{var} on the chosen one)

    // Parsed target argument:
    std::string targetPath{};  // json-pointer for *.json.* targets ("" = root)
    // targetPath compiled ONCE at parse when it has no @{} (static); dynamic or
    // malformed paths are still built per event (malformed -> rule skipped).
    nlohmann::json::json_pointer targetJsonPtr{};
    bool targetJsonPtrStatic{false};
    std::string targetKey{};   // store key for local./global. targets (admits @{var})
    std::string targetPtr{};   // json-pointer into a stored value ("" = whole value)

    // Parsed target argument (effect/control/event targets):
    std::string targetName{};     // meta.header.<name> (admits @{var})
    std::string targetMethod{};   // outState.<METHOD> / event-purge method
    std::string targetUri{};      // outState.<METHOD>.<uri> / event-purge uri (admits @{var})
    std::string targetFilePath{}; // txtFile/binFile path (admits @{var})
    std::string targetSocketPath{};  // udpSocket path (admits @{var})
    unsigned int targetSocketDelayMs{0};  // udpSocket optional delay
    std::string targetEventDomain{};  // "serverEvent" | "clientEvent" (purge target)
    std::map<std::string, std::string> targetEventParams{};  // tokenized+decoded address
    std::string targetProvisionId{};   // clientProvision id (admits @{var})
    std::string targetProvisionInState{};  // clientProvision inState (admits @{var})

    // Event addressing for serverEvent/clientEvent SOURCES. Parity with h2agent:
    // the query-param address is tokenized and URL-decoded ONCE at parse; @{var}
    // is substituted per value at evaluation (so a variable holding '&' or '='
    // cannot break the address).
    std::string sourceEventDomain{};  // "serverEvent" | "clientEvent"
    std::map<std::string, std::string> sourceEventParams{};

    // onFilterFail: fallback rules evaluated when this rule's filter fails
    // (recursive, h2agent parity). A break inside a fallback stops the pipeline.
    std::vector<Rule> onFilterFail{};

    // Filter chain (empty: no filter):
    std::vector<Filter> filters{};

    // Precompiled math source (compiled ONCE at parse time, like the regex
    // above): @{var} are bound as exprtk variables instead of being substituted
    // textually, so evaluate() only assigns values. Null when the expression
    // cannot be bound safely (evaluate() then keeps the textual v1 path).
    // Shared (not copied) so Rule stays copyable; see MathProgram in Engine.cpp.
    std::shared_ptr<MathProgram> math{};
};

}  // namespace engine
}  // namespace h2agent
