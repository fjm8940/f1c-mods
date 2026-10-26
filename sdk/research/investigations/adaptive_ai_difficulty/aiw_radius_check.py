# H17 sanity check: approximate corner radii from AIW wp_pos (read-only on game files).
# Radius = circumradius of waypoint triples (i-k, i, i+k) in the x/z plane (y is height).
# Usage: python -I aiw_radius_check.py <path-to-aiw>
import math
import re
import sys

PATH = sys.argv[1]
RX_POS = re.compile(r"^wp_pos=\(([^,]+),([^,]+),([^)]+)\)")
RX_SCORE = re.compile(r"^wp_score=\(([^,]+),([^)]+)\)")
RX_BRANCH = re.compile(r"^wp_branchID=\((\d+)\)")
RX_LEN = re.compile(r"^lap_length=([\d.]+)")

wps = []
lap_length = None
with open(PATH, encoding="utf-8", errors="replace") as fh:
    for raw in fh:
        line = raw.split("//")[0].strip()
        if lap_length is None:
            m = RX_LEN.match(line)
            if m:
                lap_length = float(m.group(1))
        m = RX_POS.match(line)
        if m:
            wps.append({"x": float(m.group(1)), "z": float(m.group(3)),
                        "dist": None, "sector": None, "branch": 0})
            continue
        if not wps:
            continue
        m = RX_SCORE.match(line)
        if m:
            wps[-1]["sector"] = int(float(m.group(1)))
            wps[-1]["dist"] = float(m.group(2))
            continue
        m = RX_BRANCH.match(line)
        if m:
            wps[-1]["branch"] = int(m.group(1))

main = [w for w in wps if w["branch"] == 0]
n = len(main)
print(f"waypoints total={len(wps)} main_branch={n} lap_length_header={lap_length}")

# Cumulative arc length along the main path (xz), for cross-checking wp_score[1].
arc = [0.0]
for i in range(1, n):
    a, b = main[i - 1], main[i]
    arc.append(arc[-1] + math.hypot(b["x"] - a["x"], b["z"] - a["z"]))
closing = math.hypot(main[0]["x"] - main[-1]["x"], main[0]["z"] - main[-1]["z"])
print(f"arc_length_xz={arc[-1]:.1f} m + closing={closing:.1f} m -> loop={arc[-1] + closing:.1f} m")


def radius(p1, p2, p3):
    ax, az = p1["x"], p1["z"]
    bx, bz = p2["x"], p2["z"]
    cx, cz = p3["x"], p3["z"]
    a = math.hypot(bx - cx, bz - cz)
    b = math.hypot(ax - cx, az - cz)
    c = math.hypot(ax - bx, az - bz)
    area2 = abs((bx - ax) * (cz - az) - (cx - ax) * (bz - az))  # twice the triangle area
    if area2 < 1e-9:
        return float("inf")
    return a * b * c / (2.0 * area2)  # R = abc / (4K), K = area2/2


def local_radius(i, k):
    # Wrap around the lap so the start/finish line is handled.
    p1 = main[(i - k) % n]
    p2 = main[i]
    p3 = main[(i + k) % n]
    return radius(p1, p2, p3)


# Use the k=3 baseline (about 12 m) as the main estimate; k=6 as a cross-check.
R3 = [local_radius(i, 3) for i in range(n)]
R6 = [local_radius(i, 6) for i in range(n)]

THRESH = 120.0  # report only corners tighter than this
cands = [i for i in range(n) if R3[i] < THRESH]
# Group contiguous indices into corners, keep the minimum-radius point of each group.
groups = []
for i in cands:
    if groups and i - groups[-1][-1] <= 3:
        groups[-1].append(i)
    else:
        groups.append([i])
corners = []
for g in groups:
    best = min(g, key=lambda j: R3[j])
    corners.append((R3[best], R6[best], best, arc[best], main[best]["dist"], main[best]["sector"]))
corners.sort()

print(f"corners with R(k=3) < {THRESH:.0f} m: {len(corners)}")
print("rank  R_k3(m)  R_k6(m)  wp_index  arc_m    wp_score_dist_m  sector")
for rank, (r3, r6, idx, arcm, dist, sec) in enumerate(corners[:8], 1):
    r6s = f"{r6:7.1f}" if math.isfinite(r6) else "    inf"
    print(f"{rank:>4}  {r3:7.1f}  {r6s}  {idx:>8}  {arcm:7.1f}  {dist if dist is not None else float('nan'):>14.1f}  {sec}")

# Also report the global median radius of the main path for scale.
finite = sorted(r for r in R3 if math.isfinite(r))
if finite:
    print(f"R_k3 median={finite[len(finite)//2]:.1f} m  p05={finite[int(0.05*len(finite))]:.1f} m  min={finite[0]:.1f} m")
