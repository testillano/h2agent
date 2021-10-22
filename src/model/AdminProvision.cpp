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

#include <sstream>
#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <time.h>       /* time_t, struct tm, time, localtime, strftime */

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminProvision.hpp>
#include <TypeConverter.hpp>
#include <MockRequestData.hpp>

#include <functions.hpp>


namespace h2agent
{
namespace model
{

/*
void printVariables(const  std::map<std::string, std::string> &variables) {

  for (auto it = variables.begin(); it != variables.end(); it++) {
      LOGDEBUG(
         std::string msg = ert::tracing::Logger::asString("Var '%s' = '%s'", it->first.c_str(), it->second.c_str());
         ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
      );
  }
}
*/

void calculateAdminProvisionKey(admin_provision_key_t &key, const std::string &inState, const std::string &method, const std::string &uri) {
    // key: <in-state>#<request-method>#<request-uri>
    // hash '#' separator eases regexp usage for stored key
    key = inState;
    key += "#";
    key += method;
    key += "#";
    key += uri;
}


AdminProvision::AdminProvision() : in_state_(DEFAULT_ADMIN_PROVISION_STATE),
    out_state_(DEFAULT_ADMIN_PROVISION_STATE),
    response_delay_ms_(0), mock_request_data_(nullptr) {;}


void AdminProvision::transform( const std::string &requestUri,
                                const std::string &requestUriPath,
                                const std::map<std::string, std::string> &queryParametersMap,
                                const std::string &requestBody,
                                const nghttp2::asio_http2::header_map &requestHeaders,
                                std::uint64_t generalUniqueServerSequence,

                                /* OUTPUT PARAMETERS WHICH ALREADY HAVE DEFAULT VALUES BEFORE TRANSFORMATIONS: */
                                unsigned int &statusCode,
                                nghttp2::asio_http2::header_map &headers,
                                std::string &responseBody,
                                unsigned int &delayMs,
                                std::string &outState,
                                std::string &outStateMethod
                              ) const {

    // Default values without transformations:
    statusCode = getResponseCode();
    headers = getResponseHeaders();
    delayMs = getResponseDelayMilliseconds();
    outState = getOutState(); // prepare next request state, with URI path before transformed with matching algorithms

    // Find out if request body will need to be parsed (this is true if any transformation uses it as source):
    bool usesRequestBodyAsTransformationSource = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
        if ((*it)->getSourceType() == Transformation::SourceType::RequestBody) {
            usesRequestBodyAsTransformationSource = true;
            break;
        }
    }
    nlohmann::json requestBodyJson;
    bool requestBodyJsonParseable = false;
    if (usesRequestBodyAsTransformationSource) {
        if (!requestBody.empty()) {
            requestBodyJsonParseable = h2agent::http2server::parseJsonContent(requestBody, requestBodyJson);
        }
        else {
            LOGINFORMATIONAL(ert::tracing::Logger::informational("No request body received: some transformations will be ignored", ERT_FILE_LOCATION));
        }
    }

    // Find out if response body will need to be cloned (this is true if any transformation uses it as target):
    bool usesResponseBodyAsTransformationTarget = false;
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {
        if ((*it)->getTargetType() == Transformation::TargetType::ResponseBodyString ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyInteger ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyUnsigned ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyFloat ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyBoolean ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyObject ||
                (*it)->getTargetType() == Transformation::TargetType::ResponseBodyJsonString) {
            usesResponseBodyAsTransformationTarget = true;
            break;
        }
    }
    nlohmann::json responseBodyJson;
    if (usesResponseBodyAsTransformationTarget) {
        responseBodyJson = getResponseBody();   // clone provision response body to manipulate this copy and finally we will dump() it over 'responseBody':
        // if(usesResponseBodyAsTransformationTarget) responseBody = responseBodyJson.dump(); <--- place this after transformations (*)
    }
    else {
        responseBody = getResponseBody().dump();
    }

    // Dynamic variables map: inherited along the transformation chain
    std::map<std::string, std::string> variables; // source & target variables (key=variable name/value=variable value)

    // Type converter:
    TypeConverter sourceVault;

