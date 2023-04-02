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

#include <memory>
#include <string>
#include <vector>
#include <regex>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <AdminSchema.hpp>
#include <Transformation.hpp>
#include <TypeConverter.hpp>
#include <DataPart.hpp>

#include <MyTrafficHttp2Client.hpp>

#include <ert/metrics/Metrics.hpp>



namespace h2agent
{
namespace model
{

// Client endpoint key:
typedef std::string admin_client_endpoint_key_t;

class AdminClientEndpoint
{
    nlohmann::json json_{}; // client endpoint reference

    admin_client_endpoint_key_t key_{};

    // Cached information:
    std::string host_{};
    int port_{};
    bool secure_{};
    bool permit_{};

    std::shared_ptr<h2agent::http2::MyTrafficHttp2Client> client_{};

    // Metrics for client:
    ert::metrics::Metrics *metrics_{};
    ert::metrics::bucket_boundaries_t response_delay_seconds_histogram_bucket_boundaries_{};
    ert::metrics::bucket_boundaries_t message_size_bytes_histogram_bucket_boundaries_{};

public:

    AdminClientEndpoint();
    ~AdminClientEndpoint();

    // setters:

    /**
     * Load client endpoint information
     *
     * @param j Json client endpoint object
     *
     * @return Operation success
     */
    bool load(const nlohmann::json &j);

    /**
     * Connects the endpoint
     *
     * @param fromScratch Indicates if the previous client must be dropped
     */
    void connect(bool fromScratch = false);

    /**
     * Set metrics data
     *
     * @param metrics Optional metrics object to compute counters and histograms
     * @param responseDelaySecondsHistogramBucketBoundaries Optional bucket boundaries for response delay seconds histogram
     * @param messageSizeBytesHistogramBucketBoundaries Optional bucket boundaries for message size bytes histogram
     */
    void setMetricsData(ert::metrics::Metrics *metrics, const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries,
                        const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries);

    // getters:

    /**
     * Gets the client endpoint key
     *
     * @return Client endpoint key
     */
    const admin_client_endpoint_key_t &getKey() const {
        return key_;
    }

    /**
     * Json for class information and also current connection status
     *
     * @return Json object
     */
    nlohmann::json asJson() const;

    /**
     * Configured host
     *
     * @return Host
     */
    const std::string &getHost() const {
        return host_;
    }

    /**
     * Configured port
     *
     * @return Port
     */
    int getPort() const {
        return port_;
    }

    /*
     * Configured secure field
     *
     * @return Secure boolean
     */
    bool getSecure() const {
        return secure_;
    }

    /*
     * Configured permit field
     *
     * @return Permit boolean
     */
    bool getPermit() const {
        return permit_;
    }

    /*
     * Get the associated http2 client
     *
     * @return Http2 client
     */
    std::shared_ptr<h2agent::http2::MyTrafficHttp2Client> getClient() {
        return client_;
    }

    /*
     * Get http2 client general unique sequence
     *
     * @return client sequence (1..N)
     */
    std::uint64_t getGeneralUniqueClientSequence() const;
};

}
}

