#include <ProvisionMetrics.hpp>

#include <string>
#include <thread>
#include <vector>

#include <ert/metrics/Metrics.hpp>

#include <gtest/gtest.h>

using h2agent::http2::ProvisionMetrics;

namespace {
const char *Requests = "h2agent_traffic_client_provision_requests_counter";
const char *Responses = "h2agent_traffic_client_provision_responses_counter";
const char *Delay = "h2agent_traffic_client_provision_response_delay_seconds";

ert::metrics::labels_t stageLabels(const std::string &provision, const std::string &state) {
    return {{"source", "h2agent"}, {"provision", provision}, {"state", state}};
}

double requests(ert::metrics::Metrics &m, const std::string &provision, const std::string &state) {
    auto &family = m.addCounterFamily(Requests, "");
    const auto labels = stageLabels(provision, state);
    return family.Has(labels) ? family.Add(labels).Value() : -1;
}

double responses(ert::metrics::Metrics &m, const std::string &provision, const std::string &state, int code) {
    auto &family = m.addCounterFamily(Responses, "");
    auto labels = stageLabels(provision, state);
    labels["status_code"] = std::to_string(code);
    return family.Has(labels) ? family.Add(labels).Value() : -1;
}

std::uint64_t delayCount(ert::metrics::Metrics &m, const std::string &provision, const std::string &state, double *sum = nullptr) {
    auto &family = m.addHistogramFamily(Delay, "");
    const auto labels = stageLabels(provision, state);
    if (!family.Has(labels)) return 0;
    const auto metric = family.Add(labels, ert::metrics::bucket_boundaries_t{}).Collect();
    if (sum) *sum = metric.histogram.sample_sum;
    return metric.histogram.sample_count;
}
}  // namespace

TEST(ProvisionMetrics_test, DisabledDoesNothing) {
    ProvisionMetrics pm;
    EXPECT_FALSE(pm.enabled());
    EXPECT_EQ(pm.request("flow", "initial"), nullptr);
    EXPECT_EQ(pm.responseCounter("flow", "initial", 200), nullptr);
    pm.response("flow", "initial", 200, 0.1);  // no crash
    pm.enable(nullptr, "h2agent_traffic_client", "h2agent", {});
    EXPECT_FALSE(pm.enabled());
}

TEST(ProvisionMetrics_test, SeriesPerProvisionStageAndStatusCode) {
    ert::metrics::Metrics metrics;
    ProvisionMetrics pm;
    pm.enable(&metrics, "h2agent_traffic_client", "h2agent", {0.01, 0.1});
    ASSERT_TRUE(pm.enabled());

    // two stages of a chain, and another provision
    pm.request("order", "initial");
    pm.request("order", "initial");
    pm.request("order", "pay");
    pm.request("refund", "initial");
    pm.response("order", "initial", 201, 0.005);
    pm.response("order", "initial", 503, 0.2);
    pm.response("order", "pay", 200, 0.05);

    EXPECT_EQ(requests(metrics, "order", "initial"), 2);
    EXPECT_EQ(requests(metrics, "order", "pay"), 1);
    EXPECT_EQ(requests(metrics, "refund", "initial"), 1);
    EXPECT_EQ(responses(metrics, "order", "initial", 201), 1);
    EXPECT_EQ(responses(metrics, "order", "initial", 503), 1);
    EXPECT_EQ(responses(metrics, "order", "pay", 200), 1);
    EXPECT_EQ(responses(metrics, "refund", "initial", 200), -1);  // no series without a response

    double sum = 0;
    EXPECT_EQ(delayCount(metrics, "order", "initial", &sum), 2u);
    EXPECT_DOUBLE_EQ(sum, 0.205);
    EXPECT_EQ(delayCount(metrics, "refund", "initial"), 0u);  // created with its request, nothing observed

    // a negative delay is not observed (response counted)
    pm.response("order", "pay", 200, -1);
    EXPECT_EQ(responses(metrics, "order", "pay", 200), 2);
    EXPECT_EQ(delayCount(metrics, "order", "pay"), 1u);

    // the stage returned by request() holds its delay histogram (server: observed on stream close)
    const ProvisionMetrics::Stage *stage = pm.request("order", "pay");
    ASSERT_NE(stage, nullptr);
    stage->delay->Observe(0.02);
    EXPECT_EQ(delayCount(metrics, "order", "pay"), 2u);
    EXPECT_EQ(requests(metrics, "order", "pay"), 2);
}

TEST(ProvisionMetrics_test, RepeatedIdsAggregateAndInstancesShareFamilies) {
    ert::metrics::Metrics metrics;
    ProvisionMetrics a, b;  // e.g. two client workers of an endpoint
    a.enable(&metrics, "h2agent_traffic_client", "h2agent", {});
    b.enable(&metrics, "h2agent_traffic_client", "h2agent", {});
    a.request("group", "initial");
    b.request("group", "initial");
    EXPECT_EQ(requests(metrics, "group", "initial"), 2);
}

TEST(ProvisionMetrics_test, ServerPrefix) {
    ert::metrics::Metrics metrics;
    ProvisionMetrics pm;
    pm.enable(&metrics, "h2agent_traffic_server", "h2agent", {});
    pm.request("books", "initial");
    auto &family = metrics.addCounterFamily("h2agent_traffic_server_provision_requests_counter", "");
    EXPECT_EQ(family.Add(stageLabels("books", "initial")).Value(), 1);
}

TEST(ProvisionMetrics_test, Concurrent) {
    ert::metrics::Metrics metrics;
    ProvisionMetrics pm;
    pm.enable(&metrics, "h2agent_traffic_client", "h2agent", {});
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pm, t]() {
            for (int i = 0; i < 1000; ++i) {
                pm.request("flow", (i % 2) ? "a" : "b");
                pm.response("flow", (i % 2) ? "a" : "b", 200 + t, 0.001);
            }
        });
    }
    for (auto &th : threads) th.join();
    EXPECT_EQ(requests(metrics, "flow", "a"), 2000);
    EXPECT_EQ(requests(metrics, "flow", "b"), 2000);
    EXPECT_EQ(responses(metrics, "flow", "a", 201), 500);
    EXPECT_EQ(delayCount(metrics, "flow", "a"), 2000u);
}
