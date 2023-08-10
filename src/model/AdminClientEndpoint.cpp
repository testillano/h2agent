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
#include <string>
#include <algorithm>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>

#include <AdminClientEndpoint.hpp>


#include <functions.hpp>


namespace h2agent
{
namespace model
{

AdminClientEndpoint::AdminClientEndpoint(const std::string &applicationName) : application_name_(applicationName) {;}
AdminClientEndpoint::~AdminClientEndpoint() {;}

void AdminClientEndpoint::connect(bool fromScratch) {

    if (fromScratch) {
        client_.reset();
    }
    if (!client_) {

        std::string keyAdaptedToMetricsName = h2agent::model::fixMetricsName(key_);
        client_ = std::make_shared<h2agent::http2::MyTrafficHttp2Client>(application_name_ + "_" + keyAdaptedToMetricsName, host_, std::to_string(port_), secure_, nullptr);
        try {
            client_->enableMetrics(metrics_, response_delay_seconds_histogram_bucket_boundaries_, message_size_bytes_histogram_bucket_boundaries_);
        }
        catch(std::exception &e)
        {
            client_->enableMetrics(nullptr); // force no metrics again
            std::string msg = ert::tracing::Logger::asString("Cannot enable metrics for client '%s': %s", keyAdaptedToMetricsName.c_str(), e.what());
            ert::tracing::Logger::error(msg, ERT_FILE_LOCATION);
        }
    }
}

void AdminClientEndpoint::setMetricsData(ert::metrics::Metrics *metrics, const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries,
        const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries) {

    metrics_ = metrics;
    response_delay_seconds_histogram_bucket_boundaries_ = responseDelaySecondsHistogramBucketBoundaries;
    message_size_bytes_histogram_bucket_boundaries_ = messageSizeBytesHistogramBucketBoundaries;
}

bool AdminClientEndpoint::load(const nlohmann::json &j) {

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto it = j.find("id");
    key_ = *it;

    it = j.find("host");
    host_ = *it;

    it = j.find("port");
    port_ = *it;

    // Optionals
    it = j.find("secure");
    secure_ = false;
    if (it != j.end() && it->is_boolean()) {
        secure_ = *it;
    }
    it = j.find("permit");
    permit_ = true;
    if (it != j.end() && it->is_boolean()) {
        permit_ = *it;
    }

    // Validations not in schema:
    if (key_.empty()) {
        ert::tracing::Logger::error("Invalid client endpoint identifier: provide a non-empty string", ERT_FILE_LOCATION);
        return false;
    }

    if (host_.empty()) {
        ert::tracing::Logger::error("Invalid client endpoint host: provide a non-empty string", ERT_FILE_LOCATION);
        return false;
    }

    return true;
}

nlohmann::json AdminClientEndpoint::asJson() const
{
    if (!client_) return json_;

    nlohmann::json result = json_;
    result["status"] = client_->getConnectionStatus();

    return result;
}

std::uint64_t AdminClientEndpoint::getGeneralUniqueClientSequence() const
{
    if (!client_) return 1;

    return client_->getGeneralUniqueClientSequence();
}

}
}

