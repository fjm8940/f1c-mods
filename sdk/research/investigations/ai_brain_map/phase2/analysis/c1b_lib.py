# c1b_lib.py — read-only helpers for the phase-2 C1b watch-folder analysis.
# Reads %TEMP%\ai_brain_live\watch_race2 and watch_race2b (disposable logs).
# No game access, no writes.
import json
import math
import os
from pathlib import Path

TEMP = Path(os.environ['TEMP']) / 'ai_brain_live'
RUNS = ('watch_race2', 'watch_race2b')
SNAP_FIELDS_HEX = ('current_waypoint_ptr', 'limiting_waypoint_ptr')


def run_dir(run, temp=TEMP):
    return Path(temp) / run


def situations(run):
    p = run_dir(run) / 'situations' / 'index.jsonl'
    for line in p.read_text(encoding='utf-8').splitlines():
        if line.strip():
            yield json.loads(line)


def snaps(run, folder):
    p = run_dir(run) / 'situations' / folder / 'snapshots.jsonl'
    with open(p, encoding='utf-8') as fh:
        for line in fh:
            if line.strip():
                yield json.loads(line)


def overview(run):
    with open(run_dir(run) / 'overview.jsonl', encoding='utf-8') as fh:
        for line in fh:
            if line.strip():
                yield json.loads(line)


def events(run):
    with open(run_dir(run) / 'events.jsonl', encoding='utf-8') as fh:
        for line in fh:
            if line.strip():
                yield json.loads(line)


def hx(v):
    if isinstance(v, str):
        return int(v, 16)
    return int(v)


def chain(s):
    f = s['fields']
    return [hx(f['current_waypoint_ptr'])] + [hx(p) for p in f['lookahead_waypoints']]


def wp_dist(s, i, j):
    w = s['waypoints']
    if i < 0 or j < 0 or i >= len(w) or j >= len(w):
        return None
    a, b = w[i], w[j]
    return math.hypot(a['x'] - b['x'], a['z'] - b['z'])


def snap_speed(s):
    return s['fields']['speed']


def active(s):
    b = s.get('derived', {}).get('braking', {})
    return bool(b.get('active'))


def stats(xs):
    xs = [x for x in xs if x is not None]
    if not xs:
        return None
    xs = sorted(xs)
    n = len(xs)
    return dict(n=n, min=xs[0], median=xs[n // 2], p90=xs[min(n - 1, int(0.9 * n))], max=xs[-1],
                mean=sum(xs) / n)


def corr(xs, ys):
    pairs = [(x, y) for x, y in zip(xs, ys) if x is not None and y is not None]
    n = len(pairs)
    if n < 3:
        return None
    mx = sum(x for x, _ in pairs) / n
    my = sum(y for _, y in pairs) / n
    sxy = sum((x - mx) * (y - my) for x, y in pairs)
    sxx = sum((x - mx) ** 2 for x, _ in pairs)
    syy = sum((y - my) ** 2 for _, y in pairs)
    if sxx == 0 or syy == 0:
        return None
    return sxy / math.sqrt(sxx * syy)
