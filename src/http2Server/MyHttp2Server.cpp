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

#include <boost/optional.hpp>
#include <sstream>
#include <map>
#include <chrono>
#include <errno.h>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>

#include <MyHttp2Server.hpp>

#include <AdminData.hpp>
#include <MockRequestData.hpp>
#include <functions.hpp>

namespace h2agent
{
namespace http2server
{

MyHttp2Server::MyHttp2Server(size_t workerThreads):
    ert::http2comm::Http2Server("MockHttp2Server", workerThreads),
    admin_data_(nullptr) {

    mock_request_data_ = new model::MockRequestData();
}

bool MyHttp2Server::checkMethodIsAllowed(
    const nghttp2::asio_http2::server::request& req,
    std::vector<std::string>& allowedMethods)
{
    allowedMethods = {"POST", "GET"};
    return (req.method() == "POST" || req.method() == "GET");
}

bool MyHttp2Server::checkMethodIsImplemented(
    const nghttp2::asio_http2::server::request& req)
{
    return (req.method() == "POST" || req.method() == "GET");
}


bool MyHttp2Server::checkHeaders(const nghttp2::asio_http2::server::request&
                                 req)
{
    return true;
    /*
        auto ctype = req.header().find("content-type");
        auto ctype_end = std::end(req.header());

        return ((ctype != ctype_end) ? (ctype->second.value == "application/json") :
                false);
    */
}

void MyHttp2Server::receive(const nghttp2::asio_http2::server::request& req,
                            const std::string& requestBody,
                            unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                            std::string& responseBody)
{
    LOGDEBUG(ert::tracing::Logger::debug("receive()",  ERT_FILE_LOCATION));

    // see uri_ref struct (https://nghttp2.org/documentation/asio_http2.h.html#asio-http2-h)
    std::string method = req.method();
    std::string uriPath = req.uri().raw_path; // percent-encoded; ('req.uri().path' is decoded)
    std::string uriRawQuery = req.uri().raw_query; // percent-encoded
    //std::string reqUriFragment = req.uri().fragment; // https://stackoverflow.com/a/65198345/2576671

    // Query parameters transformation:
    h2agent::model::AdminMatchingData::UriPathQueryParametersFilterType uriPathQueryParametersFilterType = getAdminData()->getMatchingData(). getUriPathQueryParametersFilter();

    if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::Ignore) {
        uriRawQuery = "";
    }
    else if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::Sort) {
        h2agent::http2server::sortQueryParameters(uriRawQuery);
    }
    else if (uriPathQueryParametersFilterType == h2agent::model::AdminMatchingData::SortSemicolon) {
        h2agent::http2server::sortQueryParameters(uriRawQuery, ';');
    }

    if (uriRawQuery != "") {
        uriPath += "?";
        uriPath += uriRawQuery;
    }

    LOGDEBUG(
        std::string msg;
        msg = ert::tracing::Logger::asString("Method: %s; Uri Path (and query parameters if not ignored): '%s'; Request Body: %s",
                method.c_str(),
                uriPath.c_str(),
                requestBody.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    // Admin provision & matching configuration:
    const h2agent::model::AdminProvisionData & provisionData = getAdminData()->getProvisionData();
    const h2agent::model::AdminMatchingData & matchingData = getAdminData()->getMatchingData();

    // Find mock context:
    std::string inState;
    bool requestFound = getMockRequestData()->find(method, uriPath, inState); // if not found, inState will be 'initial'

    // Matching algorithm:
    h2agent::model::AdminMatchingData::AlgorithmType algorithmType = matchingData.getAlgorithm();
    std::shared_ptr<h2agent::model::AdminProvision> provision;

    if (algorithmType == h2agent::model::AdminMatchingData::FullMatching) {
        provision = provisionData.find(inState, method, uriPath);
    }

    else if (algorithmType == h2agent::model::AdminMatchingData::FullMatchingRegexReplace) {

        std::string transformedUriPath = std::regex_replace (uriPath, matchingData.getRgx(), matchingData.getFmt());
        LOGDEBUG(
            std::string msg = ert::tracing::Logger::asString("Uri Path after regex-replace transformation: '%s'", transformedUriPath.c_str());
            ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
        );

        provision = provisionData.find(inState, method, transformedUriPath);
    }
    else if (algorithmType == h2agent::model::AdminMatchingData::PriorityMatchingRegex) {

        provision = provisionData.findWithPriorityMatchingRegex(inState, method, uriPath);
    }

    if (provision) {
        statusCode = provision->getResponseCode();
        headers = provision->getResponseHeaders();
        responseBody = provision->getResponseBody().dump();
        unsigned int delayMs = provision->getResponseDelayMilliseconds();

        // Prepare next request state, with URI path before transformed with matching algorithms:
        //if (!requestFound)
        getMockRequestData()->load(provision->getOutState(), method, uriPath, req.header(), responseBody);

        if (delayMs != 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    else {
        statusCode = 501; // not implemented
    }

}

}
}