    // Apply transformations sequentially
    for (auto it = transformations_.begin(); it != transformations_.end(); it ++) {

        auto transformation = (*it);

        // SOURCES: RequestUri, RequestUriPath, RequestUriParam, RequestBody, RequestHeader, GeneralRandom, GeneralTimestamp, GeneralStrftime, GeneralUnique, SVar, Value, Event, InState
        if (transformation->getSourceType() == Transformation::SourceType::RequestUri) {
            sourceVault.setString(requestUri);
        }
        else if (transformation->getSourceType() == Transformation::SourceType::RequestUriPath) {
            sourceVault.setString(requestUriPath);
        }
        else if (transformation->getSourceType() == Transformation::SourceType::RequestUriParam) {
            auto it = queryParametersMap.find(transformation->getSource());
            if (it != queryParametersMap.end()) sourceVault.setString(it->second);
            else {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract query parameter '%s' in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }
        }
        else if (transformation->getSourceType() == Transformation::SourceType::RequestBody) {
            if(!requestBodyJsonParseable) continue;
            if (!sourceVault.setObject(requestBodyJson, transformation->getSource() /* document path (empty or not to be whole or node) */)) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract path '%s' from request body (it is null) in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }

        }
        else if (transformation->getSourceType() == Transformation::SourceType::RequestHeader) {
            auto it = requestHeaders.find(transformation->getSource());
            if (it != requestHeaders.end()) sourceVault.setString(it->second.value);
            else {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract request header '%s' in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }
        }
        else if (transformation->getSourceType() == Transformation::SourceType::GeneralRandom) {
            int range = transformation->getSourceI2() - transformation->getSourceI1() + 1;
            sourceVault.setInteger(transformation->getSourceI1() + (rand () % range));
        }
        else if (transformation->getSourceType() == Transformation::SourceType::GeneralTimestamp) {
            if (transformation->getSource() == "s") {
                sourceVault.setInteger(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            }
            else if (transformation->getSource() == "ms") {
                sourceVault.setInteger(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            }
            else if (transformation->getSource() == "ns") {
                sourceVault.setInteger(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            }
        }
        else if (transformation->getSourceType() == Transformation::SourceType::GeneralStrftime) {
            std::time_t unixTime;
            std::time (&unixTime);
            char buffer[100] = {0};
            /*size_t size = */strftime(buffer, sizeof(buffer), transformation->getSource().c_str(), localtime(&unixTime));
            //if (size > 1) { // convert TZ offset to RFC3339 format
            //    char minute[] = { buffer[size-2], buffer[size-1], '\0' };
            //    sprintf(buffer + size - 2, ":%s", minute);
            //}

            sourceVault.setString(std::string(buffer));
        }
        else if (transformation->getSourceType() == Transformation::SourceType::GeneralUnique) {
            sourceVault.setUnsigned(generalUniqueServerSequence);
        }
        else if (transformation->getSourceType() == Transformation::SourceType::SVar) {
            auto it = variables.find(transformation->getSource());
            if (it != variables.end()) sourceVault.setString(it->second);
            else {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract source variable '%s' in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }
        }
        else if (transformation->getSourceType() == Transformation::SourceType::Value) {
            sourceVault.setStringReplacingVariables(transformation->getSource(), variables); // replace variables if they exist
        }
        else if (transformation->getSourceType() == Transformation::SourceType::Event) {
            std::string var_id_prefix = transformation->getSource();

            auto it = variables.find(var_id_prefix + ".method");
            std::string event_method = (it != variables.end()) ? (it->second):"";

            it = variables.find(var_id_prefix + ".uri");
            std::string event_uri = (it != variables.end()) ? (it->second):"";

            it = variables.find(var_id_prefix + ".number");
            std::string event_number = (it != variables.end()) ? (it->second):"";

            it = variables.find(var_id_prefix + ".path");
            std::string event_path = (it != variables.end()) ? (it->second):"";

            // Now, access the server data for the former selection values:
            nlohmann::json object;
            auto mockRequest = mock_request_data_->getMockRequest(event_method, event_uri, event_number);
            if (!mockRequest) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unable to extract event for variable '%s' in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }

            if (!sourceVault.setObject(mockRequest->getJson(), event_path /* document path (empty or not to be whole 'requests number' or node) */)) {
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Unexpected error extracting event for variable '%s' in transformation item", transformation->getSource().c_str());
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                continue;
            }

            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Extracted object from event:\n%s", sourceVault.asString().c_str());
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
        }
        else if (transformation->getSourceType() == Transformation::SourceType::InState) {
            sourceVault.setString(getInState());
        }

        std::string target;
        std::int64_t targetI;
        std::uint64_t targetU;
        double targetF;
        bool boolean;
        nlohmann::json obj;
        bool success;
        std::smatch matches;

        // FILTERS: RegexCapture, RegexReplace, Append, Prepend, AppendVar, PrependVar, Sum, Multiply, ConditionVar
        bool hasFilter = transformation->hasFilter();
        if (hasFilter) {
            std::string source;

            // all the filters except Sum/Multiply, require a string target
            if (transformation->getFilterType() != Transformation::FilterType::Sum && transformation->getFilterType() != Transformation::FilterType::Multiply) {
                source = sourceVault.getString(success);
                if (!success) continue;
            }

            try { // std::regex exceptions
                if (transformation->getFilterType() == Transformation::FilterType::RegexCapture) {

                    if (std::regex_match(source, matches, transformation->getFilterRegex()) && matches.size() >=1) {
                        target = matches[0];
                        sourceVault.setString(target);
                        LOGDEBUG(
                            std::stringstream ss;
                            ss << "Regex matches: Size = " << matches.size();
                        for(size_t i=0; i < matches.size(); ++i) {
                        ss << " | [" << i << "] = " << matches[i];
                        }
                        ert::tracing::Logger::debug(ss.str(), ERT_FILE_LOCATION);
                        );
                    }
                    else {
                        LOGDEBUG(
                            std::string msg = ert::tracing::Logger::asString("Unable to match '%s' againt regex capture '%s' in transformation item", source.c_str(), transformation->getFilter().c_str());
                            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                        );
                        continue;
                    }
                }
                else if (transformation->getFilterType() == Transformation::FilterType::RegexReplace) {
                    target = std::regex_replace (source, transformation->getFilterRegex(), transformation->getFilter() /* fmt */);
                    sourceVault.setString(target);
                }
                else if (transformation->getFilterType() == Transformation::FilterType::Append) {
                    target = source + transformation->getFilter();
                    sourceVault.setString(target);
                }
                else if (transformation->getFilterType() == Transformation::FilterType::Prepend) {
                    target = transformation->getFilter() + source;
                    sourceVault.setString(target);
                }
                else if (transformation->getFilterType() == Transformation::FilterType::AppendVar) {
                    auto it = variables.find(transformation->getFilter());
                    if (it != variables.end()) target = source + (it->second);
                    else {
                        target = source;
                        LOGDEBUG(
                            std::string msg = ert::tracing::Logger::asString("Unable to extract variable '%s' in transformation item (empty value assumed)", transformation->getFilter().c_str());
                            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                        );
                    }
                    sourceVault.setString(target);
                }
                else if (transformation->getFilterType() == Transformation::FilterType::PrependVar) {
                    auto it = variables.find(transformation->getFilter());
                    if (it != variables.end()) target = (it->second) + source;
                    else {
                        target = source;
                        LOGDEBUG(
                            std::string msg = ert::tracing::Logger::asString("Unable to extract variable '%s' in transformation item (empty value assumed)", transformation->getFilter().c_str());
                            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                        );
                    }
                    sourceVault.setString(target);
                }
                else if (transformation->getFilterType() == Transformation::FilterType::Sum) {
                    if (transformation->getFilterNumberType() == 0 /* integer */) {
                        targetI = sourceVault.getInteger(success);
                        if (success) targetI += transformation->getFilterI();
                        else continue;
                        sourceVault.setInteger(targetI);
                    }
                    else if (transformation->getFilterNumberType() == 1 /* unsigned */) {
                        targetU = sourceVault.getUnsigned(success);
                        if (success) targetU += transformation->getFilterU();
                        else continue;
                        sourceVault.setUnsigned(targetU);
                    }
                    else if (transformation->getFilterNumberType() == 2 /* double */) {
                        targetF = sourceVault.getFloat(success);
                        if (success) targetF += transformation->getFilterF();
                        else continue;
                        sourceVault.setFloat(targetF);
                    }
                }
                else if (transformation->getFilterType() == Transformation::FilterType::Multiply) {
                    if (transformation->getFilterNumberType() == 0 /* integer */) {
                        targetI = sourceVault.getInteger(success);
                        if (success) targetI *= transformation->getFilterI();
                        else continue;
                        sourceVault.setInteger(targetI);
                    }
                    else if (transformation->getFilterNumberType() == 1 /* unsigned */) {
                        targetU = sourceVault.getUnsigned(success);
                        if (success) targetU *= transformation->getFilterU();
                        else continue;
                        sourceVault.setUnsigned(targetU);
                    }
                    else if (transformation->getFilterNumberType() == 2 /* double */) {
                        targetF = sourceVault.getFloat(success);
                        if (success) targetF *= transformation->getFilterF();
                        else continue;
                        sourceVault.setFloat(targetF);
                    }
                }
                else if (transformation->getFilterType() == Transformation::FilterType::ConditionVar) {
                    // Get variable value for the variable name 'transformation->getFilter()':
                    auto it = variables.find(transformation->getFilter());
                    if ((it != variables.end()) && !(it->second.empty()))
                        sourceVault.setString(source);
                    else
                        continue;
                }
            }
            catch (std::exception& e)
            {
                ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            }
        }

        // TARGETS: ResponseBodyString, ResponseBodyInteger, ResponseBodyUnsigned, ResponseBodyFloat, ResponseBodyBoolean, ResponseBodyObject, ResponseBodyJsonString, ResponseHeader, ResponseStatusCode, ResponseDelayMs, TVar, OutState
        try { // nlohmann::json exceptions
            if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyString) {
                // extraction
                target = sourceVault.getString(success);
                if (!success) continue;
                // assignment
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());
                responseBodyJson[j_ptr] = target;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyInteger) {
                // extraction
                targetI = sourceVault.getInteger(success);
                if (!success) continue;
                // assignment
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());
                responseBodyJson[j_ptr] = targetI;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyUnsigned) {
                // extraction
                targetU = sourceVault.getUnsigned(success);
                if (!success) continue;
                // assignment
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());
                responseBodyJson[j_ptr] = targetU;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyFloat) {
                // extraction
                targetF = sourceVault.getFloat(success);
                if (!success) continue;
                // assignment
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());
                responseBodyJson[j_ptr] = targetF;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyBoolean) {
                // extraction
                boolean = sourceVault.getBoolean(success);
                if (!success) continue;
                // assignment
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());
                responseBodyJson[j_ptr] = boolean;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyObject) {
                // extraction will be object if possible, falling back to the rest of formats with this priority: string, integer, unsigned, float, boolean
                // assignment for valid extraction
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());

                obj = sourceVault.getObject(success);
                if (!success) {
                    target = sourceVault.getString(success);
                    if (!success) {
                        targetI = sourceVault.getInteger(success);;
                        if (!success) {
                            targetU = sourceVault.getUnsigned(success);
                            if (!success) {
                                targetF = sourceVault.getFloat(success);
                                if (!success) {
                                    boolean = sourceVault.getBoolean(success);
                                    if (!success) continue;
                                    else responseBodyJson[j_ptr] = boolean;
                                }
                                else responseBodyJson[j_ptr] = targetF;
                            }
                            else responseBodyJson[j_ptr] = targetU;
                        }
                        else responseBodyJson[j_ptr] = targetI;
                    }
                    else responseBodyJson[j_ptr] = target;
                }
                else responseBodyJson[j_ptr] = obj;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseBodyJsonString) {
                // assignment for valid extraction
                nlohmann::json::json_pointer j_ptr(transformation->getTarget());

                // extraction
                target = sourceVault.getString(success);
                if (!success) continue;
                if (!h2agent::http2server::parseJsonContent(target, obj))
                    continue;

                // assignment
                responseBodyJson[j_ptr] = obj;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseHeader) {
                // extraction
                target = sourceVault.getString(success);
                if (!success) continue;
                // assignment
                headers.emplace(transformation->getTarget(), nghttp2::asio_http2::header_value{target});
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseStatusCode) {
                // extraction
                targetU = sourceVault.getUnsigned(success);
                if (!success) continue;
                // assignment
                statusCode = targetU;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::ResponseDelayMs) {
                // extraction
                targetU = sourceVault.getUnsigned(success);
                if (!success) continue;
                // assignment
                delayMs = targetU;
            }
            else if (transformation->getTargetType() == Transformation::TargetType::TVar) {
                if (hasFilter && transformation->getFilterType() == Transformation::FilterType::RegexCapture) {
                    std::string varname;
                    if (matches.size() >=1) { // this protection shouldn't be needed as it would be continued above on RegexCapture matching...
                        variables[transformation->getTarget()] = matches[0]; // variable "as is" stores the entire match
                        for(size_t i=1; i < matches.size(); ++i) {
                            varname = transformation->getTarget();
                            varname += ".";
                            varname += std::to_string(i);
                            variables[varname] = matches[i];
                        }
                    }
                }
                else {
                    // extraction
                    target = sourceVault.getString(success);
                    if (!success) continue;
                    // assignment
                    variables[transformation->getTarget()] = target;
                }
            }
            else if (transformation->getTargetType() == Transformation::TargetType::OutState) {
                // extraction
                target = sourceVault.getString(success);
                if (!success) continue;
                // assignment
                outState = target;
                outStateMethod = transformation->getTarget(); // if empty, means current method
            }
        }
        catch (std::exception& e)
        {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        }
    }


    // (*) Regenerate final responseBody after transformations:
    if(usesResponseBodyAsTransformationTarget) responseBody = responseBodyJson.dump();
}

