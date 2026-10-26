"""match_values.py — name vehicle-struct offsets by matching live memory against
the drivers' own shipped .rcd files.

WHY: the persistence sweep answered "can I poke this offset?" for all 4,656
dwords, but not "what IS it?". Only ~58 vehicle fields are named. Naming the rest
by reading decompiled code is slow and lands at DERIVED at best.

There is a deterministic shortcut. A race has N live cars, each one a KNOWN
driver, and every one of those drivers has a shipped .rcd file stating his
RaceAbility, Passing, Crash, CorneringMult and so on. So for any offset, the
values across the N slots form a vector, and for any .rcd key the drivers' file
values form another vector in the same driver order. If those two vectors agree
across 20-odd cars, that offset IS that field. Twenty simultaneous agreements is
not a coincidence — it is identification, produced with no LLM in the loop,
which is what the trust model asks for.

Matching is AFFINE, not equality: the game usually stores a transform of the file
value (veh+0x44F4 = (1 - RaceAbility*0.01) * flt_5EA3D8). Fitting V = a*K + b
recovers the transform itself — and the slopes come back as round numbers
(1/22, 0.001, 0.0096), which is corroboration in its own right, since a spurious
fit produces ugly ones.

The fit is ROBUST (RANSAC-style), not least-squares, and outliers are reported
rather than tolerated. Both choices were forced by the data:
  * one car whose live value has diverged drags OLS r^2 to nothing and hides a
    relation the other twenty obey exactly (0x44CC vs Passing: 20/21, OLS 0.36).
  * an outlier is usually a SECOND WRITER. The 0x44CC outlier turned out to be
    AIDriverStructInit overwriting the parsed value, which `ask.py offset 0x44CC`
    confirms independently. That is a finding, not noise.

Three guards keep false positives near zero (without them: 6,818 "matches"):
  * tolerance tracks float32 PRECISION, not the value spread — a near-constant
    offset has a tiny spread, so a spread-relative tolerance matches everything;
  * the ability keys correlate strongly with each other, so a real field matches
    half the schema. Only the best-supported, tightest key per offset is kept
    (the rest are listed in `also_matched`);
  * the relation must explain a span well clear of the tolerance, else a slope
    of ~0 fits any key at all.

    python tools/match_values.py                  # scan, print ranked matches
    python tools/match_values.py --out m.json     # also write the full report
    python tools/match_values.py --season 2001    # override season auto-detect

Read-only: this never writes to the game.

Results are NOT auto-promoted. A match plus an independent static writer (see
`ask.py offset <hex>`) is two sources, i.e. CONFIRMED; a match alone is DERIVED.
The guard against coincidence is `distinct` — how many different values the key
actually takes across the live grid. A key constant across the grid (every 2001
driver shares CorneringMult=0.965) discriminates nothing and is dropped, so this
method can never identify one.
"""
import argparse
import io
import json
import os
import re
import struct
import sys
import collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from poke import Game  # noqa: E402  (shared plumbing)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_DIR = os.path.join(ROOT, "..", "F1 Challenge V10")

# FACT (CLAUDE.md): vehicle array base / stride / liveness marker.
VEH_BASE = 0x5EC6D8
VEH_STRIDE = 0x48C0
LIVE_MARKER = 0x32C8          # int32 != -1  <=> slot is live
NAME_OFF = 0x4620             # ASCII driver name, read live (see standings.py)
MAX_SLOTS = 104

KV_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_ %]*?)\s*=\s*(.*)$")
NUM_RE = re.compile(r"^-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?$")


def strip_comment(s):
    """ISI files use // comments, including trailing ones after a value."""
    i = s.find("//")
    return (s[:i] if i >= 0 else s).strip()


def norm(s):
    """Loose name key: lowercase, letters only. 'M. Hakkinen' -> 'mhakkinen'."""
    return re.sub(r"[^a-z]", "", s.lower())


