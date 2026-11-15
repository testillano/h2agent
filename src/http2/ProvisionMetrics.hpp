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

#include <string>

#include <ert/metrics/Metrics.hpp>
#include <ert/http2comm/MetricsCache.hpp>

namespace h2agent
{
namespace http2
{

/**
 * Per-provision metrics (rate and latency per provision and stage):
 *
 *   <prefix>_provision_requests_counter         [source] [provision] [state]
 *   <prefix>_provision_responses_counter        [source] [provision] [state] [status_code]
 *   <prefix>_provision_response_delay_seconds   [source] [provision] [state] (histogram)
 *
 * 'provision' is the provision id and 'state' its inState: literals of the loaded
 * provisions, so the cardinality is bounded. Metric references are resolved once
 * per labels combination (MetricsCache): the hot path builds a short key and does
 * a shared-lock lookup. Thread safe.
 */
class ProvisionMetrics
{
public:
    /** Metrics created together for a provision stage (same labels). */
    struct Stage
    {
        ert::metrics::counter_t *requests{};
        ert::metrics::histogram_t *delay{};
    };

private:
    ert::metrics::counter_family_t *requests_family_{};
    ert::metrics::counter_family_t *responses_family_{};
    ert::metrics::histogram_family_t *delay_family_{};
    ert::metrics::bucket_boundaries_t boundaries_{};
    std::string source_{};

    ert::http2comm::MetricsCache<Stage> stages_{};
    ert::http2comm::MetricsCache<ert::metrics::counter_t *> responses_{};

    const Stage &stage(const std::string &provision, const std::string &state) {
        return stages_.get(key(provision, state), [&] {
            const ert::metrics::labels_t labels = {{"source", source_}, {"provision", provision}, {"state", state}};
            return Stage{&requests_family_->Add(labels), &delay_family_->Add(labels, boundaries_)};
        });
    }

    static std::string key(const std::string &provision, const std::string &state) {
        std::string k = provision;
        k += '\x1f'; // not in a provision id nor in a state
        k += state;
        return k;
    }

public:
    ProvisionMetrics() = default;
    ProvisionMetrics(const ProvisionMetrics&) = delete;
    ProvisionMetrics& operator=(const ProvisionMetrics&) = delete;

    /**
     * Creates the families (call once, before traffic)
     *
     * @param metrics Metrics object (nothing is done when null)
     * @param prefix Family names prefix: 'h2agent_traffic_server' or 'h2agent_traffic_client'
     * @param source Source label
     * @param boundaries Response delay histogram bucket boundaries (seconds)
     */
    void enable(ert::metrics::Metrics *metrics, const std::string &prefix, const std::string &source, const ert::metrics::bucket_boundaries_t &boundaries) {
        if (!metrics) return;
        requests_family_ = &(metrics->addCounterFamily(prefix + "_provision_requests_counter", "Requests per provision and stage in " + prefix));
        responses_family_ = &(metrics->addCounterFamily(prefix + "_provision_responses_counter", "Responses per provision, stage and status code in " + prefix));
        delay_family_ = &(metrics->addHistogramFamily(prefix + "_provision_response_delay_seconds", "Response delay (seconds) per provision and stage in " + prefix));
        boundaries_ = boundaries;
        source_ = source;
    }

    bool enabled() const {
        return requests_family_ != nullptr;
    }

    /**
     * Counts a request of a provision stage
     *
     * @return The stage metrics (its delay histogram is observed later), or nullptr when disabled
     */
    const Stage *request(const std::string &provision, const std::string &state) {
        if (!enabled()) return nullptr;
        const Stage &s = stage(provision, state);
        s.requests->Increment();
        return &s;
    }

    /**
     * Response counter of a provision stage and status code (resolved, not incremented)
     *
     * @return Counter, or nullptr when disabled
     */
    ert::metrics::counter_t *responseCounter(const std::string &provision, const std::string &state, int statusCode) {
        if (!enabled()) return nullptr;
        return responses_.get(key(provision, state) + '\x1f' + std::to_string(statusCode), [&] {
            return &responses_family_->Add({{"source", source_}, {"provision", provision}, {"state", state}, {"status_code", std::to_string(statusCode)}});
        });
    }

    /**
     * Counts a response of a provision stage and observes its delay
     *
     * @param delaySeconds Response delay; negative to observe nothing
     */
    void response(const std::string &provision, const std::string &state, int statusCode, double delaySeconds) {
        if (!enabled()) return;
        responseCounter(provision, state, statusCode)->Increment();
        if (delaySeconds >= 0) stage(provision, state).delay->Observe(delaySeconds);
    }
};

}
}
