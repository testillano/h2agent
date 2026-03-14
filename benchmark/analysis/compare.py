#!/usr/bin/env python3
"""Compare two benchmark reports (baseline vs candidate) and produce a markdown diff.

Usage: compare.py <baseline.md> <candidate.md> [--threshold-warn N] [--threshold-crit N]

Thresholds are percentage deltas (default: warn=5, crit=10).
"""
import re, sys, argparse

WARN = 5.0
CRIT = 10.0

def parse_time(s):
    """Parse time string like '13.84ms', '356us', '0.893ms' to microseconds."""
    s = s.strip()
    if s.endswith('ms'):
        return float(s[:-2]) * 1000
    elif s.endswith('us'):
        return float(s[:-2])
    elif s.endswith('s'):
        return float(s[:-1]) * 1_000_000
    try:
        return float(s)
    except ValueError:
        return None

def fmt_time(us):
    """Format microseconds back to human-readable."""
    if us >= 1_000_000:
        return f'{us/1_000_000:.3f}s'
    if us >= 1000:
        return f'{us/1000:.2f}ms'
    return f'{us:.0f}us'

def parse_number(s):
    """Parse a number, stripping units."""
    s = s.strip().rstrip('%')
    try:
        return float(s)
    except ValueError:
        return None

def parse_tables(path):
    """Parse markdown report into structured data."""
    data = {'profile': {}, 'resources': {}, 'h2load': {}, 'prom_latency': [], 'counters': {}}
    with open(path) as f:
        lines = f.readlines()

    section = None
    headers = []
    for line in lines:
        line = line.rstrip()
        if line.startswith('## '):
            section = line[3:].strip()
            headers = []
            continue
        if '|' not in line or line.replace('|', '').replace('-', '').strip() == '':
            if '|' in line and '---' not in line and headers == []:
                headers = [h.strip() for h in line.split('|')[1:-1]]
            continue
        if not headers:
            cells = [c.strip() for c in line.split('|')[1:-1]]
            if len(cells) >= 2 and cells[0] and '---' not in cells[0]:
                headers = cells
            continue

        cells = [c.strip() for c in line.split('|')[1:-1]]
        if len(cells) < 2 or '---' in cells[0]:
            continue

        if section == 'Test Profile':
            data['profile'][cells[0]] = cells[1]
        elif section and 'Resource' in section:
            metric = cells[0]
            if len(cells) >= 4:
                data['resources'][metric] = {
                    'min': parse_number(cells[1]),
                    'avg': parse_number(cells[2]),
                    'max': parse_number(cells[3]),
                }
        elif section == 'h2load Latency':
            metric = cells[0]
            if len(cells) >= 5:
                is_time = 'time' in metric.lower()
                parse = parse_time if is_time else parse_number
                data['h2load'][metric] = {
                    'min': parse(cells[1]),
                    'max': parse(cells[2]),
                    'mean': parse(cells[3]),
                    'sd': parse(cells[4]),
                }
        elif section == 'Prometheus Latency':
            if len(cells) >= 7:
                entry = {
                    'side': cells[0], 'label': cells[1],
                    'count': parse_number(cells[2]),
                    'avg': parse_time(cells[3]),
                    'p50': parse_time(cells[4]),
                    'p90': parse_time(cells[5]),
                    'p99': parse_time(cells[6]),
                }
                data['prom_latency'].append(entry)
        elif section == 'Prometheus Counters':
            if len(cells) >= 2:
                data['counters'][cells[0]] = parse_number(cells[1])

    return data

def delta_pct(baseline, candidate):
    """Percentage change from baseline to candidate."""
    if baseline is None or candidate is None or baseline == 0:
        return None
    return ((candidate - baseline) / abs(baseline)) * 100

def indicator(pct, higher_is_worse=True):
    """Return emoji based on delta percentage."""
    if pct is None:
        return '➖'
    abspct = abs(pct)
    if higher_is_worse:
        bad = pct > 0
    else:
        bad = pct < 0
    if abspct < WARN:
        return '🟢'
    if abspct < CRIT:
        if bad:
            return '🟡'
        return '🟢'
    if bad:
        return '🔴'
    return '🟢'

def fmt_delta(pct):
    if pct is None:
        return '—'
    sign = '+' if pct > 0 else ''
    return f'{sign}{pct:.1f}%'