# ---------------------------------------------------------------------------
# the shipped .rcd files
# ---------------------------------------------------------------------------
def parse_rcd(path):
    """-> (aliases, {key: float}). Numeric keys only; strings discriminate
    nothing numerically and would only add false matches."""
    vals, aliases, header = {}, [], None
    try:
        with io.open(path, encoding="latin-1", errors="replace") as f:
            for line in f:
                line = strip_comment(line)
                if not line:
                    continue
                if header is None and "=" not in line and line not in ("{", "}"):
                    header = line
                    continue
                m = KV_RE.match(line)
                if not m:
                    continue
                key, raw = m.group(1).strip(), m.group(2).strip()
                if NUM_RE.match(raw):
                    vals[key] = float(raw)
                elif key.lower() == "abbreviation":
                    aliases.append(raw)
    except (IOError, OSError):
        return [], {}

    stem = os.path.splitext(os.path.basename(path))[0]
    aliases.append(stem)
    if header:
        aliases.append(header)
        # "Mika Hakkinen (2001)" -> surname, the form the game shows live
        bare = re.sub(r"\(.*?\)", "", header).strip().split()
        if bare:
            aliases.append(bare[-1])
    # "MHakkinen" -> "Hakkinen": drop a single leading initial
    if len(stem) > 2 and stem[0].isupper() and stem[1].isupper():
        aliases.append(stem[1:])
    return aliases, vals


def load_season(season_dir):
    """-> {normalized alias: (driver_label, {key: float})}"""
    table = {}
    for fn in sorted(os.listdir(season_dir)):
        if not fn.lower().endswith(".rcd"):
            continue
        path = os.path.join(season_dir, fn)
        aliases, vals = parse_rcd(path)
        if not vals:
            continue
        label = os.path.splitext(fn)[0]
        for a in aliases:
            k = norm(a)
            if k and k not in table:
                table[k] = (label, vals)
    return table


def pick_season(live_names, game_dir, forced=None):
    """The season folder whose drivers best cover the live grid. Auto-detecting
    beats asking: a wrong season silently produces zero matches."""
    root = os.path.join(game_dir, "SeasonData", "Drivers")
    if forced:
        d = os.path.join(root, forced)
        return forced, load_season(d)
    best, best_tab, best_hit = None, {}, -1
    for name in sorted(os.listdir(root)):
        d = os.path.join(root, name)
        if not os.path.isdir(d):
            continue
        tab = load_season(d)
        hit = sum(1 for n in live_names if norm(n) in tab)
        if hit > best_hit:
            best, best_tab, best_hit = name, tab, hit
    return best, best_tab


# ---------------------------------------------------------------------------
# live memory
# ---------------------------------------------------------------------------
def read_grid(game):
    """-> [(slot, name, struct_bytes)] for every live slot."""
    out = []
    for slot in range(MAX_SLOTS):
        base = VEH_BASE + slot * VEH_STRIDE
        marker = game.read(base + LIVE_MARKER, 4)
        if marker is None or struct.unpack("<i", marker)[0] == -1:
            continue
        blob = game.read(base, VEH_STRIDE)
        if blob is None or len(blob) < VEH_STRIDE:
            continue
        raw = blob[NAME_OFF:NAME_OFF + 40]
        name = raw.split(b"\0")[0].decode("ascii", errors="replace").strip()
        out.append((slot, name, blob))
    return out


# ---------------------------------------------------------------------------
# the fit
# ---------------------------------------------------------------------------
def usable_float(v):
    if v != v or v in (float("inf"), float("-inf")):   # NaN / inf
        return False
    a = abs(v)
    return a == 0.0 or (1e-30 < a < 1e30)


def affine_fit(xs, ys):
    """V = a*K + b, ordinary least squares.
    -> (r2, a, b) or None if either side is degenerate."""
    n = len(xs)
    mx = sum(xs) / n
    my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx <= 0 or syy <= 0:
        return None
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    r2 = (sxy * sxy) / (sxx * syy)
    a = sxy / sxx
    return r2, a, my - a * mx