bool AdminProvision::load(const nlohmann::json &j, bool priorityMatchingRegexConfigured) {

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto requestMethod_it = j.find("requestMethod");
    request_method_ = *requestMethod_it;

    auto it = j.find("responseCode");
    response_code_ = *it;

    // Optional
    it = j.find("requestUri");
    if (it != j.end() && it->is_string()) {
        request_uri_ = *it;
    }

    it = j.find("inState");
    if (it != j.end() && it->is_string()) {
        in_state_ = *it;
    }

    it = j.find("outState");
    if (it != j.end() && it->is_string()) {
        out_state_ = *it;
    }

    it = j.find("responseHeaders");
    if (it != j.end() && it->is_object()) {
        loadResponseHeaders(*it);
    }

    it = j.find("responseBody");
    if (it != j.end() && it->is_object()) {
        response_body_ = *it;
    }

    it = j.find("responseDelayMs");
    if (it != j.end() && it->is_number()) {
        response_delay_ms_ = *it;
    }

    auto transform_it = j.find("transform");
    if (transform_it != j.end()) {
        for (auto it : *transform_it) { // "it" is of type json::reference and has no key() member
            loadTransformation(it);
        }
    }

    // Store key:
    calculateAdminProvisionKey(key_, in_state_, request_method_, request_uri_);

    if (priorityMatchingRegexConfigured) {
        // Precompile regex with key, only for 'PriorityMatchingRegex' algorithm:
        try {
            regex_.assign(key_, std::regex::optimize);
        }
        catch (std::regex_error &e) {
            ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            ert::tracing::Logger::error("Invalid regular expression (detected when joining 'inState' and/or 'requestUri' from provision) for current 'PriorityMatchingRegex' server matching algorithm", ERT_FILE_LOCATION);
            return false;
        }
    }

    return true;
}

void AdminProvision::loadResponseHeaders(const nlohmann::json &j) {
    for (auto& [key, val] : j.items())
        response_headers_.emplace(key, nghttp2::asio_http2::header_value{val});
}

void AdminProvision::loadTransformation(const nlohmann::json &j) {

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Loading transformation item:\n%s", j.dump().c_str()); // avoid newlines in traces (dump(n) pretty print)
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Transformation object to fill:
    auto transformation = std::make_shared<Transformation>();

    if (transformation->load(j)) {
        transformations_.push_back(transformation);
    }
    else {
        ert::tracing::Logger::error("Discarded transform filter due to incoherent data", ERT_FILE_LOCATION);
    }
}


}
}