def compare(baseline, candidate):
    out = []
    out.append('# Benchmark Comparison')
    out.append('')
    b_profile = baseline['profile'].get('Profile', '?')
    c_profile = candidate['profile'].get('Profile', '?')
    out.append(f'Baseline: **{b_profile}** vs Candidate: **{c_profile}**')
    out.append('')
    out.append(f'Thresholds: 🟢 <{WARN:.0f}% | 🟡 {WARN:.0f}–{CRIT:.0f}% | 🔴 >{CRIT:.0f}%')
    out.append('')

    # Resource Usage
    if baseline['resources'] and candidate['resources']:
        out.append('## Resource Usage')
        out.append('')
        out.append('| Metric | Stat | Baseline | Candidate | Delta | |')
        out.append('|---|---|---|---|---|---|')
        for metric in baseline['resources']:
            if metric not in candidate['resources']:
                continue
            for stat in ('min', 'avg', 'max'):
                bv = baseline['resources'][metric].get(stat)
                cv = candidate['resources'][metric].get(stat)
                pct = delta_pct(bv, cv)
                unit = '%' if 'CPU' in metric else ' KB'
                bstr = f'{bv:.1f}{unit}' if bv is not None else '—'
                cstr = f'{cv:.1f}{unit}' if cv is not None else '—'
                ind = indicator(pct, higher_is_worse=True)
                out.append(f'| {metric} | {stat} | {bstr} | {cstr} | {fmt_delta(pct)} | {ind} |')
        out.append('')

    # h2load Latency
    if baseline['h2load'] and candidate['h2load']:
        out.append('## h2load Latency')
        out.append('')
        out.append('| Metric | Stat | Baseline | Candidate | Delta | |')
        out.append('|---|---|---|---|---|---|')
        for metric in baseline['h2load']:
            if metric not in candidate['h2load']:
                continue
            is_rps = 'req/s' in metric
            for stat in ('min', 'max', 'mean'):
                bv = baseline['h2load'][metric].get(stat)
                cv = candidate['h2load'][metric].get(stat)
                pct = delta_pct(bv, cv)
                if is_rps:
                    bstr = f'{bv:.1f}' if bv is not None else '—'
                    cstr = f'{cv:.1f}' if cv is not None else '—'
                    ind = indicator(pct, higher_is_worse=False)
                else:
                    bstr = fmt_time(bv) if bv is not None else '—'
                    cstr = fmt_time(cv) if cv is not None else '—'
                    ind = indicator(pct, higher_is_worse=True)
                out.append(f'| {metric} | {stat} | {bstr} | {cstr} | {fmt_delta(pct)} | {ind} |')
        out.append('')

    # Prometheus Latency
    if baseline['prom_latency'] and candidate['prom_latency']:
        out.append('## Prometheus Latency')
        out.append('')
        out.append('| Side | Label | Metric | Baseline | Candidate | Delta | |')
        out.append('|---|---|---|---|---|---|---|')
        # Match by side+label
        b_map = {(e['side'], e['label']): e for e in baseline['prom_latency']}
        c_map = {(e['side'], e['label']): e for e in candidate['prom_latency']}
        for key in b_map:
            if key not in c_map:
                continue
            be, ce = b_map[key], c_map[key]
            for stat in ('avg', 'p50', 'p90', 'p99'):
                bv, cv = be.get(stat), ce.get(stat)
                pct = delta_pct(bv, cv)
                bstr = fmt_time(bv) if bv is not None else '—'
                cstr = fmt_time(cv) if cv is not None else '—'
                ind = indicator(pct, higher_is_worse=True)
                out.append(f'| {key[0]} | {key[1]} | {stat} | {bstr} | {cstr} | {fmt_delta(pct)} | {ind} |')
        out.append('')

    # Counters
    if baseline['counters'] and candidate['counters']:
        out.append('## Prometheus Counters')
        out.append('')
        out.append('| Metric | Baseline | Candidate | Delta | |')
        out.append('|---|---|---|---|---|')
        for metric in baseline['counters']:
            if metric not in candidate['counters']:
                continue
            bv, cv = baseline['counters'][metric], candidate['counters'][metric]
            pct = delta_pct(bv, cv)
            ind = '➖'
            out.append(f'| {metric} | {int(bv)} | {int(cv)} | {fmt_delta(pct)} | {ind} |')
        out.append('')

    return '\n'.join(out)

def main():
    parser = argparse.ArgumentParser(description='Compare two benchmark reports')
    parser.add_argument('baseline', help='Baseline report (markdown)')
    parser.add_argument('candidate', help='Candidate report (markdown)')
    parser.add_argument('--threshold-warn', type=float, default=5.0, help='Warning threshold %% (default: 5)')
    parser.add_argument('--threshold-crit', type=float, default=10.0, help='Critical threshold %% (default: 10)')
    parser.add_argument('-o', '--output', help='Output file (default: stdout)')
    args = parser.parse_args()

    global WARN, CRIT
    WARN, CRIT = args.threshold_warn, args.threshold_crit

    baseline = parse_tables(args.baseline)
    candidate = parse_tables(args.candidate)
    result = compare(baseline, candidate)

    if args.output:
        with open(args.output, 'w') as f:
            f.write(result + '\n')
        print(f'Comparison written to {args.output}')
    else:
        print(result)

if __name__ == '__main__':
    main()