def robust_fit(xs, ys, tol, min_inliers):
    """RANSAC-style affine fit: the largest subset of cars that share one
    transform wins.

    WHY not least squares: a single car whose live value has diverged from its
    file value drags r^2 to nothing and hides a relation that 20 other cars obey
    exactly (real case: veh+0x44CC = 0.9 + Passing*0.001 holds for 20/21 cars,
    OLS r^2 = 0.36). The outliers are not noise to be tolerated, they are the
    interesting part — a car off the line is one the game has since overwritten.

    Candidate transforms are taken from well-separated point pairs, which is
    both numerically stable and enough: with mostly-inlier data a handful of
    wide pairs is overwhelmingly likely to hit a clean one.

    -> (a, b, inlier_idx) or None
    """
    n = len(xs)
    order = sorted(range(n), key=lambda i: xs[i])
    if max(ys) - min(ys) <= 0:
        return None
    # Tolerance tracks float32 PRECISION, not the value spread. Spread-based
    # tolerance is self-defeating: a near-constant offset has a tiny spread, so
    # everything lands inside it and the offset "matches" every key.
    eps = tol * max(1.0, max(abs(y) for y in ys))

    best = None
    for step in range(min(8, n // 2)):
        i, j = order[step], order[n - 1 - step]
        if xs[j] == xs[i]:
            continue
        a = (ys[j] - ys[i]) / (xs[j] - xs[i])
        b = ys[i] - a * xs[i]
        inl = [t for t in range(n) if abs(ys[t] - (a * xs[t] + b)) <= eps]
        if len(inl) >= min_inliers and (best is None or len(inl) > len(best[2])):
            best = (a, b, inl)
    if best is None:
        return None

    # Refit on the inliers alone so the reported transform is not hostage to
    # the two seed points.
    a, b, inl = best
    if len(inl) >= 2:
        fit = affine_fit([xs[t] for t in inl], [ys[t] for t in inl])
        if fit:
            _r2, a2, b2 = fit
            re_inl = [t for t in range(n)
                      if abs(ys[t] - (a2 * xs[t] + b2)) <= eps]
            if len(re_inl) >= len(inl):
                a, b, inl = a2, b2, re_inl

    # The relation must actually EXPLAIN the values, not merely sit inside the
    # noise: the span it predicts across the inliers has to clear the tolerance
    # by a wide margin. Without this a slope of ~0 fits any key at all.
    ks = [xs[t] for t in inl]
    span = abs(a) * (max(ks) - min(ks))
    if span < 10 * eps:
        return None
    resid = max(abs(ys[t] - (a * xs[t] + b)) for t in inl)
    return a, b, inl, resid / span


def scan(grid, drivers, min_distinct, tol, min_frac):
    """Every offset x every numeric .rcd key. Reports affine agreements."""
    # Align: only slots whose driver was found in the season table.
    slots, keysets = [], []
    for slot, name, blob in grid:
        hit = drivers.get(norm(name))
        if hit:
            slots.append((slot, name, blob, hit[0]))
            keysets.append(set(hit[1]))
    if len(slots) < 4:
        return [], slots
    common = set.intersection(*keysets)

    # Key vectors, in slot order. Drop keys that discriminate nothing.
    kvecs = {}
    for key in sorted(common):
        vec = [drivers[norm(n)][1][key] for _s, n, _b, _l in slots]
        if len(set(vec)) >= min_distinct:
            kvecs[key] = vec
    if not kvecs:
        return [], slots

    n = len(slots)
    min_inliers = max(6, int(round(min_frac * n)))
    labels = [l for _s, _n, _b, l in slots]

    results = []
    for off in range(0, VEH_STRIDE, 4):
        words = [b[off:off + 4] for _s, _n, b, _l in slots]
        for kind, fmt in (("f32", "<f"), ("i32", "<i")):
            vals = [struct.unpack(fmt, w)[0] for w in words]
            if kind == "f32" and not all(usable_float(v) for v in vals):
                continue
            # Reading float bits as int32 yields huge look-alike integers that
            # fit anything monotone. Only judge i32 where the dword is a
            # plausible integer quantity.
            if kind == "i32" and not all(abs(v) < 10 ** 7 for v in vals):
                continue
            if len(set(vals)) < 3:          # constant-ish offset: degenerate
                continue

            # The ability keys are strongly correlated with each other (a quick
            # driver is quick in the rain too), so a real field matches half the
            # schema. Keep only the best-supported, tightest key per offset.
            cands = []
            for key, kvec in kvecs.items():
                fit = robust_fit(kvec, vals, tol, min_inliers)
                if not fit:
                    continue
                a, b, inl, tightness = fit
                # An inlier set must span several distinct values on BOTH sides,
                # else the "match" is a handful of coincidentally equal cars.
                if len(set(kvec[t] for t in inl)) < min_distinct:
                    continue
                if len(set(vals[t] for t in inl)) < min_distinct:
                    continue
                cands.append((len(inl), -tightness, key, a, b, inl, tightness))
            if not cands:
                continue
            cands.sort(reverse=True)
            _ni, _nt, key, a, b, inl, tightness = cands[0]
            out = [t for t in range(n) if t not in set(inl)]
            results.append({
                "offset": "0x%04X" % off,
                "as": kind,
                "key": key,
                "slope": a,
                "intercept": b,
                "identity": abs(a - 1.0) < 1e-6 and abs(b) < 1e-6,
                "n_slots": n,
                "inliers": len(inl),
                "distinct": len(set(kvecs[key][t] for t in inl)),
                "tightness": tightness,
                "also_matched": [c[2] for c in cands[1:]],
                "outliers": [{"driver": labels[t], "key_value": kvecs[key][t],
                              "mem_value": vals[t]} for t in out],
            })
    results.sort(key=lambda r: (-r["inliers"], r["tightness"], r["offset"]))
    return results, slots


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--season", help="season folder name (default: auto-detect)")
    ap.add_argument("--game-dir", default=GAME_DIR)
    ap.add_argument("--out", help="write the full report as JSON")
    ap.add_argument("--min-distinct", type=int, default=4,
                    help="a key must take this many different values on the "
                         "grid, else a match proves nothing (default 4)")
    ap.add_argument("--tol", type=float, default=1e-4,
                    help="inlier tolerance, relative to the offset's value "
                         "spread (default 1e-4)")
    ap.add_argument("--min-frac", type=float, default=0.6,
                    help="fraction of cars that must obey one transform "
                         "(default 0.6)")
    ap.add_argument("--top", type=int, default=60)
    args = ap.parse_args()

    game = Game()
    grid = read_grid(game)
    if not grid:
        sys.exit("no live slots — is a session actually running?")
    print("live slots: %d" % len(grid))

    season, drivers = pick_season([n for _s, n, _b in grid], args.game_dir,
                                  args.season)
    print("season: %s (%d driver aliases)" % (season, len(drivers)))

    results, slots = scan(grid, drivers, args.min_distinct, args.tol,
                          args.min_frac)
    print("matched drivers: %d/%d" % (len(slots), len(grid)))
    for slot, name, _b, label in slots:
        print("   [%2d] %-18s -> %s" % (slot, name, label))
    if len(slots) < 4:
        sys.exit("\ntoo few identified drivers to fit anything")

    print("\n%d candidate match(es)  (tol %g, key distinct >= %d)\n"
          % (len(results), args.tol, args.min_distinct))
    print("  offset   as   key                    fit      slope       "
          "intercept   distinct  outliers")
    for r in results[:args.top]:
        out = ",".join(o["driver"] for o in r["outliers"][:3]) or "-"
        print("  %-8s %-4s %-22s %2d/%-4d %-11.5g %-11.5g %-9d %s%s"
              % (r["offset"], r["as"], r["key"], r["inliers"], r["n_slots"],
                 r["slope"], r["intercept"], r["distinct"], out,
                 "  IDENTITY" if r["identity"] else ""))
    if len(results) > args.top:
        print("  ... %d more (use --out to get them all)"
              % (len(results) - args.top))

    if args.out:
        with io.open(args.out, "w", encoding="utf-8") as f:
            json.dump({
                "_meta": {
                    "tier": "OBSERVED",
                    "season": season,
                    "n_slots": len(slots),
                    "tol": args.tol, "min_frac": args.min_frac,
                    "min_distinct": args.min_distinct,
                    "note": "affine match V = slope*key + intercept between a "
                            "vehicle-struct offset across live slots and the "
                            "drivers' own .rcd values. Candidates for "
                            "name_index.json, NOT auto-promoted.",
                },
                "grid": [{"slot": s, "live_name": n, "rcd": l}
                         for s, n, _b, l in slots],
                "matches": results,
            }, f, indent=1)
        print("\nwrote %s" % args.out)


if __name__ == "__main__":
    main()
