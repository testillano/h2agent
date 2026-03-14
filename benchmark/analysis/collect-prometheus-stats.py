#!/usr/bin/env python3
"""Collect prometheus counter deltas and latency histogram stats between two snapshots."""
import re, sys, json

def parse_counters(path, prefix):
    counts = {}
    try:
        with open(path) as f:
            for line in f:
                if line.startswith(prefix):
                    m = re.search(r'status_code="(\d+)"', line)
                    if m:
                        val = float(line.rstrip().split()[-1])
                        counts[m.group(1)] = counts.get(m.group(1), 0) + val
    except: pass
    return counts

def sum_counter(path, pfx):
    total = 0
    try:
        with open(path) as f:
            for line in f:
                if line.startswith(pfx) and not line.startswith(pfx + '_'):
                    total += float(line.rstrip().split()[-1])
    except: pass
    return total

def parse_histogram(path, metric_prefix):
    """Parse prometheus histogram: returns {label: {buckets: [(le, count)], count, sum}}"""
    histograms = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.rstrip()
                if not line.startswith(metric_prefix) or line.startswith('#'):
                    continue
                # Extract label key (method+status_code for server, source for client)
                labels_match = re.search(r'\{([^}]+)\}', line)
                labels_str = labels_match.group(1) if labels_match else ""
                val = float(line.split()[-1])

                # Build a label key excluding 'le' and 'source'
                label_parts = []
                for kv in re.findall(r'(\w+)="([^"]*)"', labels_str):
                    if kv[0] not in ('le', 'source'):
                        label_parts.append(f'{kv[0]}={kv[1]}')
                label_key = ','.join(sorted(label_parts)) or 'all'

                if label_key not in histograms:
                    histograms[label_key] = {'buckets': [], 'count': 0, 'sum': 0.0}

                if '_bucket{' in line:
                    le_match = re.search(r'le="([^"]+)"', line)
                    if le_match:
                        le = le_match.group(1)
                        le_val = float('inf') if le == '+Inf' else float(le)
                        histograms[label_key]['buckets'].append((le_val, val))
                elif line.startswith(metric_prefix + '_count'):
                    histograms[label_key]['count'] = val
                elif line.startswith(metric_prefix + '_sum'):
                    histograms[label_key]['sum'] = val
    except: pass
    return histograms

def interpolate_percentile(buckets, p, total):
    """Interpolate percentile from cumulative histogram buckets."""
    if total == 0:
        return 0.0
    target = total * p
    buckets = sorted(buckets, key=lambda x: x[0])
    prev_le, prev_count = 0.0, 0.0
    for le, count in buckets:
        if le == float('inf'):
            return prev_le  # best estimate
        if count >= target:
            # Linear interpolation within this bucket
            if count == prev_count:
                return le
            fraction = (target - prev_count) / (count - prev_count)
            return prev_le + fraction * (le - prev_le)
        prev_le, prev_count = le, count
    return prev_le

def compute_latency_stats(before_hist, after_hist):
    """Compute delta histogram stats: avg, p50, p90, p99."""
    results = {}
    all_labels = set(after_hist.keys())
    for label in all_labels:
        a = after_hist.get(label, {'buckets': [], 'count': 0, 'sum': 0.0})
        b = before_hist.get(label, {'buckets': [], 'count': 0, 'sum': 0.0})
        delta_count = a['count'] - b['count']
        delta_sum = a['sum'] - b['sum']
        if delta_count <= 0:
            continue
        avg = delta_sum / delta_count

        # Delta buckets (cumulative counts)
        b_map = {le: cnt for le, cnt in b.get('buckets', [])}
        delta_buckets = [(le, cnt - b_map.get(le, 0)) for le, cnt in a['buckets']]

        p50 = interpolate_percentile(delta_buckets, 0.50, delta_count)
        p90 = interpolate_percentile(delta_buckets, 0.90, delta_count)
        p99 = interpolate_percentile(delta_buckets, 0.99, delta_count)

        results[label] = {
            'count': int(delta_count),
            'avg_ms': avg * 1000,
            'p50_ms': p50 * 1000,
            'p90_ms': p90 * 1000,
            'p99_ms': p99 * 1000,
        }
    return results

# --- Main ---
if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <before.txt> <after.txt> [--json]", file=sys.stderr)
    sys.exit(1)

before_file = sys.argv[1]
after_file = sys.argv[2]
json_output = '--json' in sys.argv

# --- Counters (client mode) ---
prefix = 'h2agent_traffic_client_observed_responses_received_counter'
before = parse_counters(before_file, prefix)
after = parse_counters(after_file, prefix)

delta = {}
for code in after:
    delta[code] = after[code] - before.get(code, 0)

classes = {'2xx': 0, '3xx': 0, '4xx': 0, '5xx': 0}
for code, count in sorted(delta.items()):
    key = code[0] + 'xx'
    if key in classes:
        classes[key] += int(count)

prefix_to = 'h2agent_traffic_client_observed_responses_timedout_counter'
timedout = sum_counter(after_file, prefix_to) - sum_counter(before_file, prefix_to)

prefix_sent = 'h2agent_traffic_client_observed_requests_sents_counter'
prefix_unsent = 'h2agent_traffic_client_observed_requests_unsent_counter'
sent = sum_counter(after_file, prefix_sent) - sum_counter(before_file, prefix_sent)
unsent = sum_counter(after_file, prefix_unsent) - sum_counter(before_file, prefix_unsent)

# --- Latency histograms ---
server_latency = compute_latency_stats(
    parse_histogram(before_file, 'h2agent_traffic_server_responses_delay_seconds'),
    parse_histogram(after_file, 'h2agent_traffic_server_responses_delay_seconds'))

client_latency = compute_latency_stats(
    parse_histogram(before_file, 'h2agent_traffic_client_observed_responses_delay_seconds'),
    parse_histogram(after_file, 'h2agent_traffic_client_observed_responses_delay_seconds'))

# --- Output ---
if json_output:
    result = {
        'counters': {
            'sent': int(sent), 'unsent': int(unsent), 'timedout': int(timedout),
            'status_classes': classes, 'status_codes': {k: int(v) for k, v in delta.items()},
        },
        'latency': {}
    }
    if server_latency:
        result['latency']['server'] = server_latency
    if client_latency:
        result['latency']['client'] = client_latency
    print(json.dumps(result, indent=2))
else:
    # Text output (backward compatible)
    if int(sent) or int(unsent):
        parts = ', '.join(f'{v} {k}' for k, v in classes.items())
        print(f'requests: {int(sent)} sent, {int(unsent)} unsent')
        print(f'status codes: {parts}')
        if int(timedout) > 0:
            print(f'timedout: {int(timedout)}')

    def print_latency(title, latency):
        if not latency:
            return
        print(f'\n{title}:')
        print(f'  {"label":<30s} {"count":>8s} {"avg":>10s} {"p50":>10s} {"p90":>10s} {"p99":>10s}')
        for label, stats in sorted(latency.items()):
            print(f'  {label:<30s} {stats["count"]:>8d} {stats["avg_ms"]:>9.3f}ms {stats["p50_ms"]:>9.3f}ms {stats["p90_ms"]:>9.3f}ms {stats["p99_ms"]:>9.3f}ms')

    print_latency('server processing latency', server_latency)
    print_latency('client observed latency', client_latency)
