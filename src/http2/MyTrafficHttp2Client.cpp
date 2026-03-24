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
#include <errno.h>


#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/URLFunctions.hpp>

#include <MyTrafficHttp2Client.hpp>

#include <AdminData.hpp>
#include <MockClientData.hpp>
#include <Configuration.hpp>
#include <Vault.hpp>
#include <FileManager.hpp>
#include <SocketManager.hpp>
#include <functions.hpp>

namespace h2agent
{
namespace http2
{

void MyTrafficHttp2Client::enableMyMetrics(ert::metrics::Metrics *metrics, const std::string &source) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::labels_t familyLabels = {{"source", (source.empty() ? name_ : source)}};

        ert::metrics::counter_family_t& cf = metrics->addCounterFamily("h2agent_traffic_client_provisioned_requests_counter", "Requests provisioned counter in h2agent_traffic_client", familyLabels);
        provisioned_requests_successful_counter_ = &(cf.Add({{"result", "successful"}}));
        provisioned_requests_failed_counter_ = &(cf.Add({{"result", "failed"}}));

        ert::metrics::counter_family_t& cf2 = metrics->addCounterFamily("h2agent_traffic_client_purged_contexts_counter", "Contexts purged counter in h2agent_traffic_client", familyLabels);
        purged_contexts_successful_counter_ = &(cf2.Add({{"result", "successful"}}));
        purged_contexts_failed_counter_ = &(cf2.Add({{"result", "failed"}}));

        ert::metrics::counter_family_t& cf3 = metrics->addCounterFamily("h2agent_traffic_client_unexpected_response_status_code_counter", "Unexpected response status code counter in h2agent_traffic_client", familyLabels);
        unexpected_response_status_code_counter_ = &(cf3.Add({}));
    }
}

}
}
