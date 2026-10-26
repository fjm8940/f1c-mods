/* telemetry_viewer.js — F1C Telemetry Viewer (Telemetry 1.0.1, engine 4.4.0).
 *
 * Canonical sidecar beside web_telemetry.dll, served read-only at /viewer.js
 * and mounted by the /viewer page. It needs workbench_ui.js (Stop4Workbench)
 * loaded first: the recorder, lap model, imports and the DM#14 distance->time
 * interpolation live there and are reused unchanged, so the legacy workbench
 * and this viewer can never disagree about a lap.
 *
 * Read-only. Consumes /events (SSE /telemetry frames), /track, /health and
 * the legacy import endpoint; never writes game state.
 *
 * Evidence rules (CHANNEL_LEDGER_2026-10-09.md, VIEWER_4.3.0_DESIGN):
 *  - every channel carries its tier (FACT / CONFIRMED / DERIVED / RESEARCH);
 *  - missing data stays missing: no line is drawn across a gap and no zero is
 *    invented; unavailable channels say why;
 *  - RESEARCH channels (suspension, load, slip, force candidates and the AI
 *    tyre-temperature candidate) are hidden until the user enables research
 *    mode, are always drawn as unitless raw values, and are never summarized
 *    as physics.
 *  - Longitudinal acceleration is native forward-NEGATIVE; this viewer shows
 *    "long G" with acceleration positive and braking negative (-glon / g).
 */
(function (root, factory) {
    if (typeof module === 'object' && module.exports) module.exports = factory(require('./workbench_ui.js'));
    else root.F1CViewer = factory(root.Stop4Workbench);
})(typeof self !== 'undefined' ? self : this, function (WB) {
    'use strict';

    var VERSION = '4.4.0';
    /* Public product name and version (Lightloader catalog). VERSION above is the
     * engine/API build and stays the diagnostic identity in /health. */
    var PRODUCT = 'Telemetry', PRODUCT_VERSION = '1.2.5';
    var G = 9.81;
    var WHEELS = ['FL', 'FR', 'RL', 'RR'];
    var COLORS = WB.COLORS;   /* one palette for every side (workbench_ui.js) */
    var TIER = {
        FACT: 'FACT', CONFIRMED: 'CONFIRMED', DERIVED: 'DERIVED', RESEARCH: 'RESEARCH'
    };

    /* ============================================================ analysis */
    /* Pure functions over recorder samples ({t, dist, speed, throttle, ...}).
     * Exported for the offline tests. */

    function finite(v) { return typeof v === 'number' && isFinite(v); }

    /* Per-sample derived channels. Yaw rate is DERIVED from consecutive heading
     * samples (never across a segment break or a > 1 s gap). */
    function derive(points) {
        var out = [];
        var t0 = null;
        for (var i = 0; i < points.length; i++) {
            var p = points[i];
            if (t0 === null && finite(p.t)) t0 = p.t;
            var q = i > 0 ? points[i - 1] : null;
            var yr = null;
            if (q && !p.seg && finite(p.yaw) && finite(q.yaw) && finite(p.t) && finite(q.t)) {
                var dt = p.t - q.t;
                if (dt > 0.001 && dt <= 1.0) {
                    var d = p.yaw - q.yaw;
                    while (d > Math.PI) d -= 2 * Math.PI;
                    while (d < -Math.PI) d += 2 * Math.PI;
                    yr = d / dt * 180 / Math.PI;
                }
            }
            var lat = finite(p.glat) ? p.glat / G : null;
            var lon = finite(p.glon) ? -p.glon / G : null;
            out.push({
                src: p,
                x: finite(p.dist) ? p.dist : null,
                t: finite(p.t) ? p.t : null,
                el: finite(p.t) && t0 !== null ? p.t - t0 : null,
                seg: !!p.seg,
                kmh: finite(p.speed) ? p.speed * 3.6 : null,
                thr: finite(p.throttle) ? p.throttle * 100 : null,
                brk: finite(p.brake) ? p.brake * 100 : null,
                steer: finite(p.steer) ? p.steer : null,
                gear: finite(p.gear) ? p.gear : null,
                rpm: finite(p.rpm) ? p.rpm : null,
                lat: lat,
                lon: lon,
                vert: finite(p.gvert) ? p.gvert / G : null,
                comb: lat !== null && lon !== null ? Math.sqrt(lat * lat + lon * lon) : null,
                yawRate: yr,
                tread: p.tread || null,
                ttemp: p.ttemp || null,
                ttempResearch: !!p.ttempResearch,
                tcarc: p.tcarc || null,
                rx: p.rx || null,
                tx: finite(p.tx) ? p.tx : null,
                tz: finite(p.tz) ? p.tz : null
            });
        }
        return out;
    }

    /* Linear value at distance x on a derived series (no extrapolation, no
     * bridging of segment breaks). */
    function valueAt(series, x, get) {
        if (!series || !series.length || !finite(x)) return null;
        var lo = 0, hi = series.length - 1;
        while (lo <= hi && series[lo].x === null) lo++;
        while (hi >= lo && series[hi].x === null) hi--;
        if (lo > hi || x < series[lo].x || x > series[hi].x) return null;
        while (hi - lo > 1) {
            var mid = (lo + hi) >> 1;
            var m = mid;
            while (m < hi && series[m].x === null) m++;
            if (m >= hi) { hi = mid; continue; }
            if (series[m].x <= x) lo = m; else hi = m;
        }
        var a = series[lo], b = series[hi];
        var va = get(a, lo), vb = get(b, hi);
        if (a.x === x) return va;
        if (b.x === x) return vb;
        if (b.seg || va === null || vb === null || !(b.x > a.x)) return null;
        var f = (x - a.x) / (b.x - a.x);
        return va + (vb - va) * f;
    }

    function nearestIndex(series, x) {
        var best = -1, bd = Infinity;
        for (var i = 0; i < series.length; i++) {
            if (series[i].x === null) continue;
            var d = Math.abs(series[i].x - x);
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }

    /* Session time at distance using the shared DM#14 lookup. With lap
     * bounds ({start, end, len}: the recorder's interpolated line-crossing
     * times) the stretch between the line and the first/last sample is
     * interpolated against the crossing, so sector 1 and the last
     * mini-sector have a time. Never extrapolates beyond a crossing. */
    /* Nearest series sample to a screen point (track-map hover, B1.7). toScreen
     * maps (tx, tz) to canvas pixels. Returns the index, or -1 when no sample is
     * within maxPx. Pure, so the offline gate checks it. */
    function nearestOnScreen(series, toScreen, mx, my, maxPx) {
        var best = -1, bd = maxPx * maxPx;
        for (var i = 0; i < series.length; i++) {
            var q = series[i];
            if (q.tx === null || q.tz === null || q.tx === undefined || q.tz === undefined) continue;
            var v = toScreen(q.tx, q.tz);
            var dx = v[0] - mx, dy = v[1] - my, d = dx * dx + dy * dy;
            if (d <= bd) { bd = d; best = i; }
        }
        return best;
    }

    function timeAt(points, x, b) {
        var t = WB.timeAtDistance(points, x);
        if (t !== null || !b || !points.length) return t;
        var first = null, last = null, i;
        for (i = 0; i < points.length; i++) if (finite(points[i].dist) && finite(points[i].t)) { first = points[i]; break; }
        for (i = points.length - 1; i >= 0; i--) if (finite(points[i].dist) && finite(points[i].t)) { last = points[i]; break; }
        if (!first || !last) return null;
        if (finite(b.start) && x >= 0 && x < first.dist && first.dist < 60 && first.t >= b.start)
            return b.start + (first.t - b.start) * (first.dist > 0 ? x / first.dist : 1);
        if (finite(b.end) && b.len > 0 && x > last.dist && x <= b.len && b.len - last.dist < 60 && b.end >= last.t)
            return last.t + (b.end - last.t) * (x - last.dist) / (b.len - last.dist);
        return null;
    }

    /* Braking zones on one lap. A zone is a run of brake >= 10 % (or, when the
     * brake channel is missing, long G <= -0.6 g) longer than 12 m; zones
     * closer than 40 m merge. Returns [{start, end}] in metres. */
    function brakingZones(series) {
        var zones = [];
        var cur = null;
        for (var i = 0; i < series.length; i++) {
            var s = series[i];
            if (s.x === null) continue;
            var on = s.brk !== null ? s.brk >= 10 : (s.lon !== null && s.lon <= -0.6);
            if (s.seg && cur) { if (cur.end - cur.start >= 12) zones.push(cur); cur = null; }
            if (on) {
                if (!cur) cur = { start: s.x, end: s.x };
                else cur.end = s.x;
            } else if (cur) {
                if (cur.end - cur.start >= 12) zones.push(cur);
                cur = null;
            }
        }
        if (cur && cur.end - cur.start >= 12) zones.push(cur);
        var merged = [];
        zones.forEach(function (z) {
            var last = merged[merged.length - 1];
            if (last && z.start - last.end < 40) last.end = z.end;
            else merged.push({ start: z.start, end: z.end });
        });
        return merged;
    }

    /* Corner metrics for one side inside a window around a reference zone. */
    function cornerMetrics(series, points, zone, nextStart) {
        var w0 = zone.start - 150, w1 = Math.min(zone.end + 350, nextStart !== undefined ? nextStart - 20 : zone.end + 350);
        var bp = null, bEnd = null, minV = null, minX = null, peak = null, entry = null, pick = null;
        var i, s;
        for (i = 0; i < series.length; i++) {
            s = series[i];
            if (s.x === null || s.x < w0 || s.x > w1) continue;
            var on = s.brk !== null ? s.brk >= 10 : (s.lon !== null && s.lon <= -0.6);
            if (on && bp === null) { bp = s.x; entry = s.kmh; }
            if (on) bEnd = s.x;
            if (s.lon !== null && (peak === null || s.lon < peak)) peak = s.lon;
            if (s.kmh !== null && s.x >= (bp !== null ? bp : zone.start) && (minV === null || s.kmh < minV)) { minV = s.kmh; minX = s.x; }
        }
        if (minX !== null) {
            for (i = 0; i < series.length; i++) {
                s = series[i];
                if (s.x === null || s.x < minX || s.x > w1) continue;
                if (s.thr !== null && s.thr >= 50) { pick = s.x; break; }
            }
        }
        var exitX = minX !== null ? minX + 150 : null;
        var exitV = exitX !== null ? valueAt(series, exitX, function (q) { return q.kmh; }) : null;
        var ta = timeAt(points, w0), tb = timeAt(points, exitX !== null ? Math.min(exitX, w1) : w1);
        return {
            brakePoint: bp, brakeEnd: bEnd, entryKmh: entry, peakDecel: peak,
            minKmh: minV, apex: minX, throttleOn: pick, exitKmh: exitV,
            brakeLen: bp !== null && bEnd !== null ? bEnd - bp : null,
            time: ta !== null && tb !== null ? tb - ta : null,
            window: [w0, exitX !== null ? Math.min(exitX, w1) : w1]
        };
    }

    /* G-G envelope: maximum combined G inside each of `bins` angular sectors.
     * Returns [{ang, g}] (null g = no samples in that sector). */
    function ggEnvelope(series, bins) {
        var n = bins || 36, out = [];
        for (var k = 0; k < n; k++) out.push({ ang: (k + 0.5) / n * 2 * Math.PI, g: null });
        series.forEach(function (s) {
            if (s.lat === null || s.lon === null) return;
            var a = Math.atan2(s.lon, s.lat);
            if (a < 0) a += 2 * Math.PI;
            var k = Math.min(n - 1, Math.floor(a / (2 * Math.PI) * n));
            var g = Math.sqrt(s.lat * s.lat + s.lon * s.lon);
            if (out[k].g === null || g > out[k].g) out[k].g = g;
        });
        return out;
    }

    /* Whole-lap driving statistics for one side. */
    function lapStats(series) {
        var st = { samples: 0, top: null, min: null, avgKmh: null, fullThr: null, braking: null,
                   coasting: null, peakLat: null, peakDecel: null, peakAccel: null, peakYaw: null,
                   shifts: 0, avgThr: null };
        var sumV = 0, nV = 0, ft = 0, bk = 0, co = 0, nIn = 0, sumThr = 0, lastGear = null;
        series.forEach(function (s) {
            if (s.x === null) return;
            st.samples++;
            if (s.kmh !== null) {
                sumV += s.kmh; nV++;
                if (st.top === null || s.kmh > st.top) st.top = s.kmh;
                if (st.min === null || s.kmh < st.min) st.min = s.kmh;
            }
            if (s.thr !== null && s.brk !== null) {
                nIn++; sumThr += s.thr;
                if (s.thr >= 98) ft++;
                if (s.brk >= 10) bk++;
                if (s.thr < 5 && s.brk < 5) co++;
            }
            if (s.lat !== null && (st.peakLat === null || Math.abs(s.lat) > st.peakLat)) st.peakLat = Math.abs(s.lat);
            if (s.lon !== null) {
                if (st.peakDecel === null || s.lon < st.peakDecel) st.peakDecel = s.lon;
                if (st.peakAccel === null || s.lon > st.peakAccel) st.peakAccel = s.lon;
            }
            if (s.yawRate !== null && (st.peakYaw === null || Math.abs(s.yawRate) > st.peakYaw)) st.peakYaw = Math.abs(s.yawRate);
            if (s.gear !== null) {
                if (lastGear !== null && s.gear !== lastGear && !s.seg) st.shifts++;
                lastGear = s.gear;
            }
        });
        if (nV) st.avgKmh = sumV / nV;
        if (nIn) {
            st.fullThr = ft / nIn * 100; st.braking = bk / nIn * 100;
            st.coasting = co / nIn * 100; st.avgThr = sumThr / nIn;
        }
        return st;
    }

    /* Per-lap tyre wear from completed laps: tread at first and last valid
     * sample, per wheel. Returns [{lap, time, start:[4], end:[4], used:[4]}]. */
    function lapWear(laps) {
        var out = [];
        (laps || []).forEach(function (lap) {
            var pts = lap.samples || [];
            var a = null, b = null;
            for (var i = 0; i < pts.length; i++) if (pts[i].tread) { a = pts[i].tread; break; }
            for (var j = pts.length - 1; j >= 0; j--) if (pts[j].tread) { b = pts[j].tread; break; }
            if (!a || !b) return;
            out.push({
                lap: lap.lap, time: lap.end - lap.start, complete: WB.lapIsFull(lap),
                start: a.slice(), end: b.slice(),
                used: a.map(function (v, w) { return (v - b[w]) * 100; })
            });
        });
        return out;
    }

    /* Pearson correlation of two aligned arrays (nulls skipped). */
    function correlate(xs, ys) {
        var n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
        for (var i = 0; i < xs.length; i++) {
            var x = xs[i], y = ys[i];
            if (!finite(x) || !finite(y)) continue;
            n++; sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
        }
        if (n < 20) return null;
        var vx = sxx - sx * sx / n, vy = syy - sy * sy / n;
        if (!(vx > 1e-12) || !(vy > 1e-12)) return null;
        return (sxy - sx * sy / n) / Math.sqrt(vx * vy);
    }

    /* Mini-sector winner map: split [0, len] into n pieces and compare the
     * time each side spends inside (positive = A faster). */
    function miniSectors(ptsA, ptsB, len, n, bA, bB) {
        var out = [];
        if (!(len > 0)) return out;
        for (var k = 0; k < n; k++) {
            var x0 = len * k / n, x1 = len * (k + 1) / n;
            var a0 = timeAt(ptsA, x0, bA), a1 = timeAt(ptsA, x1, bA);
            var b0 = timeAt(ptsB, x0, bB), b1 = timeAt(ptsB, x1, bB);
            var da = a0 !== null && a1 !== null ? a1 - a0 : null;
            var db = b0 !== null && b1 !== null ? b1 - b0 : null;
            out.push({ x0: x0, x1: x1, a: da, b: db, gain: da !== null && db !== null ? db - da : null });
        }
        return out;
    }

    /* Sector times from boundaries (metres). */
    function sectorTimes(points, bounds, lapB) {
        var res = [];
        for (var k = 0; k + 1 < bounds.length; k++) {
            var a = timeAt(points, bounds[k], lapB), b = timeAt(points, bounds[k + 1], lapB);
            res.push(a !== null && b !== null ? b - a : null);
        }
        return res;
    }

    /* ============================================================ formatting */

    function fx(v, d, unit) {
        if (!finite(v)) return '—';
        return v.toFixed(d) + (unit ? ' ' + unit : '');
    }
    function sgn(v, d, unit) {
        if (!finite(v)) return '—';
        return (v > 0 ? '+' : v < 0 ? '−' : '±') + Math.abs(v).toFixed(d) + (unit ? ' ' + unit : '');
    }
    function lapText(sec) { return WB.lapTimeText(sec); }
    function esc(s) {
        return String(s === null || s === undefined ? '' : s).replace(/[&<>"']/g, function (c) {
            return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
        });
    }
    function niceStep(span, count) {
        var raw = span / Math.max(1, count), p = Math.pow(10, Math.floor(Math.log10(raw)));
        var m = raw / p;
        return (m < 1.5 ? 1 : m < 3 ? 2 : m < 7 ? 5 : 10) * p;
    }
    function ticks(lo, hi, count) {
        if (!(hi > lo)) return [];
        var st = niceStep(hi - lo, count), out = [];
        for (var v = Math.ceil(lo / st) * st; v <= hi + st * 1e-9; v += st) out.push(Math.abs(v) < st * 1e-9 ? 0 : v);
        return out;
    }
    function tickLabel(v, span) {
        var st = niceStep(span, 5);
        var dec = st >= 1 ? 0 : Math.min(3, Math.ceil(-Math.log10(st)));
        return v.toFixed(dec);
    }
    /* Tyre temperature colour ramp (C): cold blue, working green, hot red. */
    function tempColor(c) {
        if (!finite(c)) return '#334155';
        var stops = [[40, [59, 130, 246]], [70, [34, 211, 238]], [90, [52, 211, 153]],
                     [105, [250, 204, 21]], [120, [249, 115, 22]], [140, [239, 68, 68]]];
        if (c <= stops[0][0]) return 'rgb(' + stops[0][1].join(',') + ')';
        for (var i = 1; i < stops.length; i++) {
            if (c <= stops[i][0]) {
                var f = (c - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]);
                var a = stops[i - 1][1], b = stops[i][1];
                return 'rgb(' + a.map(function (v, k) { return Math.round(v + (b[k] - v) * f); }).join(',') + ')';
            }
        }
        return 'rgb(' + stops[stops.length - 1][1].join(',') + ')';
    }

    /* Canvas palette. Defaults are the /viewer look; the 4.4.0 console swaps
     * in the f1_telemetry.exe palette (setPalette). Only colours that frame
     * the data live here: series colours (A/B) and tier colours never change. */
    var PAL_DEFAULT = {
        band: 'rgba(255,255,255,0.012)', rowLine: '#1c2532', axis: '#6b7a8f', zero: '#2a3648', grid: '#141c27',
        label: '#a9b6c6', vgrid: '#121a24', cursor: 'rgba(230,237,245,0.55)', cursorText: '#e6edf5',
        track: '#1e293b', line: '#94a3b8', lineDim: '#64748b', start: '#e6edf5', ring: '#1c2532', bg: '#07090d',
        sans: 'Inter, Segoe UI, sans-serif', mono: 'Consolas, ui-monospace, monospace'
    };
    var PAL = {};
    function setPalette(t) {
        var k;
        for (k in PAL_DEFAULT) PAL[k] = PAL_DEFAULT[k];
        if (t) for (k in t) if (Object.prototype.hasOwnProperty.call(PAL_DEFAULT, k)) PAL[k] = t[k];
        return PAL;
    }
    setPalette(null);

    /* ================================================================ styles */

    var CSS = [
        ':root{--bg:#07090d;--bg2:#0b0f15;--panel:#0f141c;--panel2:#131a24;--line:#1c2532;--line2:#283446;',
        '--tx:#e6edf5;--tx2:#a9b6c6;--tx3:#6b7a8f;--a:' + COLORS.A + ';--b:' + COLORS.B + ';',
        '--good:#34d399;--bad:#f87171;--warn:#fbbf24;--res:#c084fc;--acc:#22d3ee;',
        '--mono:"JetBrains Mono","Cascadia Mono","Consolas",ui-monospace,monospace;',
        '--sans:"Inter","Segoe UI Variable","Segoe UI",system-ui,sans-serif}',
        '*{box-sizing:border-box}',
        'html,body{margin:0;height:100%;background:var(--bg);color:var(--tx);font:13px/1.4 var(--sans);-webkit-font-smoothing:antialiased}',
        '.tv{display:grid;grid-template-rows:auto auto 1fr auto;height:100vh;min-height:620px}',
        '.tv-top{display:flex;align-items:center;gap:14px;padding:0 14px;height:48px;background:linear-gradient(180deg,#0d121a,#0a0e14);border-bottom:1px solid var(--line)}',
        '.tv-brand{display:flex;align-items:center;gap:9px;font-weight:700;letter-spacing:.14em;font-size:12px;white-space:nowrap}',
        '.tv-brand i{display:inline-block;width:20px;height:20px;border-radius:5px;background:conic-gradient(from 200deg,var(--a),var(--acc),var(--b),var(--a));box-shadow:0 0 18px rgba(56,189,248,.35)}',
        '.tv-brand small{color:var(--tx3);font-weight:500;letter-spacing:.06em}',
        '.tv-sess{display:flex;gap:14px;align-items:center;color:var(--tx2);font-size:12px;overflow:hidden;white-space:nowrap}',
        '.tv-sess b{color:var(--tx);font-weight:600}',
        '.tv-live{display:inline-flex;align-items:center;gap:6px;font:600 11px var(--mono);letter-spacing:.08em;color:var(--good)}',
        '.tv-live:before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;box-shadow:0 0 0 0 currentColor;animation:tvp 1.6s infinite}',
        '.tv-live.stale{color:var(--warn)}.tv-live.off{color:var(--bad)}.tv-live.off:before,.tv-live.stale:before{animation:none}',
        '@keyframes tvp{0%{box-shadow:0 0 0 0 rgba(52,211,153,.6)}70%{box-shadow:0 0 0 7px rgba(52,211,153,0)}100%{box-shadow:0 0 0 0 rgba(52,211,153,0)}}',
        '.tv-sp{flex:1}',
        '.tv-btn{display:inline-flex;align-items:center;gap:6px;height:28px;padding:0 10px;border-radius:7px;border:1px solid var(--line2);background:var(--panel2);color:var(--tx);font:500 12px var(--sans);cursor:pointer;white-space:nowrap}',
        '.tv-btn:hover{border-color:#3b4a60;background:#18212d}.tv-btn.on{border-color:var(--acc);color:var(--acc);background:rgba(34,211,238,.08)}',
        '.tv-btn.res.on{border-color:var(--res);color:var(--res);background:rgba(192,132,252,.1)}',
        '.tv-sel{height:28px;border-radius:7px;border:1px solid var(--line2);background:var(--panel2);color:var(--tx);font:500 12px var(--sans);padding:0 8px;max-width:250px}',
        '.tv-cars{display:grid;grid-template-columns:1fr 1fr;gap:1px;background:var(--line);border-bottom:1px solid var(--line)}',
        '.tv-car{display:flex;align-items:center;gap:10px;padding:8px 14px;background:var(--bg2);min-width:0}',
        '.tv-car .tag{flex:none;width:26px;height:26px;border-radius:7px;display:grid;place-items:center;font:700 12px var(--mono);color:#061018}',
        '.tv-car.A .tag{background:var(--a)}.tv-car.B .tag{background:var(--b)}',
        '.tv-car .meta{display:flex;flex-direction:column;min-width:150px;flex:1}',
        '.tv-car .nm{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tv-car .sub{color:var(--tx3);font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tv-car .lt{font:600 15px var(--mono);font-variant-numeric:tabular-nums}',
        '.tv-car .st{font:600 10px var(--mono);letter-spacing:.08em;padding:2px 6px;border-radius:4px;background:#1a2330;color:var(--tx2)}',
        '.tv-car .st.LIVE{color:var(--good)}.tv-car .st.FROZEN{color:var(--acc)}.tv-car .st.PARTIAL{color:var(--warn)}',
        '.tv-main{display:grid;grid-template-columns:208px 1fr;min-height:0}',
        '.tv-nav{display:flex;flex-direction:column;gap:2px;padding:10px 8px;background:var(--bg2);border-right:1px solid var(--line);overflow:auto}',
        '.tv-nav button{display:flex;align-items:center;gap:10px;height:34px;white-space:nowrap;padding:0 10px;border:0;border-radius:8px;background:transparent;color:var(--tx2);font:500 13px var(--sans);text-align:left;cursor:pointer}',
        '.tv-nav button:hover{background:#121924;color:var(--tx)}',
        '.tv-nav button.on{background:linear-gradient(90deg,rgba(34,211,238,.14),rgba(34,211,238,.02));color:var(--tx);box-shadow:inset 2px 0 0 var(--acc)}',
        '.tv-nav button kbd{margin-left:auto;font:10px var(--mono);color:var(--tx3);border:1px solid var(--line2);border-radius:4px;padding:0 4px}',
        '.tv-nav .sep{height:1px;background:var(--line);margin:8px 4px}',
        '.tv-nav .hint{color:var(--tx3);font-size:11px;padding:4px 10px;line-height:1.5}',
        '.tv-nav svg{width:16px;height:16px;flex:none;stroke:currentColor;fill:none;stroke-width:1.7}',
        '.tv-view{position:relative;min-height:0;overflow:auto;padding:12px}',
        '.tv-grid{display:grid;gap:12px;height:100%;min-height:560px}',
        '.tv-p{position:relative;display:flex;flex-direction:column;min-height:0;min-width:0;background:var(--panel);border:1px solid var(--line);border-radius:12px;overflow:hidden}',
        '.tv-ph{display:flex;align-items:center;gap:8px;height:34px;padding:0 12px;border-bottom:1px solid var(--line);color:var(--tx2);font:600 11px var(--sans);letter-spacing:.08em;text-transform:uppercase;flex:none}',
        '.tv-ph .r{margin-left:auto;display:flex;gap:6px;align-items:center;text-transform:none;letter-spacing:0;font-weight:500}',
        '.tv-pb{position:relative;flex:1;min-height:0}',
        '.tv-car.flash,.f1x-laps.flash{outline:2px solid #38d6e8;outline-offset:-2px}',
        '.f1x-link{color:#38d6e8;text-decoration:underline;cursor:pointer}',
        '.f1x-cmp{display:grid;gap:3px;margin-bottom:10px;font:12px var(--mono);color:var(--tx2)}.f1x-cmp b{font-weight:700}',
        '.f1x-cmp-how{color:var(--tx3);font-size:11px}',
        '.tv-pb canvas{position:absolute;inset:0;width:100%;height:100%}',
        '.tv-pb.pad{padding:12px;overflow:auto}',
        '.tier{font:600 9px var(--mono);letter-spacing:.06em;padding:2px 5px;border-radius:4px;border:1px solid}',
        '.tier.FACT{color:#6ee7b7;border-color:#14532d;background:rgba(20,83,45,.25)}',
        '.tier.CONFIRMED{color:#93c5fd;border-color:#1e3a8a;background:rgba(30,58,138,.25)}',
        '.tier.DERIVED{color:#fcd34d;border-color:#713f12;background:rgba(113,63,18,.25)}',
        '.tier.RESEARCH{color:#d8b4fe;border-color:#581c87;background:rgba(88,28,135,.3)}',
        '.tv-kpis{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}',
        '.kpi{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:10px 12px}',
        '.kpi .k{color:var(--tx3);font-size:11px;letter-spacing:.04em}',
        '.kpi .v{display:flex;gap:10px;align-items:baseline;margin-top:4px;font:600 16px var(--mono);font-variant-numeric:tabular-nums}',
        '.kpi .v .a{color:var(--a)}.kpi .v .b{color:var(--b)}.kpi .d{font:500 11px var(--mono);margin-left:auto}',
        '.pos{color:var(--good)}.neg{color:var(--bad)}',
        'table.tv-t{width:100%;border-collapse:collapse;font:12px var(--mono);font-variant-numeric:tabular-nums}',
        '.tv-t th{position:sticky;top:0;background:var(--panel);text-align:right;color:var(--tx3);font:600 10px var(--sans);letter-spacing:.06em;text-transform:uppercase;padding:8px 8px;border-bottom:1px solid var(--line2)}',
        '.tv-t th:first-child,.tv-t td:first-child{text-align:left}',
        /* 1.1.2 Recordings table: names left-aligned; every row has 6 cells, the last (actions) right-aligned. */
        '.tv-t.f1x-recs th,.tv-t.f1x-recs td{text-align:left}',
        '.tv-t.f1x-recs th:last-child,.tv-t.f1x-recs td:last-child{text-align:right;white-space:nowrap}',
        '.tv-t td{text-align:right;padding:6px 8px;border-bottom:1px solid var(--line)}',
        '.tv-t tr.sel td{background:rgba(34,211,238,.08)}.tv-t tbody tr{cursor:pointer}.tv-t tbody tr:hover td{background:#121a25}',
        '.tv-t .a{color:var(--a)}.tv-t .b{color:var(--b)}',
        '.tv-empty{display:grid;place-items:center;height:100%;min-height:120px;color:var(--tx3);text-align:center;padding:20px;line-height:1.6}',
        '.tv-empty b{display:block;color:var(--tx2);font-size:14px;margin-bottom:4px}',
        '.banner{display:flex;gap:10px;align-items:flex-start;padding:10px 12px;border-radius:10px;border:1px solid #581c87;background:linear-gradient(90deg,rgba(88,28,135,.25),rgba(88,28,135,.06));color:#e9d5ff;font-size:12px;line-height:1.55}',
        '.banner b{color:#f5e8ff}',
        '.wheel-grid{display:grid;grid-template-columns:1fr 56px 1fr;grid-template-rows:auto auto;gap:8px;align-items:stretch}',
        '.wheel{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:7px 10px;display:flex;flex-direction:column;gap:3px;min-width:0}',
        '.wheel .h{display:flex;justify-content:space-between;color:var(--tx3);font:600 10px var(--mono);letter-spacing:.08em}',
        '.wheel .tt{display:flex;gap:3px;height:16px}.wheel .tt span{flex:1;border-radius:4px}',
        '.wheel .row{display:flex;justify-content:space-between;font:12px var(--mono);font-variant-numeric:tabular-nums;color:var(--tx2)}',
        '.wheel .row b{color:var(--tx);font-weight:600}',
        '.bar{height:5px;border-radius:3px;background:#1d2633;overflow:hidden}.bar i{display:block;height:100%;border-radius:3px}',
        '.chassis{grid-row:1/3;grid-column:2;border-radius:28px;border:1px solid var(--line2);background:linear-gradient(180deg,#121a26,#0c1119);display:grid;place-items:center;color:var(--tx3);font:600 10px var(--mono);letter-spacing:.1em;writing-mode:vertical-rl}',
        '.gauge{display:flex;flex-direction:column;gap:6px}',
        '.big{font:700 44px/1 var(--mono);font-variant-numeric:tabular-nums;letter-spacing:-.02em}',
        '.big small{font-size:14px;color:var(--tx3);margin-left:4px;font-weight:500}',
        '.vbars{display:flex;gap:8px;height:120px;align-items:flex-end}',
        '.vbar{width:26px;height:100%;border-radius:6px;background:#141c27;position:relative;overflow:hidden;border:1px solid var(--line)}',
        '.vbar i{position:absolute;left:0;right:0;bottom:0}',
        '.vbar span{position:absolute;bottom:-18px;width:100%;text-align:center;font:10px var(--mono);color:var(--tx3)}',
        '.rpmbar{height:12px;border-radius:6px;background:#141c27;overflow:hidden;border:1px solid var(--line)}',
        '.rpmbar i{display:block;height:100%;background:linear-gradient(90deg,#22d3ee,#34d399 60%,#facc15 85%,#ef4444)}',
        '.tv-foot{display:flex;gap:16px;align-items:center;height:26px;padding:0 14px;border-top:1px solid var(--line);background:var(--bg2);color:var(--tx3);font:11px var(--mono);white-space:nowrap;overflow:hidden}',
        '.tv-foot .ok{color:var(--good)}.tv-foot .bad{color:var(--bad)}',
        '.tv-legend{display:flex;gap:10px;align-items:center}.tv-legend i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-right:4px;vertical-align:middle}',
        '.tv-tip{position:absolute;pointer-events:none;z-index:5;background:rgba(10,14,20,.94);border:1px solid var(--line2);border-radius:8px;padding:6px 8px;font:11px var(--mono);color:var(--tx);white-space:nowrap;box-shadow:0 8px 24px rgba(0,0,0,.5)}',
        '.tv-legacy{height:100%;min-height:600px;background:var(--panel);border:1px solid var(--line);border-radius:12px;overflow:auto}',
        '.tv-modal{position:fixed;inset:0;background:rgba(3,5,8,.7);display:grid;place-items:center;z-index:20}',
        '.tv-modal>div{width:min(560px,92vw);background:var(--panel);border:1px solid var(--line2);border-radius:14px;padding:18px 20px;line-height:1.6;color:var(--tx2)}',
        '.tv-modal h3{margin:0 0 8px;color:var(--tx);font-size:15px}',
        '.tv-modal kbd{font:11px var(--mono);border:1px solid var(--line2);border-radius:4px;padding:0 5px;color:var(--tx)}',
        '.tv-dlg-opt{display:block;margin:6px 0;color:var(--tx);cursor:pointer}.tv-dlg-opt input{margin-right:6px}.tv-dlg-btns{display:flex;gap:8px;justify-content:flex-end;margin-top:14px}',
        '@media (max-width:980px){.tv{height:auto;min-height:100vh}.tv-main{grid-template-columns:1fr}.tv-nav{flex-direction:row;overflow-x:auto;border-right:0;border-bottom:1px solid var(--line)}.tv-nav .sep,.tv-nav .hint,.tv-nav kbd{display:none}',
        '.tv-top{flex-wrap:wrap;height:auto;padding:8px 10px;row-gap:6px}.tv-sess{flex-wrap:wrap}.tv-cars{grid-template-columns:1fr}.tv-car{flex-wrap:wrap}.tv-sess span:nth-child(n+4){display:none}.tv-top .tv-btn{padding:0 7px}',
        '.tv-grid{grid-template-columns:1fr!important;grid-template-rows:none!important;height:auto}',
        '.tv-grid>.tv-p{grid-column:auto!important;grid-row:auto!important;min-height:340px}.tv-grid>.tv-p:first-child{min-height:560px}}'
    ].join('\n');

    /* ============================================================ canvas kit */

    function setupCanvas(cv) {
        var r = cv.getBoundingClientRect();
        var dpr = (typeof window !== 'undefined' && window.devicePixelRatio) || 1;
        var w = Math.max(1, Math.round(r.width)), h = Math.max(1, Math.round(r.height));
        if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) {
            cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
        }
        var ctx = cv.getContext('2d');
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.clearRect(0, 0, w, h);
        return { ctx: ctx, w: w, h: h };
    }

    function el(tag, cls, html) {
        var e = document.createElement(tag);
        if (cls) e.className = cls;
        if (html !== undefined) e.innerHTML = html;
        return e;
    }

    function panel(title, tier, right) {
        var p = el('div', 'tv-p');
        var h = el('div', 'tv-ph', esc(title) + (tier ? ' <span class="tier ' + tier + '">' + tier + '</span>' : '') +
            '<span class="r"></span>');
        if (right) h.querySelector('.r').appendChild(right);
        var b = el('div', 'tv-pb');
        p.appendChild(h); p.appendChild(b);
        p.body = b; p.head = h;
        return p;
    }

    /* Multi-track strip chart sharing an x domain (distance or lap time),
     * with base + overlay canvases so the cursor never re-draws data. */
    function StripChart(host, viewer, tracks) {
        this.host = host; this.viewer = viewer; this.tracks = tracks;
        this.base = el('canvas'); this.over = el('canvas');
        host.appendChild(this.base); host.appendChild(this.over);
        this.tip = el('div', 'tv-tip'); this.tip.style.display = 'none';
        host.appendChild(this.tip);
        this.layout = null;
        var self = this, drag = null;
        this.over.addEventListener('mousemove', function (e) {
            var r = self.over.getBoundingClientRect(), mx = e.clientX - r.left;
            if (drag) {
                var L = self.layout; if (!L) return;
                var dx = (mx - drag.x) / L.pw * (drag.v1 - drag.v0);
                viewer.setZoom(drag.v0 - dx, drag.v1 - dx);
                return;
            }
            var x = self.xAt(mx);
            if (x !== null) viewer.setCursor(x, self, e.clientY - r.top, mx);
        });
        this.over.addEventListener('mouseleave', function () { self.tip.style.display = 'none'; drag = null; });
        this.over.addEventListener('mousedown', function (e) {
            var r = self.over.getBoundingClientRect(), z = viewer.zoomRange();
            drag = { x: e.clientX - r.left, v0: z[0], v1: z[1] };
        });
        window.addEventListener('mouseup', function () { drag = null; });
        this.over.addEventListener('wheel', function (e) {
            e.preventDefault();
            var r = self.over.getBoundingClientRect(), x = self.xAt(e.clientX - r.left);
            if (x === null) return;
            var z = viewer.zoomRange(), k = e.deltaY > 0 ? 1.25 : 0.8;
            viewer.setZoom(x - (x - z[0]) * k, x + (z[1] - x) * k);
        }, { passive: false });
        this.over.addEventListener('dblclick', function () { viewer.resetZoom(); });
    }

    StripChart.prototype.xAt = function (mx) {
        var L = this.layout;
        if (!L || mx < L.pl || mx > L.pl + L.pw) return null;
        return L.v0 + (mx - L.pl) / L.pw * (L.v1 - L.v0);
    };

    StripChart.prototype.draw = function () {
        var v = this.viewer, c = setupCanvas(this.base), ctx = c.ctx;
        var z = v.zoomRange(), v0 = z[0], v1 = z[1];
        var pl = 56, pr = 96, pt = 6, pb = 22;
        var pw = Math.max(10, c.w - pl - pr), ph = c.h - pt - pb;
        var total = this.tracks.reduce(function (s, t) { return s + (t.weight || 1); }, 0);
        var y = pt, rows = [];
        var useTime = v.xMode === 'time';
        ctx.font = '10px ' + 'Consolas, ui-monospace, monospace';
        for (var ti = 0; ti < this.tracks.length; ti++) {
            var tr = this.tracks[ti], hgt = ph * (tr.weight || 1) / total;
            var row = { tr: tr, y0: y, h: hgt, lo: null, hi: null, series: v.expandSeries(tr.series) };
            rows.push(row);
            /* range */
            if (tr.range) { row.lo = tr.range[0]; row.hi = tr.range[1]; }
            else {
                var lo = Infinity, hi = -Infinity;
                row.series.forEach(function (s) {
                    var data = v.seriesFor(s.side);
                    if (!data) return;
                    for (var i = 0; i < data.length; i++) {
                        var d = data[i], xx = useTime ? d.el : d.x;
                        if (xx === null || xx < v0 || xx > v1) continue;
                        var val = s.get(d, data, i);
                        if (val === null || val === undefined || !finite(val)) continue;
                        if (val < lo) lo = val; if (val > hi) hi = val;
                    }
                });
                if (!(hi >= lo)) { lo = 0; hi = 1; }
                if (tr.zero) { lo = Math.min(lo, 0); hi = Math.max(hi, 0); }
                if (tr.sym) { var m = Math.max(Math.abs(lo), Math.abs(hi)); lo = -m; hi = m; }
                var pad = (hi - lo) * 0.08 || 1;
                row.lo = lo - pad; row.hi = hi + pad;
            }
            /* frame and grid */
            ctx.fillStyle = ti % 2 ? PAL.band : 'rgba(255,255,255,0)';
            ctx.fillRect(pl, y, pw, hgt);
            ctx.strokeStyle = PAL.rowLine; ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(pl, Math.round(y + hgt) + 0.5); ctx.lineTo(pl + pw, Math.round(y + hgt) + 0.5); ctx.stroke();
            var yt = ticks(row.lo, row.hi, Math.max(2, Math.floor(hgt / 34)));
            ctx.fillStyle = PAL.axis; ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
            yt.forEach(function (val) {
                var py = row.y0 + row.h - (val - row.lo) / (row.hi - row.lo) * row.h;
                if (py < row.y0 + 6 || py > row.y0 + row.h - 4) return;
                ctx.strokeStyle = val === 0 ? PAL.zero : PAL.grid;
                ctx.beginPath(); ctx.moveTo(pl, Math.round(py) + 0.5); ctx.lineTo(pl + pw, Math.round(py) + 0.5); ctx.stroke();
                ctx.fillText(tickLabel(val, row.hi - row.lo), pl - 6, py);
            });
            ctx.save();
            ctx.textAlign = 'left'; ctx.textBaseline = 'top';
            ctx.fillStyle = PAL.label; ctx.font = '600 10px ' + PAL.sans;
            ctx.fillText(tr.label.toUpperCase() + (tr.unit ? '  ' + tr.unit : ''), pl + 6, y + 4);
            if (tr.tier) {
                /* measured in the label's own font, then the tier font */
                var lw = ctx.measureText(tr.label.toUpperCase() + (tr.unit ? '  ' + tr.unit : '')).width;
                ctx.font = '600 9px Consolas, monospace';
                ctx.fillStyle = tr.tier === 'RESEARCH' ? '#c084fc' : tr.tier === 'DERIVED' ? '#fcd34d' : '#4b5b70';
                ctx.fillText(tr.tier, pl + 14 + lw, y + 5);
            }
            ctx.restore();
            /* bands (braking zones etc.) */
            if (tr.bands) {
                tr.bands().forEach(function (b) {
                    var x0 = pl + (b[0] - v0) / (v1 - v0) * pw, x1 = pl + (b[1] - v0) / (v1 - v0) * pw;
                    if (x1 < pl || x0 > pl + pw) return;
                    ctx.fillStyle = b[2] || 'rgba(248,113,113,0.07)';
                    ctx.fillRect(Math.max(pl, x0), row.y0, Math.min(pl + pw, x1) - Math.max(pl, x0), row.h);
                });
            }
            /* series */
            ctx.save();
            ctx.beginPath(); ctx.rect(pl, row.y0, pw, row.h); ctx.clip();
            row.series.forEach(function (s) {
                var data = v.seriesFor(s.side);
                if (!data || !data.length) return;
                ctx.strokeStyle = s.color || COLORS[s.side] || '#ccc';
                ctx.lineWidth = s.width || 1.6;
                ctx.globalAlpha = s.alpha || 1;
                if (s.dash) ctx.setLineDash(s.dash); else ctx.setLineDash([]);
                ctx.lineJoin = 'round';
                /* Gaps (null values, segment breaks) restart the path:
                 * missing data is never bridged. */
                var open = false, lastPx = -1e9;
                ctx.beginPath();
                for (var i = 0; i < data.length; i++) {
                    var d = data[i], xx = useTime ? d.el : d.x;
                    var val = xx === null ? null : s.get(d, data, i);
                    if (xx === null || val === null || val === undefined || !finite(val)) { open = false; continue; }
                    if (d.seg) open = false;
                    var px = pl + (xx - v0) / (v1 - v0) * pw;
                    var py = row.y0 + row.h - (val - row.lo) / (row.hi - row.lo) * row.h;
                    if (!open) { ctx.moveTo(px, py); open = true; lastPx = px; continue; }
                    if (Math.abs(px - lastPx) < 0.35 && i !== data.length - 1) continue;
                    ctx.lineTo(px, py);
                    lastPx = px;
                }
                ctx.stroke();
            });
            ctx.restore();
            ctx.globalAlpha = 1; ctx.setLineDash([]);
            y += hgt;
        }
        /* x axis */
        ctx.fillStyle = PAL.axis; ctx.textAlign = 'center'; ctx.textBaseline = 'top';
        ctx.font = '10px Consolas, ui-monospace, monospace';
        ticks(v0, v1, Math.max(3, Math.floor(pw / 90))).forEach(function (xv) {
            var px = pl + (xv - v0) / (v1 - v0) * pw;
            ctx.strokeStyle = PAL.vgrid;
            ctx.beginPath(); ctx.moveTo(Math.round(px) + 0.5, pt); ctx.lineTo(Math.round(px) + 0.5, pt + ph); ctx.stroke();
            ctx.fillText(useTime ? xv.toFixed(xv % 1 ? 1 : 0) + ' s' : (Math.abs(v1 - v0) < 2500 ? Math.round(xv) + ' m' : (xv / 1000).toFixed(1) + ' km'), px, pt + ph + 5);
        });
        this.layout = { pl: pl, pw: pw, pt: pt, ph: ph, rows: rows, v0: v0, v1: v1, w: c.w, h: c.h, pr: pr };
        this.drawCursor();
    };

    StripChart.prototype.drawCursor = function () {
        var L = this.layout, v = this.viewer;
        var c = setupCanvas(this.over), ctx = c.ctx;
        if (!L) return;
        var x = v.cursorFor(this);
        if (x === null || x < L.v0 || x > L.v1) { this.tip.style.display = 'none'; return; }
        var px = L.pl + (x - L.v0) / (L.v1 - L.v0) * L.pw;
        ctx.strokeStyle = PAL.cursor; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(Math.round(px) + 0.5, L.pt); ctx.lineTo(Math.round(px) + 0.5, L.pt + L.ph); ctx.stroke();
        ctx.font = '600 11px Consolas, ui-monospace, monospace'; ctx.textBaseline = 'middle'; ctx.textAlign = 'left';
        var useTime = v.xMode === 'time';
        L.rows.forEach(function (row) {
            var lines = [];
            row.series.forEach(function (s) {
                if (s.noReadout) return;
                var data = v.seriesFor(s.side);
                if (!data) return;
                var val = useTime ? valueAtTime(data, x, s.get) : valueAt(data, x, function (d, i) {
                    return s.get(d, data, i);
                });
                if (val === null || !finite(val)) return;
                var py = row.y0 + row.h - (val - row.lo) / (row.hi - row.lo) * row.h;
                ctx.fillStyle = s.color || COLORS[s.side];
                ctx.beginPath(); ctx.arc(px, py, 3, 0, Math.PI * 2); ctx.fill();
                lines.push({ c: s.color || COLORS[s.side], t: (row.tr.fmt || function (q) { return q.toFixed(1); })(val), lbl: s.lbl });
            });
            var ry = row.y0 + 14;
            lines.forEach(function (ln) {
                ctx.fillStyle = ln.c;
                ctx.fillText((ln.lbl ? ln.lbl + ' ' : '') + ln.t, L.pl + L.pw + 8, ry);
                ry += 14;
            });
        });
        ctx.fillStyle = PAL.cursorText; ctx.textAlign = 'center';
        ctx.fillText(useTime ? x.toFixed(2) + ' s' : Math.round(x) + ' m', px, L.pt + L.ph + 12);
    };

    function valueAtTime(series, t, get) {
        for (var i = 1; i < series.length; i++) {
            var a = series[i - 1], b = series[i];
            if (a.el === null || b.el === null) continue;
            if (a.el <= t && b.el >= t) {
                if (b.seg) return null;
                var va = get(a, series, i - 1), vb = get(b, series, i);
                if (va === null || vb === null || !finite(va) || !finite(vb)) return null;
                var f = b.el > a.el ? (t - a.el) / (b.el - a.el) : 0;
                return va + (vb - va) * f;
            }
        }
        return null;
    }

    /* ================================================================ viewer */

    function Viewer(root, opts) {
        this.root = root;
        this.opts = opts || {};
        this.wb = this.opts.workbench || new WB.Workbench();
        this.view = 'overview';
        this.xMode = 'dist';
        this.zoom = null;
        this.cursor = null;
        this.research = false;
        this.frame = null;
        this.frameAt = 0;
        this.roster = [];
        this.rosterSig = '';
        this.context = null;
        this.waypoints = null;
        this.trackDoc = null;   /* rc.12: full /track document (dl/wl/gw/gww) for the Weather map */
        this.charts = [];
        this.canvases = [];
        this.cache = {};
        this.zoneSel = 0;
        this.health = null;
        this.weather = null;   /* rc.11 weather: /weather document while the Weather page is open */
        this.legacyUi = null;
        this.dirty = true;
        this.lapMode = { A: 'auto', B: 'auto' };   /* extra sides default to auto (lapMode[L] || 'auto') */
        this.build();
    }

    var NAV = [
        ['overview', 'Overview', '1', '<path d="M3 17l5-6 4 3 5-8 4 5"/>'],
        ['dynamics', 'Vehicle dynamics', '2', '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="4"/><path d="M12 4v16M4 12h16"/>'],
        ['braking', 'Braking & corners', '3', '<path d="M4 18c4 0 5-12 9-12s4 6 7 6"/><path d="M4 21h16"/>'],
        ['tyres', 'Tyres', '4', '<rect x="5" y="3" width="5" height="8" rx="2"/><rect x="14" y="3" width="5" height="8" rx="2"/><rect x="5" y="13" width="5" height="8" rx="2"/><rect x="14" y="13" width="5" height="8" rx="2"/>'],
        ['compare', 'Compare', '5', '<path d="M7 4v16M17 4v16M3 8h8M13 16h8"/>'],
        ['live', 'Live', '6', '<path d="M12 12m-2 0a2 2 0 1 0 4 0a2 2 0 1 0 -4 0"/><path d="M5 5a10 10 0 0 0 0 14M19 5a10 10 0 0 1 0 14"/>'],
        ['research', 'Chassis research', '7', '<path d="M9 3h6M10 3v6l-5 9a2 2 0 0 0 2 3h10a2 2 0 0 0 2-3l-5-9V3"/>'],
        ['legacy', 'Legacy layouts', '8', '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 10h18M9 10v10"/>']
    ];

    Viewer.prototype.build = function () {
        var self = this;
        if (!document.getElementById('tv-style')) {
            var st = el('style'); st.id = 'tv-style'; st.textContent = CSS;
            document.head.appendChild(st);
        }
        var r = this.root;
        r.innerHTML = '';
        r.className = 'tv';
        /* top bar */
        var top = el('div', 'tv-top');
        top.innerHTML = '<div class="tv-brand"><i></i>F1C TELEMETRY <small>' + PRODUCT_VERSION + '</small></div>' +
            '<div class="tv-sess"></div><div class="tv-sp"></div>';
        this.sessEl = top.querySelector('.tv-sess');
        this.xBtn = el('button', 'tv-btn', 'X: distance');
        this.xBtn.title = 'Switch the x axis between lap distance and lap time (D)';
        this.xBtn.onclick = function () { self.toggleX(); };
        this.resBtn = el('button', 'tv-btn res', 'Research channels: OFF');
        this.resBtn.title = 'Show RESEARCH candidates (unproven meaning and unit) (R)';
        this.resBtn.setAttribute('aria-pressed', 'false');
        this.resBtn.onclick = function () { self.setResearch(!self.research); };
        var imp = el('button', 'tv-btn', 'Import');
        imp.title = 'Load a saved lap file (saved from this page, or an old .lap / .spt file) into car A or B';
        this.fileIn = el('input'); this.fileIn.type = 'file'; this.fileIn.accept = '.json,.lap,.spt'; this.fileIn.style.display = 'none';
        imp.onclick = function () { self.fileIn.click(); };
        this.fileIn.onchange = function () { self.importFile(); };
        var exp = el('button', 'tv-btn', 'Export');
        exp.title = 'Save the laps on screen to a file you can load later';
        exp.onclick = function () { self.exportShown(); };
        var help = el('button', 'tv-btn', '?');
        help.onclick = function () { self.showHelp(); };
        [this.xBtn, this.resBtn, imp, exp, help].forEach(function (b) { top.appendChild(b); });
        top.appendChild(this.fileIn);
        r.appendChild(top);
        /* car strip */
        var cars = el('div', 'tv-cars');
        this.carEls = {};
        ['A', 'B'].forEach(function (L) {
            var c = el('div', 'tv-car ' + L);
            c.innerHTML = '<div class="tag">' + L + '</div><div class="meta"><div class="nm">—</div><div class="sub">waiting for game data</div></div>' +
                '<span class="st">WAITING</span><div class="lt">—</div>';
            var carSel = el('select', 'tv-sel'); carSel.title = 'Car for side ' + L;
            var lapSel = el('select', 'tv-sel'); lapSel.title = 'Lap for side ' + L;
            var frz = el('button', 'tv-btn', 'Freeze'); frz.title = 'Pin the display; recording continues';
            carSel.onchange = function () {
                var id = parseInt(carSel.value, 10);
                var meta = self.roster.filter(function (q) { return q.id === id; })[0];
                self.wb.select(L, id, meta); self.lapMode[L] = 'auto'; self.cache = {}; self.invalidate();
            };
            lapSel.onchange = function () { self.pickLap(L, lapSel.value); };
            frz.onclick = function () {
                var side = self.side(L);
                if (side.frozenSnapshot) side.resume(); else side.freeze();
                self.invalidate();
            };
            c.appendChild(carSel); c.appendChild(lapSel); c.appendChild(frz);
            cars.appendChild(c);
            self.carEls[L] = { root: c, car: carSel, lap: lapSel, frz: frz,
                nm: c.querySelector('.nm'), sub: c.querySelector('.sub'), st: c.querySelector('.st'), lt: c.querySelector('.lt') };
        });
        r.appendChild(cars);
        /* main */
        var main = el('div', 'tv-main');
        var nav = el('div', 'tv-nav');
        this.navBtns = {};
        NAV.forEach(function (n, i) {
            if (n[0] === 'research' || n[0] === 'legacy') nav.appendChild(el('div', 'sep'));
            var b = el('button', '', '<svg viewBox="0 0 24 24">' + n[3] + '</svg>' + esc(n[1]) + '<kbd>' + n[2] + '</kbd>');
            b.onclick = function () { self.setView(n[0]); };
            nav.appendChild(b);
            self.navBtns[n[0]] = b;
        });
        nav.appendChild(el('div', 'hint', 'Drag to pan, wheel to zoom, double-click to reset. Cursor is shared by every chart and the map.'));
        this.viewEl = el('div', 'tv-view');
        main.appendChild(nav); main.appendChild(this.viewEl);
        r.appendChild(main);
        /* footer */
        this.foot = el('div', 'tv-foot');
        r.appendChild(this.foot);
        document.addEventListener('keydown', function (e) {
            if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) return;
            var k = e.key;
            for (var i = 0; i < NAV.length; i++) if (NAV[i][2] === k) { self.setView(NAV[i][0]); return; }
            if (k === 'd' || k === 'D') self.toggleX();
            else if (k === 'r' || k === 'R') self.resBtn.click();
            else if (k === 'Escape') self.resetZoom();
            else if (k === 'a' || k === 'A') self.carEls.A.frz.click();
            else if (k === 'b' || k === 'B') self.carEls.B.frz.click();
        });
        window.addEventListener('resize', function () { self.invalidate(); });
        this.setView(this.view);
        this.loop();
    };

    Viewer.prototype.side = function (L) { return this.wb.sideOf(L); };

    /* Letters of the active sides in order (A, B, then extra sides). */
    Viewer.prototype.letters = function () {
        return this.wb.allSides().map(function (s) { return s.letter; });
    };

    Viewer.prototype.points = function (L) {
        var s = this.side(L);
        return s ? s.displayPoints() || [] : [];
    };

    /* Derived series per side, cached by identity + length. */
    Viewer.prototype.seriesFor = function (L) {
        var pts = this.points(L);
        var key = L;
        var c = this.cache[key];
        var sig = pts.length + ':' + (pts.length ? pts[0].t + ':' + pts[pts.length - 1].t : '');
        if (c && c.pts === pts && c.sig === sig) return c.data;
        var data = derive(pts);
        this.cache[key] = { pts: pts, sig: sig, data: data };
        return data;
    };

    Viewer.prototype.extent = function () {
        var lo = Infinity, hi = -Infinity, self = this;
        this.letters().forEach(function (L) {
            self.seriesFor(L).forEach(function (d) {
                var x = self.xMode === 'time' ? d.el : d.x;
                if (x === null) return;
                if (x < lo) lo = x; if (x > hi) hi = x;
            });
        });
        if (this.xMode !== 'time' && this.wb.trackLen > 0) { lo = 0; hi = Math.max(hi, this.wb.trackLen); }
        if (!(hi > lo)) { lo = 0; hi = this.xMode === 'time' ? 90 : (this.wb.trackLen || 5000); }
        return [lo, hi];
    };

    Viewer.prototype.zoomRange = function () {
        var e = this.extent();
        if (!this.zoom) return e;
        return this.zoom;
    };
    Viewer.prototype.setZoom = function (a, b) {
        var e = this.extent(), span = e[1] - e[0];
        if (b - a < Math.max(span * 0.004, 5) || !finite(a) || !finite(b)) return;
        if (b - a > span) { this.zoom = null; this.redrawCharts(); return; }
        if (a < e[0]) { b += e[0] - a; a = e[0]; }
        if (b > e[1]) { a -= b - e[1]; b = e[1]; }
        this.zoom = [a, b];
        this.redrawCharts();
    };
    Viewer.prototype.resetZoom = function () { this.zoom = null; this.redrawCharts(); };

    Viewer.prototype.toggleX = function () {
        this.xMode = this.xMode === 'dist' ? 'time' : 'dist';
        this.xBtn.textContent = this.xMode === 'dist' ? 'X: distance' : 'X: lap time';
        this.zoom = null; this.cursor = null;
        this.invalidate();
    };

    /* The cursor is stored in distance; time-axis charts convert. */
    Viewer.prototype.setCursor = function (x, src) {
        this.cursor = this.xMode === 'time' ? this.distAtTime('A', x) : x;
        this.cursorRaw = x;
        this.drawCursors();
    };
    Viewer.prototype.cursorFor = function () {
        if (this.cursor === null && this.cursorRaw === undefined) return null;
        return this.xMode === 'time' ? (this.cursorRaw !== undefined ? this.cursorRaw : null) : this.cursor;
    };
    Viewer.prototype.distAtTime = function (L, t) {
        var d = this.seriesFor(L);
        return valueAtTime(d, t, function (q) { return q.x; });
    };

    /* TAPE2 V4 (root cause, FACT in the Browser pane): the cursor, hover ball and chart cursor line are redrawn in
     * a requestAnimationFrame callback and _cq latches until it runs. Where the host never runs it (the Browser pane:
     * 1 s with no callback, reproduced 2026-10-14), _cq stays true after the first hover and no ball is drawn again.
     * A zero-delay timer coalesces the same way and always runs. */
    Viewer.prototype.drawCursors = function () {
        var self = this;
        if (this._cq) return;
        this._cq = true;
        setTimeout(function () {
            self._cq = false;
            self.charts.forEach(function (c) { if (c.drawCursor) c.drawCursor(); });
            self.canvases.forEach(function (c) { if (c.cursor) c.cursor(); });
            if (self.onCursor) self.onCursor();
        });
    };

    /* TAPE2 V7 (one helper for every track map): where a frame car sits on the map, [x, z] in the map frame (mx/mz, else
     * px/pz). Null when the frame has no position for it. Pure. */
    function frameMapPos(car) {
        if (!car) return null;
        var x = finite(car.mx) ? car.mx : car.px, z = finite(car.mz) ? car.mz : car.pz;
        return finite(x) && finite(z) ? [x, z] : null;
    }

    /* TAPE2 V7: a filled ball for each active side (A, B, extra sides) at the car's position in the current frame: the
     * newest frame in LIVE, the tape frame in PAST. view(x, z) is the map's own projection. Sides with no car
     * (B off, slot -1) and cars missing from the frame draw nothing. */
    Viewer.prototype.drawSideBalls = function (ctx, view) {
        var self = this, cars = this.frame && Array.isArray(this.frame.c) ? this.frame.c : [];
        if (!ctx || !view) return;
        this.letters().forEach(function (L) {
            var side = self.side(L), car = null;
            if (!side || typeof side.slot !== 'number' || side.slot < 0) return;
            for (var i = 0; i < cars.length; i++) if (cars[i] && cars[i].id === side.slot) { car = cars[i]; break; }
            var p = frameMapPos(car);
            if (!p) return;
            var v = view(p[0], p[1]);
            ctx.fillStyle = COLORS[L]; ctx.strokeStyle = PAL.bg; ctx.lineWidth = 2;
            ctx.beginPath(); ctx.arc(v[0], v[1], 6, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
        });
    };

    Viewer.prototype.redrawCharts = function () {
        this.charts.forEach(function (c) { c.draw(); });
        this.canvases.forEach(function (c) { c.draw(); });
    };

    Viewer.prototype.invalidate = function () { this.dirty = true; };

    Viewer.prototype.setView = function (v) {
        this.view = v;
        for (var k in this.navBtns) this.navBtns[k].classList.toggle('on', k === v);
        this.renderView();
    };

    /* One redraw of the dirty parts. The animation loop calls it; the review tape timer calls it too (RAF does not fire in the Browser pane). */
    Viewer.prototype.flush = function () {
        if (this._carSelDue) this.fillCarSelects();   /* TAPE2 V2: a deferred selector rebuild (once a second, not while open) */
        if (!this.dirty) return;
        this.dirty = false;
        this.applyAuto();
        this.refreshChrome();
        if (this.view === 'live') this.updateLive();
        else if (this.view !== 'legacy') this.refreshView();
        else if (this.legacyUi) this.legacyUi.render();
    };
    Viewer.prototype.loop = function () {
        var self = this;
        var tick = function () {
            self.flush();
            if (typeof window !== 'undefined' && window.requestAnimationFrame) window.requestAnimationFrame(tick);
            else setTimeout(tick, 100);
        };
        tick();
    };

    /* --------------------------------------------------------------- frames */

    Viewer.prototype.onFrame = function (j) {
        if (!j || !Array.isArray(j.c)) return;
        if (this.wbFeed !== false) this.wb.frame(j);   /* review tape frames do not feed the workbench (its laps come from /trace/data) */
        else {
            /* Claude review 2026-10-14: the workbench still adopts the frame's generation and track, or seedSideTrace
             * (doc.gen === wb.sessionGeneration) never accepts a recorded lap and every analysis page stays empty. */
            this.wb.sessionGeneration = j.session_generation;
            this.wb.trackKey = j.trk || '';
            this.wb.trackLen = Number.isFinite(j.lapm) && j.lapm > 0 ? j.lapm : 0;
        }
        this.frame = j; this.frameAt = Date.now();
        /* rc.2: every car is listed; order and P come from the race position
         * (WB.rosterOrder). p is part of the signature so the selector rebuilds
         * when positions change; rc.1 left it out and kept stale P numbers. */
        var next = j.c.slice();
        var sig = JSON.stringify(next.map(function (c) { return [c.id, c.n, c.cn, c.a, c.tid, c.cr, c.p]; }));
        if (sig !== this.rosterSig) { this.roster = next; this.rosterSig = sig; this.fillCarSelects(); }
        var ctx = JSON.stringify([j.session_generation, j.trk, j.lapm]);
        if (ctx !== this.context) {
            this.context = ctx;
            /* 4.4.0-rc.5 task 2: a retired or stopped player is never the default A. */
            var pick = WB.pickDefaultSides(this.roster);
            if (pick.A) this.wb.select('A', pick.A.id, pick.A); else this.wb.select('A', -1, null);
            if (pick.B && !this.bOff) this.wb.select('B', pick.B.id, pick.B); else this.wb.select('B', -1, null);
            if (pick.notes.A) this.wb.sideOf('A').note(pick.notes.A);
            if (pick.notes.B && !this.bOff) this.wb.sideOf('B').note(pick.notes.B);
            var lm = {};
            this.letters().forEach(function (L) { lm[L] = 'auto'; });
            this.lapMode = lm;
            this.fillCarSelects();
            this.cache = {}; this.zoom = null;
            if (this.opts.fetchTrack && !this.tapeTrackOk) {
                var self = this, token = (this.trackToken = (this.trackToken || 0) + 1);
                this.waypoints = null; this.trackDoc = null;
                this.opts.fetchTrack().then(function (d) {
                    if (token === self.trackToken) {
                        self.waypoints = d ? (Array.isArray(d) ? d : d.pts || null) : null;
                        self.trackDoc = d && !Array.isArray(d) ? d : null;   /* rc.12: dl/wl/gw for the Weather map */
                        self.invalidate();
                    }
                }).catch(function () {});
            }
        }
        /* Claude review 2026-10-14: a recording's tape starts at t_first, where no car has a race position yet, so the
         * context pick above finds no running car. Pick once when positions appear (sides the user set are kept). */
        if (this.wbFeed === false && !this.tapeSidesPicked) {
            var tp = WB.pickDefaultSides(this.roster), me = this;
            if (tp.A) {
                this.tapeSidesPicked = true;
                [['A', tp.A], ['B', this.bOff ? null : tp.B]].forEach(function (q) {
                    if (!q[1] || me.wb.sideOf(q[0]).slot >= 0) return;
                    if (me.pickSideCar) me.pickSideCar(q[0], q[1].id); else me.wb.select(q[0], q[1].id, q[1]);
                });
            }
        }
        this.invalidate();
    };

    /* TAPE2 V2: the car selectors list the cars by position, "P<pos> <name> #<num> [badge]". They rebuild at most once a
     * second, and never while one is focused (a rebuild closes an open list). force skips both limits (user picks). */
    Viewer.prototype.fillCarSelects = function (force) {
        var self = this, now = Date.now();
        var busy = this.letters().some(function (L) {
            return typeof document !== 'undefined' && !!self.carEls[L] && document.activeElement === self.carEls[L].car;
        });
        if (!force && (busy || (this._carSelAt && now - this._carSelAt < 1000))) { this._carSelDue = true; return; }
        this._carSelAt = now; this._carSelDue = false;
        this.letters().forEach(function (L) {
            var sel = self.carEls[L].car, side = self.side(L);
            var html = '';
            var sorted = (self.roster || []).slice().sort(function (a, b) {
                var pa = a.p > 0 ? a.p : 1000 + a.id, pb = b.p > 0 ? b.p : 1000 + b.id;
                return pa - pb || a.id - b.id;
            });
            sorted.forEach(function (c) {
                /* rc.17: a retired AI car loses AI control (a 0); only cr 0 is the local player. */
                var tag = humanBadgeText(c) || (c.cr === 2 && c.a ? " (friend's car, AI driving)" : (c.rt || c.fs > 0) ? ' (retired)' : '');
                var num = Number.isInteger(c.num) && c.num >= 0 ? '#' + c.num : '';
                var nm = cleanName(c.n) ? esc(cleanName(c.n)) + (num ? ' ' + num : '') : (num || 'Car ' + c.id);   /* no name: the number stands for it */
                html += '<option value="' + c.id + '"' + (side && side.slot === c.id ? ' selected' : '') + '>' + (c.p > 0 ? 'P' + c.p : 'P -') + '  ' +
                    nm + tag + '</option>';
            });
            if (L === 'B' && self.isConsole) {
                /* A1.7: the first item switches B off; while off it is the selected item and no car is marked. */
                if (self.bOff) html = html.replace(/ selected>/g, '>');
                html = '<option value="off"' + (self.bOff ? ' selected' : '') + '>B off (show A only)</option>' +
                    (side && side.slot < 0 && !self.bOff ? '<option value="-1" selected>' + esc(side.label) + '</option>' : '') + html;
            } else if (side && side.slot < 0) html = '<option value="-1" selected>' + esc(side.label) + '</option>' + html;
            sel.innerHTML = html;
        });
    };

    Viewer.prototype.lapOptions = function (L) {
        var side = this.side(L), laps = side ? side.displayLaps() : [];
        var best = WB.fastestLap(laps);
        var shown = side ? (side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown) : null;
        var mode = this.lapMode[L];
        var opts = [{ v: 'auto', t: 'Last full lap (auto)', sel: mode === 'auto' },
                    { v: 'open', t: 'Lap in progress', sel: mode === 'open' }];
        for (var i = laps.length - 1; i >= 0; i--) {
            var l = laps[i];
            opts.push({ v: String(i), sel: mode !== 'auto' && mode !== 'open' && shown === l,
                t: lapLabel(l) + '  ' + lapText(lapShownTime(l)) +
                   (l === best ? '  ★ fastest' : WB.lapIsFull(l) ? '' : '  partial') });
        }
        return opts;
    };

    Viewer.prototype.pickLap = function (L, v) {
        var side = this.side(L);
        if (!side) return;
        var laps = side.displayLaps();
        this.lapMode[L] = v === 'auto' || v === 'open' ? v : 'pick';
        side.shown = v === 'auto' ? (laps[laps.length - 1] || null) : v === 'open' ? null : laps[parseInt(v, 10)] || null;
        if (side.frozenSnapshot) side.frozenSnapshot.shown = side.shown;
        this.cache = {}; this.zoom = null;
        this.invalidate();
    };

    /* Auto mode follows the newest completed lap; with none yet it shows the
     * open lap. Re-applied every refresh so a lap crossing advances it. */
    Viewer.prototype.applyAuto = function () {
        var self = this;
        this.letters().forEach(function (L) {
            var side = self.side(L);
            if (!side || (self.lapMode[L] || 'auto') !== 'auto' || side.frozenSnapshot) return;
            var laps = side.displayLaps(), last = laps[laps.length - 1] || null;
            if (side.shown !== last) { side.shown = last; self.cache = {}; }
        });
    };

    /* Crossing bounds of the displayed lap when it is a completed lap. */
    Viewer.prototype.lapBounds = function (L) {
        var side = this.side(L);
        if (!side) return null;
        var shown = side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown;
        if (!shown || !shown.complete || !(shown.end > shown.start)) return null;
        return { start: shown.start, end: shown.end, len: shown.length || this.wb.trackLen };
    };

    Viewer.prototype.lapTime = function (L) {
        var side = this.side(L);
        if (!side) return null;
        var shown = side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown;
        if (shown && shown.end > shown.start) return shown.end - shown.start;
        return null;
    };

    /* One line that says what side L shows: car, lap number and time, and how
     * it was chosen (latest completed, picked, live, frozen). Used by the
     * console's showing line and by the Driver Compare header. */
    Viewer.prototype.lapDescr = function (L) {
        var side = this.side(L);
        if (!side) return '—';
        var frozen = !!side.frozenSnapshot;
        var shown = frozen ? side.frozenSnapshot.shown : side.shown;
        var what;
        if (shown) {
            what = lapLabel(shown) + ' ' + lapText(shown.end - shown.start) +
                (WB.lapIsFull(shown) ? '' : ' (partial)') + (this.lapMode[L] === 'auto' ? ' · latest completed' : '');
        } else what = side.slot >= 0 ? 'live lap' : 'nothing shown';
        if (frozen) what += ' · FROZEN';
        return side.label + ' · ' + what;
    };

    /* Driver Compare header: which lap each side compares, and where to change it. */
    Viewer.prototype.compareHead = function () {
        var self = this, where = this.isConsole ? 'change in the lap strip above' : 'change in the lap menus above';
        return '<div class="f1x-cmp">' + this.letters().map(function (L) {
            return '<div><b style="color:' + COLORS[L] + '">' + L + '</b> ' + esc(self.lapDescr(L)) + '</div>';
        }).join('') +
            '<div class="f1x-cmp-how">Compared laps are picked per side: <span class="f1x-link">' + where + '</span></div></div>';
    };

    /* Briefly outlines the lap picker so a "change in ..." link points at it. */
    Viewer.prototype.flashLapPicker = function () {
        var box = this.lapStrip || this.carEls.A.root;
        box.classList.add('flash');
        setTimeout(function () { box.classList.remove('flash'); }, 1200);
    };

    Viewer.prototype.refreshChrome = function () {
        var self = this, j = this.frame;
        /* session */
        var age = j ? Date.now() - this.frameAt : null;
        var liveCls = !j ? 'off' : age > 2500 || j.sample_age_ms > 1500 ? 'stale' : '';
        var sty = { 0: 'Test Day', 1: 'Practice 1', 2: 'Practice 2', 3: 'Practice 3', 4: 'Practice 4', 5: 'Qualifying', 6: 'Warm-up', 7: 'Race' };
        this.sessEl.innerHTML = '<span class="tv-live ' + liveCls + '">' + (!j ? 'NO DATA' : liveCls ? 'STALE' : 'LIVE') + '</span>' +
            (j ? '<span><b>' + esc(headerTrackText(j)) + '</b></span>' +
                 '<span>' + (sty[j.ty] || 'Session ' + j.ty) + '</span>' +
                 '<span>Lap length <b>' + (j.lapm > 0 ? (j.lapm / 1000).toFixed(3) + ' km' : 'unknown') + '</b></span>' +
                 '<span>Air <b>' + fx(j.at, 0, '°C') + '</b> Track <b>' + fx(j.trkt, 0, '°C') + '</b></span>' +
                 '<span>Sim <b>' + fx(j.st, 1, 's') + '</b></span>' +
                 (this.coopInfo() ? '<span class="tier CONFIRMED">CO-OP ' + this.coopInfo() + '</span>' : '')
                 : '<span>Waiting for /events. Start a session in F1 Challenge.</span>');
        this.letters().forEach(function (L) {
            var side = self.side(L), e = self.carEls[L];
            var car = j && side ? j.c.filter(function (c) { return c.id === side.slot; })[0] : null;
            e.nm.textContent = side ? side.label : '—';
            e.sub.textContent = car ? 'P' + car.p + ' · lap ' + lapNo(car.l) + ' · best ' + lapText(car.bl) + ' · last ' + lapText(car.ll) :
                side && side.slot < 0 && side.laps.length ? 'imported trace' : 'no live row';
            var st = side ? side.state || 'WAITING' : 'WAITING';
            e.st.textContent = st; e.st.className = 'st ' + st;
            var lt = self.lapTime(L);
            e.lt.textContent = lt !== null ? lapText(lt) : 'open lap';
            e.frz.classList.toggle('on', !!(side && side.frozenSnapshot));
            e.frz.textContent = side && side.frozenSnapshot ? 'Resume' : 'Freeze';
            var opts = self.lapOptions(L), sig = JSON.stringify(opts);
            if (e.lapSig !== sig) {
                e.lapSig = sig;
                e.lap.innerHTML = opts.map(function (o) { return '<option value="' + o.v + '"' + (o.sel ? ' selected' : '') + '>' + esc(o.t) + '</option>'; }).join('');
            }
        });
        /* footer */
        var h = this.health;
        var hs = h ? (h.viewer_ui && h.viewer_ui.matches === false ? '<span class="bad">viewer sidecar hash mismatch</span>' :
            '<span class="ok">sidecars verified</span>') + ' · API ' + esc(h.api_version) + ' · capture p95 ' + fx(h.capture_cost_us && h.capture_cost_us.p95, 0, 'µs') : 'health unknown';
        this.foot.innerHTML = '<span>' + PRODUCT + ' ' + PRODUCT_VERSION + '</span><span>' + hs + '</span>' +
            '<span>' + (j ? 'sample age ' + fx(j.sample_age_ms, 0, 'ms') + ' · ' + j.c.length + ' cars' : '') + '</span>' +
            '<span class="tv-sp"></span><span class="tv-legend">' + this.letters().map(function (L) {
                return '<span><i style="background:' + COLORS[L] + '"></i>' + L + '</span>';
            }).join('') + '</span>' +
            '<span>read-only</span>';
    };

    /* ---------------------------------------------------------- import/export */

    Viewer.prototype.importFile = function () {
        var self = this, f = this.fileIn.files && this.fileIn.files[0];
        if (!f) return;
        /* 1.1.x wording: the side is asked with the in-page dialog (no browser popup). Cancel does nothing. */
        this.dialog({ title: 'Load lap file', html: '<p>Load "' + esc(f.name) + '" into car A or car B?</p>',
            buttons: [{ label: 'Car A', value: 'A' }, { label: 'Car B', value: 'B' }, { label: 'Cancel', value: 'cancel' }],
            safe: 'cancel' }).then(function (res) { if (res) self.importLoad(f, res.value); });
        this.fileIn.value = '';
    };
    /* Loads the chosen file into side letter ('A' or 'B'). Called once the dialog above has picked the side. */
    Viewer.prototype.importLoad = function (f, letter) {
        var self = this;
        var adopt = function (doc) {
            if (!self.wb.importInto(letter, doc)) { self.toast('Not loaded: this file is not a lap saved by Telemetry.'); return; }
            self.lapMode[letter] = 'pick';
            self.cache = {}; self.zoom = null; self.fillCarSelects(true); self.invalidate();
            self.toast('Imported into ' + letter + ': ' + (doc.driver || f.name));
        };
        if (/\.(lap|spt)$/i.test(f.name)) {
            var fr = new FileReader();
            fr.onload = function () {
                fetch('/trace/import/legacy?car=' + letter, { method: 'POST', body: fr.result,
                    headers: { 'Content-Type': 'application/octet-stream' } })
                    .then(function (r) { return r.json(); })
                    .then(function (doc) { if (doc && doc.error) self.toast('Import refused: ' + doc.error); else adopt(doc); })
                    .catch(function (e) { self.toast('Import failed: ' + e.message); });
            };
            fr.readAsArrayBuffer(f);
        } else {
            var rd = new FileReader();
            rd.onload = function () {
                try {
                    var doc = JSON.parse(String(rd.result));
                    if (doc && doc.schema === 'f1c.trace/1') adopt(doc);
                    else if (doc && doc.schema === 'f1c.lapset/1' && Array.isArray(doc.laps)) {
                        /* 1.2.3: the page's own export (several laps) loads into one side */
                        var got = self.wb.importSetInto(letter, doc.laps);
                        if (!got) { self.toast('Not loaded: this file has no complete lap.'); return; }
                        self.lapMode[letter] = 'pick';
                        self.cache = {}; self.zoom = null; self.fillCarSelects(true); self.invalidate();
                        var d0 = doc.laps.filter(function (l) { return l && l.driver; })[0];
                        self.toast('Imported ' + got + (got === 1 ? ' lap' : ' laps') + ' into ' + letter + ': ' + (d0 ? d0.driver : f.name));
                    }
                    else self.toast('Not loaded: this file is not a lap saved by Telemetry.');
                } catch (e) { self.toast('Import refused: ' + e.message); }
            };
            rd.readAsText(f, 'utf-8');
        }
    };

    /* C1 (2026-10-14): the game's official lap time when it arrived, else the
     * derived (browser or trace) time. Pure. */
    function officialLapPick(derived, official) {
        if (typeof official === 'number' && isFinite(official) && official > 0) return { time: official, src: 'game' };
        return { time: typeof derived === 'number' && isFinite(derived) ? derived : null, src: 'derived' };
    }

    /* Export of the displayed lap(s) in the canonical sample shape. */
    function exportDoc(side, pts) {
        var shown = side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown;
        var meta = side.meta || null;
        var complete = !!(shown && shown.complete);
        var derived = shown ? shown.end - shown.start : null;
        var pick = officialLapPick(derived, shown ? shown.official_time : undefined);
        var TRF = WB.TRF;
        var samples = pts.map(function (p) {
            var f = TRF.DIST;
            function on(bit, v) { if (v !== null && v !== undefined) f |= bit; return v === undefined ? null : v; }
            var s = {
                t: p.t, d: p.dist, s: on(TRF.SPEED, p.speed), th: on(TRF.THROTTLE, p.throttle), br: on(TRF.BRAKE, p.brake),
                st: on(TRF.STEER, p.steer), gl: on(TRF.GLAT, p.glat), go: on(TRF.GLON, p.glon), gv: on(TRF.GVERT, p.gvert),
                y: on(TRF.YAW, p.yaw), yr: null, rpm: on(TRF.RPM, p.rpm), ge: on(TRF.GEAR, p.gear),
                tx: on(TRF.TRACK, p.tx), tz: p.tx !== null && p.tx !== undefined ? p.tz : null,
                tr: p.tread ? p.tread.slice() : [0, 0, 0, 0], tt: p.ttemp ? p.ttemp.slice() : null, rx: null, f: 0
            };
            if (p.tread) f |= TRF.TREAD;
            if (p.ttemp) f |= TRF.TTEMP | (p.ttempResearch ? TRF.TTEMP_RX : 0);
            if (p.rx) {
                var rx = [];
                ['su', 'ld', 'sl', 'fo'].forEach(function (k, q) {
                    var v = p.rx[k];
                    if (v) f |= [TRF.RX_SU, TRF.RX_LD, TRF.RX_SL, TRF.RX_FO][q];
                    rx = rx.concat(v || [0, 0, 0, 0]);
                });
                s.rx = rx;
            }
            if (p.seg) f |= TRF.SEGMENT;
            s.f = f;
            return s;
        });
        return {
            schema: 'f1c.trace/1', source: 'live', open: !shown, gen: 0, rev: 0, which: side.letter, slot: side.slot,
            driver: meta ? cleanName(meta.n) : cleanName(side.label.split(' · ')[0]), car: meta ? meta.cn : '',
            ai: /· AI/.test(side.label) ? 1 : 0, team: teamOf(meta),
            lap: shown ? shown.lap : 0, lap_time: complete ? pick.time : null, lap_time_src: pick.src,
            derived_lap_time: derived,
            start_sim: shown ? shown.start : (pts[0] ? pts[0].t : 0), lap_len: shown ? shown.length || 0 : 0,
            count: samples.length, flags: shown && shown.complete ? 1 : 0, channels_seen: 0, track: '',
            lid: { gen: 0, rev: 0, slot: side.slot, lap: shown ? shown.lap : 0, src: 'viewer' },
            exported_by: 'telemetry_viewer ' + VERSION, samples: samples
        };
    }

    Viewer.prototype.exportShown = function () {
        var self = this;
        this.letters().forEach(function (L) {
            var side = self.side(L), pts = self.points(L);
            if (!side || !pts.length) return;
            var blob = new Blob([JSON.stringify(exportDoc(side, pts))], { type: 'application/json' });
            var a = el('a'); a.href = URL.createObjectURL(blob);
            a.download = 'f1c_trace_' + L + '_' + (side.label.split(' · ')[0] || 'car').replace(/[^A-Za-z0-9]+/g, '_') + '.json';
            document.body.appendChild(a); a.click(); a.remove();
            setTimeout(function () { URL.revokeObjectURL(a.href); }, 2000);
        });
    };

    Viewer.prototype.toast = function (msg) {
        var t = el('div', 'tv-tip', esc(msg));
        t.style.cssText = 'position:fixed;left:50%;bottom:40px;transform:translateX(-50%);display:block;font-size:12px;padding:8px 12px';
        document.body.appendChild(t);
        setTimeout(function () { t.remove(); }, 3200);
    };

    Viewer.prototype.showHelp = function () {
        var m = el('div', 'tv-modal');
        m.innerHTML = '<div><h3>F1C Telemetry Viewer</h3>' +
            '<p>Pick any cars (player or AI) for the sides; the server records every driver, so recorded laps appear as soon as a car is picked. Then choose a lap for each. ' +
            'Every chart shares one distance cursor. The delta trace is B minus A at the same distance (positive = B slower).</p>' +
            '<p><kbd>1</kbd>–<kbd>8</kbd> views · <kbd>D</kbd> distance/time axis · <kbd>R</kbd> research channels · <kbd>A</kbd>/<kbd>B</kbd> freeze side · <kbd>Esc</kbd> reset zoom</p>' +
            '<p>Tier badges: <span class="tier FACT">FACT</span> proven in the game code and live, <span class="tier CONFIRMED">CONFIRMED</span> proven with a small open question, ' +
            '<span class="tier DERIVED">DERIVED</span> computed by this viewer, <span class="tier RESEARCH">RESEARCH</span> candidate field whose meaning and unit are not proven yet.</p>' +
            '<p>Read-only: nothing here writes to the game.</p><p style="text-align:right"><button class="tv-btn">Close</button></p></div>';
        m.onclick = function (e) { if (e.target === m || e.target.tagName === 'BUTTON') m.remove(); };
        document.body.appendChild(m);
    };

    /* --------------------------------------------------------------- views */

    Viewer.prototype.renderView = function () {
        var v = this.view, host = this.viewEl;
        this.charts = []; this.canvases = []; this.onCursor = null;
        host.innerHTML = '';
        if (v === 'legacy') return this.renderLegacy();
        var fn = this['view_' + v];
        if (fn) fn.call(this, host);
        this.mapInset(host, false);
        this.dirty = true;
    };

    /* TAPE2 V4 (one shared inset): a page with graphs and no track map gets a small map in its lower-right corner, so
     * every graph shows the car balls and the hover ball on a map. force = the caller knows the page has no map (legacy). */
    Viewer.prototype.mapInset = function (host, force) {
        if (!host) return;
        /* Replay draws its own map of every car (rpPaintMap), so it never gets an inset. */
        if (!force && (this.view === 'replay' || !host.querySelector('canvas') || this.canvases.some(function (c) { return c.isMap; }))) return;
        var pane = panel('Track', '');
        pane.style.cssText = 'position:absolute;right:12px;bottom:12px;width:240px;height:190px;z-index:6;box-shadow:0 2px 10px #000;';
        host.appendChild(pane);
        this.trackMap(pane, {});
    };

    /* TAPE2 V4: a legacy layout's hover (distance in metres, or null when the pointer leaves) drives the shared cursor. */
    Viewer.prototype.setCursorDist = function (d) {
        if (d === null || d === undefined || !finite(d)) { this.cursor = null; this.cursorRaw = undefined; }
        else { this.cursor = d; this.cursorRaw = d; }
        this.drawCursors();
    };

    Viewer.prototype.refreshView = function () {
        if (this.refreshers) this.refreshers.forEach(function (f) { f(); });
        this.redrawCharts();
    };

    Viewer.prototype.grid = function (host, cols, rows) {
        var g = el('div', 'tv-grid');
        g.style.gridTemplateColumns = cols;
        g.style.gridTemplateRows = rows;
        host.appendChild(g);
        this.refreshers = [];
        return g;
    };

    /* One descriptor for every active side. Expanded by expandSeries() at draw
     * time, so charts follow the sides picked in the console. get(d, data, i, L). */
    function both(get, opt) {
        var o = opt || {};
        return [{ side: '*', get: get, width: o.width }];
    }

    /* Replaces every '*' descriptor by one series per active side (letter L,
     * colour, label). Series for a fixed side (e.g. B - A) pass through. */
    Viewer.prototype.expandSeries = function (list) {
        var self = this, out = [];
        list.forEach(function (s) {
            if (s.side !== '*') { out.push(s); return; }
            self.letters().forEach(function (L) {
                if (s.noA && L === 'A') return;   /* delta-vs-A series skip the reference */
                var c = {};
                Object.keys(s).forEach(function (k) { c[k] = s[k]; });
                c.side = L; c.lbl = s.lblWheel ? L + ' ' + s.lblWheel : L;
                if (s.get) c.get = function (d, data, i) { return s.get(d, data, i, L); };
                out.push(c);
            });
        });
        return out;
    };

    Viewer.prototype.deltaSeries = function (L) {
        /* L - A at L's sample distances, using the shared DM#14 lookup. */
        L = L || 'B';
        var pa = this.points('A'), pb = this.points(L);
        var key = 'delta:' + L, c = this.cache[key];
        var sig = pa.length + ':' + pb.length + ':' + (pa[pa.length - 1] || {}).t + ':' + (pb[pb.length - 1] || {}).t + ':' + this.xMode;
        if (c && c.sig === sig) return c.data;
        var d = WB.dtimeSeries(pb, pa);
        var sb = this.seriesFor(L);
        var out = sb.map(function (q, i) {
            return { x: q.x, el: q.el, seg: q.seg, delta: d[i] === undefined ? null : d[i] };
        });
        this.cache[key] = { sig: sig, data: out };
        return out;
    };

    Viewer.prototype.strip = function (p, tracks) {
        var ch = new StripChart(p.body, this, tracks);
        this.charts.push(ch);
        return ch;
    };

    /* Track map canvas: driving line of A coloured by mini-sector winner,
     * cursor markers for A and B. */
    Viewer.prototype.trackMap = function (p, opts) {
        var self = this, o = opts || {};
        var base = el('canvas'), over = el('canvas');
        p.body.appendChild(base); p.body.appendChild(over);
        var view = null;
        var obj = {
            draw: function () {
                var c = setupCanvas(base), ctx = c.ctx;
                var sa = self.seriesFor('A'), sb = self.seriesFor('B');
                var line = sa.filter(function (q) { return q.tx !== null; });
                if (line.length < 10) line = sb.filter(function (q) { return q.tx !== null; });
                var wp = self.waypoints;
                var pts = [];
                if (wp && wp.length > 10) wp.forEach(function (w) {
                    var x = Array.isArray(w) ? w[0] : w.x, z = Array.isArray(w) ? w[1] : w.z;
                    if (finite(x) && finite(z)) pts.push([x, z]);
                });
                var all = pts.concat(line.map(function (q) { return [q.tx, q.tz]; }));
                if (all.length < 5) {
                    ctx.fillStyle = PAL.axis; ctx.font = '12px ' + PAL.sans; ctx.textAlign = 'center';
                    ctx.fillText('Track map appears once a car has driven part of a lap.', c.w / 2, c.h / 2);
                    view = null; return;
                }
                var minx = Infinity, maxx = -Infinity, minz = Infinity, maxz = -Infinity;
                all.forEach(function (q) { minx = Math.min(minx, q[0]); maxx = Math.max(maxx, q[0]); minz = Math.min(minz, q[1]); maxz = Math.max(maxz, q[1]); });
                var pad = 18, sc = Math.min((c.w - 2 * pad) / (maxx - minx || 1), (c.h - 2 * pad) / (maxz - minz || 1));
                var ox = (c.w - (maxx - minx) * sc) / 2, oz = (c.h - (maxz - minz) * sc) / 2;
                view = function (x, z) { return [ox + (x - minx) * sc, c.h - (oz + (z - minz) * sc)]; };
                if (pts.length) {
                    ctx.strokeStyle = PAL.track; ctx.lineWidth = 9; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
                    ctx.beginPath(); pts.forEach(function (q, i) { var v = view(q[0], q[1]); if (i) ctx.lineTo(v[0], v[1]); else ctx.moveTo(v[0], v[1]); });
                    ctx.closePath(); ctx.stroke();
                }
                /* mini-sector colouring */
                var len = self.wb.trackLen;
                var ms = o.sectors && len > 0 ? miniSectors(self.points('A'), self.points('B'), len, 40, self.lapBounds('A'), self.lapBounds('B')) : null;
                ctx.lineWidth = 3.2; ctx.lineCap = 'round';
                for (var i = 1; i < line.length; i++) {
                    var a = line[i - 1], b = line[i];
                    if (b.seg) continue;
                    var col = PAL.lineDim;
                    if (ms && a.x !== null) {
                        var k = Math.min(39, Math.floor(a.x / len * 40));
                        var g = ms[k] && ms[k].gain;
                        if (g !== null && g !== undefined) col = g > 0.005 ? COLORS.A : g < -0.005 ? COLORS.B : PAL.line;
                    } else if (!ms) col = PAL.line;
                    var va = view(a.tx, a.tz), vb = view(b.tx, b.tz);
                    ctx.strokeStyle = col;
                    ctx.beginPath(); ctx.moveTo(va[0], va[1]); ctx.lineTo(vb[0], vb[1]); ctx.stroke();
                }
                /* every other side's driving line, thin, in its own colour (mini-sector
                 * colouring above stays A against B) */
                self.letters().forEach(function (L) {
                    if (L === 'A') return;
                    var sl = self.seriesFor(L), prev = null;
                    ctx.strokeStyle = COLORS[L]; ctx.lineWidth = 2; ctx.globalAlpha = 0.85;
                    ctx.beginPath();
                    for (var k = 0; k < sl.length; k++) {
                        var q = sl[k];
                        if (q.tx === null || q.tz === null || q.seg) { prev = null; continue; }
                        var vq = view(q.tx, q.tz);
                        if (prev) ctx.lineTo(vq[0], vq[1]); else ctx.moveTo(vq[0], vq[1]);
                        prev = vq;
                    }
                    ctx.stroke(); ctx.globalAlpha = 1;
                });
                /* start line */
                var s0 = line.filter(function (q) { return q.x !== null && q.x < 30; })[0];
                if (s0) { var v0 = view(s0.tx, s0.tz); ctx.fillStyle = PAL.start; ctx.fillRect(v0[0] - 2, v0[1] - 6, 4, 12); }
                /* zone markers */
                if (o.zones) {
                    var zs = self.zones();
                    ctx.font = '600 10px Consolas, monospace'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
                    zs.forEach(function (z, n) {
                        var q = nearestIndex(sa, z.start);
                        if (q < 0 || sa[q].tx === null) return;
                        var vv = view(sa[q].tx, sa[q].tz);
                        ctx.fillStyle = n === self.zoneSel ? '#22d3ee' : '#0f141c';
                        ctx.strokeStyle = '#22d3ee'; ctx.lineWidth = 1;
                        ctx.beginPath(); ctx.arc(vv[0], vv[1], 8, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
                        ctx.fillStyle = n === self.zoneSel ? '#061018' : '#e6edf5';
                        ctx.fillText(String(n + 1), vv[0], vv[1] + 0.5);
                    });
                }
                self.drawSideBalls(ctx, view);   /* TAPE2 V7: the car of each side at its frame position, always */
                obj.cursor();
            },
            cursor: function () {
                var c = setupCanvas(over), ctx = c.ctx;
                if (!view || self.cursor === null) return;
                self.letters().slice().reverse().forEach(function (L) {
                    var s = self.seriesFor(L);
                    var x = L === 'A' || self.xMode !== 'time' ? self.cursor : self.distAtTime(L, self.cursorRaw);
                    var tx = valueAt(s, x, function (q) { return q.tx; }), tz = valueAt(s, x, function (q) { return q.tz; });
                    if (tx === null || tz === null) return;
                    var v = view(tx, tz);
                    /* TAPE2 V4: the hover ball is a ring (the frame ball is filled), so both can be on the map at once. */
                    ctx.strokeStyle = COLORS[L]; ctx.lineWidth = 3;
                    ctx.beginPath(); ctx.arc(v[0], v[1], 9, 0, Math.PI * 2); ctx.stroke();
                    ctx.strokeStyle = PAL.bg; ctx.lineWidth = 1;
                    ctx.beginPath(); ctx.arc(v[0], v[1], 9, 0, Math.PI * 2); ctx.stroke();
                });
            }
        };
        /* Hover sets the shared cursor to the nearest sample of side A's displayed
         * lap (within 40 px on the map). Leaving the map clears the cursor. */
        over.addEventListener('mousemove', function (e) {
            if (!view) return;
            var sa = self.seriesFor('A'), r = base.getBoundingClientRect();
            var i = nearestOnScreen(sa, view, e.clientX - r.left, e.clientY - r.top, 40);
            if (i < 0) return;
            var xv = self.xMode === 'time' ? sa[i].el : sa[i].x;
            if (xv === null || xv === undefined) return;
            self.setCursor(xv, 'map');
        });
        over.addEventListener('mouseleave', function () {
            self.cursor = null; self.cursorRaw = undefined; self.drawCursors();
        });
        obj.isMap = true;   /* TAPE2 V4: a page with a map does not get the inset */
        this.canvases.push(obj);
        return obj;
    };

    /* G-G diagram with friction rings, A/B scatter and the per-sector
     * envelope (max combined G) that shows where a setup gains grip. */
    Viewer.prototype.ggPlot = function (p) {
        var self = this, base = el('canvas'), over = el('canvas');
        p.body.appendChild(base); p.body.appendChild(over);
        var map = null;
        var obj = {
            draw: function () {
                var c = setupCanvas(base), ctx = c.ctx;
                var sets = self.letters().map(function (L) { return { L: L, s: self.seriesFor(L) }; });
                var z = self.zoomRange();
                var inZ = function (q) { var x = self.xMode === 'time' ? q.el : q.x; return x !== null && x >= z[0] && x <= z[1]; };
                var maxg = 2;
                sets.forEach(function (pr) { pr.s.forEach(function (q) { if (q.comb !== null && inZ(q)) maxg = Math.max(maxg, q.comb); }); });
                maxg = Math.ceil(maxg + 0.3);
                var r = Math.min(c.w, c.h) / 2 - 26, cx = c.w / 2, cy = c.h / 2;
                /* 4.4.0-rc.6 (owner O3 FACT): glat > 0 = LEFT turn, so left of centre. */
                map = function (lat, lon) { return [cx - lat / maxg * r, cy - lon / maxg * r]; };
                ctx.strokeStyle = PAL.ring; ctx.fillStyle = PAL.axis; ctx.font = '10px ' + PAL.mono;
                ctx.textAlign = 'left'; ctx.textBaseline = 'bottom';
                for (var g = 1; g <= maxg; g++) {
                    ctx.beginPath(); ctx.arc(cx, cy, g / maxg * r, 0, Math.PI * 2); ctx.stroke();
                    ctx.fillText(g + ' g', cx + g / maxg * r * 0.707 + 3, cy - g / maxg * r * 0.707);
                }
                ctx.beginPath(); ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy); ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r); ctx.stroke();
                ctx.fillStyle = PAL.axis; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
                ctx.fillText('ACCEL', cx, cy - r - 12); ctx.fillText('BRAKE', cx, cy + r + 12);
                ctx.textAlign = 'left'; ctx.fillText('RIGHT', cx + r + 4, cy - 8);
                ctx.textAlign = 'right'; ctx.fillText('LEFT', cx - r - 4, cy - 8);
                sets.slice().reverse().forEach(function (pr) {
                    var pair = [pr.L, pr.s];
                    ctx.fillStyle = COLORS[pair[0]]; ctx.globalAlpha = 0.28;
                    pair[1].forEach(function (q) {
                        if (q.lat === null || q.lon === null || !inZ(q)) return;
                        var v = map(q.lat, q.lon); ctx.fillRect(v[0] - 1.2, v[1] - 1.2, 2.4, 2.4);
                    });
                    ctx.globalAlpha = 1;
                    var env = ggEnvelope(pair[1].filter(inZ), 48);
                    ctx.strokeStyle = COLORS[pair[0]]; ctx.lineWidth = 2;
                    ctx.beginPath();
                    var started = false;
                    env.forEach(function (e) {
                        if (e.g === null) { started = false; return; }
                        var v = map(Math.cos(e.ang) * e.g, Math.sin(e.ang) * e.g);
                        if (!started) { ctx.moveTo(v[0], v[1]); started = true; } else ctx.lineTo(v[0], v[1]);
                    });
                    ctx.stroke();
                });
                obj.cursor();
            },
            cursor: function () {
                var c = setupCanvas(over), ctx = c.ctx;
                if (!map || self.cursor === null) return;
                self.letters().forEach(function (L) {
                    var s = self.seriesFor(L);
                    var lat = valueAt(s, self.cursor, function (q) { return q.lat; });
                    var lon = valueAt(s, self.cursor, function (q) { return q.lon; });
                    if (lat === null || lon === null) return;
                    var v = map(lat, lon);
                    ctx.strokeStyle = PAL.bg; ctx.lineWidth = 2; ctx.fillStyle = COLORS[L];
                    ctx.beginPath(); ctx.arc(v[0], v[1], 6, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
                });
            }
        };
        this.canvases.push(obj);
        return obj;
    };

    Viewer.prototype.zones = function () {
        var ref = this.seriesFor('A');
        if (ref.filter(function (q) { return q.x !== null; }).length < 30) ref = this.seriesFor('B');
        var key = 'zones', c = this.cache[key], sig = ref.length + ':' + (ref.length ? ref[ref.length - 1].t : '');
        if (c && c.sig === sig) return c.data;
        var z = brakingZones(ref);
        this.cache[key] = { sig: sig, data: z };
        return z;
    };

    Viewer.prototype.empty = function (host, title, text) {
        host.appendChild(el('div', 'tv-empty', '<div><b>' + esc(title) + '</b>' + esc(text) + '</div>'));
    };

    /* ---- Overview ---- */
    Viewer.prototype.view_overview = function (host) {
        var self = this;
        /* TAPE2 e2e (Claude 2026-10-15): the map row keeps 180 px; at 1280x720 the tall summary used to take the whole
         * column (map 0 px high). The summary scrolls instead. */
        var g = this.grid(host, 'minmax(0,1fr) minmax(300px,30%)', 'minmax(180px,1fr) minmax(0,auto)');
        var ch = panel('Lap trace', null);
        ch.style.gridRow = '1 / 3';
        g.appendChild(ch);
        var map = panel('Track · mini-sector winner', 'DERIVED');
        g.appendChild(map);
        var kp = panel('Lap summary', null);
        kp.body.classList.add('pad');
        kp.style.minHeight = '0'; kp.style.overflow = 'hidden'; kp.body.style.overflow = 'auto';
        g.appendChild(kp);
        var delta = this.deltaSeries.bind(this);
        this.strip(ch, [
            { label: 'Speed', unit: 'km/h', tier: 'FACT', weight: 2.4, series: both(function (d) { return d.kmh; }), fmt: function (v) { return v.toFixed(1); } },
            { label: 'Delta vs A', unit: 's', tier: 'DERIVED', weight: 1.1, zero: true,
              series: [{ side: '*', noA: true, get: function (d, data, i, L) { var ds = delta(L); return ds[i] ? ds[i].delta : null; } }],
              fmt: function (v) { return sgn(v, 3); } },
            { label: 'Throttle', unit: '%', tier: 'CONFIRMED', weight: 1, range: [-5, 105], series: both(function (d) { return d.thr; }), fmt: function (v) { return v.toFixed(0); } },
            { label: 'Brake', unit: '%', tier: 'CONFIRMED', weight: 1, range: [-5, 105], series: both(function (d) { return d.brk; }), fmt: function (v) { return v.toFixed(0); } },
            { label: 'Gear', unit: '', tier: 'FACT', weight: 0.8, range: [-0.5, 7.5], series: both(function (d) { return d.gear; }), fmt: function (v) { return v.toFixed(0); } },
            { label: 'RPM', unit: 'rpm', tier: 'FACT', weight: 1.1, series: both(function (d) { return d.rpm; }), fmt: function (v) { return WB.formatRpmValue(v); } },
            { label: 'Steering (AI input factor)', unit: '', tier: 'FACT', weight: 0.9, sym: true, series: both(function (d) { return d.steer; }), fmt: function (v) { return v.toFixed(3); } }
        ]);
        this.trackMap(map, { sectors: true });
        this.refreshers.push(function () { kp.body.innerHTML = self.kpiHtml(); });
    };

    /* CO-OP summary over a car list of {cr, a}: remote cars (cr 2) split by humanRole(): HUMAN = remote human, no badge =
     * remote car driven by the host's AI. Pure. E2E 2026-10-15: counts every remote car, as the tower badge does (no p > 0 gate). */
    function coopSummary(cars) {
        var humans = 0, bots = 0;
        (cars || []).forEach(function (c) {
            if (!c || c.cr !== 2) return;
            if (humanRole(c) === 'HUMAN') humans++; else bots++;
        });
        if (!humans && !bots) return '';
        return (bots ? 'client' : 'host') + ' · ' + humans + ' remote human' + (humans === 1 ? '' : 's') +
            (bots ? ' · ' + bots + ' host-driven AI' : '');
    }
    /* CO-OP summary: role of this PC and remote counts. E2E 2026-10-15: the same car list as the tower. In PAST that is the
     * replay info roles (rpHumanCar); the tape frame's a is 1 for every non-player car, so it cannot show a remote human. */
    Viewer.prototype.coopInfo = function () {
        if (this.source === 'past') {
            var info = this.tape && this.tape.info;
            return info && Array.isArray(info.cars) ? coopSummary(info.cars.map(function (c) { return rpHumanCar(c); })) : '';
        }
        var j = this.frame;
        if (!j || !Array.isArray(j.c)) return '';
        return coopSummary(j.c);
    };

    /* Lap Analysis KPIs: a table with one column per active side. Non-A
     * columns show the difference to A (B - A for B, as before). */
    Viewer.prototype.kpiHtml = function () {
        var self = this, letters = this.letters(), st = {}, lap = {};
        letters.forEach(function (L) { st[L] = lapStats(self.seriesFor(L)); lap[L] = self.lapTime(L); });
        var defs = [
            ['Lap time', function (L) { return lap[L]; }, function (v) { return v !== null ? lapText(v) : 'open'; }, 'low', true],
            ['Top speed km/h', function (L) { return st[L].top; }, function (v) { return fx(v, 1); }, 'high', true],
            ['Average km/h', function (L) { return st[L].avgKmh; }, function (v) { return fx(v, 1); }, 'high', true],
            ['Full throttle %', function (L) { return st[L].fullThr; }, function (v) { return fx(v, 1); }, 'high', true],
            ['Braking %', function (L) { return st[L].braking; }, function (v) { return fx(v, 1); }, null, false],
            ['Coasting %', function (L) { return st[L].coasting; }, function (v) { return fx(v, 1); }, null, false],
            ['Peak lateral g', function (L) { return st[L].peakLat; }, function (v) { return fx(v, 2); }, 'high', true],
            ['Peak braking g', function (L) { return st[L].peakDecel !== null ? -st[L].peakDecel : null; }, function (v) { return fx(v, 2); }, null, false],
            ['Gear changes', function (L) { return st[L].shifts; }, function (v) { return v === null || v === undefined ? '—' : String(v); }, null, false]
        ];
        var td = 'padding:5px 9px;border-bottom:1px solid var(--line);text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap';
        var h = '<div style="overflow:auto"><table style="width:100%;border-collapse:collapse;font:600 13px var(--mono)"><thead><tr>' +
            '<th style="' + td + ';text-align:left;color:var(--tx3);font-weight:600">Lap analysis</th>' +
            letters.map(function (L) {
                return '<th style="' + td + ';color:' + COLORS[L] + '">' + L + '<div style="font:500 10px var(--mono);color:var(--tx3);white-space:normal;max-width:180px">' +
                    esc(self.side(L).label) + '</div></th>';
            }).join('') + '</tr></thead><tbody>';
        defs.forEach(function (d) {
            var base = d[1]('A');
            h += '<tr><td style="' + td + ';text-align:left;color:var(--tx3)">' + d[0] + '</td>' + letters.map(function (L) {
                var v = d[1](L), txt = d[2](v), extra = '';
                if (d[4] && L !== 'A') {
                    var dv = finite(v) && finite(base) ? v - base : NaN;
                    if (finite(dv)) {
                        var good = d[3] === 'low' ? dv < 0 : dv > 0;
                        extra = ' <span class="' + (good ? 'pos' : 'neg') + '" style="font-weight:500;font-size:11px">' +
                            sgn(dv, Math.abs(dv) < 10 ? 2 : 1) + '</span>';
                    }
                }
                return '<td style="' + td + '">' + txt + extra + '</td>';
            }).join('') + '</tr>';
        });
        h += '</tbody></table></div>';
        return h + this.metaHtml();
    };

    /* ---- Vehicle dynamics ---- */
    Viewer.prototype.view_dynamics = function (host) {
        var g = this.grid(host, 'minmax(320px,38%) minmax(0,1fr)', 'minmax(0,1fr) minmax(0,1fr)');
        var gg = panel('G-G diagram · grip envelope', 'FACT');
        gg.style.gridRow = '1 / 3';
        g.appendChild(gg);
        var tr = panel('Accelerations and yaw', null);
        tr.style.gridRow = '1 / 3';
        g.appendChild(tr);
        this.ggPlot(gg);
        this.strip(tr, [
            { label: 'Speed', unit: 'km/h', tier: 'FACT', weight: 1.4, series: both(function (d) { return d.kmh; }) },
            { label: 'Lateral (+ LEFT / - RIGHT)', unit: 'g', tier: 'FACT', weight: 1.4, sym: true, series: both(function (d) { return d.lat; }), fmt: function (v) { return v.toFixed(2); } },
            { label: 'Longitudinal (accel +)', unit: 'g', tier: 'FACT', weight: 1.4, sym: true, series: both(function (d) { return d.lon; }), fmt: function (v) { return v.toFixed(2); } },
            { label: 'Combined', unit: 'g', tier: 'DERIVED', weight: 1.1, zero: true, series: both(function (d) { return d.comb; }), fmt: function (v) { return v.toFixed(2); } },
            { label: 'Yaw rate (+ right turn)', unit: '°/s', tier: 'DERIVED', weight: 1.2, sym: true, series: both(function (d) { return d.yawRate; }), fmt: function (v) { return v.toFixed(1); } },
            { label: 'Vertical', unit: 'g', tier: 'FACT', weight: 1, series: both(function (d) { return d.vert; }), fmt: function (v) { return v.toFixed(2); } }
        ]);
    };

    /* ---- Braking & corners ---- */
    Viewer.prototype.view_braking = function (host) {
        var self = this;
        var g = this.grid(host, 'minmax(0,1fr) minmax(280px,30%)', 'minmax(240px,42%) minmax(0,1fr)');
        var tbl = panel('Braking zones · corner by corner', 'DERIVED');
        tbl.body.classList.add('pad');
        g.appendChild(tbl);
        var map = panel('Track · zones', null);
        g.appendChild(map);
        var ch = panel('Selected zone', null);
        ch.style.gridColumn = '1 / 3';
        g.appendChild(ch);
        this.trackMap(map, { zones: true, sectors: true });
        var bands = function () { return self.zones().map(function (z) { return [z.start, z.end]; }); };
        this.strip(ch, [
            { label: 'Speed', unit: 'km/h', tier: 'FACT', weight: 2, series: both(function (d) { return d.kmh; }), bands: bands },
            { label: 'Brake', unit: '%', tier: 'CONFIRMED', weight: 1, range: [-5, 105], series: both(function (d) { return d.brk; }), bands: bands },
            { label: 'Throttle', unit: '%', tier: 'CONFIRMED', weight: 1, range: [-5, 105], series: both(function (d) { return d.thr; }) },
            { label: 'Longitudinal (accel +)', unit: 'g', tier: 'FACT', weight: 1.3, sym: true, series: both(function (d) { return d.lon; }), fmt: function (v) { return v.toFixed(2); } },
            { label: 'Gear', unit: '', tier: 'FACT', weight: 0.8, range: [-0.5, 7.5], series: both(function (d) { return d.gear; }), fmt: function (v) { return v.toFixed(0); } }
        ]);
        var render = function () {
            var zs = self.zones();
            if (!zs.length) {
                tbl.body.innerHTML = '<div class="tv-empty"><div><b>No braking zones yet</b>Zones are detected from the brake trace (≥ 10 %) of side A once part of a lap is recorded.</div></div>';
                return;
            }
            var letters = self.letters(), others = letters.slice(1), ser = {}, pts = {};
            letters.forEach(function (L) { ser[L] = self.seriesFor(L); pts[L] = self.points(L); });
            /* one value per side, in the side colour; ' / ' separated in letter order */
            function per(m, f) {
                return letters.map(function (L) {
                    return '<span style="color:' + COLORS[L] + '">' + f(m[L]) + '</span>';
                }).join(' / ');
            }
            var h = '<table class="tv-t"><thead><tr><th>Zone</th><th>Brake pt</th><th>Brake pt vs A</th><th>Entry km/h</th><th>Peak decel g</th><th>Min km/h</th><th>Throttle-on</th><th>Exit km/h</th><th>Time Δ vs A</th></tr></thead><tbody>';
            zs.forEach(function (z, i) {
                var nx = zs[i + 1] ? zs[i + 1].start : undefined, m = {};
                letters.forEach(function (L) { m[L] = cornerMetrics(ser[L], pts[L], z, nx); });
                var bpdTxt = others.map(function (L) {
                    var bpd = m[L].brakePoint !== null && m.A.brakePoint !== null ? m[L].brakePoint - m.A.brakePoint : null;
                    return '<span style="color:' + COLORS[L] + '">' + L + ' ' + (bpd === null ? '—' : sgn(bpd, 0, 'm') + (bpd > 0 ? ' later' : bpd < 0 ? ' earlier' : '')) + '</span>';
                }).join('<br>') || '—';
                var dtTxt = others.map(function (L) {
                    var dt = m[L].time !== null && m.A.time !== null ? m[L].time - m.A.time : null;
                    return '<span class="' + (dt === null ? '' : dt > 0 ? 'neg' : 'pos') + '" style="' + (dt === null ? '' : '') + '">' + L + ' ' + sgn(dt, 3, 's') + '</span>';
                }).join('<br>') || '—';
                h += '<tr data-i="' + i + '"' + (i === self.zoneSel ? ' class="sel"' : '') + '><td>T' + (i + 1) + ' <span style="color:var(--tx3)">' + Math.round(z.start) + ' m</span></td>' +
                    '<td>' + per(m, function (q) { return fx(q.brakePoint, 0, 'm'); }) + '</td>' +
                    '<td>' + bpdTxt + '</td>' +
                    '<td>' + per(m, function (q) { return fx(q.entryKmh, 0); }) + '</td>' +
                    '<td>' + per(m, function (q) { return fx(q.peakDecel !== null ? -q.peakDecel : null, 2); }) + '</td>' +
                    '<td>' + per(m, function (q) { return fx(q.minKmh, 1); }) + '</td>' +
                    '<td>' + per(m, function (q) { return fx(q.throttleOn, 0); }) + '</td>' +
                    '<td>' + per(m, function (q) { return fx(q.exitKmh, 0); }) + '</td>' +
                    '<td>' + dtTxt + '</td></tr>';
            });
            h += '</tbody></table><div style="color:var(--tx3);font-size:11px;margin-top:8px">Time Δ = each side minus A from 150 m before the zone to 150 m after the apex (negative: that side faster than A). Click a row to zoom.</div>';
            tbl.body.innerHTML = h;
            Array.prototype.forEach.call(tbl.body.querySelectorAll('tbody tr'), function (tr) {
                tr.onclick = function () {
                    self.zoneSel = parseInt(tr.getAttribute('data-i'), 10);
                    var z = zs[self.zoneSel], w = cornerMetrics(ser.A, pts.A, z, zs[self.zoneSel + 1] ? zs[self.zoneSel + 1].start : undefined).window;
                    if (self.xMode === 'time') self.toggleX();
                    self.setZoom(w[0] - 60, w[1] + 60);
                    self.invalidate();
                };
            });
        };
        this.refreshers.push(render);
    };

    /* ---- Tyres ---- */
    Viewer.prototype.view_tyres = function (host) {
        var self = this;
        var g = this.grid(host, 'minmax(0,1fr) minmax(0,1fr)', 'minmax(250px,40%) minmax(0,1fr)');
        /* one card per active side, in a box that scrolls past two sides */
        var cardsPanel = panel('Tyres', null);
        cardsPanel.body.classList.add('pad');
        cardsPanel.body.style.overflow = 'auto';
        var cardsBox = el('div');
        cardsBox.style.cssText = 'display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:10px;min-height:0';
        cardsPanel.body.appendChild(cardsBox);
        g.appendChild(cardsPanel);
        var cardEls = {}, cardsSig = null;
        function ensureCards() {
            var sig = self.letters().join('');
            if (sig === cardsSig) return;
            cardsSig = sig; cardsBox.innerHTML = ''; cardEls = {};
            self.letters().forEach(function (L) {
                var c = el('div');
                c.style.cssText = 'border:1px solid var(--line);border-top:2px solid ' + COLORS[L] + ';padding:6px;min-width:0';
                c.innerHTML = '<div style="font:600 11px var(--sans);letter-spacing:.06em;color:' + COLORS[L] + ';margin-bottom:4px">SIDE ' + L + '</div><div class="tyre-body"></div>';
                cardsBox.appendChild(c);
                cardEls[L] = c.querySelector('.tyre-body');
            });
        }
        var ch = panel('Tyre temperature and tread', null);
        g.appendChild(ch);
        var wear = panel('Wear per lap', 'DERIVED');
        wear.body.classList.add('pad');
        g.appendChild(wear);
        var tracks = [];
        WHEELS.forEach(function (w, k) {
            tracks.push({ label: w + ' temp', unit: '°C', tier: 'FACT', weight: 1,
                series: both(function (d) { return d.ttemp && (!d.ttempResearch || self.research) ? d.ttemp[k] : null; }),
                fmt: function (v) { return v.toFixed(1); } });
        });
        tracks.push({ label: 'Tread remaining (all four)', unit: '%', tier: 'FACT', weight: 1.6, series: [].concat.apply([], WHEELS.map(function (w, k) {
            return [{ side: '*', alpha: 1 - k * 0.18, width: 1.3, lblWheel: w, get: function (d, data, i, L) { return d.tread ? d.tread[k] * 100 : null; } }];
        })), fmt: function (v) { return v.toFixed(2); } });
        this.strip(ch, tracks);
        var cardHtml = function (L) {
            var s = self.seriesFor(L);
            var cur = null;
            if (self.cursor !== null) { var i = nearestIndex(s, self.cursor); if (i >= 0) cur = s[i]; }
            if (!cur) for (var j = s.length - 1; j >= 0; j--) if (s[j].x !== null) { cur = s[j]; break; }
            if (!cur) return '<div class="tv-empty"><div><b>No samples</b>Select a car that is on track.</div></div>';
            var researchT = cur.ttempResearch;
            var showT = cur.ttemp && (!researchT || self.research);
            /* H3: AI cars have no valid tyre temperature in the game; no Surface row for them. */
            var sideAI = /· AI/.test((self.side(L) || {}).label || '');
            var html = '<div class="wheel-grid">';
            WHEELS.forEach(function (w, k) {
                var t = showT ? cur.ttemp[k] : null;
                var tread = cur.tread ? cur.tread[k] * 100 : null;
                var carc = cur.tcarc ? cur.tcarc[k] : null;
                /* Three-zone stripes only for the live player row (FACT);
                 * otherwise one stripe for the recorded mean. */
                var zones = self.liveZones(L, k) || '<span style="background:' + tempColor(t) + '"></span>';
                var pos = k === 0 ? 'grid-column:1;grid-row:1' : k === 1 ? 'grid-column:3;grid-row:1' : k === 2 ? 'grid-column:1;grid-row:2' : 'grid-column:3;grid-row:2';
                html += '<div class="wheel" style="' + pos + '"><div class="h"><span>' + w + '</span><span>' +
                    (showT ? (researchT ? '<span class="tier RESEARCH">RESEARCH</span>' : '<span class="tier FACT">FACT</span>') : '') + '</span></div>' +
                    '<div class="tt">' + zones + '</div>' +
                    (sideAI ? '' : '<div class="row"><span>Surface</span><b>' + (showT ? fx(t, 1, '°C') : (cur.ttemp && researchT ? 'research only' : 'not available')) + '</b></div>') +
                    '<div class="row"><span>Carcass</span><b>' + fx(carc, 1, '°C') + '</b></div>' +
                    '<div class="row"><span>Tread</span><b>' + fx(tread, 1, '%') + '</b></div>' +
                    '<div class="bar"><i style="width:' + (finite(tread) ? Math.max(0, Math.min(100, tread)) : 0) + '%;background:' + (tread > 50 ? 'var(--good)' : tread > 25 ? 'var(--warn)' : 'var(--bad)') + '"></i></div>' +
                    '</div>';
            });
            html += '<div class="chassis">' + esc(self.side(L).label.split(' · ')[0]) + '</div></div>';
            if (sideAI) html += '<div class="w" style="color:#5f7fc4">AI tyre temperature: not available from the game (player only)</div>';
            return html;
        };
        var wearHtml = function () {
            var h = '';
            self.letters().forEach(function (L) {
                var side = self.side(L), rows = lapWear(side ? side.displayLaps() : []);
                h += '<div style="margin-bottom:10px"><div style="font:600 11px var(--sans);letter-spacing:.06em;color:var(--' + L.toLowerCase() + ');margin-bottom:4px">SIDE ' + L + '</div>';
                if (!rows.length) { h += '<div style="color:var(--tx3);font-size:12px">Needs at least one completed lap with tread data.</div></div>'; return; }
                h += '<table class="tv-t"><thead><tr><th>Lap</th><th>Time</th>' + WHEELS.map(function (w) { return '<th>' + w + ' used</th>'; }).join('') + '<th>Laps left*</th></tr></thead><tbody>';
                rows.forEach(function (r) {
                    var worst = Math.max.apply(null, r.used.map(function (u) { return finite(u) ? u : 0; }));
                    var minLeft = Math.min.apply(null, r.end.map(function (v) { return v * 100; }));
                    h += '<tr><td>' + r.lap + (r.complete ? '' : ' <span style="color:var(--tx3)">partial</span>') + '</td><td>' + lapText(r.time) + '</td>' +
                        r.used.map(function (u) { return '<td>' + fx(u, 3, '%') + '</td>'; }).join('') +
                        '<td>' + (worst > 0.0005 ? fx(minLeft / worst, 0) : '—') + '</td></tr>';
                });
                h += '</tbody></table></div>';
            });
            return h + '<div style="color:var(--tx3);font-size:11px">* Remaining tread of the most worn tyre divided by that lap’s worst-wheel wear. Wheel order for AI cars follows F1CUI labels (CONFIRMED, order not independently proven).</div>';
        };
        var cardsRender = function () {
            ensureCards();
            self.letters().forEach(function (L) { cardEls[L].innerHTML = cardHtml(L); });
        };
        var render = function () {
            cardsRender();
            wear.body.innerHTML = wearHtml();
        };
        this.refreshers.push(render);
        this.onCursor = cardsRender;
    };

    /* Player three-zone surface temperatures from the live frame (FACT).
     * Only for the live player row and only while showing the open lap. */
    Viewer.prototype.liveZones = function (L, k) {
        var side = this.side(L), j = this.frame;
        if (!j || !side || side.slot < 0 || side.shown || side.frozenSnapshot) return null;
        var car = j.c.filter(function (c) { return c.id === side.slot; })[0];
        var local = car && (car.cr === 0 || (car.cr === undefined && !car.a));
        if (!car || !local || !Array.isArray(j.ttmp) || !Array.isArray(j.ttmp[k])) return null;
        var z = j.ttmp[k];
        if (!z.every(function (v) { return finite(v) && v > -900; })) return null;
        return z.map(function (v) { return '<span title="' + v.toFixed(1) + ' °C" style="background:' + tempColor(v) + '"></span>'; }).join('');
    };

    /* ---- Compare ---- */
    Viewer.prototype.view_compare = function (host) {
        var self = this;
        var g = this.grid(host, 'minmax(0,1fr) minmax(0,1fr)', 'auto minmax(260px,1fr) minmax(0,1fr)');
        var kp = panel('Head to head', null); kp.body.classList.add('pad'); kp.style.gridColumn = '1 / 3';
        g.appendChild(kp);
        var sec = panel('Sectors and mini-sectors', 'DERIVED'); sec.body.classList.add('pad');
        g.appendChild(sec);
        var map = panel('Who is faster where', 'DERIVED');
        g.appendChild(map);
        var ch = panel('Speed and delta', null); ch.style.gridColumn = '1 / 3';
        g.appendChild(ch);
        this.trackMap(map, { sectors: true });
        var delta = this.deltaSeries.bind(this);
        this.strip(ch, [
            { label: 'Speed', unit: 'km/h', tier: 'FACT', weight: 2, series: both(function (d) { return d.kmh; }) },
            { label: 'Delta vs A', unit: 's', tier: 'DERIVED', weight: 1.2, zero: true,
              series: [{ side: '*', noA: true, get: function (d, data, i, L) { var ds = delta(L); return ds[i] ? ds[i].delta : null; } }],
              fmt: function (v) { return sgn(v, 3); } }
        ]);
        var render = function () {
            kp.body.innerHTML = self.compareHead() + self.kpiHtml();
            var lnk = kp.body.querySelector('.f1x-link');
            if (lnk) lnk.onclick = function () { self.flashLapPicker(); };
            var j = self.frame, len = self.wb.trackLen;
            var pa = self.points('A'), pb = self.points('B');
            var h = '';
            if (j && j.sc1 > 0 && j.sc2 > j.sc1 && len > 0) {
                var bounds = [0, j.sc1, j.sc2, len];
                var ta = sectorTimes(pa, bounds, self.lapBounds('A')), tb = sectorTimes(pb, bounds, self.lapBounds('B'));
                h += '<table class="tv-t"><thead><tr><th>Sector</th><th>A</th><th>B</th><th>B−A</th></tr></thead><tbody>';
                for (var k = 0; k < 3; k++) {
                    var d = ta[k] !== null && tb[k] !== null ? tb[k] - ta[k] : null;
                    h += '<tr><td>S' + (k + 1) + '</td><td class="a">' + fx(ta[k], 3) + '</td><td class="b">' + fx(tb[k], 3) + '</td><td class="' + (d === null ? '' : d > 0 ? 'pos' : 'neg') + '">' + sgn(d, 3) + '</td></tr>';
                }
                h += '</tbody></table>';
            } else h += '<div style="color:var(--tx3);font-size:12px;margin-bottom:8px">Sector boundaries unknown (track AIW not read); mini-sectors only.</div>';
            /* more than two sides: compact lap and sector table, lap delta vs A */
            var letters = self.letters();
            if (letters.length > 2) {
                var bnd = j && j.sc1 > 0 && j.sc2 > j.sc1 && len > 0 ? [0, j.sc1, j.sc2, len] : null, lapA = self.lapTime('A');
                h += '<table class="tv-t" style="margin-top:10px"><thead><tr><th>Side</th><th>Lap</th><th>S1</th><th>S2</th><th>S3</th><th>Lap vs A</th></tr></thead><tbody>';
                letters.forEach(function (L) {
                    var lt = self.lapTime(L), sec = bnd ? sectorTimes(self.points(L), bnd, self.lapBounds(L)) : [null, null, null];
                    var dv = lt !== null && lapA !== null && L !== 'A' ? lt - lapA : null;
                    h += '<tr><td style="color:' + COLORS[L] + '">' + L + ' · ' + esc(self.side(L).label) + '</td><td>' + fx(lt, 3, '') .replace('—', lt === null ? '—' : '') +
                        '</td><td>' + fx(sec[0], 3) + '</td><td>' + fx(sec[1], 3) + '</td><td>' + fx(sec[2], 3) + '</td>' +
                        '<td class="' + (dv === null ? '' : dv > 0 ? 'neg' : 'pos') + '">' + (L === 'A' ? 'reference' : sgn(dv, 3, 's')) + '</td></tr>';
                });
                h += '</tbody></table>';
            }
            var ms = miniSectors(pa, pb, len, 20, self.lapBounds('A'), self.lapBounds('B'));
            var wa = 0, wbn = 0;
            ms.forEach(function (m) { if (m.gain !== null) { if (m.gain > 0.005) wa++; else if (m.gain < -0.005) wbn++; } });
            h += '<div style="display:flex;gap:2px;margin:12px 0 4px">' + ms.map(function (m) {
                var c = m.gain === null ? '#1e293b' : m.gain > 0.005 ? COLORS.A : m.gain < -0.005 ? COLORS.B : '#475569';
                return '<span title="' + Math.round(m.x0) + '–' + Math.round(m.x1) + ' m: ' + sgn(m.gain, 3, 's') + '" style="flex:1;height:22px;border-radius:3px;background:' + c + '"></span>';
            }).join('') + '</div><div style="display:flex;justify-content:space-between;font:12px var(--mono);color:var(--tx2)"><span class="a" style="color:var(--a)">A faster in ' + wa + ' of 20</span><span style="color:var(--b)">B faster in ' + wbn + ' of 20</span></div>';
            sec.body.innerHTML = h;
        };
        this.refreshers.push(render);
    };

    Viewer.prototype.metaHtml = function () {
        var a = this.side('A'), b = this.side('B'), j = this.frame;
        var ca = j && a ? j.c.filter(function (c) { return c.id === a.slot; })[0] : null;
        var cb = j && b ? j.c.filter(function (c) { return c.id === b.slot; })[0] : null;
        var warn = [];
        if (ca && cb && ca.cn !== cb.cn) warn.push('different cars (' + esc(ca.cn) + ' vs ' + esc(cb.cn) + ')');
        if (ca && cb && ca.tc !== cb.tc) warn.push('different tyre compounds');
        if (ca && cb && Math.abs((ca.fl || 0) - (cb.fl || 0)) > 5) warn.push('fuel differs by ' + Math.abs(ca.fl - cb.fl).toFixed(1) + ' (raw units)');
        var rem = [ca, cb].filter(function (c) { return c && c.cr === 2; });
        var remHtml = rem.length ? '<div class="banner" style="margin-top:10px;border-color:#1e3a8a;background:linear-gradient(90deg,rgba(30,58,138,.3),rgba(30,58,138,.06));color:#dbeafe">' +
            '<div><b>CO-OP remote car: ' + rem.map(function (c) { return esc(c.n); }).join(', ') + '.</b> It is simulated on the other PC. Brake shows only on or off and steering comes from the network (coarse); ' +
            'throttle, g-forces and tyres are not available here; speed is derived from its network position (DERIVED) and lap timing follows the host. For a full-channel comparison, the other driver runs Telemetry too, ' +
            'presses <b>Export</b> after a lap, sends you the file, and you <b>Import</b> it into side B.</div></div>' : '';
        if (!warn.length) return remHtml;
        return '<div style="margin-top:10px;color:var(--warn);font-size:12px">⚠ Not like-for-like: ' + warn.join(' · ') + '. Differences may be setup, car or strategy, not driving.</div>' + remHtml;
    };

    /* ---- Live ---- */
    Viewer.prototype.view_live = function (host) {
        var g = this.grid(host, 'minmax(0,1fr) minmax(0,1fr)', 'minmax(0,1fr)');
        g.style.gridTemplateColumns = 'repeat(auto-fill,minmax(340px,1fr))';   /* 2 to 3 per row */
        g.style.gridTemplateRows = 'auto';
        this.liveGrid = g; this.liveSig = null; this.liveCards = {};
        this.updateLive();
    };

    Viewer.prototype.updateLive = function () {
        if (!this.liveCards) return;
        var j = this.frame, self = this;
        var sig = this.letters().join('');
        if (sig !== this.liveSig) {   /* side set changed: one card per side */
            this.liveSig = sig; this.liveGrid.innerHTML = ''; this.liveCards = {};
            this.letters().forEach(function (L) {
                var p = panel('Live · ' + L, null);
                p.body.classList.add('pad');
                self.liveGrid.appendChild(p);
                self.liveCards[L] = p;
            });
        }
        this.letters().forEach(function (L) {
            var p = self.liveCards[L], side = self.side(L);
            var car = j && side ? j.c.filter(function (c) { return c.id === side.slot; })[0] : null;
            if (!car) { p.body.innerHTML = '<div class="tv-empty"><div><b>No live row</b>Live gauges need a car that is in the current session.</div></div>'; return; }
            var vf = car.vf || 0, vf6 = car.vf6 || 0;
            var kmh = vf & 1 ? car.s * 3.6 : null;
            var thr = vf & 2 ? car.th * 100 : null, brk = vf & 4 ? car.br * 100 : null;
            var lat = vf & 32 ? car.glat / G : null, lon = vf & 64 ? -car.glon / G : null;
            var rpm = finite(car.rpm) ? car.rpm : null, mr = finite(car.max_rpm) && car.max_rpm > 0 ? car.max_rpm : 19000;
            var bars = inputBars(car);   /* 1.2.2: CO-OP remote car: brake on/off, no throttle */
            var dot = '<svg viewBox="-60 -60 120 120" width="150" height="150"><circle r="56" fill="#0b1018" stroke="#1c2532"/><circle r="37" fill="none" stroke="#1c2532"/><circle r="18.5" fill="none" stroke="#1c2532"/>' +
                '<line x1="-56" y1="0" x2="56" y2="0" stroke="#1c2532"/><line x1="0" y1="-56" x2="0" y2="56" stroke="#1c2532"/>' +
                (lat !== null && lon !== null ? '<circle cx="' + Math.max(-56, Math.min(56, -lat / 3 * 56)).toFixed(1) + '" cy="' + Math.max(-56, Math.min(56, -lon / 3 * 56)).toFixed(1) + '" r="6" fill="' + COLORS[L] + '"/>' : '') +
                '<text x="0" y="-44" fill="#6b7a8f" font-size="8" text-anchor="middle" font-family="Consolas">3 g</text></svg>';
            var tyres = '';
            var ttc = Array.isArray(car.ttc) ? car.ttc : null;
            var tShow = ttc && ((vf6 & 4) || ((vf6 & 256) && self.research));
            WHEELS.forEach(function (w, k) {
                var t = tShow ? ttc[k] : null;
                tyres += '<div class="wheel" style="padding:6px 8px"><div class="h"><span>' + w + '</span><span>' + fx(t, 0, '°C') + '</span></div><div class="tt" style="height:10px"><span style="background:' + tempColor(t) + '"></span></div></div>';
            });
            p.body.innerHTML = '<div style="display:grid;grid-template-columns:1fr auto;gap:16px;align-items:start">' +
                '<div class="gauge"><div class="big">' + (kmh !== null ? kmh.toFixed(0) : '—') + '<small>km/h</small></div>' +
                '<div style="display:flex;gap:18px;align-items:baseline"><div class="big" style="font-size:56px;color:' + COLORS[L] + '">' + (car.g > 0 ? car.g : car.g === 0 ? 'N' : 'R') + '</div>' +
                '<div style="font:600 16px var(--mono)">' + (rpm !== null ? WB.formatRpmValue(rpm) : '—') + ' <span style="color:var(--tx3);font-size:11px">rpm</span></div></div>' +
                '<div class="rpmbar"><i style="width:' + (rpm !== null ? Math.max(0, Math.min(100, rpm / mr * 100)) : 0) + '%"></i></div>' +
                '<div style="display:flex;gap:20px;margin-top:10px;font:12px var(--mono);color:var(--tx2)">' +
                '<span>Lat <b style="color:var(--tx)">' + fx(lat, 2, 'g') + ' ' + latWord(lat) + '</b></span><span>Long <b style="color:var(--tx)">' + fx(lon, 2, 'g') + '</b></span>' +
                '<span title="' + esc(car.steerwhy || (car.cr === 2 ? 'remote car (CO-OP): steering from the network packet, coarse steps' : '')) + '">Steer <b style="color:var(--tx)">' + steerLabel(car) + '</b></span></div>' +
                '<div style="display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:12px">' + tyres + '</div></div>' +
                '<div style="display:flex;flex-direction:column;align-items:center;gap:28px"><div class="vbars">' +
                '<div class="vbar" title="' + esc(bars.thrTitle) + '"><i style="height:' + (thr || 0) + '%;background:var(--good)"></i><span>' + bars.thrLabel + '</span></div>' +
                '<div class="vbar" title="' + esc(bars.brkTitle) + '"><i style="height:' + (brk || 0) + '%;background:var(--bad)"></i><span>' + bars.brkLabel + '</span></div></div>' + dot + '</div></div>' +
                '<div style="margin-top:14px;color:var(--tx3);font-size:11px">P' + car.p + ' · lap ' + lapNo(car.l) + ' · ' + WB.roleOf(car) + ' · sample age ' + fx(j.sample_age_ms, 0, 'ms') + '</div>';
        });
    };

    /* 4.4.0-rc.5 task 9: one setter for the research state (button click and R key).
     * The button shows ON or OFF, aria-pressed follows, and the open view redraws. */
    Viewer.prototype.setResearch = function (on) {
        this.research = !!on;
        if (this.resBtn) {
            this.resBtn.classList.toggle('on', this.research);
            this.resBtn.textContent = 'Research channels: ' + (this.research ? 'ON' : 'OFF');
            this.resBtn.setAttribute('aria-pressed', this.research ? 'true' : 'false');
        }
        if (this.view === 'research') this.renderView(); else this.invalidate();
    };

    /* ---- Chassis research ---- */
    var RX = [
        ['su', 'Suspension deflection candidate', 'wheel+0x10'],
        ['ld', 'Vertical load candidate', 'wheel+0x44 (AI) / W+796 (player)'],
        ['sl', 'Slip-angle-like candidate', 'wheel+0x38'],
        ['fo', 'wheel signal rx12-15 (meaning not proven)', 'wheel+0xD8']
    ];

    Viewer.prototype.view_research = function (host) {
        var self = this;
        var wrap = el('div');
        wrap.style.cssText = 'display:flex;flex-direction:column;gap:12px;height:100%';
        host.appendChild(wrap);
        this.refreshers = [];
        var ban = el('div', 'banner', '<div><b>Research channels — not proven physics.</b> These are raw game fields that correlate with suspension, load, slip and the wheel signal rx12-15 (meaning not proven) in short live captures, ' +
            'but their meaning, sign and unit are not proven yet (CHANNEL_LEDGER section 1.3). They are recorded so one guarded session can prove or disprove them. ' +
            'Values are shown unitless. The correlation table checks the physics a real channel must obey.</div>');
        wrap.appendChild(ban);
        if (!this.research) {
            var off = el('div', 'tv-p');
            off.style.flex = '1';
            off.innerHTML = '<div class="tv-empty"><div><b>Research channels are hidden</b>Press <span class="tier RESEARCH">R</span> or the Research channels button to show them.</div></div>';
            wrap.appendChild(off);
            return;
        }
        var g = el('div', 'tv-grid');
        g.style.cssText = 'grid-template-columns:minmax(0,1fr) minmax(320px,34%);grid-template-rows:minmax(0,1fr);flex:1;min-height:520px';
        wrap.appendChild(g);
        var ch = panel('Candidate fields per wheel', 'RESEARCH');
        var sel = el('select', 'tv-sel');
        sel.innerHTML = RX.map(function (r) { return '<option value="' + r[0] + '">' + r[1] + '</option>'; }).join('');
        sel.value = this.rxSel || 'ld';
        sel.onchange = function () { self.rxSel = sel.value; self.renderView(); };
        ch.head.querySelector('.r').appendChild(sel);
        g.appendChild(ch);
        var cor = panel('Physics consistency checks', 'DERIVED');
        cor.body.classList.add('pad');
        g.appendChild(cor);
        var key = this.rxSel || 'ld';
        var tracks = [{ label: 'Speed', unit: 'km/h', tier: 'FACT', weight: 1.2, series: both(function (d) { return d.kmh; }) },
            { label: 'Lat / long', unit: 'g', tier: 'FACT', weight: 1.2, sym: true, series: [
                { side: 'A', lbl: 'A lat', get: function (d) { return d.lat; } }, { side: 'A', lbl: 'A long', dash: [4, 3], get: function (d) { return d.lon; } }],
              fmt: function (v) { return v.toFixed(2); } }];
        WHEELS.forEach(function (w, k) {
            tracks.push({ label: w + ' ' + key, unit: 'raw', tier: 'RESEARCH', weight: 1,
                series: both(function (d) { return d.rx && d.rx[key] ? d.rx[key][k] : null; }), fmt: function (v) { return v.toPrecision(4); } });
        });
        this.strip(ch, tracks);
        var render = function () {
            var h = '';
            self.letters().forEach(function (L) {
                var s = self.seriesFor(L);
                var has = s.some(function (d) { return d.rx; });
                h += '<div style="font:600 11px var(--sans);letter-spacing:.06em;color:var(--' + L.toLowerCase() + ');margin:4px 0">SIDE ' + L + ' · ' + esc(self.side(L).label) + '</div>';
                if (!has) { h += '<div style="color:var(--tx3);font-size:12px;margin-bottom:10px">No research samples. Candidates are captured for AI cars (load also for the player).</div>'; return; }
                var lat = s.map(function (d) { return d.lat; }), lon = s.map(function (d) { return d.lon; }), v = s.map(function (d) { return d.kmh; });
                h += '<table class="tv-t"><thead><tr><th>Field</th>' + WHEELS.map(function (w) { return '<th>' + w + '</th>'; }).join('') + '</tr></thead><tbody>';
                RX.forEach(function (r) {
                    [['lat', lat], ['long', lon], ['speed', v]].forEach(function (ref) {
                        h += '<tr><td>' + r[0] + ' × ' + ref[0] + '</td>' + WHEELS.map(function (w, k) {
                            var c = correlate(s.map(function (d) { return d.rx && d.rx[r[0]] ? d.rx[r[0]][k] : null; }), ref[1]);
                            var col = c === null ? 'var(--tx3)' : Math.abs(c) > 0.7 ? 'var(--tx)' : Math.abs(c) > 0.4 ? 'var(--tx2)' : 'var(--tx3)';
                            return '<td style="color:' + col + '">' + (c === null ? '—' : c.toFixed(2)) + '</td>';
                        }).join('') + '</tr>';
                    });
                });
                h += '</tbody></table>';
            });
            h += '<div style="color:var(--tx3);font-size:11px;line-height:1.6;margin-top:8px">Pearson r against FACT channels over the shown lap. ' +
                'Player rx[4..7] = FL,FR,RL,RR loads (order DERIVED, unit not proven); player rx[0..3] read 0. AI rx front/rear: unverified. ' +
                'A true vertical load must move opposite on left/right wheels with lateral g and opposite front/rear with longitudinal g; ' +
                'a suspension deflection should follow load and rise with speed (downforce). Record results in findings.jsonl before promoting any field.</div>';
            cor.body.innerHTML = h;
        };
        this.refreshers.push(render);
    };

    /* ---- Legacy layouts (the 23 old-viewer layouts, unchanged) ---- */
    Viewer.prototype.renderLegacy = function () {
        var box = el('div', 'tv-legacy');
        this.viewEl.appendChild(box);
        this.legacyUi = WB.mount(box, this.wb, this.legacyMountOpts({ label: 'LEGACY LAYOUTS' }));
        if (this.waypoints) this.legacyUi.setWaypoints(this.waypoints);
        this.legacyUi.render();
        if (!WB.layoutHasMap(this.legacyFile)) this.mapInset(this.viewEl, true);
    };
    /* TAPE2 V4/V7: the legacy renderer gets the shared frame balls and reports its hover to the viewer's cursor. */
    Viewer.prototype.legacyMountOpts = function (more) {
        var self = this, o = { roster: this.roster, frameBalls: function (ctx, view) { self.drawSideBalls(ctx, view); },
            onCursor: function (d) { self.setCursorDist(d); } };
        Object.keys(more || {}).forEach(function (k) { o[k] = more[k]; });
        return o;
    };

    /* ================================================================ console */
    /* 4.4.0 unified console, served at "/". One window in the look of the old
     * f1_telemetry.exe (navy title bar, lap strip, arrow + layout combo,
     * black panes with blue frames, navy status bar) that holds every page:
     *  - Race pages rebuilt from the classic dashboard (timing tower, track map
     *    with every car, session, race control, race pace, driver focus);
     *  - the 4.3.0 analysis views of this file (unchanged logic);
     *  - the 23 legacy layouts (workbench renderer, same model).
     * It subclasses Viewer, so laps, cursor, zoom, import/export and the
     * A/B model are the same code as /viewer. Read-only like everything here. */

    var LEGACY_PAL = {
        band: 'rgba(0,0,0,0)', rowLine: '#16307a', axis: '#5f7fc4', zero: '#33509a', grid: '#0d1d4a',
        label: '#4f86ff', vgrid: '#0c1a44', cursor: 'rgba(255,255,255,0.7)', cursorText: '#dfe7ff',
        track: '#1b2a55', line: '#8fa6d8', lineDim: '#4d5f8c', start: '#ffffff', ring: '#16307a', bg: '#000000',
        sans: 'Tahoma, Verdana, Arial, sans-serif', mono: 'Tahoma, Verdana, Arial, sans-serif'
    };
    var LEGACY_WB = {
        title: '#4f86ff', unavailable: '#c9a24a', frame: '#16307a', axis: '#5f7fc4', minor: '#081236',
        grid: '#0d1d4a', zero: '#33509a', cursor: '#ffffff', track: '#1b2a55', marker: '#ffffff', ref: '#9aa5b1',
        font: 'Tahoma, Verdana, Arial, sans-serif', titleFont: '10px Tahoma, Verdana, Arial, sans-serif',
        titleRight: true
    };

    var CONSOLE_CSS = [
        '.f1x{--navy:#0a1678;--navy2:#06104f;--navy3:#030a33;--frame:#1d3fa8;--frame2:#14307f;--lbl:#4f86ff;--cyan:#38d6e8;',
        '--bg:#000;--bg2:#02061a;--panel:#000;--panel2:#040b26;--line:#14307f;--line2:#1d3fa8;--tx:#dfe7ff;--tx2:#9fb2e6;--tx3:#5f7fc4;',
        '--sans:Tahoma,Verdana,Arial,sans-serif;--mono:Tahoma,Verdana,Arial,sans-serif;',
        'position:fixed;inset:0;display:grid;grid-template-rows:40px auto 34px auto auto minmax(0,1fr) auto 22px;height:auto;min-height:0;background:#000;color:var(--tx);font:11px/1.35 var(--sans);user-select:none}',
        /* title bar */
        '.f1x-title{display:flex;align-items:center;gap:14px;padding:0 12px;background:#050b2e;border-bottom:1px solid #1d3fa8;min-width:0}',
        '.f1x-brand{display:flex;align-items:center;gap:6px;flex:none;font:bold 13px var(--sans);letter-spacing:.08em;color:#fff;white-space:nowrap}',
        '.f1x-brand i{width:4px;height:20px;background:#e10600;display:block}',
        '.f1x-brand span{font-weight:normal;color:#9fb2e6;letter-spacing:.14em;font-size:11px}',
        '.f1x-hsess{flex:1 1 auto;min-width:0;font:12px var(--sans);color:#dfe7ff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.f1x-hsess.run::before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:#3ce07a;margin-right:8px;vertical-align:1px}',
        '.f1x-hsess.idle{color:#9fb2e6}.f1x-hsess.past{color:#ffe2a6}',
        '.f1x-tabs{display:flex;flex:none;border:1px solid #1d3fa8;border-radius:4px;overflow:hidden}',
        '.f1x-tab{border:0;background:transparent;color:#9fb2e6;font:bold 11px var(--sans);letter-spacing:.04em;padding:6px 14px;cursor:pointer}',
        '.f1x-tab+.f1x-tab{border-left:1px solid #1d3fa8}',
        '.f1x-tab:hover{color:#fff;background:#0d1d4a}',
        '.f1x-tab.on{background:#1d3fa8;color:#fff}',
        '.f1x-help{width:22px;height:22px;border-radius:50%;border:1px solid #2a43a8;background:transparent;color:#9fb2e6;font:bold 11px var(--sans);cursor:pointer;padding:0}',
        '.f1x-help:hover{color:#fff;border-color:#38d6e8}',
        '.f1x-ico{width:18px;height:18px;display:grid;place-items:center;border:1px solid #6f86d8;border-right-color:#020830;border-bottom-color:#020830;background:#0b1a7e;color:#ffd84a;font:bold 10px var(--sans);cursor:pointer;padding:0}',
        '.f1x-ico:active{border-color:#020830 #6f86d8 #6f86d8 #020830}',
        '.f1x-ico.on{background:#13289e;color:#fff}',
        '.f1x-name{flex:1;text-align:center;font:italic bold 13px "Times New Roman",Georgia,serif;color:#b9c4f2;letter-spacing:.02em;text-shadow:1px 1px 0 #000;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.f1x-tr{display:flex;flex:none;align-items:center;gap:10px;color:#9fb2e6;font:11px var(--sans);white-space:nowrap}',
        '.f1x-tr a{color:#9fb2e6;text-decoration:none;border:1px solid #2a43a8;padding:0 5px;background:#071066}',
        '.f1x-tr a:hover{color:#fff}',
        '.f1x-live{font:bold 10px var(--sans);letter-spacing:.06em;color:#3ce07a}.f1x-live.stale{color:#ffd84a}.f1x-live.off{color:#ff5a5a}',
        /* lap strip */
        '.f1x-laps{background:#000;border-bottom:2px solid #0a1678;padding:2px 0 3px;overflow:hidden}',
        /* TAPE2 V2: the row grows to fit its horizontal scrollbar; a fixed 17 px row let the bar cover the chips. */
        '.f1x-lrow{display:flex;align-items:center;gap:0;min-height:17px;white-space:nowrap;overflow-x:auto;overflow-y:hidden;scrollbar-width:auto;scrollbar-color:#3d6fe8 #06102a}',
        /* 1.2.3 (DVOA report 2026-10-17): a visible slider when the laps do not fit; the mouse wheel scrolls it too */
        '.f1x-lrow::-webkit-scrollbar{height:9px}.f1x-lrow::-webkit-scrollbar-thumb{background:#3d6fe8;border-radius:4px}.f1x-lrow::-webkit-scrollbar-track{background:#06102a}',
        '.f1x-hb{display:inline-block;margin-left:5px;padding:0 4px;border-radius:3px;color:#fff;font:bold 9px var(--sans);line-height:13px;vertical-align:middle;letter-spacing:.04em}',
        '.f1x-lrow+.f1x-lrow{border-top:1px solid #0a2a40}',
        '.f1x-ltag{flex:none;width:210px;padding:0 6px;font:bold 10px var(--sans);overflow:hidden;text-overflow:ellipsis;cursor:default}',
        '.f1x-ltag i{font-style:normal;display:inline-block;width:14px;text-align:center;color:#000;margin-right:5px}',
        '.f1x-chip{flex:none;padding:0 8px;color:var(--cyan);font:9px var(--sans);cursor:pointer;border-right:1px solid #06263a;line-height:15px}',
        '.f1x-chip:hover{background:#06263a;color:#fff}',
        '.f1x-chip.on{background:#0d2f8f;color:#fff;outline:1px solid #4f86ff;outline-offset:-1px}',
        '.f1x-chip.fast{color:#d58cff}.f1x-chip.part{color:#2c8796;font-style:italic}.f1x-chip.live{color:#3ce07a}',
        '.f1x-chip.none{color:#2c4f7a;cursor:default}',
        /* layout bar */
        '.f1x-bar{display:flex;align-items:center;gap:8px;padding:0 10px;background:#02061a;border-bottom:1px solid #14307f;min-width:0;overflow:visible;position:relative;z-index:20}',   /* no overflow clip: the page menu drops out of the bar */
        '.f1x-drv{display:inline-flex;align-items:center;gap:5px;padding:2px 4px 2px 6px;border:1px solid #14307f;border-radius:4px;background:#040b26;flex:none}',
        '.f1x-drv.A{border-left:3px solid var(--a)}.f1x-drv.B{border-left:3px solid var(--b)}',
        '.f1x-tools{display:inline-flex;align-items:center;gap:4px;margin-left:auto;flex:none}',
        '.f1x-arrow{width:24px;height:22px;padding:0;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#8fb6ff;font:bold 12px var(--sans);cursor:pointer;line-height:1;flex:none}',
        '.f1x-arrow:active{border-color:#020830 #6f86d8 #6f86d8 #020830}',
        '.f1x-combo{flex:0 1 260px;min-width:180px;height:22px;border:1px solid #2a43a8;border-radius:3px;background:#071066;color:#fff;font:bold 12px var(--sans);padding:0 8px;cursor:pointer;outline:0}',
        '.f1x-combo optgroup{font-style:normal;color:#ffd84a;background:#030a33}.f1x-combo option{font-style:normal;color:#dfe7ff;background:#071066}',
        '.f1x-bar .f1x-sep{width:1px;height:16px;background:#2a43a8;margin:0 3px}',
        '.f1x-bar label{color:#9fb2e6;font:bold 10px var(--sans);padding:0 2px 0 4px}',
        '.f1x-bar label.A{color:var(--a)}.f1x-bar label.B{color:var(--b)}',
        '.f1x-bar select.tv-sel,.f1x-bar .tv-btn{height:22px;border-radius:3px;border:1px solid #2a43a8;background:#071066;color:#dfe7ff;font:11px var(--sans);padding:0 8px;max-width:220px}',
        '.f1x-bar .tv-btn{background:#0b1a5e;gap:3px;flex:none}',
        '.f1x-bar .tv-btn:hover{background:#13289e;border-color:#38d6e8}',
        '.f1x-bar .tv-btn.on{color:#fff;background:#1d3fa8}.f1x-bar .tv-btn.res.on{color:#e9c9ff;background:#3b1d7a;border-color:#a970ff #20063f #20063f #a970ff}',
        /* stage + panes */
        '.f1x .tv-view.f1x-stage{position:relative;min-height:0;overflow:auto;padding:2px;background:#000}',
        '.f1x .tv-grid{gap:2px;min-height:0;height:100%}',
        '.f1x .tv-p{background:#000;border:1px solid var(--frame);border-radius:0;box-shadow:none}',
        '.f1x .tv-ph{height:16px;padding:0 6px;border-bottom:0;background:transparent;color:var(--lbl);font:10px var(--sans);letter-spacing:0;text-transform:none;justify-content:flex-end;position:absolute;top:0;right:0;left:0;z-index:2;pointer-events:none}',
        '.f1x .tv-ph .r{order:-1;margin-left:0;margin-right:auto;pointer-events:auto}',
        '.f1x .tv-ph .tier{order:2}',
        '.f1x .tv-pb{margin-top:0}',
        '.f1x .tv-pb.pad{padding:18px 8px 8px}',
        '.f1x .tv-p.hdr .tv-ph{position:static;border-bottom:1px solid #0a1a52;height:16px}',
        '.f1x .tier{font:9px var(--sans);padding:0 3px;border-radius:0}',
        '.f1x .kpi{background:#02061a;border:1px solid #14307f;border-radius:0}',
        '.f1x .kpi .v{font:bold 13px var(--sans)}',
        '.f1x table.tv-t{font:11px var(--sans)}',
        '.f1x .tv-t th{background:#06104f;color:#9fb2e6;font:bold 9px var(--sans);letter-spacing:.02em;padding:3px 5px;border-bottom:1px solid #1d3fa8}',
        '.f1x .tv-t td{padding:2px 5px;border-bottom:1px solid #07123a}',
        '.f1x .tv-t tbody tr:hover td{background:#06123f}.f1x .tv-t tr.sel td{background:#0a1d63}',
        '.f1x .tv-empty{color:var(--tx3)}.f1x .tv-empty b{color:var(--lbl)}',
        '.f1x .banner{border-radius:0}',
        '.f1x .wheel{background:#02061a;border-radius:0}.f1x .bar,.f1x .rpmbar,.f1x .vbar{border-radius:0}',
        '.f1x .chassis{border-radius:6px}',
        '.f1x .tv-legacy{border:0;border-radius:0;background:#000;min-height:0;height:100%}',
        '.f1x .tv-legacy .wbx{padding:0;gap:2px;min-height:0}',
        '.f1x .tv-legacy .wb-bar,.f1x .tv-legacy .wb-laps{display:none}',
        '.f1x .tv-legacy .wb-g{padding:1px}',
        '.f1x .tv-legacy .wb-g canvas{background:#000;border:1px solid var(--frame);border-radius:0}',
        '.f1x .tv-legacy .wb-readout,.f1x .tv-legacy .wb-reasons{background:#000;border:1px solid #14307f;border-radius:0;color:#9fb2e6;font:10px var(--sans);padding:2px 6px}',
        '.f1x .tv-tip{border-radius:0;background:#02061a;border-color:#1d3fa8;font:10px var(--sans)}',
        '.f1x .tv-modal>div{border-radius:0;background:#030a33;border:2px solid #1d3fa8}',
        /* 4.4.0 batch 1: lap reference flag, reference line, showing line, page list, live gauges */
        '.f1x-flag{display:inline-block;margin-left:4px;padding:0 2px;color:#5f7fc4;font:bold 10px var(--sans);cursor:pointer}',
        '.f1x-flag:hover{color:#fff}.f1x-flag.on{color:#ffffff;background:#7a5a00;border:1px solid #ffd84a;padding:0 1px}',
        '.f1x-lref{height:15px;line-height:15px;padding:0 8px;font:10px var(--sans);color:#b9a45a;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;border-top:1px solid #0a1a52}',
        '.f1x-show{min-height:15px;line-height:15px;padding:0 8px;background:#02061a;border-bottom:1px solid #0a1678;color:#9fb2e6;font:10px var(--sans);white-space:normal;overflow:hidden}',
        '.f1x-show b.A{color:#ff3b30}.f1x-show b.B{color:#2ea8ff}.f1x-show b{font-weight:bold}',
        '.f1x-shsep{color:#2a43a8;margin:0 8px}',
        '.f1x-pick{position:relative;flex:1;min-width:180px;display:flex}',
        '.f1x-pick .f1x-combo{flex:1;width:100%;text-align:left;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;cursor:pointer}',
        '.f1x-menu{position:absolute;left:0;top:100%;z-index:60;min-width:100%;width:max-content;max-width:520px;max-height:62vh;overflow-y:auto;background:#030a33;border:2px solid #1d3fa8;box-shadow:0 4px 14px #000;outline:0;padding:0 0 4px}',
        '.f1x-mg{position:sticky;top:0;background:#071066;color:#ffd84a;font:bold 10px var(--sans);padding:3px 8px;border-bottom:1px solid #14307f;letter-spacing:.03em}',
        '.f1x-mi{padding:2px 14px;color:#a9c4ff;font:italic bold 11px var(--sans);cursor:pointer;white-space:nowrap}',
        '.f1x-mi:hover,.f1x-mi.act{background:#13289e;color:#fff}',
        '.f1x-mi.on{color:#fff;outline:1px solid #4f86ff;outline-offset:-1px}',
        '.f1x-lg{display:flex;flex-direction:column;gap:8px;font:10px var(--sans);color:#9fb2e6}',
        '.f1x-lgrow{display:flex;gap:8px;flex-wrap:wrap;align-items:stretch}',
        '.f1x-lgrow2{display:grid;grid-template-columns:minmax(0,1fr) 150px;gap:10px;align-items:center}',
        '.f1x-lgbars{display:grid;grid-template-columns:34px minmax(0,1fr);gap:4px 6px;align-items:center}',
        '.f1x-lbl{font:bold 10px var(--sans);color:#5f7fc4}',
        '.f1x-lcdbox{flex:1 1 110px;background:#02061a;border:1px solid #14307f;padding:3px 8px;min-width:0}',
        '.f1x-lcdl{font:bold 9px var(--sans);color:#5f7fc4;letter-spacing:.04em}',
        '.f1x-lcd{font:bold 34px var(--sans);color:#9fe4ff;text-align:right;font-variant-numeric:tabular-nums;text-shadow:0 0 8px #1d6fd8;line-height:1.1;overflow:hidden;white-space:nowrap}',
        '.f1x-lcd.big{font-size:46px}.f1x-lcd.gear{font-size:58px;color:#ffd84a;text-shadow:0 0 8px #a07a00}.f1x-lcd.sm{font-size:18px}',
        '.f1x-seg{display:grid;gap:2px;height:14px}',
        '.f1x-seg i{display:block;background:#0d1d4a;border:1px solid #14307f}',
        '.f1x-seg i.on.grn{background:#2fb35c;border-color:#2fb35c}.f1x-seg i.on.amb{background:#ffd84a;border-color:#ffd84a}.f1x-seg i.on.red{background:#ff4136;border-color:#ff4136}',
        '.f1x-gg{position:relative;width:150px;height:150px;border:1px solid #1d3fa8;background:#000;justify-self:end}',
        '.f1x-gg .ax{position:absolute;background:#14307f}.f1x-gg .h{left:0;right:0;top:50%;height:1px}.f1x-gg .v{top:0;bottom:0;left:50%;width:1px}',
        '.f1x-gg .ring{position:absolute;inset:14%;border:1px solid #14307f;border-radius:50%}',
        '.f1x-gg b{position:absolute;width:9px;height:9px;margin:-5px 0 0 -5px;border:1px solid #000}',
        '.f1x-gg span{position:absolute;left:4px;bottom:2px;font:9px var(--sans);color:#5f7fc4}',
        /* status bar */
        '.f1x-status{display:flex;align-items:center;gap:14px;padding:0 6px;background:var(--navy2);border-top:1px solid #1d3fa8;color:#9fb2e6;font:10px var(--sans);white-space:nowrap;overflow:hidden}',
        '.f1x-status .tv-sess,.f1x-status .tv-foot{display:flex;gap:12px;align-items:center;height:auto;padding:0;border:0;background:transparent;color:#9fb2e6;font:10px var(--sans);overflow:hidden}',
        '.f1x-status .tv-sess b{color:#dfe7ff}.f1x-status .tv-foot{margin-left:auto}',
        '.f1x-status .tv-live{font:bold 10px var(--sans)}.f1x-status .tv-live:before{display:none}',
        '.f1x-status .ok{color:#3ce07a}.f1x-status .bad{color:#ff5a5a}',
        /* race pages */
        '.f1x-tw{width:100%;border-collapse:collapse;font:11px var(--sans);font-variant-numeric:tabular-nums}',
        '.f1x-tw th{position:sticky;top:0;z-index:1;background:#06104f;color:#9fb2e6;font:bold 9px var(--sans);text-align:right;padding:3px 4px;border-bottom:1px solid #1d3fa8;white-space:nowrap}',
        '.f1x-tw td{text-align:right;padding:2px 4px;border-bottom:1px solid #07123a;color:#c9d4f2;white-space:nowrap}',
        '.f1x-tw th.l,.f1x-tw td.l{text-align:left}',
        '.f1x td.team,.tvd-table td.team{color:#9fb2e6;max-width:120px;overflow:hidden;text-overflow:ellipsis}',
        '.f1x-tw tbody tr{cursor:pointer}.f1x-tw tbody tr:hover td{background:#06123f}',
        '.f1x-tw tr.foc td{background:#0a1d63}.f1x-tw tr.plr td.l{color:#ffd84a}',
        '.f1x-tw tr.dn td{color:#55607a}.f1x-tw tr.pit td{color:#7fd4ff}.f1x-tw tr.doomed td{color:#ff9a8a}',
        '.f1x-tw .pos{display:inline-block;min-width:18px;text-align:center;font-weight:bold;color:#dfe7ff}',
        '.f1x-tw .p1{background:#c9a400;color:#000}.f1x-tw .p2{background:#9aa3ad;color:#000}.f1x-tw .p3{background:#9a6430;color:#000}',
        '.f1x-tw .sct{color:#d58cff;font-weight:bold}.f1x-tw .pb{color:#3ce07a;font-weight:bold}',
        '.f1x-tw .gn{color:#3ce07a}.f1x-tw .rd{color:#ff6a6a}',
        '.f1x-tw .ab{font:bold 9px var(--sans);padding:0 3px;margin-left:4px;color:#000}',
        '.f1x-tag{display:inline-block;padding:0 3px;font:bold 9px var(--sans);color:#000;margin-right:3px}',
        '.f1x-tyre{display:inline-block;min-width:22px;text-align:center;border:1px solid #2a43a8;font:bold 9px var(--sans);color:#dfe7ff}',
        '.f1x-kv{display:grid;grid-template-columns:auto 1fr;gap:1px 10px;font:11px var(--sans)}',
        '.f1x-kv .k{color:#5f7fc4;font:bold 9px var(--sans);letter-spacing:.02em;padding-top:2px;white-space:nowrap}',
        '.f1x-kv .v{color:#dfe7ff;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}',
        '.f1x-kv .w{grid-column:1/3}',
        '.f1x-rc{font:11px var(--sans)}',
        '.f1x-rc div{display:flex;gap:6px;padding:2px 6px;border-bottom:1px solid #07123a;color:#c9d4f2}',
        '.f1x-rc .lp{flex:none;width:28px;color:#5f7fc4}',
        '.f1x-rc .s1{color:#ffd84a}.f1x-rc .s2{color:#ff6a6a}.f1x-rc .sI{color:#7fd4ff}',
        '.f1x-flagz{display:flex;gap:1px;line-height:0;padding:2px 0}.f1x-flagz span{flex:1;height:9px;min-width:3px}',
        '.f1x-sect{display:grid;grid-template-columns:1fr 1fr 1fr;gap:2px;line-height:16px;font:9px var(--sans);text-align:center}.f1x-sect span{background:#0d1d4a;color:#9fb2e6;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.f1x-sect span b{color:#dfe7ff;font-weight:bold}.f1x-sect span.on{background:#2fb35c;color:#001a08}.f1x-sect span.on b{color:#001a08}',
        '.f1x-ts{display:inline-block;padding:1px 10px;font:bold 11px var(--sans);letter-spacing:.08em;color:#000}',
        '.f1x-bars{display:grid;grid-template-columns:30px 1fr 36px;gap:3px 6px;align-items:center;font:bold 9px var(--sans);color:#5f7fc4}',
        '.f1x-bars .t{height:9px;background:#02061a;border:1px solid #14307f}.f1x-bars .t i{display:block;height:100%}',
        '.f1x-btns{display:flex;flex-wrap:wrap;gap:3px;margin:4px 0 6px}',
        '.f1x-btns button{height:18px;border:1px solid;border-color:#6f86d8 #020830 #020830 #6f86d8;background:#0b1a7e;color:#dfe7ff;font:10px var(--sans);padding:0 6px;cursor:pointer}',
        '.f1x-btns button.A{color:var(--a)}.f1x-btns button.B{color:var(--b)}',
        '.f1x-note{color:#5f7fc4;font:10px var(--sans);margin:4px 0;line-height:1.45}',
        '.f1x-h4{color:#4f86ff;font:bold 10px var(--sans);margin:8px 0 3px;letter-spacing:.03em}',
        '.f1x-scroll{position:absolute;inset:17px 0 0 0;overflow:auto}',
        '.f1x-dials{display:flex;gap:4px;justify-content:center}.f1x-dials canvas{width:170px;height:170px;position:static}',
        '@media (max-width:980px){.f1x{position:static;min-height:100vh;grid-template-rows:auto auto auto auto minmax(600px,1fr) auto}',
        '.f1x-bar{flex-wrap:wrap;padding:3px}.f1x-combo,.f1x-pick{flex-basis:100%}.f1x-ltag{width:120px}.f1x-name{font-size:11px}',
        '.f1x .tv-grid{grid-template-columns:1fr!important;grid-template-rows:none!important;height:auto}',
        '.f1x .tv-grid>.tv-p{grid-column:auto!important;grid-row:auto!important;min-height:300px}.f1x-status{flex-wrap:wrap;height:auto}}',
        /* rc.14 weather page: chart boxes are positioned so their canvas fills only the box. */
        '.f1x-wxbox{position:relative;border-top:1px solid #0a1a52;background:#000}',
        '.f1x-wxbar{display:inline-block;width:64px;height:8px;margin-right:4px;vertical-align:middle;background:#02061a;border:1px solid #14307f}',
        '.f1x-wxbar i{display:block;height:100%}',
        '.f1x-modal{position:fixed;inset:0;z-index:9000;background:rgba(0,0,0,.6);display:flex;align-items:center;justify-content:center}',
        '.f1x-dlg{width:560px;max-width:94vw;max-height:86vh;display:flex;flex-direction:column;background:#040b26;border:1px solid #38d6e8;border-radius:6px;box-shadow:0 12px 40px rgba(0,0,0,.7);font:12px var(--sans);color:#dfe7ff}',
        '.f1x-dlg-h{display:flex;align-items:center;justify-content:space-between;padding:10px 14px;border-bottom:1px solid #14307f;font-size:13px}',
        '.f1x-dlg-x{border:0;background:transparent;color:#9fb2e6;font-size:18px;cursor:pointer;line-height:1}',
        '.f1x-dlg-b{padding:12px 14px;overflow:auto;display:flex;flex-direction:column;gap:10px}',
        '.f1x-dlg-row{display:flex;align-items:flex-start;gap:10px}',
        '.f1x-dlg-row>label:first-child{width:70px;flex:none;color:#9fb2e6;padding-top:3px}',
        '.f1x-dlg-row label.chk{display:inline-flex;align-items:center;gap:5px;color:#dfe7ff;margin-left:10px}',
        '.f1x-dlg-list{flex:1;max-height:220px;overflow:auto;border:1px solid #14307f;border-radius:3px;padding:4px 6px;display:grid;grid-template-columns:1fr 1fr;gap:2px 10px}',
        '.f1x-dlg-list label{display:flex;align-items:center;gap:6px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;cursor:pointer}',
        '.f1x-dlg-list span{color:#5f7fc4}',
        '.f1x-dlg-msg{min-height:18px;color:#9fb2e6;white-space:pre-wrap}',
        '.f1x-dlg-f{display:flex;justify-content:flex-end;gap:8px;padding:10px 14px;border-top:1px solid #14307f}',
        '.f1x-dlg .tv-btn{height:26px;padding:0 14px;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#dfe7ff;cursor:pointer}',
        '.f1x-dlg .tv-btn.primary{background:#e10600;border-color:#ff4a3d;color:#fff}'
    ].join('\n');

    /* 1.2.1: source switch, source line, past-mode states, TV page message, Recordings cards. */
    var SRC_CSS = [
        '.f1x-srcsw{display:flex;flex:none;border:1px solid #1d3fa8;border-radius:4px;overflow:hidden}',
        /* collapsed, not display:none: the console grid maps rows by child order */
        '.f1x-srcline.live.run{height:0;min-height:0;padding:0;border:0;overflow:hidden}',
        '.f1x-show{height:0;min-height:0 !important;padding:0 !important;border:0 !important;overflow:hidden}',
        '.f1x-bar .f1x-dd-b{height:22px;font-size:11px;border-radius:3px}',
        '.f1x-bar .f1x-combo{max-width:320px}',
        '.f1x-src{font:bold 11px var(--sans);letter-spacing:.08em;padding:6px 14px;border:0;background:transparent;color:#9fb2e6;cursor:pointer}',
        '.f1x-src+.f1x-src{border-left:1px solid #1d3fa8}',
        '.f1x-src:hover{color:#fff;background:#0d1d4a}',
        '.f1x-src:hover{filter:brightness(1.2)}',
        '.f1x-src.live.on{background:#1f9d55;border-color:#3ce07a;color:#fff}',
        '.f1x-src.past.on{background:#c98a10;border-color:#ffb84a;color:#1a1200}',
        '.f1x.f1x-src-live .f1x-title{box-shadow:inset 0 -2px 0 #3ce07a}',
        '.f1x.f1x-src-past .f1x-title{box-shadow:inset 0 -2px 0 #ffb84a}',
        '.f1x-srcline{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:4px 8px;font:12px var(--sans);color:#dfe7ff;background:#02061a;border-bottom:1px solid #14307f}',
        '.f1x-srcline.past{background:#2a1d00;color:#ffe2a6;border-bottom:3px solid #ffb84a}',
        '.f1x-srcline .tv-btn{font-size:11px}',
        '.f1x-srctag{margin-left:4px;font:bold 9px var(--sans);letter-spacing:.05em;padding:1px 4px;background:#1f9d55;color:#fff}',
        '.f1x-srctag.past{background:#c98a10;color:#1a1200}',
        '.f1x-srctag.off{background:#2b3a5a;color:#9fb2e6}',
        '.f1x.f1x-past-nodata .f1x-laps{opacity:.35;pointer-events:none}',
        '.f1x.f1x-src-past .f1x-status .tv-foot{opacity:.35}',
        '.f1x-pastnote{padding:18px 14px;font:13px var(--sans);color:#ffe2a6;max-width:760px}',
        '.f1x-pastnote p{margin:6px 0}',
        '.tvd-pastnote{display:none;padding:14px;font:600 13px var(--sans);color:#ffe2a6}',
        '.tvd-pastmode>.tvd-pastnote{display:block}',
        '.tvd-pastmode>:not(.tvd-pastnote){display:none !important}',
        '.f1x-sess-bar{display:flex;gap:6px;align-items:center;margin:0 0 8px}',
        '.f1x-sess-q{flex:1 1 auto;max-width:420px;font:12px var(--sans);background:#02061a;color:#dfe7ff;border:1px solid #1d3fa8;padding:4px 6px}',
        '.f1x-card{border:1px solid #1d3fa8;background:#040b26;padding:8px 12px 10px;margin:0 0 8px}',
        '.f1x-card.on{border-color:#ffb84a}',
        '.f1x-hdr{display:flex;gap:12px;align-items:center;flex-wrap:wrap;font:12px var(--sans);color:#dfe7ff}',
        '.f1x-wlevel.ok{color:#8fd18f}.f1x-wlevel.warn{color:#ffd84a}',
        '.f1x-pill{padding:2px 9px;border-radius:999px;background:#1f9d55;color:#fff;font:bold 11px var(--sans)}',
        '.f1x-pill.idle{background:#14307f;color:#9fb2e6}',
        '.f1x-notice{font:600 12px var(--sans);padding:4px 8px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.f1x-notice.ok{color:#8fd18f}.f1x-notice.bad{color:#ff8a8a;border:1px solid #6b1d1d;background:#1a0606}',
        '.f1x-note.bad{color:#ff8a8a;font-weight:600}',
        '.f1x-selbar{position:sticky;top:0;z-index:2;display:flex;gap:8px;align-items:center;flex-wrap:wrap;padding:6px 8px;background:#14307f;border:1px solid #38d6e8;margin-bottom:8px;font:12px var(--sans);color:#dfe7ff}',
        '.f1x-sep{color:#9fb2e6}',
        '.f1x-card-h{display:flex;justify-content:space-between;align-items:flex-start;gap:10px;flex-wrap:wrap}',
        '.f1x-card h3{margin:0;font:bold 13px var(--sans);color:#dfe7ff}',
        '.f1x-card small{color:#9fb2e6;font:11px var(--sans)}',
        '.f1x-card-b{display:flex;gap:8px;align-items:center}',
        '.f1x-link{background:none;border:0;color:#7fd4ff;font:11px var(--sans);text-decoration:underline;cursor:pointer;padding:2px 4px}',
        '.f1x-link:disabled{color:#3d528f;text-decoration:none;cursor:not-allowed;font-style:italic}',
        '.f1x-sess-t{border-collapse:collapse;margin-top:8px;font:12px var(--sans);width:100%;table-layout:auto}',
        '.f1x-sess-t td{padding:4px 8px;vertical-align:top;color:#dfe7ff;text-align:left;white-space:nowrap;border-top:1px solid #14307f}',
        '.f1x-sess-t td.n{width:100%;min-width:150px}','.f1x-sess-t tr:hover td{background:#071640}',
        '.f1x-sess-t td.a{text-align:right}',
        '.f1x-sess-t tr.open td{background:#0b2a1a}',
        '.f1x-badge{display:inline-block;padding:1px 7px;border-radius:3px;font:bold 10px var(--sans);letter-spacing:.04em}',
        '.f1x-badge.live{background:#1f9d55;color:#fff}',
        '.f1x-badge.closed{background:#2b3a5a;color:#dfe7ff}',
        '.f1x-badge.warn{background:#c98a10;color:#1a1200}',
        '.f1x-badge.bad{background:#a32020;color:#fff}',
        '.f1x-size{color:#9fb2e6;font-size:11px;margin-left:6px}',
        '.f1x-det summary{cursor:pointer;color:#5f7fc4;font-size:10px}',
        '.f1x-det div{color:#9fb2e6;font-size:10px;margin-top:2px;white-space:normal;max-width:520px}'
    ].join('\n');


    var RACE_PAGES = [['race', 'Session Overview'], ['timing', 'Timing & Race Pace'], ['focus', 'Driver Focus']];
    var ANALYSIS_PAGES = [['overview', 'Lap Analysis'], ['dynamics', 'Vehicle Dynamics'], ['braking', 'Braking & Corners'],
        ['tyres', 'Tyres'], ['compare', 'Driver Compare (A vs B)'], ['live', 'Live Gauges'], ['research', 'Chassis Research']];

    /* ---- race helpers (ported from the classic dashboard, same evidence) ---- */

    /* Session type: FACT ladder from SessionTypeNameLookup 0x0042CE30
     * (findings test-day-session-type-0): 0 TestDay, 1-4 Practice, 5 Qualifying,
     * 7 Race. 6 is not in the finding (warm-up is an inference). The classic
     * dashboard's Q1/Q2 labels for 4/5 are not used here. */
    var SESSION = { 0: 'Test Day', 1: 'Practice 1', 2: 'Practice 2', 3: 'Practice 3', 4: 'Practice 4', 5: 'Qualifying', 6: 'Warm-up', 7: 'Race' };
    var PHASE = { 0: '?', 1: 'Grid', 2: 'Race', 3: 'Fin', 4: 'Pod' };
    function sessName(ty) { return SESSION[ty] !== undefined ? SESSION[ty] : 'Session ' + ty; }
    function phaseName(p) { return PHASE[p] !== undefined ? PHASE[p] : 'ph ' + p; }
    /* Damage tiers: CONFIRMED per-part records (part_detach_commentary). */
    var DMG = [{ t: '', c: '', d: '' },
        { t: 'BODYWORK', c: '#888', d: 'Panel damage, cosmetic, no stop required' },
        { t: 'FRONT WING', c: '#fc0', d: 'Front wing detached, will pit and REJOIN (repairable)' },
        { t: 'REAR WING', c: '#ff4136', d: 'Rear wing detached, car WILL RETIRE at the pit box' },
        { t: 'WHEEL', c: '#ff4136', d: 'Wheel loss, terminal' },
        { t: 'ENGINE STOP', c: '#ff4136', d: 'Engine stopped: RPM 0 AND EngineAngVel 0 while the car has not finished, during a live session.' }];
    var DOOMWHY = {
        1: { tag: 'REAR WING', txt: 'Rear wing detached. The pit-box blocking condition tests only this record, so the game will retire the car when it reaches the box; it cannot be repaired.' },
        2: { tag: 'WHEEL', txt: 'Wheel lost (damage severity 2). Terminal, the car cannot continue.' },
        3: { tag: 'ENGINE', txt: 'Engine has stopped: RPM and crankshaft angular velocity are both zero while the car has not finished.' }
    };
    /* FailureCategoryNameLookup 0x004BB730 (static proof; 3 and 7 both Clutch). */
    var RCAT = { 0: 'N/A', 1: 'Engine', 2: 'Gearbox', 3: 'Clutch', 4: 'Suspension', 5: 'Brakes', 6: 'Accident', 7: 'Clutch', 8: 'Electronics', 9: 'Fuel' };
    /* 4.4.0-rc.6 (Batch 2i task 3): wetness is shown as the game's numbers only.
     * onw = on-line, offw = off-line. 'dry' only when both are 0. No Dry/Damp/Wet/Rain/Storm state. */
    function wetText(d) {
        var on = d.onw || 0, off = d.offw || 0;
        if (on === 0 && off === 0) return 'dry (both 0)';
        return 'on-line ' + (on * 100).toFixed(0) + '% · off-line ' + (off * 100).toFixed(0) + '%';
    }
    /* 4.4.0-rc.6 (owner O3 FACT): glat > 0 = LEFT turn, glat < 0 = RIGHT turn. */
    function latWord(g) { return g === null || g === undefined || !isFinite(g) ? '' : Math.abs(g) < 0.05 ? '(straight)' : g > 0 ? 'LEFT' : 'RIGHT'; }
    var POSCOL = ['#ffd700', '#c0c0c0', '#cd7f32'];

    function code3(s) {
        var p = String(s || '').replace(/\s*\(\d{4}\)\s*$/, '').trim().split(/\s+/);
        return (p[p.length - 1] || '').substring(0, 3).toUpperCase();
    }
    function cleanName(s) { return String(s || '').replace(/\s*\(\d{4}\)\s*$/, ''); }

    /* TAPE2 V1: the one rule for "who drives this car", used by every place a driver is named (tower, car selectors,
     * lap rows, map labels, Focus, Replay, Race Control). YOU = the local human (cr 0). HUMAN = a remote human (cr 2 that is
     * not AI-controlled, c.a 0). AI = no badge. Pure. */
    function humanRole(c) {
        if (!c) return '';
        if (c.cr === 0) return 'YOU';
        if (c.cr === 2 && !c.a) return 'HUMAN';
        return '';
    }
    /* DOM badge for a car: white text on the side colour when the car is on a side, else on the default blue. */
    function humanBadgeHtml(c, sideLetter, color) {
        var r = humanRole(c);
        if (!r) return '';
        var bg = color || (sideLetter && COLORS[sideLetter] ? COLORS[sideLetter] : (r === 'YOU' ? '#1f5fd1' : '#6b4fd6'));
        return ' <span class="f1x-hb" style="background:' + bg + '" title="' + (r === 'YOU' ? 'You drive this car' : 'A remote human driver') + '">' + r + '</span>';
    }
    /* Text badge for <option> and canvas labels (no HTML there): " [YOU]" or " [HUMAN]", else "". */
    function humanBadgeText(c) { var r = humanRole(c); return r ? ' [' + r + ']' : ''; }

    /* 1.2.2 CO-OP remote inputs. A remote car (cr 2) on a client: the game writes brake as 0 or 1.0 from one network flag
     * (vf6 4096), steering from the network packet, and no throttle (findings coop-remote-brake-binary-receiver-2026-10-16,
     * coop-remote-throttle-not-applied-2026-10-16; DERIVED, live CO-OP check pending). Labels and titles for the THR/BRK
     * bars; values still follow car_valid (vf bits 2, 4). */
    function inputBars(car) {
        var remote = !!car && car.cr === 2, vf = car ? car.vf || 0 : 0;
        var onOff = remote && (vf & 4) !== 0;
        return {
            remote: remote,
            brakeOnOff: onOff,
            thrLabel: remote ? 'THR n/a' : 'THR',
            thrTitle: remote ? 'Throttle: the game does not apply it for a CO-OP remote car on this PC' : 'Throttle pedal',
            brkLabel: onOff ? (car.br >= 0.5 ? 'BRK ON' : 'BRK off') : 'BRK',
            brkTitle: onOff ? 'Brake on/off only: the CO-OP network sends one flag, not the pedal position' :
                remote ? 'Brake: no valid value in this sample' : 'Brake pedal',
            thrText: function (th) { return remote ? 'n/a' : th === null ? '—' : th.toFixed(0) + '%'; },
            brkText: function (br) { return br === null ? '—' : onOff ? (br >= 50 ? 'ON' : 'off') : br.toFixed(0) + '%'; }
        };
    }
    /* Steer text: the value, marked "(network)" for a remote car; else why it is missing. */
    function steerLabel(car) {
        var vf = car.vf || 0;
        if (vf & 8) return car.steer.toFixed(3) + (car.cr === 2 ? ' (network)' : '');
        return car.cr === 2 ? 'not in this sample' : (car.a ? 'invalid' : 'unproven for player');
    }

    /* TAPE2 V2: one lap label for a lap chip, option or tag. A recorded lap (seeded from the server, review or live
     * recording) is "L<n>"; a file the user imported keeps "Import". */
    /* 1.2.4: the time a lap chip shows: the game's official time when it arrived (same as the timing tower), else the
     * time between the recorded line crossings. Pure. */
    function lapShownTime(l) {
        return l && typeof l.official_time === 'number' && isFinite(l.official_time) && l.official_time > 0 ?
            l.official_time : (l ? l.end - l.start : NaN);
    }
    /* 1.2.5: the tooltip of a partial lap chip: the reason, and whether the game's time still ranks it. Pure. */
    function partialLapTitle(l) {
        var t = 'partial lap' + (l && l.why ? ': ' + l.why : '');
        if (l && l.repair && !l.repairTried) t += ' (asking the game PC for the full lap)';
        if (WB.lapIsRanked(l)) t += '. Ranked by the game lap time.';
        return t;
    }
    /* 1.2.3: one lap-strip row; a vertical mouse wheel scrolls it sideways when its laps do not fit. */
    function lapRowEl() {
        var row = el('div', 'f1x-lrow');
        row.addEventListener('wheel', function (e) {
            if (row.scrollWidth <= row.clientWidth || Math.abs(e.deltaY) <= Math.abs(e.deltaX)) return;
            row.scrollLeft += e.deltaY;
            e.preventDefault();
        }, { passive: false });
        return row;
    }
    function lapLabel(l) {
        if (!l) return '';
        if (l.recorded) return 'L' + l.lap;
        return l.src === 'import' ? 'Import' : 'Lap ' + l.lap;
    }
    /* C3 (2026-10-14): team name from the car string; the year prefix ("1995 McLaren") is removed. */
    function teamOf(c) { return String((c && (c.tn || c.cn)) || '').replace(/^\d{4}\s+/, '').trim(); }
    function tsec(s) {
        if (!s || s <= 0 || s > 99999) return '-';
        var m = Math.floor(s / 60), sc = Math.floor(s % 60), ms = Math.floor((s % 1) * 1000);
        return m + ':' + (sc < 10 ? '0' : '') + sc + '.' + (ms < 100 ? (ms < 10 ? '00' : '0') : '') + ms;
    }
    /* 4.4.0-rc.7 (Batch 2j task 1): the server adds tcn per car (SEASONDATA .veh ->
     * .hdv TireBrand -> .tbc [COMPOUND] Name, DERIVED). Short label = first word
     * ("Intermediate" -> "Inter"); without tcn the label stays C<tc>. */
    function tyreName(tc, tcn) {
        if (tc === undefined || tc === null || tc < 0) return '-';
        if (typeof tcn === 'string' && tcn.length) {
            var w = tcn.split(' ')[0];
            if (w === 'Intermediate') w = 'Inter';
            return w.slice(0, 8);
        }
        return 'C' + tc;
    }
    function tyreTitle(tc, tcn) {
        if (tc === undefined || tc === null || tc < 0) return '';
        if (typeof tcn === 'string' && tcn.length) return tcn.replace(/[<>"&]/g, '') + ' (DERIVED: SEASONDATA .tbc)';
        return 'Tyre index ' + tc + ' (compound name not resolved)';
    }
    /* 4.4.0-rc.8 (Batch 2k task 1): Mechanical failures host state from /health
     * mods.mechfail (server probe of F1C_MECHFAIL_CTL, read-only). The host struct
     * has no pit-failure field, so no pit-failure section is shown (OPEN). */
    function mfStatus(h) {
        if (!h) return { state: 'unknown', text: 'Mechanical failures mod: unknown (no /health yet)' };
        var m = h.mods && h.mods.mechfail;
        if (!m || !m.present) return { state: 'not loaded', text: 'Mechanical failures mod: not loaded' };
        if (m.host_verified && m.activated) {
            return { state: 'active', text: 'Mechanical failures mod: active (' + (m.families_started | 0) + ' families started)' };
        }
        return { state: 'loaded', text: 'Mechanical failures mod: loaded (not activated)' };
    }
    /* Forecast from <game>\LOG\weather.txt, parsed by the server. rc.14: the GAME
     * writes this file at weekend load (weather init sub_4FCAA0 ->
     * WeatherConditionsFileWriter 0x4FE820): it is the timeline the session follows.
     * Shown only when the server marked it fresh (written during this game run).
     * null = "no forecast for this session". */
    function forecastOf(h) {
        var f = h && h.forecast;
        return f && f.available === true && f.fresh === true && Array.isArray(f.steps) ? f : null;
    }
    /* rc.11 weather: pure helpers for the Weather console page. The server's
     * /weather document carries rows [st,at,trkt,onw,offw,tgf] (session time
     * in s, temperatures C, wetness 0..1, raw grip factor; null = missing). */
    function weatherHistory(doc) {
        var rows = doc && Array.isArray(doc.samples) ? doc.samples : [];
        var out = { t: [], air: [], track: [], on: [], off: [], grip: [], now: null };
        rows.forEach(function (r) {
            if (!Array.isArray(r) || r.length < 6 || !finite(r[0])) return;
            out.t.push(r[0]);
            out.air.push(finite(r[1]) ? r[1] : null);
            out.track.push(finite(r[2]) ? r[2] : null);
            out.on.push(finite(r[3]) ? r[3] * 100 : null);      /* wetness as percent */
            out.off.push(finite(r[4]) ? r[4] * 100 : null);
            out.grip.push(finite(r[5]) ? r[5] : null);
            out.now = r[0];                                     /* now-marker: newest st */
        });
        return out;
    }
    /* NOW values from the live frame (-900 sentinel, same rule as sessionPane)
     * with the /weather tier strings carried through for the row badges. */
    function weatherNow(d, tiers) {
        var t = tiers || {};
        var at = d && finite(d.at) && d.at > -900 ? d.at : null;
        var trkt = d && finite(d.trkt) && d.trkt > -900 ? d.trkt : null;
        var onw = d && finite(d.onw) ? d.onw : null;
        var offw = d && finite(d.offw) ? d.offw : null;
        return {
            at: at, trkt: trkt,
            delta: at !== null && trkt !== null ? trkt - at : null,
            onwPct: onw === null ? null : onw * 100,
            offwPct: offw === null ? null : offw * 100,
            state: wetText({ onw: onw === null ? 0 : onw, offw: offw === null ? 0 : offw }),
            grip: d && finite(d.tgf) ? d.tgf : null,
            tiers: { at: t.at || '', trkt: t.trkt || '', onw: t.onw || '', offw: t.offw || '', tgf: t.tgf || '' }
        };
    }
    /* FORECAST panel state: available exactly when forecastOf() shows it;
     * otherwise the server reason. The game writes the file at weekend load, so a
     * missing or old file means the weekend was loaded before this game run. */
    function weatherForecastInfo(h) {
        var f = h && h.forecast;
        var fc = forecastOf(h);
        if (fc) return { available: true, fcst: fc,
            note: 'Game weather timeline: LOG\\weather.txt, written by the game when the weekend loads.',
            reason: '', file_mtime: f.file_mtime, run_start: f.run_start };
        return { available: false, fcst: null,
            reason: f && f.reason ? f.reason : 'no forecast for this session',
            note: 'The game writes its weekend weather timeline to LOG\\weather.txt when a weekend loads; it appears here once a weekend is loaded in this game run.',
            file_mtime: f ? f.file_mtime : undefined, run_start: f ? f.run_start : undefined };
    }
    /* rc.14: conditions/ambient at a session minute, linear between the
     * timeline steps (the game evaluates its timeline once per second at
     * session clock x time scale; linear interpolation here is DERIVED). */
    function wxForecastAt(f, minute) {
        if (!f || !f.start || !finite(minute)) return null;
        var pts = [{ m: 0, c: Number(f.start.conditions), a: Number(f.start.ambient_temp) }];
        (f.steps || []).forEach(function (s) {
            if (finite(Number(s.minute))) pts.push({ m: Number(s.minute), c: Number(s.conditions), a: Number(s.ambient_temp) });
        });
        pts.sort(function (x, y) { return x.m - y.m; });
        if (minute <= pts[0].m) return { cond: pts[0].c, amb: pts[0].a };
        for (var i = 1; i < pts.length; i++) {
            if (minute <= pts[i].m) {
                var t = (minute - pts[i - 1].m) / ((pts[i].m - pts[i - 1].m) || 1);
                return { cond: pts[i - 1].c + (pts[i].c - pts[i - 1].c) * t, amb: pts[i - 1].a + (pts[i].a - pts[i - 1].a) * t };
            }
        }
        var L = pts[pts.length - 1];
        return { cond: L.c, amb: L.a };
    }
    /* rc.15: the game's own condition classes (ThresholdBasedReturn 0x4FEAA0 on
     * Conditions / 100): < 61.65 Dry, < 70 Overcast, < 88 Raining, else Monsoon.
     * The pre-race screen's CURRENT CONDITIONS = this class of the timeline at
     * the session clock (sub_4FF130). Live 2026-10-12: timeline 59.96 -> 'Dry'
     * on the game screen. Replaces the compendium's 'rain above 50'. */
    var WX_CLASS = [[61.65, 'Dry'], [70, 'Overcast'], [88, 'Raining'], [Infinity, 'Monsoon']];
    function wxSky(c) {
        if (!finite(c)) return '-';
        for (var i = 0; i < WX_CLASS.length; i++) if (c < WX_CLASS[i][0]) return WX_CLASS[i][1];
        return 'Monsoon';
    }
    function wxIsWet(word) { return word === 'Raining' || word === 'Monsoon'; }
    /* ---- rc.16 weather FORECAST (owner: no spoilers, may be wrong like reality) ----
     * The game's timeline is the truth; the page never shows it directly. It
     * shows a forecast issued every 10 session minutes: per time block the
     * worst sky in that block plus a seeded error that grows with lead time
     * (sigma = 2 + 0.12 x lead minutes on the 0-100 conditions scale, smooth over time), turned
     * into a rain chance (estimate vs the game's Raining class at 70), rounded
     * to 10%. Same session + same issue = same forecast (no flicker); a new
     * issue can revise it, as real forecasts do. Air temperature as a range. */
    var WX_BLOCKS = [[0, 15], [15, 30], [30, 45], [45, 60], [60, 90], [90, 120]];
    function wxHash(s) {
        var h = 2166136261;
        for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
        return h >>> 0;
    }
    function wxRng(seed) {   /* mulberry32 */
        var a = seed >>> 0;
        return function () {
            a = (a + 0x6D2B79F5) >>> 0;
            var t = Math.imul(a ^ (a >>> 15), 1 | a);
            t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        };
    }
    function wxGauss(r) { var u = Math.max(1e-9, r()), v = r(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }
    function wxNormCdf(x) {   /* Abramowitz-Stegun 26.2.17 */
        var t = 1 / (1 + 0.2316419 * Math.abs(x));
        var p = t * (0.31938153 + t * (-0.356563782 + t * (1.781477937 + t * (-1.821255978 + t * 1.330274429))));
        var c = 1 - 0.3989422804014327 * Math.exp(-x * x / 2) * p;
        return x >= 0 ? c : 1 - c;
    }
    /* Forecast blocks from `minute` on. key = session identity (track, session,
     * file time). Returns null without a timeline. Blocks past the end of the
     * game's timeline are left out. */
    function wxForecast(f, minute, key) {
        if (!f || !f.start || !finite(minute)) return null;
        var last = 0;
        (f.steps || []).forEach(function (q) { if (finite(Number(q.minute))) last = Math.max(last, Number(q.minute)); });
        var issue = Math.floor(minute / 10) * 10;
        var r = wxRng(wxHash(String(key) + '|' + issue));
        /* Errors are smooth over time: one shared bias for the weather system plus
         * a slowly wandering part (AR(1) across blocks), so neighbouring blocks agree. */
        var shared = wxGauss(r), wander = wxGauss(r), tShared = wxGauss(r);
        var out = [];
        WX_BLOCKS.forEach(function (b) {
            var a = minute + b[0], e = minute + b[1];
            wander = 0.75 * wander + 0.66 * wxGauss(r);
            if (a > last) return;
            var worst = -Infinity, sum = 0, amb = 0, n = 0;
            for (var m = a; m <= Math.min(e, last) + 1e-9; m += 1) {
                var v = wxForecastAt(f, m);
                if (!v) continue;
                if (v.cond > worst) worst = v.cond;
                sum += v.cond; amb += v.amb; n++;
            }
            if (!n) return;
            var mean = sum / n; amb /= n;
            var lead = (b[0] + b[1]) / 2, sig = 2 + 0.12 * lead;
            var err = sig * (0.75 * shared + 0.45 * wander);
            var rain = 1 - wxNormCdf((70 - (worst + err)) / sig);
            var pct = Math.max(0, Math.min(100, Math.round(rain * 10) * 10));
            var est = mean + err;
            /* sky word follows the forecast, not the truth: rain words only when the rain chance backs them */
            var sky = pct >= 60 ? (est >= 88 && pct >= 90 ? 'Monsoon' : 'Raining') : est >= 61.65 || pct >= 30 ? 'Overcast' : 'Dry';
            var tErr = (0.3 + 0.015 * lead) * tShared, tw = 0.5 + 0.02 * lead;
            out.push({ from: b[0], to: b[1], sky: sky, rain: pct,
                       tLo: Math.round(amb + tErr - tw), tHi: Math.round(amb + tErr + tw) });
        });
        return { issued: issue, blocks: out };
    }
    function wxForecastKey(d, f) {
        return (d && d.trk ? d.trk : '') + '|' + (d ? d.ty : '') + '|' + (f && f.file_mtime ? f.file_mtime : '');
    }
    function wxRainWord(p) { return p >= 70 ? 'likely' : p >= 40 ? 'possible' : p >= 20 ? 'low chance' : 'unlikely'; }
    /* First timeline step after `minute` whose class differs from now. */
    function wxNextRainChange(f, minute) {
        if (!f || !finite(minute)) return null;
        var cur = wxForecastAt(f, minute);
        if (!cur) return null;
        var now = wxSky(cur.cond), steps = (f.steps || []).slice().sort(function (x, y) { return x.minute - y.minute; });
        for (var i = 0; i < steps.length; i++) {
            var m = Number(steps[i].minute), c = Number(steps[i].conditions);
            if (m > minute && finite(c) && wxSky(c) !== now) return { minute: m, cond: c, from: now, to: wxSky(c), toRain: wxIsWet(wxSky(c)) && !wxIsWet(now) };
        }
        return null;
    }
    /* The pre-race screen's PROJECTED CONDITIONS (sub_4FF1D0): min and max of the
     * timeline from now to now + 50 min (race: 90 min), including the first step
     * past the window, then class((min + max) / 2 + offset). The offset is a
     * game value at 0x8AFEC0+0x104 that this server does not read (OPEN), so this
     * is the game rule WITHOUT it (DERIVED); the screen can differ by one class. */
    function wxProjected(f, minute, isRace) {
        if (!f || !finite(minute)) return null;
        if (!f.start || !finite(Number(f.start.conditions))) return null;
        var steps = (f.steps || []).slice().sort(function (x, y) { return x.minute - y.minute; });
        /* the game starts at the current node: the last step at or before now */
        var base = Number(f.start.conditions), i;
        for (i = 0; i < steps.length; i++) if (Number(steps[i].minute) <= minute && finite(Number(steps[i].conditions))) base = Number(steps[i].conditions);
        var end = minute + (isRace ? 90 : 50), lo = base, hi = base;
        for (i = 0; i < steps.length; i++) {
            var m = Number(steps[i].minute), c = Number(steps[i].conditions);
            if (!(m > minute) || !finite(c)) continue;
            if (c < lo) lo = c;
            if (c > hi) hi = c;
            if (m > end) break;
        }
        var mid = (lo + hi) / 2;
        return { cond: mid, word: wxSky(mid), lo: lo, hi: hi, window: isRace ? 90 : 50 };
    }
    /* ---- rc.14 wetness profile (owner question: "wet spots") -----------------
     * CONFIRMED (static: Hex-Rays + disassembly, not live-poked): the game
     * blends on-line and off-line wetness by the lateral distance from the
     * nearest waypoint's wp_path[1] line. RangeInterpolationTable 0x4FD6F0 fills 16 floats at 0x8AFF24..0x8AFF60 = on + f x (off - on) with the
     * fractions below; F_1DTableLookupWithLerp 0x4FD7E0 reads them at distance x 4
     * (0.25 m steps, >= 3.75 m = full off-line). Callers: per WHEEL in
     * sub_4423B0 (wheel+0x270, used by the tyre friction 0x436870 and tyre
     * cooling 0x438130) and per CAR in sub_4EE2D0 (spray/particle intensity). The
     * driest point is 0.75 m either side of the line = two tyre tracks. */
    var WX_PROFILE = [0.05, 0.03, 0.01, 0.0, 0.01, 0.06, 0.14, 0.25, 0.37, 0.50, 0.63, 0.75, 0.86, 0.94, 0.99, 1.0];
    function wxProfileFrac(d) {
        if (!finite(d)) return null;
        var a = Math.abs(d);
        if (a >= 3.75) return 1;
        var x = a * 4, i = Math.floor(x);
        return WX_PROFILE[i] + (WX_PROFILE[i + 1] - WX_PROFILE[i]) * (x - i);
    }
    /* Local wetness 0..1 at lateral distance d (m) from the wp_path[1] line. */
    function wxSpotWetness(on, off, d) {
        var f = wxProfileFrac(d);
        if (f === null || !finite(on) || !finite(off)) return null;
        return on + (off - on) * f;
    }
    /* Shortest 2D distance (m) from point p to a closed polyline (map frame). */
    function wxDistToLine(line, p) {
        if (!line || line.length < 2 || !p || !finite(p[0]) || !finite(p[1])) return null;
        var best = Infinity;
        for (var i = 0; i < line.length; i++) {
            var a = line[i], b = line[(i + 1) % line.length];
            var vx = b[0] - a[0], vz = b[1] - a[1], wx = p[0] - a[0], wz = p[1] - a[1];
            var L2 = vx * vx + vz * vz, t = L2 > 0 ? Math.max(0, Math.min(1, (wx * vx + wz * vz) / L2)) : 0;
            var dx = wx - vx * t, dz = wz - vz * t, d2 = dx * dx + dz * dz;
            if (d2 < best) best = d2;
        }
        return Math.sqrt(best);
    }
    /* Session clock as the history x axis: m:ss, or h:mm:ss past an hour. */
    function wxClock(st) {
        if (!finite(st) || st < 0) return '-';
        var h = Math.floor(st / 3600), m = Math.floor((st % 3600) / 60), s = Math.floor(st % 60);
        return (h > 0 ? h + ':' + (m < 10 ? '0' : '') + m : String(m)) + ':' + (s < 10 ? '0' : '') + s;
    }
    /* ---- rc.12 weather map helpers ----------------------------------------
     * The dry/wet driving lines come from the server's /track document (from
     * the live AI database: line = wp_pos + wp_path[n] * wp_perp; FACT file
     * fields, DERIVED line, docs/52 section 4.6). wxTrackLines accepts the
     * document only when wc and both arrays agree, so a truncated or unread
     * track draws nothing rather than a wrong line. */
    function wxTrackLines(doc) {
        if (!doc || !finite(doc.wc) || doc.wc <= 0) return null;
        function arr(a) {
            if (!Array.isArray(a) || a.length !== doc.wc) return null;
            var out = [];
            for (var i = 0; i < a.length; i++) {
                var q = a[i];
                if (!Array.isArray(q) || q.length < 2 || !finite(q[0]) || !finite(q[1])) return null;
                out.push([q[0], q[1]]);
            }
            return out;
        }
        var dl = arr(doc.dl), wl = arr(doc.wl);
        if (!dl || !wl) return null;
        return { wc: doc.wc, dl: dl, wl: wl,
                 gw: finite(doc.gw) && doc.gw > 0 ? doc.gw : null,
                 gww: finite(doc.gww) && doc.gww > 0 ? doc.gww : null };
    }
    /* Wetness colour ramp for the map: dry slate -> wet blue (0..1). */
    function wxWetColor(v) {
        var t = finite(v) ? Math.max(0, Math.min(1, v)) : 0;
        var r = Math.round(74 + (47 - 74) * t);
        var g = Math.round(90 + (111 - 90) * t);
        var b = Math.round(134 + (224 - 134) * t);
        return 'rgb(' + r + ',' + g + ',' + b + ')';
    }
    /* rc.14: map ramp with a clear brightness step - dry asphalt grey -> bright
     * wet blue; t^0.6 so the low wetness range (0..30%) stays readable. */
    function wxMapWet(v) {
        var t = Math.pow(finite(v) ? Math.max(0, Math.min(1, v)) : 0, 0.6);
        return 'rgb(' + Math.round(58 + (30 - 58) * t) + ',' + Math.round(62 + (120 - 62) * t) + ',' + Math.round(72 + (255 - 72) * t) + ')';
    }
    /* rc.14: focus-car path colour by local wetness 0..1 (yellow dry -> blue wet). */
    function wxTrailColor(v) {
        var t = finite(v) ? Math.max(0, Math.min(1, v)) : 0;
        return 'rgb(' + Math.round(255 - 215 * t) + ',' + Math.round(216 - 56 * t) + ',' + Math.round(74 + 181 * t) + ')';
    }
    /* Per-surface dry/wet grip reference (SEASONDATA\Circuits\terrain.ini
     * [TRACKVARS] + [FEEDBACK] groups, FACT file values; the relative loss is
     * DERIVED: dry/wet coefficients are one part of the tyre+terrain grip
     * equation). 2002_Silverstone.gdb carries no [TRACKVARS] override. */
    function wxGripRefs() {
        return [
            { mat: 'Road (on track)', dry: 1.00, wet: 0.80, n: 'road, conc, roadpit, marbles…' },
            { mat: 'Kerb / rumble',   dry: 0.95, wet: 0.60, n: 'rmbl, rmbbl, brick, rmbld' },
            { mat: 'Grass',           dry: 0.50, wet: 0.45, n: 'gras, fldgrass, rdgrass' },
            { mat: 'Gravel / sand',   dry: 0.52, wet: 0.50, n: 'grv, sand, dirt, grav' }
        ];
    }
    /* String coercion for server tokens; never invents a value. */
    function wxToken(v) { return v === undefined || v === null ? '' : String(v); }
    /* rc.12: the server's GDB weather script (/weather .gdb). Returns null when
     * absent; sessions and steps are sanitized so the pane renders only what
     * the server actually parsed. Steps carry either a minute or a lap. */
    function wxGdb(doc) {
        var g = doc && doc.gdb;
        if (!g || typeof g !== 'object' || !Array.isArray(g.sessions)) return null;
        var sessions = [];
        g.sessions.forEach(function (s) {
            if (!s || typeof s !== 'object') return;
            var steps = [];
            if (Array.isArray(s.steps)) s.steps.forEach(function (t) {
                if (!t || typeof t !== 'object') return;
                var isLap = finite(t.lap);
                var pos = isLap ? t.lap : finite(t.minute) ? t.minute : null;
                if (pos === null) return;
                steps.push({ lap: !!isLap, at: pos,
                    conditions: wxToken(t.conditions), track_wetness: wxToken(t.track_wetness),
                    ambient: wxToken(t.ambient), tracktemp: wxToken(t.tracktemp) });
            });
            sessions.push({ name: wxToken(s.name), conditions: wxToken(s.conditions),
                track_wetness: wxToken(s.track_wetness),
                on_path_wetness: wxToken(s.on_path_wetness), off_path_wetness: wxToken(s.off_path_wetness),
                ambient: wxToken(s.ambient), tracktemp: wxToken(s.tracktemp), steps: steps });
        });
        return { file: wxToken(g.file), truncated: g.truncated === true, sessions: sessions };
    }
    /* rc.12: the game's weather model, its words/values and its effects on the
     * car - one reference block for the Weather page. Sources: the GDB model
     * from the owner-supplied third-party compendium (J21C rev.2, 2023-02-02)
     * with local file checks (RESEARCH_WEATHER section E); the effects from the
     * decompiled consumers; surface grips from terrain.ini. Not live data. */
    function wxModelRefs() {
        return {
            file: 'game code 0x4FE0D0 / 0x4FE1E0 / 0x4FE2B0 / 0x4FEAA0, SEASONDATA circuit .GDB / .AIW + terrain.ini',
            conditions: {
                note: 'GDB Conditions words as the game reads them (WeatherNameWetnessMultiplier 0x4FE0D0, value / 100; a number is taken as is). The game classes conditions as Dry < 61.65, Overcast < 70, Raining < 88, Monsoon from 88 (0x4FEAA0). The third-party compendium values (Clear 0, Overcast 50, rain above 50) do not match the code.',
                words: [
                    { w: 'Best',     v: '3',  target: 'Dry' },
                    { w: 'Nice',     v: '30', target: 'Dry' },
                    { w: 'Clear',    v: '60', target: 'Dry' },
                    { w: 'Overcast', v: '65', target: 'Overcast' },
                    { w: 'Drizzle',  v: '73', target: 'Raining' },
                    { w: 'Rain',     v: '85', target: 'Raining' },
                    { w: 'Monsoon',  v: '97', target: 'Monsoon' }
                ]
            },
            wetness: {
                note: 'TrackWetness / OnPathWetness / OffPathWetness words as the game reads them (WeatherSeverityGripMultiplier 0x4FE1E0, value / 100).',
                words: [
                    { w: 'Dry', v: '0' }, { w: 'Damp', v: '25' }, { w: 'Wet', v: '50' },
                    { w: 'Soaked', v: '75' }, { w: 'Monsoon', v: '100' }
                ]
            },
            temps: {
                note: 'Temperature words (sub_4FE2B0); a number is taken as degrees C. Track temperature is one global value for the whole track.',
                words: [
                    { w: 'Cold', v: '7.5' }, { w: 'Cool', v: '13.75' }, { w: 'Medium', v: '20' },
                    { w: 'Warm', v: '26.25' }, { w: 'Hot', v: '32.5' }
                ]
            },
            schedule: 'The GDB script is the track\'s authored weather: Minute = or Lap = steps (leader lap), each carrying any of Conditions / TrackWetness / OnPathWetness / OffPathWetness / AmbientTemp / TrackTemp. At weekend load the game generates its own weekend timeline and writes it to LOG\\weather.txt; the live values follow that timeline (Forecast pane). How the script feeds the generator is OPEN.',
            effects: [
                { e: 'Wetness per wheel', d: 'each wheel: lateral distance from the wp_path[1] line -> profile table -> local wetness between on-line and off-line', s: 'sub_4423B0 (wheel +0x270)', t: 'CONFIRMED' },
                { e: 'Tyre friction', d: 'surface grip = dry - wheel wetness x wet loss; the tyre grip terms drop with the same wheel wetness', s: 'CollisionContactGenerationAndFrictionResponse 0x436870', t: 'CONFIRMED' },
                { e: 'Tyre cooling', d: 'cooling x (1 + 0x8AFEE4 + wheel wetness)', s: 'Tyre model 0x438130', t: 'CONFIRMED' },
                { e: 'Spray', d: 'per-car spray / water particle intensity = local wetness at the car (same profile)', s: 'sub_4EE2D0', t: 'CONFIRMED' },
                { e: 'World grip multiplier', d: '1.0 - (on-line + off-line) x 0.5, applied per physics node', s: 'PhysicsNodePropagation 0x401C90', t: 'CONFIRMED' },
                { e: 'Wet corner-speed cap', d: 'v = wetness x (limit - 1) + 1; cap scaled by (sqrt(v) + v) / 2', s: 'VehicleMainPhysicsUpdate 0x4441F0', t: 'CONFIRMED' },
                { e: 'Tyre wear', d: 'wear from temps + load; per-wheel scaling interpolated by on-line wetness', s: 'FuelConsumptionAndWear 0x4ED960', t: 'CONFIRMED' },
                { e: 'AI wet factor', d: 'AI corner-speed scale = max(0.5, 1 - wetness x RainAbility)', s: 'AIPushLap 0x42A180', t: 'OBSERVED' },
                { e: 'AI line choice', d: 'the AI moves toward the wp_path[1] line as grip drops; blend = 1 - average grip', s: 'docs/52 section 4.6', t: 'DERIVED' },
                { e: 'Surface grips', d: 'dry/wet per surface (table above); relative wet loss is largest on kerbs', s: 'terrain.ini [FEEDBACK]', t: 'FACT' }
            ]
        };
    }
    /* 4.4.0-rc.8 (Batch 2k task 3, DERIVED): blue-flag estimate. A lapped car X
     * (WB.lapsDown) is paired with a car Y that is at least one lap ahead on the lap
     * counter when Y is less than 1.0 s behind X on track. Metres = fraction of a lap
     * from Y to X times trkl (track length). Seconds = metres / Y.s (m/s; the tower
     * shows s * 3.6). Assumptions (lap fraction tp, speed unit, trkl unit) are not
     * yet checked live. This is an estimate, not the game's blue flag. */
    function blueFlags(cars, trkl) {
        var out = {}, list = cars || [], len = Number(trkl) || 0, lapped;
        if (!(len > 0) || list.length < 2) return out;
        lapped = WB.lapsDown(list);
        list.forEach(function (x) {
            if (!lapped[x.id] || !(x.tp2 >= 0)) return;
            list.forEach(function (y) {
                if (!y || y.id === x.id || !(y.p > 0) || y.rt || y.fs || !(y.tp2 >= 0)) return;
                if (!((y.l || 0) - (x.l || 0) >= 1)) return;
                /* tp2 = lap distance in metres (FACT +0x4044). `tp` is frozen
                 * track_pos, not a lap fraction (Claude review 2026-10-12). */
                var metres = (((x.tp2 - y.tp2) % len) + len) % len;
                if (!(y.s > 0) || !(metres > 0)) return;
                var t = metres / y.s;
                if (!(t < 1.0)) return;
                if (!out[x.id]) out[x.id] = { role: 'lapped', gap_s: t, by: y.id };
                if (!out[y.id]) out[y.id] = { role: 'lapping', gap_s: t, of: x.id };
            });
        });
        return out;
    }
    function blueTagOf(map, id) {
        var b = map && map[id];
        if (!b) return '';
        var what = b.role === 'lapped' ? 'lapped car, a lapping car is ' : 'lapping car, the lapped car is ';
        return ' <span class="blue" style="background:#2f7bff;color:#fff;padding:0 3px;border-radius:2px;font-size:10px" ' +
            'title="blue flag (estimated): ' + what + (Math.round(b.gap_s * 10) / 10) + ' s away (DERIVED)">BLUE est.</span>';
    }
    /* 4.4.0-rc.8 (Batch 2k task 4): old ?page= ids. A smoothed twin id opens its base
     * entry with smoothing on. forceSmooth is true only for a smoothed twin id. */
    function canonicalLegacy(id) {
        /* rc.11 weather: the legacy track_conditions.ini layout is replaced by
         * the Weather page; an old ?page=legacy:track_conditions.ini opens it. */
        if (id === 'legacy:track_conditions.ini') return { id: 'weather', forceSmooth: false };
        var f = typeof id === 'string' && id.indexOf('legacy:') === 0 ? id.slice(7) : null;
        if (f && SMOOTH_BASE[f]) return { id: 'legacy:' + SMOOTH_BASE[f], forceSmooth: true };
        return { id: id, forceSmooth: false };
    }
    function layoutFileFor(base, smooth) {
        return SMOOTH_PAIR[base] && smooth ? SMOOTH_PAIR[base] : base;
    }
    /* Gap from per-car cumulative race time tt (+0x4098, live-verified),
     * lap-aware: a lapped car shows +NL. */
    function gapTo(c, ref, raceSession) {
        if (!c || !ref) return '-';
        if (raceSession === false) return '-';
        if (c.id === ref.id) return 'LEADER';
        /* rc.9 (owner): a DNF car is not "lapped"; Status already says DNF. */
        if (c.rt || c.fs > 0) return '-';
        var dl = (ref.l || 0) - (c.l || 0);
        if (dl >= 1) return '+' + dl + 'L';
        if (!c.tt || !ref.tt || c.tt <= 0 || ref.tt <= 0) return '-';
        var g = c.tt - ref.tt;
        return g < -0.001 ? '-' : '+' + Math.abs(g).toFixed(3);
    }
    /* 4.4.0-rc.2 SECTOR SPLITS. The game's ls1/ls2 (and bs1/bs2) are CUMULATIVE
     * elapsed times from the line: ls2 = S1 + S2, ll = S1 + S2 + S3. So the
     * split S2 = ls2 - ls1, and S3 = ll - ls2. Every view uses these helpers.
     * bestS2Of is the best cumulative S2 minus best cumulative S1 (DERIVED: the
     * two bests may come from different laps). Finding
     * web-telemetry-sectors-cumulative-s2-20261012. */
    function s2Of(c) { return c.ls1 > 0 && c.ls2 > c.ls1 ? c.ls2 - c.ls1 : 0; }
    function s3Of(c) { return c.ll > 0 && c.ls2 > 0 && c.ll > c.ls2 ? c.ll - c.ls2 : 0; }
    /* Dossier laps from the server: rc.2 sends s2d/s3 split keys; an rc.1 server
     * does not, so fall back to deriving them from the cumulative s2 (same maths). */
    function lapS2(q) { return q.s2d !== undefined ? q.s2d : (q.s1 > 0 && q.s2 > q.s1 ? q.s2 - q.s1 : 0); }
    function lapS3(q) { return q.s2d !== undefined ? q.s3 : (q.t > 0 && q.s2 > 0 && q.t > q.s2 ? q.t - q.s2 : 0); }
    function bestS2Of(c) { return c.bs1 > 0 && c.bs2 > c.bs1 ? c.bs2 - c.bs1 : 0; }
    /* 4.4.0-rc.2 LAP NUMBER. The game's lap counter (car.l, lap_number) is 0 on
     * the grid and counts COMPLETED laps, so the lap being driven is l + 1 (the
     * game HUD shows that). Finding web-telemetry-lap-number-offset-20261012. */
    function lapNo(l) { return (l || 0) + 1; }
    function lifeShort(sec) {
        if (sec === undefined) return '-';
        if (sec < 0) return 'FAILED';
        if (sec === 0) return '—';
        if (sec >= 3600) return (sec / 3600).toFixed(1) + 'h';
        return Math.round(sec / 60) + 'm';
    }
    function lifeCol(pct) { return pct === undefined || pct < 0 ? '' : pct <= 15 ? 'color:#ff4136;font-weight:bold' : pct <= 35 ? 'color:#fc0' : ''; }
    function lifeTxt(sec, pct, lastlap) {
        if (sec === undefined) return '-';
        if (sec < 0) return 'FAILED';
        if (sec === 0) return '— (out for another reason)';
        var t = lifeShort(sec);
        if (pct >= 0) t += ' (' + pct + '%)';
        if (lastlap > 0 && sec > 0) t += ' ≈' + Math.floor(sec / lastlap) + ' laps';
        return t;
    }
    /* Tyre life % = the game's own (tread-0.75)*4 normalization. */
    function treadLife(v) { if (v === undefined || v === null) return null; return Math.max(0, Math.min(1, (v - 0.75) * 4)) * 100; }
    function lifeCol2(p) { return p === null ? '' : p <= 40 ? 'color:#ff4136' : p <= 70 ? 'color:#fc0' : ''; }
    function doomTag(c) { var w = DOOMWHY[c.dc]; return w ? (c.hr >= 3 ? 'ACCIDENT / ' : '') + w.tag : 'OUT'; }
    function doomWhy(c) {
        var w = DOOMWHY[c.dc], base = w ? w.txt : 'Terminal damage, cause not resolved.';
        if (c.dpr & 2) base += ' (Rear wing was already gone when telemetry attached, so no bulletin was raised for it.)';
        else if (c.dpr & 1) base += ' (Front wing was already gone when telemetry attached.)';
        if (c.hr > 0) base += ' Impacts this race: ' + c.hr + '.';
        if (c.rc > 0) base += ' Failure category: ' + (RCAT[c.rc] || 'code ' + c.rc) + '.';
        if (c.cb) base += ' Called to the pits.';
        return base;
    }
    function reasonTxt(c) {
        if (!c) return '-';
        if (c.rc > 0) return RCAT[c.rc] || 'code ' + c.rc;
        if (c.fs === 2 || c.fs === 3 || c.rt) return 'Not classified';
        /* on track (cars() keeps p > 0), no failure code, not out: running */
        return c.fs === 0 ? 'running' : '-';
    }
    /* rc.17: parked in the pit box / garage (pitting >= 3, standing still). In
     * practice, qualifying and warm-up that is the normal place to be. */
    function wxGarage(c) { return c && c.pt >= 3 && !(c.s > 1) && !c.rt && !(c.fs > 0); }
    function statusCell(c) {
        var st = c.fs === 1 ? 'FIN' : c.fs === 2 ? 'DNF' : c.fs === 3 ? 'DSQ' : c.rt ? 'RET' : c.pt >= 2 ? (wxGarage(c) ? 'GARAGE' : 'PIT') : c.cb ? 'BOX' : c.pn > 0 ? 'P' + c.pn : '';
        if (c.ed) return '<span class="f1x-tag" style="background:#ff4136" title="Engine stopped: rpm 0 and EngineAngVel 0 while still on track' +
            (c.rc === 1 ? ', classified ENGINE failure' : '') + '">ENGINE</span>' + st;
        if (c.dm) return '<span class="f1x-tag" style="background:#ff4136" title="' + esc(doomWhy(c)) + '">' + esc(doomTag(c)) + '</span>' + st;
        if (c.dt) { var D = DMG[c.dt] || DMG[0]; return '<span class="f1x-tag" style="background:' + D.c + '" title="' + esc(D.d) + '">' + D.t + '</span>' + st; }
        return st;
    }
    /* Race-control bulletin text from the server incident log (the game
     * exposes no race-control messages; same wording as the classic page). */
    function rcMsg(e, zones) {
        var n = esc(e.n), lp = ' (Lap ' + e.lp + ')', z = zones || '?';
        switch (e.ty) {
            case 'YELLOW': return { s: 's1', t: 'YELLOW FLAG — ZONE ' + (e.a + 1) + '/' + z + ' — SLOW OR STOPPED CAR' + lp };
            case 'GREEN': return { s: '', t: 'GREEN FLAG — ZONE ' + (e.a + 1) + '/' + z + ' CLEAR' + lp };
            case 'FLAG': return { s: 's1', t: 'FLAG STATE ' + e.b + ' — ZONE ' + (e.a + 1) + lp + ' — unmapped state, shown raw' };
            case 'START': return { s: '', t: 'RACE CONTROL — LIGHTS OUT, RACE STARTED' };
            case 'HALF': return { s: '', t: 'RACE CONTROL — HALFWAY POINT REACHED (Lap ' + e.lp + '/' + e.a + ')' };
            case 'CHEQUERED': return { s: '', t: 'RACE CONTROL — CHEQUERED FLAG, LEADER HAS FINISHED' + lp };
            case 'LIMITS': return { s: 's1', t: 'TRACK LIMITS — ' + n + ' — NOTED' + lp };
            case 'PENALTY': return { s: 's2', t: 'TIME PENALTY — ' + n + ' — COUNT NOW ' + e.b + ' — reason not exposed by the game' + lp };
            case 'PENALTY_TL': return { s: 's2', t: 'TIME PENALTY — ' + n + ' — COUNT NOW ' + e.b + ' — inferred TRACK LIMITS (same-tick correlation)' + lp };
            case 'CONTACT': return { s: 's1', t: 'CONTACT (heuristic) — ' + n + ' damage coincided with car ' + e.b + ' within 25m — not a decoded guilty/victim record' + lp };
            case 'BOXSOON': return { s: 'sI', t: 'PIT WATCH (heuristic) — ' + n + ' — our guess, not the game\'s call_to_box flag' + lp };
            case 'ENGINE': return { s: 's2', t: 'ENGINE — ' + n + ' — SEIZED ON IMPACT, CAR IS OUT' + lp };
            case 'FRONTWING': return { s: 's1', t: 'FRONT WING — ' + n + ' — DETACHED, CAR REPAIRABLE' + lp };
            case 'REARWING': return { s: 's2', t: 'REAR WING — ' + n + ' — DETACHED, CANNOT BE REPAIRED, RETIREMENT EXPECTED AT PIT BOX' + lp };
            case 'DAMAGE': return { s: e.b >= 2 ? 's2' : 's1', t: 'CAR DAMAGE — ' + n + ' — ' + (e.b >= 2 ? 'SEVERE / WHEEL LOSS' : 'DAMAGE REPORTED') + lp };
            case 'RETIRE': return e.b === 1 ? { s: '', t: 'CLASSIFICATION — ' + n + ' — CLASSIFIED FINISHER' + lp } :
                e.b === 3 ? { s: 's2', t: 'DISQUALIFICATION — ' + n + ' — DSQ' + lp } : { s: 's2', t: 'RETIREMENT — ' + n + ' — DID NOT FINISH' + lp };
            case 'PIT_IN': return { s: 'sI', t: 'PIT LANE — ' + n + ' — ENTERED PIT LANE' + lp };
            case 'PIT_OUT': return { s: 'sI', t: 'PIT LANE — ' + n + ' — EXITED PIT LANE' + lp };
            case 'RC_PIT': return { s: 'sI', t: 'RACE CONTROL — ' + n + ' — SUMMONED TO PITS' + lp };
            case 'RC_RETIRE': return { s: 's2', t: 'RACE CONTROL — ' + n + ' — BLACK-FLAGGED / FORCED RETIREMENT' + lp };
            case 'RC_MODE': return { s: e.b === 2 ? 's2' : e.b === 1 ? 's1' : '', t: 'RACE CONTROL — TRACK STATUS: ' + (['GREEN', 'VSC', 'RED FLAG'][e.b] || '?') };
        }
        return null;
    }

    /* Keyed row diff: rows stay alive across ticks so clicks are never lost. */
    function diffRows(tbody, store, ids, rowHtml, rowCls, onRow) {
        var prev = null;
        ids.forEach(function (id) {
            var tr = store[id];
            if (!tr) {
                tr = document.createElement('tr'); tr._h = '';
                tr.addEventListener('click', function (e) { onRow(id, e); });
                tr.addEventListener('dblclick', function (e) { onRow(id, e, true); });
                store[id] = tr;
            }
            var cls = rowCls(id); if (tr.className !== cls) tr.className = cls;
            var h = rowHtml(id); if (tr._h !== h) { tr.innerHTML = h; tr._h = h; }
            var want = prev ? prev.nextSibling : tbody.firstChild;
            if (want !== tr) tbody.insertBefore(tr, want);
            prev = tr;
        });
        Object.keys(store).forEach(function (k) {
            if (ids.indexOf(+k) < 0) { if (store[k].parentNode) store[k].parentNode.removeChild(store[k]); delete store[k]; }
        });
    }

    function Console(root, opts) {
        this.isConsole = true;
        this.focusId = null;
        this.incidents = [];
        this.pace = [];
        this.driverDoc = null;
        this.gen = null;
        this.legacyFile = null;
        this.mapState = { zoom: 1, panX: 0, panY: 0 };
        this.sideDom = {};         /* letter -> DOM of an extra side (C to F) */
        this.sideMemory = {};      /* letter -> {n, cn}: car remembered for that side */
        this.pendingSides = this.loadSides();   /* saved extra sides, restored once the roster is known */
        /* 1.2.0 A1.7 (owner): driver B can be switched off so only A is shown live. Remembered in this browser. */
        try { this.bOff = window.localStorage.getItem('f1x.bOff') === '1'; } catch (e) { this.bOff = false; }
        /* 1.0.1 review mode (/?session=<id>): every endpoint answers from the
         * archived session on disk; the live feed is not connected. A review
         * keeps every lap of a side, not the live view's last six. */
        this.review = opts && typeof opts.review === 'string' && /^[A-Za-z0-9_-]{8,47}$/.test(opts.review) ? opts.review : null;
        if (this.review) { WB.LIMITS.completedLapsKept = REVIEW_LAPS_KEPT; WB.LIMITS.historyPoints = 4000000; }
        /* 1.2.1: source (live or past) and the Recordings page filter. */
        this.source = this.review ? 'past' : 'live';
        this.past = null;
        /* 1.3.0 tape (TAPE_SPEC 3.1): one object holds the review clock, the Replay page state and the recording info.
         * Review tape frames do not feed the workbench: its laps come from /trace/data, and a second feed would duplicate them. */
        this.wbFeed = !this.review;
        this._rp = this.tape = this.tapeNew(this.review);
        if (this.review && !/^v2_[0-9a-f]{16}$/.test(this.review)) {
            this.tape.infoState = 'error'; this.tape.msgBad = true;
            this.tape.msg = 'This saved session has no replay data. Only recordings made by this version can be played.';
        }
        this.sessFilter = { q: '', mode: 'all' };
        this.sessionsDoc = null;
        Viewer.call(this, root, opts);
    }
    var REVIEW_LAPS_KEPT = 200;
    Console.prototype = Object.create(Viewer.prototype);
    Console.prototype.constructor = Console;

    /* 4.4.0-rc.6 task 6: menu groups in the order of the Batch 2g brief. Menu order = arrow order (no wrap). */
    var MENU_GROUPS = ['Race', 'Driver & Lap', 'Car', 'Tyres', 'Chassis & Suspension', 'Conditions', 'Custom', 'My layouts', 'Data'];   /* 1.2.5: My layouts */
    var ANALYSIS_GROUP = { overview: 'Driver & Lap', braking: 'Driver & Lap', compare: 'Driver & Lap', dynamics: 'Chassis & Suspension',
        research: 'Chassis & Suspension', tyres: 'Tyres', live: 'Car' };
    var LEGACY_GROUP = { 'overview.ini': 'Driver & Lap', 'time_diff.ini': 'Driver & Lap', 'basic.ini': 'Driver & Lap', 'track.ini': 'Driver & Lap',
        'chassis_gears.ini': 'Car', 'chassis_inputs.ini': 'Car',
        'chassis_gxyz.ini': 'Chassis & Suspension', 'chassis_gg.ini': 'Chassis & Suspension', 'chassis_slip.ini': 'Chassis & Suspension',
        'chassis_rideheight.ini': 'Chassis & Suspension', 'chassis_rideheight_filtered.ini': 'Chassis & Suspension',
        'dampers_all.ini': 'Chassis & Suspension', 'dampers_all_filtered.ini': 'Chassis & Suspension', 'susp_deflection_all.ini': 'Chassis & Suspension',
        'tyre_temp_all.ini': 'Tyres', 'tyre_temp_front.ini': 'Tyres', 'tyre_temp_rear.ini': 'Tyres', 'tyre_temp.ini': 'Tyres', 'tyre_wear.ini': 'Tyres', 'wheels_slip.ini': 'Tyres',
        'track_conditions.ini': 'Conditions', '_revert_layout.ini': 'Custom', '_single_layout.ini': 'Custom' };
    /* 4.4.0-rc.8 (Batch 2k task 4): one menu entry per raw/smoothed layout pair. */
    var SMOOTH_PAIR = { 'dampers_all.ini': 'dampers_all_filtered.ini', 'chassis_rideheight.ini': 'chassis_rideheight_filtered.ini' };
    var SMOOTH_BASE = {};
    Object.keys(SMOOTH_PAIR).forEach(function (k) { SMOOTH_BASE[SMOOTH_PAIR[k]] = k; });
    Console.prototype.pages = function () {
        var out = [];
        RACE_PAGES.forEach(function (p) { out.push({ id: p[0], name: p[1], group: 'Race' }); });
        ANALYSIS_PAGES.forEach(function (p) { out.push({ id: p[0], name: p[1], group: ANALYSIS_GROUP[p[0]] || 'Custom' }); });
        out.push({ id: 'engine', name: 'Engine Live', group: 'Car' });
        out.push({ id: 'chassis', name: 'Car Chassis Live', group: 'Car' });   /* 1.1.0 */
        out.push({ id: 'weather', name: 'Weather', group: 'Conditions' });   /* rc.11 weather */
        out.push({ id: 'sessions', name: 'Recordings', group: 'Data' });   /* 1.1.0: Recordings manager (API 9) */
        out.push({ id: 'replay', name: 'Replay', group: 'Data' });   /* 1.2.2: replay of a finished v2 recording */
        (WB.LAYOUTS || []).forEach(function (l) {
            if (SMOOTH_BASE[l.file]) return; /* smoothed twin: reached by the base entry's toggle */
            if (l.file === 'track_conditions.ini') return;   /* rc.11 weather: replaced by the Weather page */
            if (l.file === '_revert_layout.ini' || l.file === '_single_layout.ini') return;   /* A1.5 (owner): "current layout" and "single graph layout" removed */
            var item = { id: 'legacy:' + l.file, name: String(l.desc || l.name).replace(/^\[Advanced\]\s*-?\s*/, ''), group: LEGACY_GROUP[l.file] || 'Custom' };
            if (SMOOTH_PAIR[l.file]) item.pair = true;
            out.push(item);
        });
        /* 1.2.5 (DVOA report 2026-10-18): the user's own layouts and their editor */
        (WB.userLayouts ? WB.userLayouts() : []).forEach(function (l) {
            out.push({ id: 'legacy:' + l.file, name: l.name, group: 'My layouts' });
        });
        out.push({ id: 'layouts', name: 'Layout editor', group: 'My layouts' });
        var rank = function (g) { var i = MENU_GROUPS.indexOf(g); return i < 0 ? 99 : i; };
        return out.map(function (p, i) { return { p: p, i: i }; })
            .sort(function (a, b) { return rank(a.p.group) - rank(b.p.group) || a.i - b.i; })
            .map(function (x) { return x.p; });
    };

    Console.prototype.pref = function (k, d) {
        try { return (window.localStorage && window.localStorage.getItem('f1x.' + k)) || d; } catch (e) { return d; }
    };
    Console.prototype.setPref = function (k, v) {
        /* TAPE2 V5: a review never stores the live page (Back to live returns to it). */
        if (k === 'page' && this.review) return;
        try { if (window.localStorage) window.localStorage.setItem('f1x.' + k, v); } catch (e) { /* private mode */ }
    };

    Console.prototype.build = function () {
        var self = this;
        setPalette(LEGACY_PAL);
        if (WB.setTheme) WB.setTheme(LEGACY_WB);
        if (!document.getElementById('tv-style')) {
            var st = el('style'); st.id = 'tv-style'; st.textContent = CSS;
            document.head.appendChild(st);
        }
        if (!document.getElementById('f1x-style')) {
            var st2 = el('style'); st2.id = 'f1x-style'; st2.textContent = CONSOLE_CSS + SRC_CSS + REPLAY_CSS;
            document.head.appendChild(st2);
        }
        if (!document.getElementById('tvd-style')) {
            var st3 = el('style'); st3.id = 'tvd-style'; st3.textContent = TVD_CSS;
            document.head.appendChild(st3);
        }
        if (document.title.indexOf('Telemetry') < 0) document.title = 'F1 Challenge 99-02 Telemetry';
        var r = this.root;
        r.innerHTML = '';
        r.className = 'tv f1x';
        r.classList.add(this.isPast() ? 'f1x-src-past' : 'f1x-src-live');

        /* title bar */
        /* 1.2.0 top bar redesign (Claude, owner F2): brand | LIVE/PAST | session line | Telemetry/TV Director | status, help, clock */
        var top = el('div', 'f1x-title');
        top.appendChild(el('div', 'f1x-brand', '<i></i><b>F1C</b><span>TELEMETRY</span>'));
        /* 1.2.1: one always-visible source switch: LIVE (this game session) or PAST SESSION (a stored recording). */
        var srcBox = el('div', 'f1x-srcsw');
        this.srcLiveBtn = el('button', 'f1x-src live' + (this.isPast() ? '' : ' on'), 'LIVE');
        this.srcLiveBtn.title = 'Watch the game that is running now: the live lap and this session\'s laps';
        this.srcLiveBtn.onclick = function () { self.goLive(); };
        this.srcPastBtn = el('button', 'f1x-src past' + (this.isPast() ? ' on' : ''), 'PAST');
        this.srcPastBtn.title = 'Watch an earlier recording. Opens the Recordings page to pick one.';
        this.srcPastBtn.onclick = function () { self.goPast(); };
        srcBox.appendChild(this.srcLiveBtn); srcBox.appendChild(this.srcPastBtn);
        top.appendChild(srcBox);
        /* session line: what is shown (live track and session, or the past recording) */
        this.hdrSess = el('div', 'f1x-hsess', '');
        top.appendChild(this.hdrSess);
        /* TV Director phase 0 (read-only): top-level tabs. The Telemetry tab is the page set below, unchanged. */
        var tabs = el('div', 'f1x-tabs');
        this.topTab = 'tel';
        this.tabTel = el('button', 'f1x-tab on', 'Telemetry'); this.tabTel.title = 'Telemetry pages';
        this.tabTel.onclick = function () { self.setTopTab('tel'); };
        this.tabTv = el('button', 'f1x-tab', 'TV Director'); this.tabTv.title = 'TV Director: TV camera control for the game on this PC';
        this.tabTv.onclick = function () { self.setTopTab('tv'); };
        tabs.appendChild(this.tabTel); tabs.appendChild(this.tabTv);
        top.appendChild(tabs);
        var tr = el('div', 'f1x-tr');
        this.liveEl = el('span', 'f1x-live off', 'NO DATA');
        this.clockEl = el('span', '', '');
        var help = el('button', 'f1x-help', '?'); help.title = 'Help';
        help.onclick = function () { self.showHelp(); };
        tr.appendChild(this.liveEl); tr.appendChild(this.clockEl); tr.appendChild(help);
        top.appendChild(tr);
        r.appendChild(top);
        /* 1.2.1: one line under the header: the LIVE status, or the PAST banner with Change and Back to live. */
        this.srcLine = el('div', 'f1x-srcline');
        this.srcLine.addEventListener('click', function (e) {
            var t = e.target && e.target.closest ? e.target.closest('[data-act]') : null, a = t && t.getAttribute('data-act');
            if (a === 'open-past') self.goPast();
            else if (a === 'change') self.goChange();
            else if (a === 'live') self.goLive();
        });
        r.appendChild(this.srcLine);

        /* lap strip */
        this.lapStrip = el('div', 'f1x-laps');
        this.lapRows = {};
        ['A', 'B'].forEach(function (L) {
            var row = lapRowEl();
            self.lapStrip.appendChild(row);
            self.lapRows[L] = row;
        });
        this.refEl = el('div', 'f1x-lref');
        this.lapStrip.appendChild(this.refEl);
        r.appendChild(this.lapStrip);

        /* layout bar */
        var bar = el('div', 'f1x-bar');
        var prev = el('button', 'f1x-arrow', '&#8592;'); prev.title = 'Previous page (Page Up)';
        var next = el('button', 'f1x-arrow', '&#8594;'); next.title = 'Next page (Page Down)';
        prev.onclick = function () { self.stepPage(-1); };
        next.onclick = function () { self.stepPage(1); };
        this.buildPageMenu();
        bar.appendChild(prev); bar.appendChild(next); bar.appendChild(this.pick);
        bar.appendChild(el('span', 'f1x-sep'));

        /* A/B pickers: same element contract as Viewer.build (carEls) so the
         * base refresh, freeze and selection code runs unchanged. The lap
         * select stays hidden: the lap strip is the lap picker here. */
        this.carEls = {};
        ['A', 'B'].forEach(function (L) {
            var lab = el('label', L, L);
            var srcTag = el('span', 'f1x-srctag', 'Live');
            lab.appendChild(srcTag);
            var carSel = el('select', 'tv-sel'); carSel.title = 'Car for side ' + L;
            var lapSel = el('select'); lapSel.style.display = 'none';
            var frz = el('button', 'tv-btn', 'Freeze ' + L); frz.title = 'Pin side ' + L + '; recording continues (' + L + ')';
            carSel.onchange = function () {
                if (L === 'B' && carSel.value === 'off') { self.setBOff(true); return; }
                var id = parseInt(carSel.value, 10);   /* read first: setBOff refills the list */
                if (L === 'B' && self.bOff) self.setBOff(false, true);
                var meta = self.roster.filter(function (q) { return q.id === id; })[0];
                self.wb.select(L, id, meta); self.lapMode[L] = 'auto'; self.cache = {}; self.invalidate();
                if (self.view === 'legacy' && self.legacyUi) self.legacyUi.refreshSelection();
            };
            lapSel.onchange = function () { self.pickLap(L, lapSel.value); };
            frz.onclick = function () {
                var side = self.side(L);
                if (side.frozenSnapshot) side.resume(); else side.freeze();
                self.invalidate();
            };
            var grpD = el('span', 'f1x-drv ' + L);
            grpD.appendChild(lab); grpD.appendChild(carSel); grpD.appendChild(lapSel); grpD.appendChild(frz);
            bar.appendChild(grpD);
            var hid = el('span'); hid.style.display = 'none';
            var nm = el('span'), sub = el('span'), stt = el('span'), lt = el('span');
            [nm, sub, stt, lt].forEach(function (n) { hid.appendChild(n); });
            bar.appendChild(hid);
            self.carEls[L] = { root: hid, car: carSel, lap: lapSel, frz: frz, nm: nm, sub: sub, st: stt, lt: lt, src: srcTag };
        });

        /* extra sides (C to F) go between Freeze B and this button */
        this.bar = bar;
        this.addBtn = el('button', 'tv-btn', '+ Driver');
        this.addBtn.title = 'Add another driver as an extra side (C to F; six sides in all)';
        this.addBtn.onclick = function () {
            var L = self.nextFreeLetter();
            if (!L) self.toast('Six sides at most: remove one first');
            else self.addSide(L);
        };
        bar.appendChild(this.addBtn);
        this.xBtn = el('button', 'tv-btn', 'X: distance');
        this.xBtn.title = 'Lap distance or lap time on the x axis (D)';
        this.xBtn.onclick = function () { self.toggleX(); };
        this.resBtn = el('button', 'tv-btn res', 'Research');
        this.resBtn.title = 'Show RESEARCH candidates: unproven meaning and unit (R)';
        this.resBtn.onclick = function () { self.setResearch(!self.research); };
        var imp = el('button', 'tv-btn', 'Import');
        imp.title = 'Load a saved lap file (saved from this page, or an old .lap / .spt file) into car A or B';
        this.fileIn = el('input'); this.fileIn.type = 'file'; this.fileIn.accept = '.json,.lap,.spt'; this.fileIn.style.display = 'none';
        imp.onclick = function () { self.fileIn.click(); };
        this.fileIn.onchange = function () { self.importFile(); };
        var exp = el('button', 'tv-btn', 'Export'); exp.title = 'Export laps: drivers or a team, all completed laps, JSON or CSV';
        exp.onclick = function () { self.openExportDialog(); };
        var tools = el('span', 'f1x-tools');
        [this.xBtn, this.resBtn, imp, exp].forEach(function (b) { tools.appendChild(b); });
        bar.appendChild(tools);
        bar.appendChild(this.fileIn);
        r.insertBefore(bar, this.lapStrip);   /* 1.2.0: toolbar right under the header, lap strip below it */
        this.showEl = el('div', 'f1x-show');
        r.appendChild(this.showEl);

        /* stage */
        this.navBtns = {};
        this.viewEl = el('div', 'tv-view f1x-stage');
        r.appendChild(this.viewEl);
        this.tapeBar = this.buildTapeBar();   /* 1.3.0: the transport bar, shown in review only (refreshChrome) */
        r.appendChild(this.tapeBar);

        /* status bar */
        var status = el('div', 'f1x-status');
        this.sessEl = el('div', 'tv-sess');
        this.foot = el('div', 'tv-foot');
        status.appendChild(this.sessEl); status.appendChild(this.foot);
        r.appendChild(status);
        /* TV Director phase 0: the telemetry parts are hidden on the TV tab; the panel is built once. */
        this.telParts = [this.lapStrip, bar, this.showEl, this.viewEl, status];
        this.tvEl = this.buildTv();
        this.tvEl.style.display = 'none';
        r.appendChild(this.tvEl);
        /* 1.2.1: the TV page message in past mode (CSS .tvd-pastmode shows only this note). */
        this.tvPastNote = el('div', 'tvd-pastnote', 'TV Director controls the live game. Switch to LIVE to use it.');
        this.tvEl.insertBefore(this.tvPastNote, this.tvEl.firstChild);
        this.tvEl.classList.toggle('tvd-pastmode', this.isPast());
        this.tvProbe();

        document.addEventListener('keydown', function (e) {
            if (self.topTab === 'tv') return;   /* TV Director tab: no telemetry page keys */
            if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) return;
            var k = e.key;
            if (self.tape.kind === 'rec' && self.rpKey(e)) return;   /* 1.3.0: the tape keys work on every page in review */
            if (k === 'PageUp' || k === 'ArrowLeft') { self.stepPage(-1); e.preventDefault(); }
            else if (k === 'PageDown' || k === 'ArrowRight') { self.stepPage(1); e.preventDefault(); }
            else if (k === 'd' || k === 'D') self.toggleX();
            else if (k === 'r' || k === 'R') self.resBtn.click();
            else if (k === 'Escape') self.resetZoom();
            else if (k === 'a' || k === 'A') self.carEls.A.frz.click();
            else if (k === 'b' || k === 'B') self.carEls.B.frz.click();
        });
        window.addEventListener('resize', function () { self.invalidate(); });
        setInterval(function () {
            var n = new Date();
            self.clockEl.textContent = n.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        }, 1000);
        this.tapePaintBar();
        this.startPolls();
        var start = this.pref('page', 'race'), sl = canonicalLegacy(start);
        if (sl.forceSmooth) { this.setPref('smooth:' + sl.id.slice(7), '1'); start = sl.id; }
        /* 1.2.2: a v2 recording opened without a page, or with page=replay, starts on Replay (other page= values keep the stored page). */
        if (this.review && /^v2_/.test(this.review) && (!/[?&]page=/.test(location.search) || /[?&]page=replay(&|$)/.test(location.search))) start = 'replay';
        /* TAPE2 V5: a live load with ?page=<id> (from Back to live or Change) opens that page once. */
        var lp = !this.review ? /[?&]page=([^&]+)/.exec(location.search) : null;
        if (lp) start = decodeURIComponent(lp[1]);
        if (!this.pages().some(function (p) { return p.id === start; })) start = 'race';
        this.setPage(start);
        this.loop();
    };

    /* ---- TV Director, phase 1 (2026-10-13). Reads: GET /director/state (4 Hz while this tab is shown) and GET /telemetry
     * (about 1 Hz, driver list). Writes: POST /director/cut, /director/angle and /director/restore only, with the header
     * X-Telemetry-Action: director, and only when the game PC answers write_enabled. No confirm, alert or prompt.
     * Spec: TV_DIRECTOR_SPEC_2026-10-13.md 3.2 (a, b, c, d, f) and 6.3. Phase 2 (2026-10-14): leader gap column (gapTo), Cut to
     * another car needs CarSwitch=1 in the game (tvdCutBlock), next-frame verify text. Battle flag is still a placeholder. ---- */
    var TVD_FIXED = ['NOSECAM', 'COCKPIT', 'TVCOCKPIT', 'SWINGMAN'];
    var TVD_POLL_MS = 250;
    var TVD_TEL_EVERY = 4;
    var TVD_DASH = '—';
    /* 1.2.0 TV Director redesign (Claude, owner T3): on-air card, drivers on the left, camera tiles + controls on the right,
     * technical rows in a closed Details block. Same element contract as phase 2 (tvStatus, tvAir, tvTable, tvNames, buttons). */
    var TVD_CSS = [
        '.tvd{flex:1 1 auto;min-height:0;overflow:auto;padding:14px 18px;font:13px/1.45 Tahoma,Verdana,Arial,sans-serif;color:#dfe7ff;background:#000}',
        '.tvd h2{display:none}',
        '.f1x > .tvd{grid-row:3 / -1}',   /* the TV panel fills every row under the header and the source line */
        '.tvd-air-card{display:flex;align-items:center;gap:18px;padding:12px 16px;border:1px solid #1d3fa8;border-left:4px solid #e10600;border-radius:4px;background:#040b26;margin:0 0 12px}',
        '.tvd-onair{flex:none;font:bold 11px Tahoma,Verdana,sans-serif;letter-spacing:.12em;color:#fff;background:#e10600;padding:3px 8px;border-radius:2px}',
        '.tvd-onair.off{background:#2b3a5a;color:#9fb2e6}',
        '.tvd-air-main{flex:1 1 auto;min-width:0}',
        '.tvd-air-drv{font:bold 20px Tahoma,Verdana,sans-serif;color:#fff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tvd-air-sub{color:#9fb2e6;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tvd-air-cam{flex:none;text-align:right}',
        '.tvd-air-cam b{display:block;font:bold 16px Tahoma,Verdana,sans-serif;color:#ffd84a}',
        '.tvd-air-cam span{color:#5f7fc4;font-size:11px}',
        '.tvd-pills{display:flex;gap:6px;flex-wrap:wrap;align-items:center;margin:0 0 12px}',
        '.tvd-pill{font:bold 10px Tahoma,Verdana,sans-serif;letter-spacing:.06em;padding:3px 8px;border-radius:999px;background:#14307f;color:#9fb2e6}',
        '.tvd-pill.ok{background:#1f9d55;color:#fff}.tvd-pill.warn{background:#c98a10;color:#1a1200}',
        '.tvd-switch{display:inline-flex;align-items:center;gap:7px;margin-left:auto;padding:2px 10px 2px 4px;border:1px solid #2a43a8;border-radius:999px;background:#040b26;color:#dfe7ff;font:bold 10px Tahoma,Verdana,sans-serif;letter-spacing:.04em;cursor:pointer}',
        '.tvd-switch i{width:26px;height:14px;border-radius:7px;background:#2b3a5a;position:relative;display:inline-block}',
        '.tvd-switch i::after{content:"";position:absolute;left:2px;top:2px;width:10px;height:10px;border-radius:50%;background:#9fb2e6;transition:left .12s}',
        '.tvd-switch.on i{background:#1f9d55}.tvd-switch.on i::after{left:14px;background:#fff}',
        '.tvd-switch:disabled{opacity:.45;cursor:not-allowed}',
        '.tvd-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) 360px;gap:14px;align-items:start}',
        '.tvd-map{height:380px;position:relative}.tvd-map canvas{width:100%;height:100%;display:block}',
        '.tvd-events{max-height:260px;overflow:auto;font-size:12px}',
        '.tvd-events div{display:flex;gap:8px;padding:3px 0;border-bottom:1px solid #0a1a52}',
        '.tvd-events .lp{flex:none;width:34px;color:#5f7fc4}',
        '.tvd-events .new{color:#fff;font-weight:bold}',
        '.tvd-card{border:1px solid #14307f;border-radius:4px;background:#02061a;padding:10px 12px}',
        '.tvd-card+.tvd-card{margin-top:12px}',
        '.tvd h3{margin:0 0 8px;font:bold 11px Tahoma,Verdana,sans-serif;letter-spacing:.1em;text-transform:uppercase;color:#9fb2e6}',
        '.tvd-note{margin:0 0 8px;color:#ffd84a;font-size:12px}',
        '.tvd-status{min-height:1.45em;margin:4px 0;font-size:12px;color:#9fb2e6}',
        '.tvd-bad{color:#ff8a8a}',
        '.tvd-row{display:flex;gap:10px;margin:2px 0;font-size:12px}',
        '.tvd-k{width:130px;flex:0 0 130px;color:#5f7fc4;font-size:11px;text-transform:uppercase;letter-spacing:.04em;padding-top:1px}',
        '.tvd-v{color:#dfe7ff}',
        '.tvd-table{border-collapse:collapse;width:100%;font-size:12px}',
        '.tvd-table th{padding:4px 8px;text-align:left;color:#5f7fc4;font:bold 10px Tahoma,Verdana,sans-serif;letter-spacing:.06em;text-transform:uppercase;border-bottom:1px solid #1d3fa8;white-space:nowrap}',
        '.tvd-table td{padding:4px 8px;border-bottom:1px solid #0a1a52;text-align:left;white-space:nowrap}',
        '.tvd-table td:first-child{width:34px;font-weight:bold;color:#fff;text-align:right}',
        '.tvd-table td:nth-child(2){font-weight:bold;color:#fff}',
        '.tvd-table tr{cursor:pointer}',
        '.tvd-table tr:hover td{background:#071640}',
        '.tvd-table tr.sel td{background:#1d3fa8;color:#fff}',
        '.tvd-table tr.air td{background:#2a0606;color:#fff;font-weight:bold}',
        '.tvd-table tr.air td:first-child{box-shadow:inset 4px 0 0 #e10600}',
        '.tvd-table tr.air td:nth-child(2)::after{content:"ON AIR";margin-left:8px;padding:1px 5px;border-radius:2px;background:#e10600;color:#fff;font:bold 9px Tahoma,Verdana,sans-serif;letter-spacing:.08em;vertical-align:1px}',
        '.tvd-table tr.air.sel td{background:#1d3fa8}',
        '.tvd li.dis{opacity:.35;cursor:not-allowed}',
        '.tvd-table tr.pit td{color:#7fd4ff}',
        '.tvd ul{list-style:none;margin:0;padding:0;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:6px}',
        '.tvd li{position:relative;padding:6px 26px 6px 10px;border:1px solid #1d3fa8;border-radius:3px;background:#040b26;cursor:pointer;min-height:34px}',
        '.tvd li:hover{border-color:#38d6e8}',
        '.tvd li b{display:block;font-size:12px;color:#fff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tvd li small{display:block;font-size:10px;color:#5f7fc4;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
        '.tvd li.on{border-color:#e10600;box-shadow:inset 3px 0 0 #e10600}',
        '.tvd li.sel{background:#1d3fa8;border-color:#38d6e8}',
        '.tvd li.sel small{color:#c9d6ff}',
        '.tvd li .ren{position:absolute;right:4px;top:4px;width:18px;height:18px;padding:0;border:0;border-radius:2px;background:transparent;color:#5f7fc4;cursor:pointer;font-size:11px;line-height:18px}',
        '.tvd li .ren:hover{color:#fff;background:#14307f}',
        '.tvd li input{width:100%;box-sizing:border-box;font:12px Tahoma,Verdana,sans-serif;background:#000;color:#fff;border:1px solid #38d6e8;padding:2px 4px}',
        '.tvd-actions{margin:12px 0 6px;display:grid;grid-template-columns:1fr 1fr;gap:6px}',
        '.tvd-actions button,.tvd-goto button{padding:7px 10px;font:bold 12px Tahoma,Verdana,sans-serif;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#dfe7ff;cursor:pointer}',
        '.tvd-actions button:hover,.tvd-goto button:hover{border-color:#38d6e8;color:#fff}',
        '.tvd-actions button.primary{grid-column:1 / -1;background:#e10600;border-color:#ff4a3d;color:#fff;font-size:13px;padding:9px 10px}',
        '.tvd-actions button:disabled,.tvd-goto button:disabled{opacity:.4;cursor:not-allowed}',
        '.tvd-goto{display:flex;align-items:center;gap:6px;margin:6px 0;font-size:12px;color:#9fb2e6}',
        '.tvd-goto input{width:70px;font:12px Tahoma,Verdana,sans-serif;background:#000;color:#fff;border:1px solid #2a43a8;border-radius:3px;padding:5px 6px}',
        '.tvd details{margin-top:12px;border:1px solid #14307f;border-radius:4px;background:#02061a;padding:6px 12px}',
        '.tvd summary{cursor:pointer;color:#5f7fc4;font:bold 10px Tahoma,Verdana,sans-serif;letter-spacing:.1em;text-transform:uppercase;padding:4px 0}',
        '@media (max-width:1400px){.tvd-grid{grid-template-columns:minmax(0,1fr) 360px}.tvd-grid>div:nth-child(2){grid-column:1 / -1;grid-row:2}}',
        '@media (max-width:1000px){.tvd-grid{grid-template-columns:1fr}}'
    ].join('\n');

    function tvdCircuit(j) { return !!j && (j.view_owner === 'circuit_static' || j.view_owner === 'circuit_tracking'); }
    function tvdAngleText(j) {
        if (!j) return '-';
        if (tvdCircuit(j)) return 'Circuit camera (trackside)';
        if (j.view_owner === 'unknown') return 'unknown';
        return j.angle ? j.angle : '-';
    }

    /* Pure: the on-air rows for one /director/state document (also used by viewer_test.js). */
    function tvdLines(j) {
        var f = j && j.followed, has = !!(j && j.mode !== null && j.mode !== undefined);
        var out = [];
        out.push({ k: 'Car', v: f ? (f.car_name || '(no name)') : '-' });
        out.push({ k: 'Driver', v: f ? (f.driver_name || '(no name)') + '  roster ' + f.roster_id : '-' });
        out.push({ k: 'Angle', v: tvdAngleText(j) });
        out.push({ k: 'Mode / index', v: has ? 'mode ' + j.mode + '  index ' + j.index + '  count ' + j.count : '-' });
        out.push({ k: 'Camera engine', v: (j && j.camera_engine) || '-', bad: !!(j && j.camera_engine === 'refused'),
                   note: (j && j.camera_engine_reason) || '' });
        out.push({ k: 'Sample', v: j ? 'age ' + j.sample_age_ms + ' ms  sim ' + j.sim_time_s + ' s  seq ' + j.capture_sequence +
                   (j.game_running ? '' : '  (no live sample)') : '-' });
        out.push({ k: 'Director stored', v: tvdCircuit(j) && j.director_mode_raw !== null && j.director_mode_raw !== undefined
                   ? 'mode ' + j.director_mode_raw + ' (not on screen)' : '-' });
        return out;
    }

    /* Pure (phase 1). Angle name from the names list -> the key the DLL takes: fixed names as they are, ONBOARD names with
     * the ONBOARD: prefix (web_telemetry director_state.inc dcam_angle_key). */
    function tvdAngleKey(name) { return TVD_FIXED.indexOf(name) >= 0 || name === 'CIRCUIT' ? name : 'ONBOARD:' + name; }

    /* Pure. Pit lane = pitting enum 2 (entering), 3 (stopped in box), 4 (exiting); see web_telemetry.c pitting enum. */
    function tvdPitText(pt) { return pt >= 2 ? 'PIT LANE' : ''; }

    /* Pure. Driver rows from one /telemetry document: live cars with a position, sorted by position. Gaps are metres to
     * the car ahead and behind on track (ga/gb with gai/gbi), not time gaps. */
    function tvdDrivers(tel) {
        var cars = (tel && tel.c) || [], byId = {}, out = [], leader = null;
        cars.forEach(function (c) { byId[c.id] = c; if (!leader && c.p === 1) leader = c; });
        cars.forEach(function (c) {
            if (!(c.p > 0) || !c.n) return;
            var a = c.gai >= 0 ? byId[c.gai] : null, b = c.gbi >= 0 ? byId[c.gbi] : null;
            out.push({ id: c.id, name: c.n, team: teamOf(c), pos: c.p, lap: c.l, tt: c.tt, rt: c.rt, fs: c.fs,
                       gap: tvdGapText(c, leader),
                       ahead: a ? a.n : '', aheadM: (c.gai >= 0 && a) ? c.ga : null,
                       behind: b ? b.n : '', behindM: (c.gbi >= 0 && b) ? c.gb : null,
                       pit: c.pt >= 2, pitText: tvdPitText(c.pt) });
        });
        out.sort(function (x, y) { return x.pos - y.pos; });
        return out;
    }

    /* Pure (phase 2). Leader gap cell: gapTo (the Telemetry page Leader column rules) against the car at position 1.
     * The value is the gap at each car's last line crossing (race clock tt), not a live gap. No leader: dash. */
    function tvdGapText(c, leader) { return leader ? gapTo(c, leader, true) : TVD_DASH; }

    /* Pure. What one key does on the TV tab. count = rows listed, sel = selected row index (-1 = none). */
    function tvdKeyAction(key, count, sel) {
        if (key === 'ArrowUp') return { select: Math.max(0, sel - 1) };
        if (key === 'ArrowDown') return { select: Math.min(Math.max(0, count - 1), sel + 1) };
        if (/^[1-4]$/.test(key)) return { angle: TVD_FIXED[Number(key) - 1] };
        if (key === 'Enter') return { cut: true };
        return null;
    }

    /* Pure. Request body for one command. A cut by race position (2026-10-14) sends { position, angle } instead of the driver. */
    function tvdBody(kind, driver, angleKey, position) {
        if (kind === 'cut' && position) return { position: position, angle: angleKey };
        if (kind === 'cut') return { driver: driver, angle: angleKey };
        if (kind === 'angle') return { angle: angleKey };
        return {};
    }

    /* Pure (2026-10-14). A race position typed in the box: a whole number from 1 to 104, else 0 (nothing is sent). */
    function tvdPositionValue(text) {
        var s = String(text === undefined || text === null ? '' : text).replace(/^\s+|\s+$/g, '');
        if (!/^[0-9]{1,3}$/.test(s)) return 0;
        var n = parseInt(s, 10);
        return (n >= 1 && n <= 104) ? n : 0;
    }

    /* Pure (2026-10-14). TV hotkeys (1-4, arrows, Enter) work only when the key does not go to a text field, select or
     * editable element. */
    function tvdHotkeyAllowed(target) {
        if (!target) return true;
        if (target.isContentEditable) return false;
        return !/^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName || '');
    }

    /* Pure (phase 2). The next-frame verify of a command in plain words. null (nothing was written) says nothing. */
    function tvdVerifyText(v) {
        if (v === 'pending') return 'checking...';
        if (v === true) return 'confirmed on the next frame';
        if (v === false) return 'not confirmed on the next frame';
        return '';
    }

    /* Pure (phase 2). Why a Cut is not sent, or '' when it may go. With [Director] CarSwitch=0 the game refuses any change
     * of car, so a Cut to a car other than the one on screen is not sent. Cuts to the car on screen and angle changes are not
     * affected. */
    function tvdCutBlock(j, driverId) {
        if (!j || j.carswitch_enabled !== false) return '';
        if (j.followed && j.followed.roster_id === driverId) return '';
        return 'Car switch is off: set [Director] CarSwitch=1 in web_telemetry.ini. Nothing was sent.';
    }

    /* Pure. One line for the result of a POST: status, reason, the read-back and the next-frame verify. */
    function tvdResultText(x) {
        var j = x && x.j;
        if (!j) return 'HTTP ' + (x ? x.code : '?') + ': no result from the game PC. Nothing was confirmed.';
        if (!j.status) return 'HTTP ' + x.code + ': ' + (j.error || 'refused') + '. Nothing was sent to the game.';
        var rb = j.readback ? '  (read back: mode ' + j.readback.mode + ', index ' + j.readback.index + ')' : '';
        var vf = tvdVerifyText(j.verified_next_frame);
        return j.status + ': ' + (j.reason || '') + rb + (vf ? ' - ' + vf : '') + '  [game thread ' + (j.thread_id || '-') + ']';
    }

    Console.prototype.buildTv = function () {
        var self = this, box = el('div', 'tvd');
        box.appendChild(el('h2', '', 'TV Director'));
        /* on-air card: what the TV shows now */
        this.tvAirCard = el('div', 'tvd-air-card',
            '<span class="tvd-onair off">ON AIR</span><div class="tvd-air-main"><div class="tvd-air-drv">-</div>' +
            '<div class="tvd-air-sub">No game state yet.</div></div><div class="tvd-air-cam"><b>-</b><span></span></div>');
        box.appendChild(this.tvAirCard);
        this.tvPills = el('div', 'tvd-pills');
        this.tvPills.addEventListener('click', function (e) {
            var b = e.target && e.target.closest ? e.target.closest('[data-opt]') : null;
            if (b && !b.disabled) self.tvSetOption(b.getAttribute('data-opt'), !b.classList.contains('on'));
        });
        box.appendChild(this.tvPills);
        this.tvStatus = el('div', 'tvd-status');
        box.appendChild(this.tvStatus);
        var grid = el('div', 'tvd-grid'), left = el('div', 'tvd-card'), mid = el('div'), right = el('div');
        /* owner: the TV Director shows the track and what is going on */
        var mapCard = el('div', 'tvd-card tvd-mapcard');
        mapCard.appendChild(el('h3', '', 'Track'));
        var mapBody = el('div', 'tvd-map');
        mapCard.appendChild(mapBody);
        this.tvMap = this.raceMap({ body: mapBody });
        this.canvases = (this.canvases || []).filter(function (o) { return o !== self.tvMap; });   /* drawn by tvRender, not by page refresh */
        var evCard = el('div', 'tvd-card');
        evCard.appendChild(el('h3', '', 'Race events'));
        this.tvEvents = el('div', 'tvd-events');
        evCard.appendChild(this.tvEvents);
        mid.appendChild(mapCard);
        mid.appendChild(evCard);
        left.appendChild(el('h3', '', 'Drivers'));
        this.tvTable = el('div');
        /* 2026-10-16 owner: a driver pick took several clicks. The table is rebuilt every second, so a click whose
         * mouse up lands on the new rows is dropped by the browser. One listener on this stable box, picked on mouse down. */
        this.tvTable.addEventListener('mousedown', function (ev) {
            if (ev.button !== 0) return;
            var tr = ev.target && ev.target.closest ? ev.target.closest('tr[data-id]') : null;
            if (tr) self.tvPickDriver(Number(tr.getAttribute('data-id')));
        });
        left.appendChild(this.tvTable);
        var cams = el('div', 'tvd-card');
        cams.appendChild(el('h3', '', 'Cameras'));
        this.tvNames = el('ul');
        cams.appendChild(this.tvNames);
        var acts = el('div', 'tvd-actions');
        this.tvBtnCut = el('button', 'primary', 'Cut to driver (Enter)');
        this.tvBtnCut.type = 'button';
        this.tvBtnCut.title = 'Cut the TV to the selected driver with the selected camera';
        this.tvBtnCut.onclick = function () { self.tvCut(); };
        this.tvBtnAngle = el('button', '', 'Camera on this car');
        this.tvBtnAngle.type = 'button';
        this.tvBtnAngle.title = 'Change only the camera; stay on the car the TV shows now';
        this.tvBtnAngle.onclick = function () { self.tvSend('angle', null); };
        this.tvBtnRestore = el('button', '', 'Back to my car');
        this.tvBtnRestore.type = 'button';
        this.tvBtnRestore.title = 'Restore: your own car with the camera you had before';
        this.tvBtnRestore.onclick = function () { self.tvSend('restore', null); };
        acts.appendChild(this.tvBtnCut);
        acts.appendChild(this.tvBtnAngle);
        acts.appendChild(this.tvBtnRestore);
        cams.appendChild(acts);
        var goRow = el('div', 'tvd-goto');
        var goLabel = el('label', '', 'Go to position ');
        this.tvPosInput = el('input');
        this.tvPosInput.type = 'number';
        this.tvPosInput.min = '1';
        this.tvPosInput.max = '104';
        this.tvPosInput.step = '1';
        this.tvPosInput.placeholder = '1-104';
        this.tvPosInput.setAttribute('aria-label', 'Race position to open');
        this.tvPosInput.onkeydown = function (ev) {
            if (ev.key === 'Enter') { ev.preventDefault(); ev.stopPropagation(); self.tvGoPosition(); }
        };
        this.tvBtnGo = el('button', '', 'Go');
        this.tvBtnGo.type = 'button';
        this.tvBtnGo.onclick = function () { self.tvGoPosition(); };
        goLabel.appendChild(this.tvPosInput);
        goRow.appendChild(goLabel);
        goRow.appendChild(this.tvBtnGo);
        cams.appendChild(goRow);
        this.tvResult = el('div', 'tvd-status');
        cams.appendChild(this.tvResult);
        right.appendChild(cams);
        grid.appendChild(left);
        grid.appendChild(mid);
        grid.appendChild(right);
        box.appendChild(grid);
        /* technical rows, closed by default */
        var det = el('details');
        det.appendChild(el('summary', '', 'Details'));
        var note = el('p', 'tvd-note');
        note.textContent = 'Phase 2. Cut, angle and restore act on this PC\'s game only. Writes need [Director] Enabled=1; a Cut to another car also needs CarSwitch=1.';
        det.appendChild(note);
        this.tvAir = el('div', 'tvd-air');
        det.appendChild(this.tvAir);
        this.tvFoot = el('div', 'tvd-status');
        det.appendChild(this.tvFoot);
        box.appendChild(det);
        this.tvDrivers = [];
        this.tvSelDriver = null;
        this.tvAngle = 'TVCOCKPIT';
        this.tvLast = null;
        this.tvWriteOk = false;
        this.tvTick = 0;
        return box;
    };

    /* Driver chosen in the list or on the map. COCKPIT is the player's view only: an AI pick moves a COCKPIT choice to TVCOCKPIT. */
    Console.prototype.tvPickDriver = function (id) {
        this.tvSelDriver = id;
        var car = this.cars().filter(function (q) { return q.id === id; })[0];
        if (this.tvAngle === 'COCKPIT' && car && car.cr !== 0) this.tvAngle = 'TVCOCKPIT';
        this.tvRenderDrivers();
        this.tvRenderAngles();
        if (this.tvMap) this.tvMap.draw();
    };

    /* Track map + race events on the TV tab (owner: show everything that is going on). The car on air = the followed roster id. */
    Console.prototype.tvRenderLive = function () {
        var self = this, j = this.tvLast, f = j && j.followed;
        this.tvAirId = null;
        /* the car on air: roster id = pool slot = car id (research A13); the name is the fallback */
        if (f) this.cars().forEach(function (q) { if (q.id === f.roster_id) self.tvAirId = q.id; });
        if (f && this.tvAirId === null) this.cars().forEach(function (q) { if (q.n && f.driver_name && cleanName(q.n) === cleanName(f.driver_name)) self.tvAirId = q.id; });
        if (this.tvAirId !== this._tvAirDrawn) { this._tvAirDrawn = this.tvAirId; this.tvRenderDrivers(); }
        if (this.tvMap && this.topTab === 'tv') this.tvMap.draw();
        if (!this.tvEvents) return;
        var d = this.frame, zones = d && d.sg ? d.sg.length : 0;
        var rel = (this.incidents || []).filter(function (e) { return e.ty !== 'POS' && rcMsg(e, zones); });
        var h = '';
        rel.slice().reverse().slice(0, 30).forEach(function (e, i) {
            var m = rcMsg(e, zones);
            h += '<div class="' + m.s + (i === 0 ? ' new' : '') + '"><span class="lp">L' + (e.lp || 0) + '</span><span>' + m.t + '</span></div>';
        });
        h = h || '<div><span>No race events yet this session.</span></div>';
        if (this.tvEvents._sig !== h) { this.tvEvents._sig = h; this.tvEvents.innerHTML = h; }
    };

    /* On-air card + state pills (pure view of the last /director/state document). */
    Console.prototype.tvRenderOnAir = function (j) {
        var f = j && j.followed, c = this.tvAirCard;
        if (!c) return;
        var live = !!(j && j.game_running);
        c.querySelector('.tvd-onair').className = 'tvd-onair' + (live ? '' : ' off');
        c.querySelector('.tvd-air-drv').textContent = f ? (f.driver_name || '(no name)') : '-';
        c.querySelector('.tvd-air-sub').textContent = f ? (f.car_name || '') : (live ? '' : 'No live game sample.');
        var raw = tvdAngleText(j);
        c.querySelector('.tvd-air-cam b').textContent = tvdCamLabel(raw);
        c.querySelector('.tvd-air-cam span').textContent = tvdCamLabel(raw) !== raw ? raw : '';
        var pills = [];
        pills.push(j && j.viewer_local ? (j.write_enabled ? ['ok', 'Control on'] : ['warn', 'View only (Enabled=0)']) : ['warn', 'Not on the game PC']);
        if (j && j.viewer_local && j.write_enabled) pills.push(j.carswitch_enabled === false ? ['warn', 'Car switch off'] : ['ok', 'Car switch on']);
        if (j && j.mute_commentary !== undefined) pills.push(j.mute_commentary ? ['ok', 'Commentary muted on cuts'] : ['', 'Commentary on cuts']);
        var html = pills.map(function (p) { return '<span class="tvd-pill ' + p[0] + '">' + esc(p[1]) + '</span>'; }).join('');
        /* owner: Hide HUD is switched here (POST /director/options); shown when the game PC reports it */
        if (j && j.hide_hud !== undefined) {
            var canSet = !!(j.viewer_local && j.write_enabled);
            html += '<button type="button" class="tvd-switch' + (j.hide_hud ? ' on' : '') + '" data-opt="hide_hud"' + (canSet ? '' : ' disabled') +
                ' title="SWINGMAN, ONBOARD and circuit cameras hide the game HUD (key 2); NOSECAM, COCKPIT and TVCOCKPIT give it back">' +
                '<i></i>Hide HUD on cinematic cameras</button>';
        }
        if (this.tvPills._sig !== html) { this.tvPills._sig = html; this.tvPills.innerHTML = html; }
    };

    Console.prototype.setTopTab = function (id) {
        var tv = id === 'tv';
        if (tv && this.tvDisabled) return;
        this.topTab = tv ? 'tv' : 'tel';
        (this.telParts || []).forEach(function (x) { x.style.display = tv ? 'none' : ''; });
        this.tvEl.style.display = tv ? '' : 'none';
        this.tabTel.className = tv ? 'f1x-tab' : 'f1x-tab on';
        this.tabTv.className = tv ? 'f1x-tab on' : 'f1x-tab';
        if (tv) this.tvStart();
        else { this.tvStop(); this.invalidate(); }
    };

    Console.prototype.tvStart = function () {
        var self = this;
        if (this.tvTimer) return;
        if (!this.tvKeyFn) {
            this.tvKeyFn = function (ev) { self.tvKey(ev); };
            document.addEventListener('keydown', this.tvKeyFn);
        }
        this.tvPoll();
        this.tvTimer = setInterval(function () { self.tvPoll(); }, TVD_POLL_MS);
    };

    Console.prototype.tvStop = function () {
        if (this.tvTimer) clearInterval(this.tvTimer);
        this.tvTimer = null;
        if (this.tvKeyFn) {
            document.removeEventListener('keydown', this.tvKeyFn);
            this.tvKeyFn = null;
        }
    };

    /* Kill switch: a 404 from GET /director/state hides the tab (server with the route switched off). */
    Console.prototype.tvOff = function () {
        this.tvDisabled = true;
        this.tabTv.style.display = 'none';
        this.tvStop();
        if (this.topTab === 'tv') this.setTopTab('tel');
    };

    Console.prototype.tvProbe = function () {
        this.tvPoll(true);
    };

    Console.prototype.tvPoll = function (probeOnly) {
        var self = this;
        if (this.tvBusy) return;
        this.tvBusy = true;
        this.tvTick = (this.tvTick || 0) + 1;
        if (!probeOnly && this.tvTick % TVD_TEL_EVERY === 1) this.tvPollTel();
        fetch('/director/state', { cache: 'no-store' }).then(function (r) {
            if (r.status === 404) { self.tvOff(); return null; }
            if (!r.ok) throw Error('HTTP ' + r.status);
            return r.json();
        }).then(function (j) {
            self.tvBusy = false;
            if (j && !probeOnly) self.tvRender(j);
        }).catch(function (e) {
            self.tvBusy = false;
            if (!probeOnly) {
                self.tvStatus.className = 'tvd-status tvd-bad';
                self.tvStatus.textContent = 'No answer from the game PC (' + (e && e.message ? e.message : 'error') + '). Nothing was sent.';
            }
        });
    };

    /* Driver list from /telemetry (the same document the other pages use). */
    Console.prototype.tvPollTel = function () {
        var self = this;
        fetch('/telemetry', { cache: 'no-store' }).then(function (r) {
            if (!r.ok) throw Error('HTTP ' + r.status);
            return r.json();
        }).then(function (tel) {
            self.tvDrivers = tvdDrivers(tel);
            if (self.tvIndexOf(self.tvSelDriver) < 0 && self.tvDrivers.length) self.tvSelDriver = self.tvDrivers[0].id;
            self.tvRenderDrivers();
        }).catch(function () { /* the status line shows the game PC state; the list keeps its last rows */ });
    };

    Console.prototype.tvIndexOf = function (id) {
        var i;
        for (i = 0; i < this.tvDrivers.length; i++) if (this.tvDrivers[i].id === id) return i;
        return -1;
    };

    Console.prototype.tvRenderDrivers = function () {
        var self = this, t = el('table', 'tvd-table'), hd = el('tr');
        ['Pos', 'Driver', 'Team', 'Lap', 'Ahead', 'Behind', 'Leader gap', 'Pit', 'Battle'].forEach(function (h) { hd.appendChild(el('th', '', h)); });
        t.appendChild(hd);
        this.tvDrivers.forEach(function (d) {
            var tr = el('tr', (d.id === self.tvSelDriver ? 'sel' : '') + (d.pit ? ' pit' : '') + (d.id === self.tvAirId ? ' air' : ''));
            tr.setAttribute('data-id', String(d.id));   /* picked by the tvTable mouse-down listener */
            var cells = [
                d.pos, d.name, d.team, d.lap,
                d.aheadM === null ? TVD_DASH : d.ahead + ' ' + Math.round(d.aheadM) + ' m',
                d.behindM === null ? TVD_DASH : d.behind + ' ' + Math.round(d.behindM) + ' m',
                d.gap,
                d.pitText || TVD_DASH,
                TVD_DASH
            ];
            cells.forEach(function (c, i) {
                var td = el('td', '', String(c));
                if (i === 2) td.className = 'team';
                if (i === 6) td.title = 'Gap to the leader at this car\'s last line crossing (race clock, /telemetry tt). Not a live gap.';
                if (i === 8) td.title = 'Battle flag: placeholder in phase 1 (depends on AI research, spec D-TV3).';
                tr.appendChild(td);
            });
            t.appendChild(tr);
        });
        this.tvTable.innerHTML = '';
        if (!this.tvDrivers.length) this.tvTable.appendChild(el('p', 'tvd-note', 'No live driver list yet (no race data).'));
        else this.tvTable.appendChild(t);
    };

    /* T2 (owner): user names for cameras, kept in this browser. Key = the game's camera name. Empty name = back to the game name. */
    function tvdCamNames() {
        try { return JSON.parse(window.localStorage.getItem('tvd.camNames') || '{}') || {}; } catch (e) { return {}; }
    }
    function tvdCamLabel(name) {
        var m = typeof window !== 'undefined' ? tvdCamNames() : {};
        return (name && m[name]) || name;
    }
    function tvdSetCamName(name, label) {
        var m = tvdCamNames();
        label = String(label || '').trim().slice(0, 40);
        if (!label || label === name) delete m[name]; else m[name] = label;
        try { window.localStorage.setItem('tvd.camNames', JSON.stringify(m)); } catch (e) { /* not remembered */ }
    }

    Console.prototype.tvRenderAngles = function () {
        var self = this, j = this.tvLast;
        if (this.tvRenaming) return;   /* do not rebuild under an open name box */
        this.tvNames.innerHTML = '';
        /* T1: the circuit camera (as PGDN) is always the first tile */
        ['CIRCUIT'].concat((j && j.names) || TVD_FIXED).forEach(function (nm) {
            var label = nm === 'CIRCUIT' && tvdCamLabel(nm) === nm ? 'Circuit camera' : tvdCamLabel(nm), key = tvdAngleKey(nm);
            var li = el('li', '', '<b>' + esc(label) + '</b>' + (label !== nm ? '<small>' + esc(nm) + '</small>' : '<small>&nbsp;</small>'));
            li.title = label !== nm ? label + ' (' + nm + ')' : nm;
            if (j && !tvdCircuit(j) && j.view_owner !== 'unknown' && j.angle && nm === j.angle) li.className = 'on';
            if (nm === 'CIRCUIT' && j && tvdCircuit(j)) li.className = 'on';
            var selCar = self.cars().filter(function (q) { return q.id === self.tvSelDriver; })[0];
            if (nm === 'COCKPIT' && selCar && selCar.cr !== 0) {
                li.className += ' dis';
                li.title = 'COCKPIT exists only for your own car: AI cars have no simulated cockpit view';
                li.onclick = function () { self.tvShow('COCKPIT exists only for your own car (AI cars have no simulated cockpit view).', true); };
                self.tvNames.appendChild(li);
                return;
            }
            if (key === self.tvAngle) li.className += ' sel';
            li.onclick = function () { self.tvAngle = key; self.tvRenderAngles(); };
            var ren = el('button', 'ren', '&#9998;');
            ren.type = 'button';
            ren.title = 'Rename this camera (only in this browser)';
            ren.onclick = function (ev) {
                ev.stopPropagation();
                self.tvRenaming = true;
                var inp = el('input');
                inp.value = tvdCamLabel(nm);
                inp.setAttribute('aria-label', 'Name for ' + nm);
                li.innerHTML = '';
                li.appendChild(inp);
                li.appendChild(el('small', '', esc(nm) + ' - Enter saves, Esc cancels, empty = game name'));
                var done = function (save) {
                    if (!self.tvRenaming) return;
                    self.tvRenaming = false;
                    if (save) tvdSetCamName(nm, inp.value);
                    self.tvRenderAngles();
                    if (self.tvLast) self.tvRenderOnAir(self.tvLast);
                };
                inp.onkeydown = function (e2) {
                    e2.stopPropagation();
                    if (e2.key === 'Enter') { e2.preventDefault(); done(true); }
                    else if (e2.key === 'Escape') { e2.preventDefault(); done(false); }
                };
                inp.onblur = function () { done(true); };
                inp.onclick = function (e3) { e3.stopPropagation(); };
                inp.focus(); inp.select();
            };
            li.appendChild(ren);
            self.tvNames.appendChild(li);
        });
    };

    Console.prototype.tvShow = function (text, bad) {
        this.tvResult.className = 'tvd-status' + (bad ? ' tvd-bad' : '');
        this.tvResult.textContent = text;
    };

    Console.prototype.tvWhyOff = function () {
        var j = this.tvLast;
        if (!j) return 'No game state yet.';
        if (!j.viewer_local) return 'Needs the game running on this PC (open this page at localhost).';
        if (!j.write_enabled) return 'Writes are off: set [Director] Enabled=1 in web_telemetry.ini. Nothing was sent.';
        return '';
    };

    Console.prototype.tvCut = function () {
        var i = this.tvIndexOf(this.tvSelDriver);
        if (i < 0) { this.tvShow('Select a driver row first (Up/Down).', true); return; }
        this.tvSend('cut', this.tvDrivers[i].name, this.tvDrivers[i].id);
    };

    /* 2026-10-14: the Go button and Enter in the position box. Sends a cut to the driver shown at that position in the live list,
     * with the angle currently selected. Nothing is sent for a position that is not a whole number from 1 to 104 or has no row. */
    Console.prototype.tvGoPosition = function () {
        var n = tvdPositionValue(this.tvPosInput.value), i, row = null;
        if (!n) { this.tvShow('Type a race position from 1 to 104. Nothing was sent.', true); return; }
        for (i = 0; i < this.tvDrivers.length; i++) {
            if (this.tvDrivers[i].pos === n) { row = this.tvDrivers[i]; break; }
        }
        if (!row) { this.tvShow('No live driver is at position ' + n + '. Nothing was sent.', true); return; }
        this.tvSend('cut', row.name, row.id, n);
    };

    /* kind = cut | angle | restore. The angle is the one selected in the list (this.tvAngle). driverId = roster id (cut only). */
    Console.prototype.tvSend = function (kind, driverName, driverId, position) {
        var self = this, why = this.tvWhyOff(), block = kind === 'cut' ? tvdCutBlock(this.tvLast, driverId) : '';
        /* 1.2.1: past mode never sends a TV Director command. */
        if (this.isPast()) { this.tvShow('TV Director controls the live game. Switch to LIVE to use it.', true); return; }
        if (why) { this.tvShow(why, true); return; }
        /* phase 2: a Cut to another car is not sent while [Director] CarSwitch=0. Nothing is sent, angle changes still work. */
        if (block) { this.tvShow(block, true); return; }
        this.tvShow('Sending ' + kind + '...', false);
        fetch('/director/' + kind, {
            method: 'POST', cache: 'no-store',
            headers: { 'Content-Type': 'application/json', 'X-Telemetry-Action': 'director' },
            body: JSON.stringify(tvdBody(kind, driverName, this.tvAngle, position))
        }).then(function (r) {
            return r.json().then(function (j) { return { code: r.status, j: j }; }, function () { return { code: r.status, j: null }; });
        }).then(function (x) {
            var v = x.j ? x.j.verified_next_frame : null;
            /* phase 2: the verify lands on a later capture; tvRender shows it when last_result carries this seq. */
            self.tvWaitSeq = (v === 'pending' && x.j) ? x.j.seq : null;
            self.tvShow(tvdResultText(x), !(x.j && x.j.status === 'applied') || v === false);
            self.tvPoll(true);
        }).catch(function (e) {
            self.tvShow('No answer from the game PC (' + (e && e.message ? e.message : 'error') + '). Nothing was confirmed.', true);
        });
    };

    /* Owner option switch (hide_hud). Same guards as the commands: game PC only, Enabled=1, X-Telemetry-Action. */
    Console.prototype.tvSetOption = function (name, on) {
        var self = this, body = {};
        body[name] = !!on;
        fetch('/director/options', {
            method: 'POST', cache: 'no-store',
            headers: { 'Content-Type': 'application/json', 'X-Telemetry-Action': 'director' },
            body: JSON.stringify(body)
        }).then(function (r) {
            return r.json().then(function (j) { return { code: r.status, j: j }; }, function () { return { code: r.status, j: null }; });
        }).then(function (x) {
            if (x.code !== 200) self.tvShow('Option not changed: ' + ((x.j && x.j.error) || 'HTTP ' + x.code), true);
            else self.tvShow(on ? 'Cinematic cameras now hide the HUD.' : 'The HUD is no longer changed by the TV Director.', false);
            self.tvPoll(true);
        }).catch(function () { self.tvShow('No answer from the game PC. The option was not changed.', true); });
    };

    /* Hotkeys: only on the TV tab, never while a text field or select has focus. Enter on a focused button is left to the
     * browser (it clicks that button once). Up/Down move the driver row; 1-4 pick the fixed angle; Enter cuts. */
    Console.prototype.tvKey = function (ev) {
        var t = ev.target, a, idx;
        if (this.topTab !== 'tv' || ev.ctrlKey || ev.metaKey || ev.altKey) return;
        if (!tvdHotkeyAllowed(t)) return;
        if (ev.key === 'Enter' && t && t.tagName === 'BUTTON') return;
        idx = this.tvIndexOf(this.tvSelDriver);
        a = tvdKeyAction(ev.key, this.tvDrivers.length, idx);
        if (!a) return;
        ev.preventDefault();
        if (a.select !== undefined) {
            if (this.tvDrivers[a.select]) this.tvSelDriver = this.tvDrivers[a.select].id;
            this.tvRenderDrivers();
        } else if (a.angle) {
            this.tvAngle = tvdAngleKey(a.angle);
            this.tvRenderAngles();
        } else if (a.cut) {
            this.tvCut();
        }
    };

    Console.prototype.tvRender = function (j) {
        var self = this;
        this.tvLast = j;
        this.tvWriteOk = !!(j.write_enabled && j.viewer_local);
        this.tvBtnCut.disabled = !this.tvWriteOk;
        if (this.tvBtnGo) this.tvBtnGo.disabled = !this.tvWriteOk;
        if (this.tvPosInput) this.tvPosInput.disabled = !this.tvWriteOk;
        this.tvBtnAngle.disabled = !this.tvWriteOk;
        this.tvBtnRestore.disabled = !this.tvWriteOk;
        this.tvRenderOnAir(j);
        this.tvRenderLive();
        this.tvAir.innerHTML = '';
        tvdLines(j).forEach(function (ln) {
            var row = el('div', 'tvd-row'), k = el('div', 'tvd-k'), v = el('div', 'tvd-v');
            k.textContent = ln.k;
            v.textContent = ln.v + (ln.note ? '  (' + ln.note + ')' : '');
            if (ln.bad) v.className = 'tvd-v tvd-bad';
            row.appendChild(k); row.appendChild(v);
            self.tvAir.appendChild(row);
        });
        this.tvRenderAngles();
        this.tvStatus.className = 'tvd-status' + (j.viewer_local ? '' : ' tvd-bad');
        var csNote = (j.write_enabled && j.viewer_local && j.carswitch_enabled === false)
            ? 'Car switch is off ([Director] CarSwitch=0): a Cut to another car is not sent. Angle changes still work.' : '';
        this.tvStatus.textContent = j.viewer_local
            ? (j.write_enabled ? csNote : 'Viewing only: writes are off ([Director] Enabled=0).')
            : 'Needs the game running on this PC (open this page at localhost).';
        /* phase 2: a pending next-frame verify of the last command finishes here (same seq as the POST that started it). */
        var lr = j.last_result;
        if (this.tvWaitSeq && lr && lr.seq === this.tvWaitSeq && lr.verified_next_frame !== 'pending') {
            this.tvWaitSeq = null;
            this.tvShow(tvdResultText({ code: 200, j: lr }), !(lr.status === 'applied') || lr.verified_next_frame === false);
        }
        this.tvFoot.textContent = j.names_source === 'Default.cfg'
            ? 'ONBOARD names: Default.cfg in the game save folder.'
            : 'Default.cfg not found: only the four fixed angles are listed.';
    };

    Console.prototype.toggleX = function () {

        Viewer.prototype.toggleX.call(this);
        if (this.legacyUi && this.legacyUi.setXMode) this.legacyUi.setXMode(this.xMode);
    };

    Console.prototype.stepPage = function (dir) {
        var ids = this.pages().map(function (p) { return p.id; });
        var cur = this.page, i = ids.indexOf(cur);
        if (i < 0) i = 0;
        /* Batch 2c: no wrap. Left on the first page and right on the last page do nothing. */
        var j = i + dir;
        if (j < 0 || j >= ids.length) return;
        this.setPage(ids[j]);
    };

    /* Custom page list (replaces the native select, which does not reliably open
     * on the current item). Grouped. The current page is scrolled into view when
     * the list opens; Up/Down move, Enter picks, Escape closes (zoom is kept). */
    Console.prototype.buildPageMenu = function () {
        var self = this, wrap = el('span', 'f1x-pick');
        var btn = el('button', 'f1x-combo');
        btn.type = 'button'; btn.title = 'Page / layout: click or press Down to open';
        var menu = el('div', 'f1x-menu');
        menu.tabIndex = -1; menu.style.display = 'none';
        var list = [], seen = {};
        this.pages().forEach(function (p) {
            if (!seen[p.group]) { seen[p.group] = 1; menu.appendChild(el('div', 'f1x-mg', esc(p.group))); }
            var it = el('div', 'f1x-mi', '<span class="nm">' + esc(p.name) + '</span>');
            it.setAttribute('data-id', p.id);
            if (p.pair) {
                var sm = el('span', 'f1x-sm', '');
                sm.setAttribute('role', 'button');
                sm.title = 'Show the smoothed (filtered) version of this layout (S key)';
                sm.style.cssText = 'margin-left:8px;text-decoration:underline;opacity:.85';
                sm.onclick = function (e) { e.stopPropagation(); self.toggleSmooth(p.id.slice(7)); };
                it.appendChild(sm);
            }
            it.onclick = function (e) { e.stopPropagation(); self.setPage(p.id); self.closeMenu(); };
            menu.appendChild(it); list.push(it);
        });
        wrap.appendChild(btn); wrap.appendChild(menu);
        this.pick = wrap; this.pageBtn = btn; this.pageMenu = menu; this.pageItems = list;
        this.menuOpen = false; this.activeItem = null;
        btn.onclick = function (e) { e.stopPropagation(); if (self.menuOpen) self.closeMenu(); else self.openMenu(); };
        btn.onkeydown = function (e) {
            if (e.key === 'ArrowDown' || e.key === 'ArrowUp' || e.key === 'Enter' || e.key === ' ') {
                e.preventDefault(); e.stopPropagation(); self.openMenu();
            }
        };
        menu.addEventListener('keydown', function (e) { self.menuKey(e); });
        document.addEventListener('mousedown', function (e) { if (self.menuOpen && !wrap.contains(e.target)) self.closeMenu(); });
    };
    Console.prototype.syncPageBtn = function () {
        var self = this, cur = null;
        this.pageItems.forEach(function (it) {
            var on = it.getAttribute('data-id') === self.page;
            it.classList.toggle('on', on);
            var smEl = it.querySelector('.f1x-sm');
            if (smEl) smEl.textContent = self.smoothOn(it.getAttribute('data-id').slice(7)) ? '[x] smoothed' : '[ ] smoothed';
            if (on) cur = it;
        });
        this.pageBtn.textContent = (cur ? (cur.querySelector('.nm') || cur).textContent : String(this.page)) + '  \u25BE';
    };
    Console.prototype.markActive = function (it) {
        this.pageItems.forEach(function (x) { x.classList.toggle('act', x === it); });
        this.activeItem = it || null;
    };
    Console.prototype.openMenu = function () {
        var self = this, cur = null;
        this.pageItems.forEach(function (it) { if (it.getAttribute('data-id') === self.page) cur = it; });
        this.menuOpen = true;
        this.pageMenu.style.display = 'block';
        this.markActive(cur || this.pageItems[0]);
        if (this.activeItem) this.activeItem.scrollIntoView({ block: 'center' });
        this.pageMenu.focus();
    };
    Console.prototype.closeMenu = function () {
        var hadFocus = document.activeElement === this.pageMenu;
        this.menuOpen = false;
        this.pageMenu.style.display = 'none';
        if (hadFocus) this.pageBtn.focus();
    };
    Console.prototype.menuKey = function (e) {
        var k = e.key, n = this.pageItems.length, i = this.pageItems.indexOf(this.activeItem);
        e.stopPropagation(); /* page and zoom shortcuts stay off while the list is open */
        if (k === 'ArrowDown' || k === 'ArrowUp') {
            i = (i + (k === 'ArrowDown' ? 1 : -1) + n) % n;
            this.markActive(this.pageItems[i]);
            this.pageItems[i].scrollIntoView({ block: 'nearest' });
        } else if (k === 'Enter' || k === ' ') {
            if (this.activeItem) this.setPage(this.activeItem.getAttribute('data-id'));
            this.closeMenu();
        } else if ((k === 's' || k === 'S') && this.activeItem && this.activeItem.querySelector('.f1x-sm')) {
            this.toggleSmooth(this.activeItem.getAttribute('data-id').slice(7));
        } else if (k === 'Escape') {
            this.closeMenu();
        } else return;
        e.preventDefault();
    };

    Console.prototype.smoothOn = function (base) {
        return this.pref('smooth:' + base, '0') === '1';
    };
    Console.prototype.toggleSmooth = function (base) {
        if (!SMOOTH_PAIR[base]) return;
        this.setPref('smooth:' + base, this.smoothOn(base) ? '0' : '1');
        if (this.page === 'legacy:' + base) this.setPage(this.page);
        else if (this.pageBtn) this.syncPageBtn();
    };
    Console.prototype.setPage = function (id) {
        var cl = canonicalLegacy(id);
        if (cl.forceSmooth) {
            this.setPref('smooth:' + cl.id.slice(7), '1');
            id = cl.id;
        }
        this.page = id;
        if (id !== 'replay') this.setPref('page', id);   /* 1.2.2: Replay is opened from a recording, not remembered as the start page (TAPE2 V5: setPref ignores review) */
        if (this.pageBtn) this.syncPageBtn();
        if (id === 'weather') this.pollWeather();   /* rc.11 weather: fetch once on open */
        if (id === 'focus' && this.review) this.pollDriver();   /* 1.0.1: no polling in review */
        if (id.indexOf('legacy:') === 0) {
            this.legacyFile = layoutFileFor(id.slice(7), this.smoothOn(id.slice(7)));
            this.setView('legacy');
        } else {
            this.legacyFile = null;
            this.setView(id);
        }
    };

    /* ---- 1.2.5 (DVOA report 2026-10-18): My layouts. Build a layout from any channel the viewer draws (one graph per
     * channel, optional track map and G-G), save it in this browser, open it like a built-in layout, share it as a
     * legacy .ini or f1c.layout/1 file. A built-in layout is copied with "Customize" (never changed). ---- */
    var LE_CSS = [
        '.f1x-le{display:flex;gap:12px;height:100%;min-height:0;padding:10px;box-sizing:border-box;font:12px var(--sans);color:#dfe7ff}',
        '.f1x-le-col{display:flex;flex-direction:column;gap:8px;min-height:0;border:1px solid #14307f;background:#040b26;padding:10px;box-sizing:border-box}',
        '.f1x-le-list{flex:0 0 300px;overflow:auto}',
        '.f1x-le-ed{flex:1 1 auto;overflow:auto}',
        '.f1x-le h3{margin:0 0 4px;font-size:13px;color:#38d6e8;font-weight:600}',
        '.f1x-le-row{display:flex;align-items:center;gap:6px;padding:4px 6px;border:1px solid #0a1a52;border-radius:3px}',
        '.f1x-le-row .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
        '.f1x-le-row.on{border-color:#38d6e8}',
        '.f1x-le .tv-btn{height:24px;padding:0 10px;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#dfe7ff;cursor:pointer;font:12px var(--sans)}',
        '.f1x-le .tv-btn.primary{background:#e10600;border-color:#ff4a3d;color:#fff}',
        '.f1x-le .tv-btn:disabled{opacity:.4;cursor:not-allowed}',
        '.f1x-le .x{border:0;background:transparent;color:#9fb2e6;font-size:16px;cursor:pointer;line-height:1;padding:0 4px}',
        '.f1x-le input[type=text]{background:#02061a;border:1px solid #2a43a8;color:#dfe7ff;height:24px;padding:0 6px;border-radius:3px;width:260px}',
        '.f1x-le-chk{display:inline-flex;align-items:center;gap:5px;margin-right:14px;cursor:pointer}',
        '.f1x-le-g{display:flex;flex-direction:column;gap:3px}',
        '.f1x-le-grp{color:#9fb2e6;margin-top:6px}',
        '.f1x-le-chs{display:flex;flex-wrap:wrap;gap:4px}',
        '.f1x-le-ch{height:22px;padding:0 8px;border:1px solid #2a43a8;border-radius:11px;background:#02061a;color:#dfe7ff;cursor:pointer;font:12px var(--sans)}',
        '.f1x-le-ch:disabled{opacity:.35;cursor:default}',
        '.f1x-le-prev{position:relative;width:320px;height:170px;border:1px solid #14307f;background:#000;flex:none}',
        '.f1x-le-prev div{position:absolute;box-sizing:border-box;border:1px solid #2a43a8;color:#9fb2e6;font-size:10px;padding:2px 4px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}',
        '.f1x-le-msg{min-height:16px;color:#9fb2e6;white-space:pre-wrap}',
        '.f1x-le-ask{display:flex;gap:6px;align-items:center;color:#ffb35c}',
        '.f1x-le-custom{margin-left:6px}'
    ].join('\n');

    /* Pure. Plain-language summary of a spec (editor and tests). */
    function layoutSpecText(spec) {
        if (!spec) return '';
        var parts = [];
        if (spec.map) parts.push('track map');
        if (spec.gg) parts.push('G-G');
        var n = (spec.graphs || []).length;
        if (n) parts.push(n + (n === 1 ? ' graph' : ' graphs'));
        return parts.join(', ') || 'empty';
    }

    Console.prototype.view_layouts = function (host) {
        var self = this;
        if (!document.getElementById('f1x-le-style')) {
            var st = el('style'); st.id = 'f1x-le-style'; st.textContent = LE_CSS; document.head.appendChild(st);
        }
        if (!this.leDraft) this.leDraft = { file: null, spec: { name: 'My layout', map: true, gg: false, graphs: [] } };
        var d = this.leDraft;
        var wrap = el('div', 'f1x-le');
        var list = el('div', 'f1x-le-col f1x-le-list'), ed = el('div', 'f1x-le-col f1x-le-ed');
        wrap.appendChild(list); wrap.appendChild(ed);
        host.appendChild(wrap);
        var msgEl = el('div', 'f1x-le-msg');
        function msg(t) { msgEl.textContent = t || ''; }
        function redraw(note) { self.leNote = note || ''; self.leScroll = [list.scrollTop, ed.scrollTop]; self.setView('layouts'); }

        /* left: saved layouts */
        list.appendChild(el('h3', '', 'My layouts'));
        var tools = el('div', '');
        var nb = el('button', 'tv-btn', 'New layout');
        nb.onclick = function () { self.leDraft = { file: null, spec: { name: 'My layout', map: true, gg: false, graphs: [] } }; redraw(''); };
        var ib = el('button', 'tv-btn', 'Import file...');
        ib.title = 'A layout .ini of the old f1_telemetry viewer, or a .json saved here';
        var fin = el('input'); fin.type = 'file'; fin.accept = '.ini,.json'; fin.style.display = 'none';
        ib.onclick = function () { fin.click(); };
        fin.onchange = function () { self.leImport(fin.files && fin.files[0]); fin.value = ''; };
        tools.appendChild(nb); tools.appendChild(document.createTextNode(' ')); tools.appendChild(ib); tools.appendChild(fin);
        list.appendChild(tools);
        var saved = WB.userLayouts();
        if (!saved.length) list.appendChild(el('div', 'f1x-le-msg', 'No layout saved yet. Build one on the right, or press Customize on any layout page.'));
        saved.forEach(function (l) {
            var row = el('div', 'f1x-le-row' + (d.file === l.file ? ' on' : ''));
            row.appendChild(el('span', 'nm', esc(l.name)));
            row.title = layoutSpecText(l.spec);
            var ob = el('button', 'tv-btn', 'Open'); ob.onclick = function () { self.setPage('legacy:' + l.file); };
            var eb = el('button', 'tv-btn', 'Edit'); eb.onclick = function () { self.leDraft = { file: l.file, spec: WB.layoutSpec(l) }; redraw(''); };
            var xb = el('button', 'x', '&times;'); xb.title = 'Delete this layout';
            xb.onclick = function () {
                row.innerHTML = '';
                var ask = el('div', 'f1x-le-ask', 'Delete ' + esc(l.name) + '?');
                var yes = el('button', 'tv-btn primary', 'Delete'), no = el('button', 'tv-btn', 'Keep');
                yes.onclick = function () {
                    WB.deleteUserLayout(l.file);
                    if (self.leDraft && self.leDraft.file === l.file) self.leDraft.file = null;
                    self.rebuildPageMenu();
                    redraw('Deleted ' + l.name + '.');
                };
                no.onclick = function () { redraw(''); };
                ask.appendChild(yes); ask.appendChild(no); row.appendChild(ask);
            };
            [ob, eb, xb].forEach(function (b) { row.appendChild(b); });
            var ex = el('div', 'f1x-le-row');
            ex.style.border = '0'; ex.style.paddingTop = '0';
            var e1 = el('button', 'tv-btn', 'Save .ini'); e1.title = 'Layout file of the old f1_telemetry viewer';
            e1.onclick = function () { self.leDownload(WB.layoutToIni(l), l.name, 'ini'); };
            var e2 = el('button', 'tv-btn', 'Save .json'); e2.title = 'Layout file of this viewer (f1c.layout/1)';
            e2.onclick = function () { self.leDownload(WB.layoutToJson(l), l.name, 'json'); };
            ex.appendChild(e1); ex.appendChild(e2);
            list.appendChild(row); list.appendChild(ex);
        });

        /* right: the editor */
        ed.appendChild(el('h3', '', d.file ? 'Edit layout' : 'New layout'));
        var top = el('div', '');
        var nameIn = el('input'); nameIn.type = 'text'; nameIn.maxLength = 40; nameIn.value = d.spec.name || '';
        nameIn.oninput = function () { d.spec.name = nameIn.value; };
        top.appendChild(el('span', '', 'Name ')); top.appendChild(nameIn);
        ed.appendChild(top);
        var opts = el('div', '');
        [['map', 'Track map'], ['gg', 'G-G (friction circle)']].forEach(function (p) {
            var lab = el('label', 'f1x-le-chk'), cb = el('input'); cb.type = 'checkbox'; cb.checked = !!d.spec[p[0]];
            cb.onchange = function () { d.spec[p[0]] = cb.checked; redraw(''); };
            lab.appendChild(cb); lab.appendChild(document.createTextNode(p[1])); opts.appendChild(lab);
        });
        ed.appendChild(opts);

        var cat = WB.channelCatalog(), labelOf = {};
        cat.forEach(function (c) { labelOf[c.name] = c.label + (c.unit ? ' (' + c.unit + ')' : ''); });
        var max = WB.USER_GRAPH_MAX, gs = d.spec.graphs;
        ed.appendChild(el('div', 'f1x-le-grp', 'Graphs, top to bottom (' + gs.length + ' of ' + max + ')'));
        var gl = el('div', 'f1x-le-g');
        if (!gs.length) gl.appendChild(el('div', 'f1x-le-msg', 'Add a channel below. Each channel gets its own graph over the lap.'));
        gs.forEach(function (g, i) {
            var row = el('div', 'f1x-le-row');
            row.appendChild(el('span', 'nm', (i + 1) + '. ' + esc(labelOf[g.y] || g.y)));
            if (g.y !== 'gear' && g.y !== 'dtime') {
                var lab = el('label', 'f1x-le-chk'), cb = el('input'); cb.type = 'checkbox'; cb.checked = !!g.smooth;
                cb.onchange = function () { g.smooth = cb.checked; };
                lab.appendChild(cb); lab.appendChild(document.createTextNode('smoothed')); row.appendChild(lab);
            }
            var up = el('button', 'tv-btn', '&#8593;'), dn = el('button', 'tv-btn', '&#8595;'), x = el('button', 'x', '&times;');
            up.title = 'Move up'; dn.title = 'Move down'; x.title = 'Remove this graph';
            up.disabled = i === 0; dn.disabled = i === gs.length - 1;
            up.onclick = function () { gs.splice(i - 1, 0, gs.splice(i, 1)[0]); redraw(''); };
            dn.onclick = function () { gs.splice(i + 1, 0, gs.splice(i, 1)[0]); redraw(''); };
            x.onclick = function () { gs.splice(i, 1); redraw(''); };
            row.appendChild(up); row.appendChild(dn); row.appendChild(x);
            gl.appendChild(row);
        });
        ed.appendChild(gl);

        ed.appendChild(el('div', 'f1x-le-grp', 'Add a channel'));
        var lastGroup = null, chs = null;
        cat.forEach(function (c) {
            if (c.group !== lastGroup) {
                lastGroup = c.group;
                ed.appendChild(el('div', 'f1x-le-grp', esc(c.group)));
                chs = el('div', 'f1x-le-chs'); ed.appendChild(chs);
            }
            var b = el('button', 'f1x-le-ch', esc(c.label));
            b.title = c.unit ? c.label + ' in ' + c.unit : c.label;
            b.disabled = gs.length >= max || gs.some(function (g) { return g.y === c.name; });
            b.onclick = function () { gs.push({ y: c.name, smooth: false }); redraw(''); };
            chs.appendChild(b);
        });

        /* preview of the boxes as they will be placed */
        ed.appendChild(el('div', 'f1x-le-grp', 'Preview'));
        var pv = el('div', 'f1x-le-prev'), lay = WB.buildUserLayout(d.spec, 'user:preview');
        (lay ? lay.graphs : []).forEach(function (g) {
            var r = g.rect, box = el('div', '');
            box.style.left = (r[0] * 100) + '%'; box.style.width = ((r[1] - r[0]) * 100) + '%';
            box.style.top = (r[2] * 100) + '%'; box.style.height = ((r[3] - r[2]) * 100) + '%';
            box.textContent = g.x === 'x' ? 'Track map' : g.x === 'lat accel' ? 'G-G' : (labelOf[g.y.replace(/^filter\[(.+),\d+\]$/, '$1')] || g.y);
            pv.appendChild(box);
        });
        if (!lay) pv.appendChild(el('div', '', 'Nothing to show yet'));
        ed.appendChild(pv);

        var foot = el('div', '');
        var sv = el('button', 'tv-btn', 'Save'), so = el('button', 'tv-btn primary', 'Save and open');
        sv.disabled = so.disabled = !lay;
        function save(open) {
            var l = WB.saveUserLayout(d.spec, d.file || undefined);
            if (!l) { msg('Not saved: the layout is empty or the list is full (40 layouts).'); return; }
            d.file = l.file;
            self.rebuildPageMenu();
            if (open) self.setPage('legacy:' + l.file);
            else redraw('Saved ' + l.name + ' (' + layoutSpecText(l.spec) + '). It is in the page menu under My layouts.');
        }
        sv.onclick = function () { save(false); };
        so.onclick = function () { save(true); };
        foot.appendChild(sv); foot.appendChild(document.createTextNode(' ')); foot.appendChild(so);
        ed.appendChild(foot);
        ed.appendChild(msgEl);
        msg(this.leNote || 'Layouts are kept in this browser only. Use Save .ini or Save .json to keep a copy or share it.');
        this.leNote = '';
        if (this.leScroll) { list.scrollTop = this.leScroll[0]; ed.scrollTop = this.leScroll[1]; this.leScroll = null; }   /* a click keeps the scroll */
    };

    /* Import a .ini (old viewer) or .json (this viewer) layout file into the editor (not saved until Save). */
    Console.prototype.leImport = function (file) {
        var self = this;
        if (!file) return;
        if (file.size > 256 * 1024) { this.leNote = 'Not imported: the file is larger than 256 KB.'; this.setView('layouts'); return; }
        var rd = new FileReader();
        rd.onload = function () {
            var text = String(rd.result || ''), spec = null, base = String(file.name || '').replace(/\.[^.]*$/, '');
            if (/\.json$/i.test(file.name)) { try { spec = WB.jsonToSpec(JSON.parse(text)); } catch (e) { spec = null; } }
            else spec = WB.iniToSpec(text, base);
            if (!spec || !WB.buildUserLayout(spec, 'user:tmp')) {
                self.leNote = 'Not imported: this is not a layout file, or none of its graphs can be drawn here.';
            } else {
                var skipped = spec.skipped || [];
                delete spec.skipped;
                self.leDraft = { file: null, spec: spec };
                self.leNote = 'Imported ' + spec.name + ' (' + layoutSpecText(spec) + '). Press Save to keep it.' +
                    (skipped.length ? '\nLeft out (no source in this viewer): ' + skipped.join(', ') + '.' : '');
            }
            self.setView('layouts');
        };
        rd.readAsText(file);
    };
    Console.prototype.leDownload = function (text, name, ext) {
        var blob = new Blob([text], { type: ext === 'json' ? 'application/json' : 'text/plain' });
        var a = el('a'); a.href = URL.createObjectURL(blob);
        a.download = 'f1c_layout_' + String(name || 'layout').replace(/[^A-Za-z0-9_.-]+/g, '_') + '.' + ext;
        document.body.appendChild(a); a.click(); a.remove();
        setTimeout(function () { URL.revokeObjectURL(a.href); }, 2000);
    };
    /* The page menu is built once; a saved or deleted layout rebuilds it in place. */
    Console.prototype.rebuildPageMenu = function () {
        var old = this.pick, btn = typeof document !== 'undefined' ? document.querySelector('.f1x-le-custom') : null;
        if (!old || !old.parentNode) return;
        this.buildPageMenu();
        old.parentNode.replaceChild(this.pick, old);
        if (btn) this.pick.parentNode.insertBefore(btn, this.pick.nextSibling);
        this.syncPageBtn();
    };
    /* Customize / Edit button in the page bar, next to the page menu, on a layout page: copies the shown layout into the
     * editor. Any other page removes it (renderView). */
    Console.prototype.leCustomButton = function () {
        var self = this, lay = WB.allLayouts().filter(function (l) { return l.file === self.legacyFile; })[0];
        var old = document.querySelector('.f1x-le-custom');
        if (old) old.remove();
        if (!lay || !this.pick || !this.pick.parentNode) return;
        if (!document.getElementById('f1x-le-style')) {
            var st = el('style'); st.id = 'f1x-le-style'; st.textContent = LE_CSS; document.head.appendChild(st);
        }
        var b = el('button', 'tv-btn f1x-le-custom', lay.user ? 'Edit layout' : 'Customize');
        b.title = lay.user ? 'Change this layout' : 'Make your own copy of this layout: add or remove channel graphs';
        b.style.cssText = 'height:22px;padding:0 10px;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#dfe7ff;cursor:pointer';
        b.onclick = function () {
            self.leDraft = { file: lay.user ? lay.file : null, spec: WB.layoutSpec(lay) };
            self.setPage('layouts');
        };
        this.pick.parentNode.insertBefore(b, this.pick.nextSibling);
    };

    Console.prototype.renderLegacy = function () {
        var box = el('div', 'tv-legacy');
        this.viewEl.appendChild(box);
        this.legacyUi = WB.mount(box, this.wb, this.legacyMountOpts({ label: 'LEGACY LAYOUTS', layout: this.legacyFile || undefined, fill: true }));
        if (this.legacyUi && this.legacyUi.setXMode) this.legacyUi.setXMode(this.xMode);
        if (this.waypoints) this.legacyUi.setWaypoints(this.waypoints);
        this.legacyUi.render();
        if (!WB.layoutHasMap(this.legacyFile)) this.mapInset(this.viewEl, true);
        this.leCustomButton();   /* 1.2.5: Customize (built-in) / Edit layout (My layouts) */
    };

    Console.prototype.renderView = function () {
        var leOld = typeof document !== 'undefined' ? document.querySelector('.f1x-le-custom') : null;
        if (leOld) leOld.remove();   /* 1.2.5: the layout button belongs to layout pages only */
        this.towerRows = {};
        this.legacyUi = null;
        /* 1.2.1: past mode with a storage recording (or none chosen): no live data is drawn; the Recordings page still works. */
        if (this.pastNoData() && this.view !== 'sessions') {
            var host = this.viewEl, self = this, note = el('div', 'f1x-pastnote');
            this.charts = []; this.canvases = []; this.onCursor = null; this.refreshers = [];
            host.innerHTML = '';
            if (this.past) {
                note.innerHTML = '<b>' + esc(pastBannerText(this.past.stem || this.past.folder)) + '</b>' +
                    '<p>Viewing a stored recording is not available in this version yet. The pages draw live game data only, so nothing is drawn here.</p>' +
                    '<p>Manage recordings on the Recordings page, or go back to live.</p>';
            } else {
                note.innerHTML = '<b>No past session is open.</b><p>Open one on the Recordings page. The pages draw live data only.</p>';
            }
            var go = el('button', 'tv-btn', 'Recordings'); go.onclick = function () { self.goChange(); };
            var lv = el('button', 'tv-btn', 'Back to live'); lv.onclick = function () { self.goLive(); };
            note.appendChild(go); note.appendChild(document.createTextNode(' ')); note.appendChild(lv);
            host.appendChild(note);
            this.dirty = true;
            return;
        }
        Viewer.prototype.renderView.call(this);
    };

    /* ---- data: frames, incidents, pace, driver dossier ---- */

    Console.prototype.fetchJson = function (url) {
        url = reviewUrl(url, this.review);
        if (this.opts.fetchJson) return this.opts.fetchJson(url);
        return fetch(url).then(function (r) { if (!r.ok) throw Error(url + ' ' + r.status); return r.json(); });
    };

    Console.prototype.onFrame = function (j) {
        if (!j || !Array.isArray(j.c)) return;
        if (this.gen === null || j.session_generation !== this.gen) {
            this.gen = j.session_generation;
            this.incidents = []; this.pace = []; this.driverDoc = null; this.mapState.mb = null;
            var self = this;
            setTimeout(function () { self.pollIncidents(); self.pollPace(); }, 50);
            if (this.view === 'weather') this.pollWeather();   /* rc.11 weather: new session */
        }
        if (j.session_generation !== this.pchGen) { this.pchGen = j.session_generation; this.pchHist = null; }
        pchPush(this, j);   /* 1.1.0 player chassis history */
        var hadWaypoints = this.waypoints;
        Viewer.prototype.onFrame.call(this, j);
        if (this.legacyUi && this.waypoints && this.waypoints !== hadWaypoints) this.legacyUi.setWaypoints(this.waypoints);
        if (this.pendingSides && this.roster.length) this.restoreSides();
        this.reselectSides();
        this.syncTraceSides();
        this.seedFromServer();
        if (this.focusId === null || !j.c.some(function (c) { return c.id === this.focusId && c.p > 0; }, this)) {
            var me = j.c.filter(function (c) { return c.id === j.ps && c.p > 0; })[0] ||
                     j.c.filter(function (c) { return c.cr === 0 && c.p > 0; })[0] ||
                     j.c.filter(function (c) { return c.p === 1; })[0];
            this.focusId = me ? me.id : null;
        }
    };

    /* R2: open a storage v2 recording in review (same page load as 1.0.1 review). */
    Console.prototype.openReview = function (rid, name) {
        if (!/^v2_[0-9a-f]{16}$/.test(rid)) return;
        location.href = '/?session=' + encodeURIComponent(rid) + (name ? '&name=' + encodeURIComponent(name) : '');
    };

    /* R2: the server builds the lap index of a v2 recording on first use; until it is ready the routes answer 503. The page waits
     * here with a notice and reloads once when the index is ready (&ready=1 stops a second reload). */
    Console.prototype.pollReviewIndex = function () {
        var self = this, tries = 0, rid = this.review, already = /[?&]ready=1/.test(location.search);
        var step = function () {
            fetch('/recordings/review?session=' + encodeURIComponent(rid), { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (j) {
                if (j && j.state === 'ready') {
                    if (!already && self._reviewWaited) location.href = location.pathname + location.search + '&ready=1';
                    return;
                }
                if (j && j.state === 'error') { self.toast('This recording cannot be read: ' + (j.reason || 'error')); return; }
                self._reviewWaited = true;
                self.toast('Preparing the recording (reading its laps)...');
                if (++tries < 120) setTimeout(step, 1000);
            }).catch(function () { if (++tries < 120) setTimeout(step, 2000); });
        };
        step();
    };

    Console.prototype.startPolls = function () {
        var self = this;
        if (this.review) {
            this.pollSessions();
            if (/^v2_/.test(this.review)) this.pollReviewIndex();
            this.tapeInit();
            return;   /* archived data is static: fetched once per frame/side (onFrame, seedSideTrace) */
        }
        setInterval(function () { self.retryEmptySeeds(); }, 5000);
        setInterval(function () { self.repairPartialLaps(); }, 3000);   /* 1.2.5 (DVOA 2026-10-18) */
        setInterval(function () { self.pollSessions(); }, 10000);   /* 1.2.1: the live line needs the recording state on every page */
        setInterval(function () { self.pollIncidents(); }, 3000);
        setInterval(function () { self.pollPace(); }, 5000);
        setInterval(function () { if (self.view === 'focus') self.pollDriver(); }, 3000);
        setInterval(function () { if (self.view === 'weather') self.pollWeather(); }, 10000);   /* rc.11 weather */
    };
    /* rc.11 weather: the session weather ring, polled every 10 s while the
     * Weather page is open and fetched once when it opens or a session changes. */
    Console.prototype.pollWeather = function () {
        var self = this, gen = this.gen;
        if (this.review) return;   /* 1.3.0: a recording has no weather (the Weather page says so) */
        this.fetchJson('/weather').then(function (d) {
            if (!d || d.error) return;
            if (gen !== null && d.session_generation !== undefined && d.session_generation !== gen) return;
            if (self.gen !== null && gen !== null && self.gen !== gen) return;
            self.weather = d;
            self.invalidate();
        }).catch(function () {});
    };
    Console.prototype.pollIncidents = function () {
        var self = this, gen = this.gen;
        if (gen === null) return;
        this.fetchJson('/incidents').then(function (d) {
            if (d && (d.gen === undefined || d.gen === gen) && self.gen === gen) { self.incidents = d.ev || []; self.invalidate(); }
        }).catch(function () {});
    };
    Console.prototype.pollPace = function () {
        var self = this, gen = this.gen;
        if (gen === null) return;
        this.fetchJson('/laps').then(function (d) {
            if (d && (d.gen === undefined || d.gen === gen) && self.gen === gen) { self.pace = d.cars || []; self.invalidate(); }
        }).catch(function () {});
        /* 4.4.0-rc.5 task 8: pit stop events for the Race Control summary. */
        this.fetchJson('/pits').then(function (d) {
            if (d && (d.gen === undefined || d.gen === gen) && self.gen === gen) { self.pits = d.pits || []; self.invalidate(); }
        }).catch(function () {});
    };
    Console.prototype.pollDriver = function () {
        var self = this, id = this.focusId, gen = this.gen;
        if (id === null || id === undefined) return;
        if (this.tape && this.tape.kind === 'rec') {   /* 1.3.0: the dossier at the tape time (/driver &t=) */
            if (this.tape.driverBusy) return;
            this.tape.driverBusy = true;
            this.fetchLapDoc('/driver?id=' + id + '&t=' + this.tape.t.toFixed(3)).then(function (res) {
                self.tape.driverBusy = false;
                if (id !== self.focusId) return;
                var d = res.j;
                if (res.code === 200 && d && !d.err) { self.driverDoc = d; self.driverErr = ''; }
                else { self.driverDoc = null; self.driverErr = d && d.err ? String(d.err) : 'The driver dossier is not in this recording (' + (res.code || 'no answer') + ').'; }
                self.invalidate();
            });
            return;
        }
        this.fetchJson('/driver?id=' + id).then(function (d) {
            if (!d || d.err || (d.gen !== undefined && d.gen !== gen) || id !== self.focusId) return;
            self.driverDoc = d; self.invalidate();
        }).catch(function () {});
    };

    Console.prototype.trackStatus = function () {
        var m = 0;
        this.incidents.forEach(function (e) { if (e.ty === 'RC_MODE') m = e.b; });
        return m;
    };

    Console.prototype.cars = function () {
        var j = this.frame;
        return j ? j.c.filter(function (c) { return c.p > 0; }).sort(function (a, b) { return a.p - b.p; }) : [];
    };

    /* Row click = focus; double-click = show on side A; shift-click = side B. */
    Console.prototype.pickCar = function (id, e, dbl) {
        this.focusId = id;
        this.driverDoc = null;
        if (dbl || (e && e.shiftKey)) {
            var L = dbl ? 'A' : 'B';
            var meta = this.roster.filter(function (q) { return q.id === id; })[0];
            if (meta) { this.wb.select(L, id, meta); this.lapMode[L] = 'auto'; this.cache = {}; this.fillCarSelects(true); }
        }
        if (this.view === 'focus') this.pollDriver();
        this.invalidate();
    };

    /* ---- 1.2.1 source switch: LIVE (this game session) or PAST SESSION (one stored recording) ----
     * source: 'live' or 'past'. past = { folder, stem } after Load on the Recordings page; review (?session=) is always past.
     * Past mode with a storage recording has no data path yet: the pages draw a notice and nothing from the live game. */
    Console.prototype.isPast = function () { return this.source === 'past'; };
    Console.prototype.pastNoData = function () { return this.isPast() && !this.review; };
    Console.prototype.pastShortLabel = function () {
        if (this.review) return reviewTitle(this.review);
        return this.past ? pastShortName(this.past.stem || this.past.folder) : '';
    };
    Console.prototype.enterPast = function (folder, stem) {
        this.source = 'past';
        this.past = { folder: folder, stem: stem || '' };
        this.applyMode();
    };
    /* LIVE: the running game. A stored recording selection is cleared on the game PC (no file changes). */
    /* TAPE2 V5: from a review, LIVE is a fresh load of the game page on the live page the user had before
     * PAST (the stored page; review pages never store it, see setPage), else Race. */
    Console.prototype.liveReturnPage = function () {
        var p = this.pref('page', 'race');
        return !p || p === 'sessions' || p === 'replay' ? 'race' : p;
    };
    Console.prototype.goLive = function () {
        if (this.review) { location.href = '/?page=' + encodeURIComponent(this.liveReturnPage()); return; }
        var had = !!this.past;
        this.source = 'live';
        this.past = null;
        if (had) this.recordingPost('load', { recording: '' });
        this.applyMode();
    };
    /* PAST SESSION: pick one on the Recordings page. An open past session stays open. */
    Console.prototype.goPast = function () {
        if (this.review) return;
        this.source = 'past';
        if (!this.past) { this.setTopTab('tel'); this.setPage('sessions'); }
        this.applyMode();
    };
    /* TAPE2 V5: from a review, Change is a fresh load of the live page on Recordings (the review has no list). */
    Console.prototype.goChange = function () {
        if (this.review) { location.href = '/?page=sessions'; return; }
        this.setTopTab('tel');
        this.setPage('sessions');
        this.applyMode();
    };
    Console.prototype.applyMode = function () {
        var past = this.isPast(), nodata = this.pastNoData();
        this.root.classList.toggle('f1x-src-live', !past);
        this.root.classList.toggle('f1x-src-past', past);
        this.root.classList.toggle('f1x-past-nodata', nodata);
        this.srcLiveBtn.className = 'f1x-src live' + (past ? '' : ' on');
        this.srcPastBtn.className = 'f1x-src past' + (past ? ' on' : '');
        this.tvEl.classList.toggle('tvd-pastmode', past);
        if (this.topTab === 'tv') { if (past) this.tvStop(); else this.tvStart(); }
        if (this.view !== 'sessions') this.renderView();
        this._srcSig = null;
        this.refreshSource();
        this.invalidate();
    };
    /* The line under the header: LIVE status (with Open a past session), or the PAST banner (Change, Back to live). */
    Console.prototype.refreshSource = function () {
        if (!this.srcLine) return;
        var self = this, past = this.isPast(), j = this.frame, sd = this.sessionsDoc, html, sig, txt;
        if (!past) {
            var age = j ? Date.now() - this.frameAt : null;
            var running = !!j && !(age > 2500 || j.sample_age_ms > 1500);
            txt = liveLineText({ running: running, track: headerTrackText(j), type: j && j.ty, recording: !!(sd && sd.recording) });
            html = '<span class="f1x-srctxt">' + esc(txt) + '</span>' +
                (running ? '' : ' <button class="tv-btn" data-act="open-past">Open a past session</button>');
            sig = 'L|' + txt + '|' + (running ? 1 : 0);
        } else {
            /* TAPE2 V5: on the Recordings page with nothing open, Change is hidden and the line says where to pick. */
            var onRec = this.page === 'sessions', change = this.review || this.past || !onRec;
            txt = this.review ? 'Watching past session: ' + reviewTitle(this.review) :
                this.past ? pastBannerText(this.past.stem || this.past.folder) :
                onRec ? 'Past session: none open. Pick a recording below.' :
                'Past session: none open. Pick a recording on the Recordings page.';
            html = '<span class="f1x-srctxt">' + esc(txt) + '</span> ' + (change ? '<button class="tv-btn" data-act="change">Change</button> ' : '') +
                '<button class="tv-btn" data-act="live">Back to live</button>';
            sig = 'P|' + txt + '|' + (change ? 1 : 0);
        }
        if (sig === this._srcSig) return;
        this._srcSig = sig;
        this.srcLine.className = 'f1x-srcline ' + (past ? 'past' : 'live') + (!past && running ? ' run' : '');
        this.srcLine.innerHTML = html;
        if (this.hdrSess) {
            this.hdrSess.textContent = txt;
            this.hdrSess.className = 'f1x-hsess' + (past ? ' past' : running ? ' run' : ' idle');
            this.hdrSess.title = txt;
        }
        var tag = slotSourceText(past, this.pastShortLabel());
        ['A', 'B'].forEach(function (L) {
            var c = self.carEls && self.carEls[L];
            if (c && c.src) {
                var off = L === 'B' && self.bOff;
                c.src.textContent = off ? 'Off' : tag;
                c.src.className = 'f1x-srctag' + (off ? ' off' : past ? ' past' : '');
            }
        });
    };

    /* ---- chrome ---- */

    Console.prototype.refreshChrome = function () {
        Viewer.prototype.refreshChrome.call(this);
        var j = this.frame, self = this;
        var age = j ? Date.now() - this.frameAt : null;
        var past = this.isPast(), nodata = this.pastNoData();
        var cls = past ? '' : !j ? 'off' : age > 2500 || j.sample_age_ms > 1500 ? 'stale' : '';
        var word = this.review ? 'REVIEW' : past ? 'PAST' : !j ? 'NO DATA' : cls ? 'STALE' : 'LIVE';
        this.liveEl.className = 'f1x-live ' + cls + (past ? ' review' : '');
        this.liveEl.textContent = word;
        var noticeTxt = statusNoticeText(past, !!this.review, !!this.past);
        if (noticeTxt) this.sessEl.innerHTML = '<span class="tv-live stale">PAST</span><span>' + noticeTxt + '</span>';
        else if (j) {
            this.sessEl.innerHTML = '<span class="tv-live ' + cls + '">' + word + '</span>' +
                (this.review ? '<span>Saved session <b>' + esc(reviewTitle(this.review)) + '</b> · <a href="/" style="color:#7fd4ff">back to live</a></span>' + (this.tape.info ? '<span>Tape <b>' + rpClock(this.tape.t - this.tape.info.tf) + '</b></span>' : '') : '') +
                '<span><b>' + esc(headerTrackText(j)) + '</b></span><span>' + esc(sessName(j.ty)) + (j.ph > 0 ? ' / ' + esc(phaseName(j.ph)) : '') + '</span>' +
                '<span>Lap length <b>' + (j.lapm > 0 ? (j.lapm / 1000).toFixed(3) + ' km' : 'unknown') + '</b></span>' +
                '<span>Air <b>' + fx(j.at > -900 ? j.at : null, 0, '°C') + '</b> Track <b>' + fx(j.trkt > -900 ? j.trkt : null, 0, '°C') + '</b></span>' +
                (this.coopInfo() ? '<span>CO-OP ' + esc(this.coopInfo()) + '</span>' : '');
        }
        this.xBtn.textContent = this.xMode === 'dist' ? 'X: distance' : 'X: lap time';
        if (this.tapeBar) this.tapeBar.style.display = this.tape.kind === 'rec' && this.topTab === 'tel' ? '' : 'none';
        this.refreshSource();
        var sh = this.showingHtml();
        if (sh !== this._shSig) { this._shSig = sh; this.showEl.innerHTML = sh; }
        var rl = this.wb.refLap, rtxt = rl && rl.lap
            ? '⚑ Reference lap: ' + rl.letter + ' · ' + lapLabel(rl.lap) + ' ' +
              lapText(rl.lap.end - rl.lap.start) + '   (click the flag on that lap again to clear)'
            : '⚑ Reference lap: none. Click the flag on a lap to set it; Incremental Time Difference uses it.';
        if (rtxt !== this._refTxt) { this._refTxt = rtxt; this.refEl.textContent = rtxt; }
        this.letters().forEach(function (L) {
            var side = self.side(L);
            self.carEls[L].frz.textContent = (side && side.frozenSnapshot ? 'Resume ' : 'Freeze ') + L;
            self.renderLapRow(L);
        });
        if (this.addBtn) this.addBtn.disabled = this.letters().length >= WB.MAX_SIDES;
    };

    /* Reference-lap flag on a lap chip: sets wb.refLap (the same pick as the
     * /workbench strip) so Incremental Time Difference and the legacy delta use it. */
    Console.prototype.refFlag = function (L, lap, isRef) {
        var self = this, f = el('span', 'f1x-flag' + (isRef ? ' on' : ''), '\u2691');
        f.title = isRef ? 'Reference lap for side ' + L + ': click the flag to clear' :
            'Use this lap as the reference lap (Incremental Time Difference, legacy delta)';
        f.onclick = function (e) {
            e.stopPropagation();
            /* decided at click time, not at render time, so a stale flag cannot re-set it */
            var on = !!(self.wb.refLap && self.wb.refLap.lap === lap);
            self.wb.refLap = on ? null : { letter: L, lap: lap };
            self.invalidate();
        };
        return f;
    };

    /* One row per side: the car, Fastest, Last full lap (auto), Live, then every
     * recorded lap. Each lap chip carries its reference flag. */
    Console.prototype.renderLapRow = function (L) {
        var self = this, row = this.lapRows[L], side = this.side(L);
        if (!row) return;
        var laps = side ? side.displayLaps() : [];
        var best = WB.fastestLap(laps);
        var shown = side ? (side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown) : null;
        var mode = this.lapMode[L];
        var ref = this.wb.refLap && this.wb.refLap.letter === L ? this.wb.refLap.lap : null;
        /* TAPE2 V1: the side's car badge (YOU / HUMAN) in the row header */
        var sideCar = side ? (this.roster || []).filter(function (c) { return c.id === side.slot; })[0] : null;
        var sig = [side ? side.label : '', side && side.frozenSnapshot ? 1 : 0, mode, laps.length,
            /* 1.2.5: the game time (arrives after the line) and the full/partial state also redraw the row */
            laps.map(function (l) { return lapLabel(l) + l.lap + ':' + (l.end - l.start).toFixed(3) + ':' +
                (l.official_time || '') + (l.complete ? 'c' : 'p') + (l.repairTried ? 'r' : ''); }).join(','),
            laps.indexOf(shown), laps.indexOf(ref), humanRole(sideCar)].join('|');
        if (row._sig === sig) return;
        row._sig = sig;
        row.innerHTML = '';
        var tag = el('div', 'f1x-ltag', '<i style="background:' + COLORS[L] + '">' + L + '</i>' + esc(side ? side.label : '—') +
            humanBadgeHtml(sideCar, L) + (side && side.frozenSnapshot ? ' <span style="color:#38d6e8">[frozen]</span>' : ''));
        tag.style.color = COLORS[L];
        tag.title = side ? side.label : '';
        row.appendChild(tag);
        function chip(text, cls, on, v, title) {
            var c = el('span', 'f1x-chip ' + cls + (on ? ' on' : ''), esc(text));
            if (title) c.title = title;
            if (v !== null) c.onclick = function () { self.pickLap(L, v); };
            row.appendChild(c);
            return c;
        }
        if (best) chip('Fastest Lap  ' + lapText(lapShownTime(best)), 'fast', mode !== 'auto' && mode !== 'open' && shown === best,
            String(laps.indexOf(best)), 'Fastest complete lap on side ' + L);
        chip('Last full lap (auto)', '', mode === 'auto', 'auto', 'Follow the newest completed lap');
        chip('Lap in progress', 'live', mode === 'open', 'open', 'The lap being driven now');
        if (!laps.length) chip('no completed laps yet', 'none', false, null);
        for (var i = 0; i < laps.length; i++) {
            var l = laps[i], full = WB.lapIsFull(l);
            var c = chip(lapLabel(l) + ' ' + lapText(lapShownTime(l)), (l === best ? 'fast' : full ? '' : 'part'),
                mode !== 'auto' && mode !== 'open' && shown === l, String(i), full ? '' : partialLapTitle(l));
            c.appendChild(this.refFlag(L, l, l === ref));
        }
    };

    /* Line under the layout bar: the lap shown on each side (B1.2). */
    Console.prototype.showingHtml = function () {
        var self = this;
        return this.letters().map(function (L) {
            return '<b style="color:' + COLORS[L] + '">' + L + '</b> ' + esc(self.lapDescr(L));
        }).join('<span class="f1x-shsep">|</span>');
    };

    /* ---- multi-driver sides (C to F) ----
     * A and B are built once in build(). C to F come from "+ Driver" or from
     * the saved f1x.sides list. Each extra side has a car picker, Freeze, a
     * remove (x) control and one lap-strip row. Saved sides are matched by
     * driver and car name; a car that is not in the session is dropped. */
    Console.prototype.loadSides = function () {
        var raw;
        try { raw = JSON.parse(this.pref('sides', '[]')); } catch (e) { return []; }
        if (!Array.isArray(raw)) return [];
        var seen = {};
        return raw.filter(function (p) {
            if (!p || typeof p.n !== 'string' || typeof p.L !== 'string' || ['C', 'D', 'E', 'F'].indexOf(p.L) < 0 || seen[p.L]) return false;
            return (seen[p.L] = true);
        }).slice(0, WB.MAX_SIDES - 2);
    };

    Console.prototype.nextFreeLetter = function () {
        var self = this, free = null;
        WB.LETTERS.forEach(function (L) { if (!free && !self.wb.sideOf(L)) free = L; });
        return free;
    };

    Console.prototype.addSide = function (L) {
        if (!L || L === 'A' || L === 'B') return null;
        if (this.wb.sideOf(L)) return this.wb.sideOf(L);
        if (!this.wb.addSide(L)) { this.toast('Six sides at most: remove one first'); return null; }
        this.lapMode[L] = 'auto';
        this.buildSideDom(L);
        this.cache = {};
        this.fillCarSelects(true);
        this.invalidate();
        return this.wb.sideOf(L);
    };

    Console.prototype.removeSide = function (L) {
        if (L === 'A' || L === 'B' || !this.wb.removeSide(L)) return;
        var dom = this.sideDom[L];
        if (dom && dom.grp.parentNode) dom.grp.parentNode.removeChild(dom.grp);
        if (this.lapRows[L] && this.lapRows[L].parentNode) this.lapRows[L].parentNode.removeChild(this.lapRows[L]);
        delete this.lapRows[L]; delete this.carEls[L]; delete this.sideDom[L];
        delete this.lapMode[L]; delete this.sideMemory[L];
        this.cache = {};
        this.persistSides();
        if (this.view === 'legacy' && this.legacyUi) this.legacyUi.refreshSelection();
        this.invalidate();
    };

    /* A1.7: switch driver B off (only A is shown) or back on. keep = the caller picks the B car itself. */
    Console.prototype.setBOff = function (on, keep) {
        this.bOff = !!on;
        try { window.localStorage.setItem('f1x.bOff', on ? '1' : '0'); } catch (e) { /* private mode: not remembered */ }
        if (on) this.wb.select('B', -1, null);
        else if (!keep) {
            var pick = WB.pickDefaultSides(this.roster || []);
            if (pick.B) this.wb.select('B', pick.B.id, pick.B);
        }
        if (this.lapMode) this.lapMode.B = 'auto';
        this.cache = {};
        this._srcSig = null;
        if (this.refreshSource) this.refreshSource();
        if (!keep) this.fillCarSelects(true);   /* keep: the user's pick is already the selected item */
        if (this.view === 'legacy' && this.legacyUi) this.legacyUi.refreshSelection();
        this.invalidate();
    };

    Console.prototype.buildSideDom = function (L) {
        var self = this;
        var grp = el('span', 'f1x-xside');
        grp.style.cssText = 'display:inline-flex;align-items:center;gap:4px;margin-right:6px';
        var lab = el('label', L, L); lab.style.color = COLORS[L];
        var carSel = el('select', 'tv-sel'); carSel.title = 'Car for side ' + L;
        var lapSel = el('select'); lapSel.style.display = 'none';
        var frz = el('button', 'tv-btn', 'Freeze ' + L); frz.title = 'Pin side ' + L + '; recording continues';
        var x = el('button', 'tv-btn', '×'); x.title = 'Remove side ' + L;
        var hid = el('span'); hid.style.display = 'none';
        var nm = el('span'), sub = el('span'), stt = el('span'), lt = el('span');
        [nm, sub, stt, lt].forEach(function (n) { hid.appendChild(n); });
        [lab, carSel, frz, x, hid, lapSel].forEach(function (n) { grp.appendChild(n); });
        this.bar.insertBefore(grp, this.addBtn);
        carSel.onchange = function () { self.pickSideCar(L, parseInt(carSel.value, 10)); };
        lapSel.onchange = function () { self.pickLap(L, lapSel.value); };
        frz.onclick = function () {
            var side = self.side(L);
            if (side.frozenSnapshot) side.resume(); else side.freeze();
            self.invalidate();
        };
        x.onclick = function () { self.removeSide(L); };
        this.carEls[L] = { root: hid, car: carSel, lap: lapSel, frz: frz, nm: nm, sub: sub, st: stt, lt: lt };
        this.sideDom[L] = { grp: grp };
        var row = lapRowEl();
        this.lapStrip.insertBefore(row, this.refEl);
        this.lapRows[L] = row;
    };

    /* Car picked for an extra side: remembered by name for later sessions. */
    /* 4.4.0 all-cars: the server records every car, so any side (A-F) reads the
     * already-recorded trace by numeric slot (seedSideTrace). The A/B pin below
     * is kept for the selected-only fallback, where only the pinned pair is
     * recorded; it is harmless when every car is traced. Re-sent when the slot
     * changes or the page opens. */
    Console.prototype.syncTraceSides = function () {
        var self = this;
        this.traceSynced = this.traceSynced || {};
        this.wb.allSides().forEach(function (s) {
            if (s.letter !== 'A' && s.letter !== 'B') return;
            var slot = typeof s.slot === 'number' ? s.slot : -1;
            if (slot < 0 || self.traceSynced[s.letter] === slot) return;
            self.traceSynced[s.letter] = slot;
            try {
                self.fetchJson('/trace/select?which=' + s.letter + '&car=' + slot).catch(function () {
                    self.traceSynced[s.letter] = undefined;
                });
            } catch (err) { self.traceSynced[s.letter] = undefined; }
        });
    };

    /* 4.4.0 (owner rule 2026-10-12): every car is recorded server-side from
     * session start, so no side has to be "traced" first. One /trace/status
     * fetch lists every recorded car (mode all) or the selected pair (fallback);
     * a side then loads its last three completed laps and its open lap by
     * numeric slot. Read-only; partial is fine: a failed lap is skipped. */
    Console.prototype.seedTraceRequests = function (st, slot) {
        var entry = ((st && st.cars) || []).filter(function (c) { return c.slot === slot; })[0];
        if (!entry) return [];
        var out = [], seen = {};
        var laps = (entry.laps || []).filter(function (l) { if (seen[l.lap]) return false; return (seen[l.lap] = true); });
        laps.slice(this.review ? -REVIEW_LAPS_KEPT : -3).forEach(function (l) {
            out.push('/trace/data?car=' + slot + '&lap=' + l.lap);
        });
        if (!this.review) out.push('/trace/open?car=' + slot);
        return out;
    };

    /* Fetch current status when seeding: later garage returns and laps must be
     * visible in the same broker generation. */
    Console.prototype.traceStatus = function () {
        return this.fetchJson('/trace/status').catch(function () { return null; });
    };

    /* ---- C2 (2026-10-14): Export laps dialog. Pure helpers are exported and unit-tested. ---- */
    var LAPSET_CSV_COLS = ['session', 'car_slot', 'driver', 'team', 'lap', 'lap_time', 'lap_time_src', 'sim_t', 'dist_m',
        'speed_mps', 'throttle', 'brake', 'steer', 'glat_mps2', 'glon_mps2', 'gvert_mps2', 'yaw_rad', 'yaw_rate_rps',
        'rpm', 'gear', 'tx', 'tz', 'tread_fl', 'tread_fr', 'tread_rl', 'tread_rr', 'flags'];

    /* Pure. The one header line of the CSV (one row per sample). */
    function lapsetCsvHeader() { return LAPSET_CSV_COLS.join(','); }

    function twoDigits(n) { return (n < 10 ? '0' : '') + n; }

    /* Pure. f1c_laps_<live|session>_<driver-or-team>_<YYYYMMDD-HHMM>.<ext>; runs of characters outside
     * [A-Za-z0-9_.-] (spaces included) become one underscore. */
    function exportFileName(src, who, date, ext) {
        var stamp = date.getFullYear() + twoDigits(date.getMonth() + 1) + twoDigits(date.getDate()) + '-' +
            twoDigits(date.getHours()) + twoDigits(date.getMinutes());
        var w = String(who || 'car').trim().replace(/[^A-Za-z0-9_.-]+/g, '_');
        return 'f1c_laps_' + (src === 'session' ? 'session' : 'live') + '_' + w + '_' + stamp + '.' + ext;
    }

    /* Pure. Laps are {lap, t, flags, slot, ...} from /trace/status. flags & 1 = completed; flags & 2 = pitted.
     * mode 'best' keeps the fastest lap (t > 0) per slot; any other mode keeps every completed lap. */
    function pickLaps(laps, mode, includePit) {
        var done = (laps || []).filter(function (l) {
            return !!l && (l.flags & 1) !== 0 && (includePit || (l.flags & 2) === 0);
        });
        if (mode !== 'best') return done;
        var best = {}, order = [];
        done.forEach(function (l) {
            if (!(l.t > 0)) return;
            if (!best[l.slot]) { best[l.slot] = l; order.push(l.slot); }
            else if (l.t < best[l.slot].t) best[l.slot] = l;
        });
        return order.map(function (k) { return best[k]; });
    }

    /* Pure. traceLaps: the completed laps of one car in /trace/status order. officialTimes: that car's /laps
     * array (k-th completed lap = laps[k]). Returns one value per trace lap: the official seconds when
     * |t - official| <= 0.25 s, else null (the caller then uses the derived time). */
    /* 1.2.5 (DVOA export 2026-10-17: 3 of 4 laps "derived"): the server ring keeps 6 laps + the fastest, in slot
     * order, while /laps lists every lap of the session, so the k-th ring lap is not the k-th official lap. Each
     * official time is now used once, closest pairs first, within the same 0.25 s window. */
    function joinOfficial(traceLaps, officialTimes) {
        var laps = traceLaps || [], offs = officialTimes || [], pairs = [], out = laps.map(function () { return null; });
        laps.forEach(function (l, k) {
            if (typeof l.t !== 'number' || !(l.t > 0)) return;
            offs.forEach(function (off, j) {
                if (typeof off === 'number' && off > 0 && Math.abs(l.t - off) <= 0.25) pairs.push({ k: k, j: j, d: Math.abs(l.t - off) });
            });
        });
        pairs.sort(function (a, b) { return a.d - b.d || a.k - b.k; });
        var usedK = {}, usedJ = {};
        pairs.forEach(function (p) {
            if (usedK[p.k] || usedJ[p.j]) return;
            usedK[p.k] = usedJ[p.j] = true;
            out[p.k] = offs[p.j];
        });
        return out;
    }

    /* Pure. RFC 4180 cell: quoted only when it holds a comma, quote or line break. */
    function csvCell(v) {
        if (v === null || v === undefined) return '';
        var s = String(v);
        return /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
    }

    /* Pure. laps: f1c.trace/1 lap docs with slot, driver, team, lap, lap_time, lap_time_src (see
     * Console.prototype.exportLapsRun). Missing channels are empty cells. */
    function lapsetCsv(laps, session) {
        var lines = [lapsetCsvHeader()];
        (laps || []).forEach(function (doc) {
            (doc.samples || []).forEach(function (s) {
                var tr = s.tr || [];
                var row = [session, doc.slot, doc.driver, doc.team, doc.lap, doc.lap_time, doc.lap_time_src,
                    s.t, s.d, s.s, s.th, s.br, s.st, s.gl, s.go, s.gv, s.y, s.yr, s.rpm, s.ge, s.tx, s.tz,
                    tr[0], tr[1], tr[2], tr[3], s.f];
                lines.push(row.map(csvCell).join(','));
            });
        });
        return lines.join('\n') + '\n';
    }

    /* Pure. One f1c.lapset/1 document. */
    function lapsetJson(laps, source, exported) {
        return JSON.stringify({ schema: 'f1c.lapset/1', source: source, exported: exported, laps: laps || [] });
    }

    /* Pure. The result-list line for a lap that was not exported. */
    function lapProblem(lap, res) {
        if (res.code === 404) return 'Lap ' + lap + ': ' + (res.j && res.j.evicted ? 'evicted' : 'not recorded');
        if (res.code === 409) return 'Lap ' + lap + ': session or identity changed';
        return 'Lap ' + lap + ': request failed' + (res.code ? ' (HTTP ' + res.code + ')' : '');
    }

    /* One GET, reported as {code, j}. Never throws, so a failed lap is listed, not fatal. */
    Console.prototype.fetchLapDoc = function (url) {
        var u = reviewUrl(url, this.review);
        if (this.opts.fetchJson) return this.opts.fetchJson(u).then(function (j) { return { code: 200, j: j }; }, function () { return { code: 0, j: null }; });
        return fetch(u).then(function (r) {
            return r.json().then(function (j) { return { code: r.status, j: j }; }, function () { return { code: r.status, j: null }; });
        }, function () { return { code: 0, j: null }; });
    };

    function downloadText(text, name, ext) {
        var blob = new Blob([text], { type: ext === 'json' ? 'application/json' : 'text/csv' });
        var a = el('a'); a.href = URL.createObjectURL(blob); a.download = name;
        document.body.appendChild(a); a.click(); a.remove();
        setTimeout(function () { URL.revokeObjectURL(a.href); }, 2000);
    }

    /* Export laps dialog. Opens over the console; "Shown laps (A/B)" runs the old one-click export. */
    Console.prototype.openExportDialog = function () {
        var self = this, root = this.root;
        var old = root.querySelector('.f1x-modal'); if (old) old.remove();
        var modal = el('div', 'f1x-modal');
        modal.innerHTML =
            '<div class="f1x-dlg" role="dialog" aria-label="Export laps">' +
              '<div class="f1x-dlg-h"><b>Export laps</b><button class="f1x-dlg-x" title="Close">&times;</button></div>' +
              '<div class="f1x-dlg-b">' +
                '<div class="f1x-dlg-row"><label>What</label><select class="w-what">' +
                  '<option value="shown">Shown laps (A/B)</option>' +
                  '<option value="drivers">All laps of chosen drivers</option>' +
                  '<option value="team">All laps of a team</option></select></div>' +
                '<div class="f1x-dlg-row f1x-dlg-pick"><label>Drivers</label><div class="f1x-dlg-list"></div></div>' +
                '<div class="f1x-dlg-row f1x-dlg-team"><label>Team</label><select class="w-team"></select></div>' +
                '<div class="f1x-dlg-row"><label>Laps</label><select class="w-laps">' +
                  '<option value="all">All completed laps</option>' +
                  '<option value="best">Best lap per driver</option></select>' +
                  '<label class="chk"><input type="checkbox" class="w-pit"> Include pit laps</label></div>' +
                '<div class="f1x-dlg-row"><label>Format</label><select class="w-fmt">' +
                  '<option value="csv">CSV (one row per sample)</option>' +
                  '<option value="json">JSON (f1c.lapset/1)</option></select></div>' +
                '<div class="f1x-dlg-msg"></div>' +
              '</div>' +
              '<div class="f1x-dlg-f"><button class="tv-btn btn-cancel">Cancel</button><button class="tv-btn primary btn-export">Export</button></div>' +
            '</div>';
        function q(sel) { return modal.querySelector(sel); }
        function msg(text) { q('.f1x-dlg-msg').textContent = text; }
        function close() { modal.remove(); }
        var roster = [], checks = [];
        var pickRow = q('.f1x-dlg-pick'), teamRow = q('.f1x-dlg-team');
        function refreshRows() {
            var what = q('.w-what').value;
            pickRow.style.display = what === 'drivers' ? '' : 'none';
            teamRow.style.display = what === 'team' ? '' : 'none';
        }
        q('.w-what').onchange = refreshRows;
        q('.f1x-dlg-x').onclick = close;
        q('.btn-cancel').onclick = close;
        modal.onclick = function (e) { if (e.target === modal) close(); };
        root.appendChild(modal);
        refreshRows();
        msg('Reading the recorded cars...');

        this.traceStatus().then(function (st) {
            if (!modal.parentNode) return;
            if (!st) { msg('Could not read /trace/status.'); return; }
            roster = (st.cars || []).filter(function (c) { return typeof c.slot === 'number' && c.slot >= 0; });
            var list = q('.f1x-dlg-list'), pos = {};
            self.cars().forEach(function (c) { pos[c.id] = c.p; });
            roster.forEach(function (c) {
                var cb = el('input'); cb.type = 'checkbox';
                var lab = el('label');
                lab.appendChild(cb);
                lab.appendChild(document.createTextNode(' ' + (pos[c.slot] ? 'P' + pos[c.slot] + ' ' : '') + cleanName(c.n) + ' '));
                var sp = el('span'); sp.textContent = teamOf(c); lab.appendChild(sp);
                list.appendChild(lab);
                checks.push({ cb: cb, car: c });
            });
            var teams = roster.map(teamOf).filter(function (t, i, a) { return t && a.indexOf(t) === i; }).sort();
            var teamSel = q('.w-team');
            teams.forEach(function (t) { var o = el('option', '', ''); o.value = t; o.textContent = t; teamSel.appendChild(o); });
            msg(roster.length ? '' : 'No recorded cars yet.');
        });

        q('.btn-export').onclick = function () { self.exportLapsRun(modal, roster, checks, msg, close); };
        return modal;
    };

    /* One Export click. Reads the chosen cars' completed laps (status), the official times (/laps), then
     * fetches each lap ONE REQUEST AT A TIME (the DLL has one HTTP buffer), then downloads one file. */
    Console.prototype.exportLapsRun = function (modal, roster, checks, msg, close) {
        var self = this;
        var q = function (sel) { return modal.querySelector(sel); };
        var what = q('.w-what').value;
        if (what === 'shown') { close(); this.exportShown(); return; }
        var chosen, who;
        if (what === 'drivers') {
            chosen = checks.filter(function (x) { return x.cb.checked; }).map(function (x) { return x.car; });
            who = chosen.length === 1 ? cleanName(chosen[0].n) : 'drivers';
        } else {
            who = q('.w-team').value;
            chosen = roster.filter(function (c) { return who && teamOf(c) === who; });
        }
        if (!chosen.length) { msg('Pick at least one driver, or a team with recorded cars.'); return; }
        var mode = q('.w-laps').value === 'best' ? 'best' : 'all';
        var includePit = !!q('.w-pit').checked;
        var fmt = q('.w-fmt').value === 'json' ? 'json' : 'csv';
        var src = this.review ? 'session' : 'live';
        var sessionId = this.review || 'live';
        var problems = [], docs = [], list = [];
        var btn = q('.btn-export');
        btn.disabled = true;

        function progress(text) { msg(text + (problems.length ? '\n' + problems.join('\n') : '')); }
        progress('Reading official lap times...');
        this.fetchJson('/laps').catch(function () { return null; }).then(function (lapsDoc) {
            if (!lapsDoc) problems.push('Official times unavailable (/laps): derived times used.');
            var official = {};
            ((lapsDoc && lapsDoc.cars) || []).forEach(function (c) { official[c.id] = c.laps || []; });
            chosen.forEach(function (car) {
                var all = car.laps || [];
                var completed = all.filter(function (l) { return (l.flags & 1) !== 0; });
                var offs = joinOfficial(completed, official[car.slot]), offByLap = {};
                completed.forEach(function (l, k) { offByLap[l.lap] = offs[k]; });
                var flat = all.map(function (l) { return Object.assign({}, l, { slot: car.slot }); });
                pickLaps(flat, mode, includePit).forEach(function (rec) {
                    list.push({ car: car, rec: rec, official: offByLap[rec.lap] === undefined ? null : offByLap[rec.lap] });
                });
            });
            var n = list.length;
            if (!n) { btn.disabled = false; msg('No laps match these choices. Nothing downloaded.'); return; }
            return list.reduce(function (p, item, i) {
                return p.then(function () {
                    progress('Lap ' + (i + 1) + ' of ' + n + '...');
                    return self.fetchLapDoc('/trace/data?car=' + item.car.slot + '&lap=' + item.rec.lap + '&fmt=json')
                        .then(function (res) {
                            if (res.code === 200 && res.j && res.j.schema === 'f1c.trace/1') {
                                var doc = res.j, pick = officialLapPick(doc.lap_time, item.official);
                                doc.driver = cleanName(doc.driver || item.car.n);
                                doc.car = doc.car || item.car.cn || '';
                                doc.team = teamOf(item.car);
                                doc.lap_time = (doc.flags & 1) ? pick.time : null;
                                doc.lap_time_src = pick.src;
                                if (doc.slot === undefined) doc.slot = item.car.slot;   /* review: car slot always on the row */
                                if (doc.lap_time === null) doc.lap_time_src = 'incomplete';
                                docs.push(doc);
                            } else {
                                problems.push(lapProblem(item.rec.lap, res));
                            }
                        });
                });
            }, Promise.resolve()).then(function () {
                if (!docs.length) { btn.disabled = false; progress('No lap could be exported.'); return; }
                var now = new Date(), text, ext = fmt;
                text = fmt === 'json' ? lapsetJson(docs, src === 'session' ? self.review : 'live', now.toISOString())
                                      : lapsetCsv(docs, sessionId);
                downloadText(text, exportFileName(src, who, now, ext), ext);
                btn.disabled = false;
                progress('Exported ' + docs.length + ' of ' + n + ' laps.');
            });
        }).catch(function (e) { btn.disabled = false; progress('Export failed: ' + (e && e.message ? e.message : e)); });
    };

    /* Seed one side (any letter A-F) from the server ring: completed laps and
     * the open lap. Runs once per side + car + generation; re-runs when a side
     * picks another car. */
    Console.prototype.seedSideTrace = function (L) {
        var self = this;
        var side = this.side(L);
        if (!side || typeof side.slot !== 'number' || side.slot < 0) return Promise.resolve();
        var history = side.historySource || side;
        var slot = side.slot, generation = this.wb.sessionGeneration;
        var key = L + ':' + side.slot + ':' + this.wb.sessionGeneration;
        this.seedState = this.seedState || {};
        if (this.seedState[key]) return Promise.resolve();
        this.seedState[key] = 'pending';
        return this.traceStatus().then(function (st) {
            var jobs = self.seedTraceRequests(st, slot).map(function (url) {
                var open = url.indexOf('/trace/open') === 0;
                return self.fetchJson(url)
                    .then(function (doc) { return { side: history, doc: doc, open: open }; })
                    .catch(function () { return null; });
            });
            return Promise.all(jobs);
        }).then(function (res) {
            var changed = false;
            (res || []).forEach(function (r) {
                if (!r || !r.doc || r.doc.schema !== 'f1c.trace/1') return;
                if (self.wb.sessionGeneration !== generation || r.doc.gen !== generation || r.doc.slot !== slot) return;
                if (r.open) changed = r.side.seedOpen(r.doc) || changed;
                else changed = (r.side.seedLaps([r.doc]) > 0) || changed;
            });
            self.seedState[key] = changed ? 'done' : 'empty';
            if (changed) self.invalidate();
        }).catch(function () { self.seedState[key] = 'failed'; });
    };

    /* 1.2.5 (DVOA report 2026-10-18): a lap this page holds as partial (a pause in the live feed, a background tab)
     * is asked once from the server ring, which samples every car at the game rate. One lap per call: the DLL has one
     * HTTP buffer. Only the cars on screen; only live. */
    Console.prototype.repairPartialLaps = function () {
        var self = this;
        if (this.review || this.repairBusy) return Promise.resolve(false);
        var gen = this.wb.sessionGeneration, job = null;
        this.wb.allSides().forEach(function (s) {
            if (job || typeof s.slot !== 'number' || s.slot < 0) return;
            var h = s.historySource || s;
            (h.laps || []).forEach(function (l) {
                if (!job && l.repair && !l.repairTried && !l.recorded) job = { h: h, lap: l, slot: s.slot };
            });
        });
        if (!job) return Promise.resolve(false);
        job.lap.repairTried = true;
        this.repairBusy = true;
        return this.traceStatus().then(function (st) {
            var entry = ((st && st.cars) || []).filter(function (c) { return c.slot === job.slot; })[0];
            var srv = entry ? WB.serverLapFor(job.lap, entry.laps) : null;
            if (!srv || (srv.flags & 6)) return false;
            return self.fetchJson('/trace/data?car=' + job.slot + '&lap=' + srv.lap).then(function (doc) {
                if (!doc || doc.schema !== 'f1c.trace/1' || doc.gen !== gen || doc.slot !== job.slot ||
                    self.wb.sessionGeneration !== gen) return false;
                var done = job.h.repairLap(job.lap, doc);
                if (done) self.invalidate();
                return done;
            });
        }).catch(function () { return false; }).then(function (r) { self.repairBusy = false; return r; });
    };

    /* Every side on screen seeds after the first roster. */
    Console.prototype.seedFromServer = function () {
        var self = this;
        if (this.seedInit) return;
        this.seedInit = true;
        this.wb.allSides().forEach(function (s) { self.seedSideTrace(s.letter); });
    };

    Console.prototype.retryEmptySeeds = function () {
        var self = this;
        this.wb.allSides().forEach(function (s) {
            var key = s.letter + ':' + s.slot + ':' + self.wb.sessionGeneration;
            if (self.seedState && self.seedState[key] !== 'pending') {
                delete self.seedState[key];
                self.seedSideTrace(s.letter);
            }
        });
    };

    Console.prototype.pickSideCar = function (L, id) {
        var meta = this.roster.filter(function (q) { return q.id === id; })[0];
        this.wb.select(L, id, meta);
        if (this.seedState) delete this.seedState[L + ':' + id + ':' + this.wb.sessionGeneration];
        this.syncTraceSides();
        this.seedSideTrace(L);
        this.lapMode[L] = 'auto';
        this.cache = {};
        if (meta) this.sideMemory[L] = { n: meta.n || '', cn: meta.cn || '' };
        this.fillCarSelects(true);
        this.persistSides();
        if (this.view === 'legacy' && this.legacyUi) this.legacyUi.refreshSelection();
        this.invalidate();
    };

    Console.prototype.persistSides = function () {
        var self = this, out = [];
        this.wb.allSides().forEach(function (s) {
            var m = self.sideMemory[s.letter];
            if (s.letter !== 'A' && s.letter !== 'B' && m) out.push({ L: s.letter, n: m.n, cn: m.cn });
        });
        this.setPref('sides', JSON.stringify(out));
    };

    /* First roster after the page opens: recreate the saved sides whose car is
     * in this session. Saved cars that are not here are dropped. */
    Console.prototype.restoreSides = function () {
        var self = this, pend = this.pendingSides;
        this.pendingSides = null;
        pend.forEach(function (p) {
            var meta = self.roster.filter(function (c) { return c.n === p.n && (!p.cn || c.cn === p.cn); })[0] ||
                       self.roster.filter(function (c) { return c.n === p.n; })[0];
            if (!meta) return;
            var L = !self.wb.sideOf(p.L) ? p.L : self.nextFreeLetter();
            if (!L || !self.addSide(L)) return;
            self.pickSideCar(L, meta.id);
        });
        this.persistSides();
    };

    /* A side whose car left the session (sideMemory set, no slot) takes the
     * same car again when it returns. */
    Console.prototype.reselectSides = function () {
        var self = this;
        this.wb.allSides().forEach(function (s) {
            var m = self.sideMemory[s.letter];
            if (s.letter === 'A' || s.letter === 'B' || s.slot >= 0 || !m) return;
            var meta = self.roster.filter(function (c) { return c.n === m.n && (!m.cn || c.cn === m.cn); })[0];
            if (meta) self.pickSideCar(s.letter, meta.id);
        });
    };

    Console.prototype.showHelp = function () {
        var m = el('div', 'tv-modal');
        m.innerHTML = '<div><h3>F1 Challenge 99-02 Telemetry</h3>' +
            '<p>One window for the race and the car. Pick a page in the layout box, or step with the arrows (<kbd>PgUp</kbd>/<kbd>PgDn</kbd>).</p>' +
            '<p><b>Race</b> pages: timing tower, track map with every car, race control, race pace, driver focus. Click a driver to focus it; ' +
            'double-click to show it on side <b style="color:var(--a)">A</b>, shift-click for side <b style="color:var(--b)">B</b>.</p>' +
            '<p><b>Analysis</b> pages and the <b>legacy layouts</b> compare side A and B. Pick laps in the lap strip at the top. ' +
            'Every chart shares one cursor; wheel zooms, drag pans, double-click resets.</p>' +
            '<p>The line under the layout bar names the lap shown on each side. In the lap strip, click a lap to show it; click its <b>\u2691</b> to make it the reference lap (Incremental Time Difference).</p>' +
            '<p><kbd>D</kbd> distance/time axis · <kbd>R</kbd> research channels · <kbd>A</kbd>/<kbd>B</kbd> freeze a side · <kbd>Esc</kbd> reset zoom</p>' +
            '<p>Tier badges: <span class="tier FACT">FACT</span> <span class="tier CONFIRMED">CONFIRMED</span> <span class="tier DERIVED">DERIVED</span> ' +
            '<span class="tier RESEARCH">RESEARCH</span>. Read-only: nothing here writes to the game.</p>' +
            '<p style="text-align:right"><button class="tv-btn">Close</button></p></div>';
        m.onclick = function (e) { if (e.target === m || e.target.tagName === 'BUTTON') m.remove(); };
        document.body.appendChild(m);
    };

    /* ---- race panes ---- */

    function hpanel(title, right) {
        var p = panel(title, null, right);
        p.classList.add('hdr');
        return p;
    }

    /* ---- 1.0.1 session archive helpers (pure; unit-tested) ---- */
    /* Review mode: every console endpoint gets ?session=<id>; the session list
     * and data-manager routes do not. */
    function reviewUrl(url, id) {
        if (!id || typeof url !== 'string' || url.indexOf('/sessions') === 0 || url.indexOf('/recordings') === 0) return url;
        return url + (url.indexOf('?') >= 0 ? '&' : '?') + 'session=' + encodeURIComponent(id);
    }
    /* "20261012-213454_s5_g12" -> "2026-10-12 21:34 Qualifying" */
    function reviewTitle(id) {
        /* R2: a storage v2 recording (v2_<hex>) carries its display name in &name= (set by the Recordings page) */
        if (/^v2_[0-9a-f]{16}$/.test(String(id || ''))) {
            var nm = typeof location !== 'undefined' ? /[?&]name=([^&]*)/.exec(location.search) : null;
            try { return nm ? decodeURIComponent(nm[1].replace(/\+/g, ' ')) : 'Past recording'; } catch (e) { return 'Past recording'; }
        }
        var m = /^(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})(\d{2})_s(\d+)/.exec(String(id || ''));
        if (!m) return String(id || '');
        return m[1] + '-' + m[2] + '-' + m[3] + ' ' + m[4] + ':' + m[5] + ' ' + sessName(+m[7]);
    }
    /* ---- 1.2.1 GUI helpers (pure; unit-tested): source labels, recording names, session order, notices ---- */
    var MODE_TEXT = { 'Race': 'Single player', 'CO-OP': 'CO-OP', 'Multiplayer': 'Multiplayer' };
    var WEEKDAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    var MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    var NOTICE_HIDE_MS = 6000;
    /* Storage v2 names (storage_v2.inc): weekend "<COUNTRY> <YEAR> - <Mode> - YYYY-MM-DD HH-MM"; session
     * "<COUNTRY> <YEAR> - <Mode> - <Session> - YYYY-MM-DD HH-MM". Test Day has no session part. null = not a storage name. */
    function parseRecName(name) {
        /* "_2", "_3" ... is the storage collision suffix (sv2_unique_stem); it is accepted and ignored. */
        var m = /^(.+?) (\d{4}) - (.+?) - (?:(.+?) - )?(\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})(?:_\d+)?$/.exec(String(name || ''));
        if (!m) {
            /* Weekend folder written with no game mode (older build): "<COUNTRY> <YEAR> - YYYY-MM-DD HH-MM". */
            var w = /^(.+?) (\d{4}) - (\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})(?:_\d+)?$/.exec(String(name || ''));
            if (!w) return null;
            var pw = { country: w[1], year: w[2], mode: '', session: '', y: +w[3], mo: +w[4], d: +w[5], hh: w[6], mi: w[7] };
            if (pw.mo < 1 || pw.mo > 12 || pw.d < 1 || pw.d > 31) return null;
            return pw;
        }
        var p = { country: m[1], year: m[2], mode: m[3], session: m[4] || '', y: +m[5], mo: +m[6], d: +m[7], hh: m[8], mi: m[9] };
        if (p.mo < 1 || p.mo > 12 || p.d < 1 || p.d > 31) return null;
        return p;
    }
    function titleWords(s) {
        return String(s).toLowerCase().replace(/(^|[\s\-])([a-z])/g, function (m, a, b) { return a + b.toUpperCase(); });
    }
    /* Plain-words parts of a storage name. "Warmup" is shown as "Warm-up"; Test Day has no session part, so its mode is the session. */
    function recParts(name) {
        var p = parseRecName(name);
        if (!p) return null;
        var wd = new Date(Date.UTC(p.y, p.mo - 1, p.d)).getUTCDay();
        var session = p.session === 'Warmup' ? 'Warm-up' : p.session;
        if (!session && p.mode === 'Test Day') session = 'Test Day';
        return { track: titleWords(p.country) + ' ' + p.year, mode: MODE_TEXT[p.mode] || p.mode || 'mode not recorded', session: session,
            when: WEEKDAYS[wd] + ' ' + p.d + ' ' + MONTHS[p.mo - 1] + ', ' + p.hh + ':' + p.mi };
    }
    /* Weekend title: "Brazil 1995 - Single player - Tue 13 Oct, 16:14". The raw name when it is not a storage name. */
    function recTitle(name) {
        var r = recParts(name);
        return r ? r.track + ' - ' + r.mode + ' - ' + r.when : String(name || '');
    }
    /* Session label ("Practice 1", "Qualifying", "Warm-up", "Race", "Test Day"); '' for a weekend name. */
    function sessionLabel(name) {
        var r = recParts(name);
        return r && r.session ? r.session : '';
    }
    /* Track, session (or mode) and time without a prefix: "Brazil 1995 - Race - Tue 13 Oct, 16:14". */
    function recShortTitle(name) {
        var r = recParts(name);
        return r ? r.track + ' - ' + (r.session || r.mode) + ' - ' + r.when : String(name || '');
    }
    /* Past-mode banner text. */
    function pastBannerText(name) { return 'Watching past session: ' + recShortTitle(name); }
    /* Short name for a slot label: "Brazil 1995 - Race". */
    function pastShortName(name) {
        var r = recParts(name);
        return r ? r.track + ' - ' + (r.session || r.mode) : String(name || '');
    }
    /* Slot source label: "Live", or "Past: <short name>" in past mode. */
    function slotSourceText(past, shortName) { return past ? 'Past: ' + (shortName || 'none chosen') : 'Live'; }
    /* Filter key of a recording: single | coop | mp | other (Championship and Test Day are other). */
    function recModeKey(name) {
        var p = parseRecName(name);
        if (!p) return 'other';
        return p.mode === 'Race' ? 'single' : p.mode === 'CO-OP' ? 'coop' : p.mode === 'Multiplayer' ? 'mp' : 'other';
    }
    /* Recording filter: modeKey 'all' or one key; query matches the storage name or its plain title (case-insensitive). */
    function recMatches(name, query, modeKey) {
        if (modeKey && modeKey !== 'all' && recModeKey(name) !== modeKey) return false;
        var q = String(query || '').trim().toLowerCase();
        if (!q) return true;
        return (String(name || '') + ' ' + recTitle(name)).toLowerCase().indexOf(q) >= 0;
    }
    /* Weekend order inside a recording: Practice 1-4, Qualifying, Warm-up, Race (other labels 0). */
    function sessionRank(label) {
        var l = String(label || '').toLowerCase().replace(/[\s\-]/g, ''), m = /^practice(\d)$/.exec(l);
        if (m) return +m[1];
        return l === 'qualifying' ? 5 : l === 'warmup' ? 6 : l === 'race' ? 7 : 0;
    }
    /* Newest-first key from the date and time in a storage name ("202610131614"); '' when absent. */
    function recKey(name) {
        var m = /(\d{4})-(\d{2})-(\d{2})[ _](\d{2})-(\d{2})/.exec(String(name || ''));
        return m ? m[1] + m[2] + m[3] + m[4] + m[5] : '';
    }
    /* Card subtitle: "2 sessions · 2.0 MB · Recording now · 1 recovered". */
    function recSummary(rec) {
        var s = rec.sessions || [], open = 0, recovered = 0, archived = 0;
        s.forEach(function (x) {
            if (x.state === 'open') open++;
            else if (x.state === 'recovered') recovered++;
            else if (x.state === 'archived') archived++;
        });
        var out = [s.length + (s.length === 1 ? ' session' : ' sessions'), bytesText(rec.bytes)];
        if (open) out.push('Recording now');
        if (recovered) out.push(recovered + ' recovered');
        if (archived) out.push(archived + ' archived');
        return out.join(' · ');
    }
    /* Session state badge: text, css class and tooltip. */
    function stateBadge(state) {
        switch (state) {
        case 'open': return { text: 'Recording now', cls: 'live', tip: 'This session is being recorded now. It cannot be selected or deleted.' };
        case 'closed': return { text: 'Closed', cls: 'closed', tip: 'Finished and complete.' };
        case 'recovered': return { text: 'Recovered', cls: 'warn', tip: 'A previous run was cut short. The readable part is kept.' };
        case 'archived': return { text: 'Archived', cls: 'closed', tip: 'Compact permanent copy. The working copy was moved.' };
        case 'archive_interrupted': return { text: 'Archive interrupted', cls: 'warn', tip: 'An archive was interrupted. The working copy is intact.' };
        case 'damaged': return { text: 'Damaged', cls: 'bad', tip: 'Damaged file, could not be repaired. It is kept.' };
        default: return { text: String(state || 'unknown'), cls: 'closed', tip: '' };
        }
    }
    /* Success notices hide after 6 s; errors stay until Dismiss. Returns the delay in ms (0 = stays). */
    function noticeHideMs(bad) { return bad ? 0 : NOTICE_HIDE_MS; }
    /* Header line in LIVE mode: "Live: Brazil Race - recording now", or "Live: game not running". */
    function liveLineText(st) {
        if (!st || !st.running) return 'Live: game not running';
        return 'Live: ' + (st.track || 'unknown track') + ' ' + (st.type === undefined || st.type === null ? 'session' : sessName(st.type)) +
            (st.recording ? ' - recording now' : ' - not recording');
    }
    /* Lap count text for the session panel: "4 / 53" when the game has a real lap limit, "4" otherwise.
     * tl <= 0 = unknown; tl >= 1000 = INT_MAX sentinel (practice has no lap limit). ll is the 0-based lap. */
    function lapCountText(tl, ll) {
        var cur = tl > 0 ? lapNo(ll) : (ll || 0);
        return tl > 0 && tl < 1000 ? cur + ' / ' + tl : String(cur);
    }
    /* Track name for the header and the live line: the display name when the DLL sends one, else the folder name. */
    function headerTrackText(j) {
        return (j && (j.trknm || trackDisplayName(j.trk))) || 'unknown track';
    }
    /* B5 (API 9, no DLL field): "2002_Silverstone" -> "Great Britain 2002 - Silverstone". Country per track folder name, taken
     * from the game's own SEASONDATA\CIRCUITS\<Country>\<YEAR>_<Track> folders (owner install, 2026-10-14). Unknown track:
     * "Track 2002"; no year prefix: the raw name. */
    var TRACK_COUNTRY = {"a-1 ring": "Austria", "adelaide": "Australia", "aida": "Pacific", "albert park": "Australia", "barcelona": "Spain",
        "buenos": "Argentina", "buenos aires": "Argentina", "donington": "Europe", "estoril": "Portugal", "hockenheim": "Germany",
        "hungaroring": "Hungary", "imola": "San Marino", "indianapolis": "United States", "interlagos": "Brazil", "istanbul": "Turkey",
        "jerez": "Europe", "kyalami": "South Africa", "magny-cours": "France", "monaco": "Monaco", "montreal": "Canada", "monza": "Italy",
        "nurburgring": "Europe", "oscar": "Argentina", "sakhir": "Bahrain", "sepang": "Malaysia", "shanghai": "China",
        "silverstone": "Great Britain", "spa": "Belgium", "spa francorchamps": "Belgium", "suzuka": "Japan", "montjuich": "Spain"};
    function trackDisplayName(trk) {
        var m = /^(\d{4})[A-Za-z]*_(.+)$/.exec(String(trk || ''));
        if (!m) return trk ? String(trk) : '';
        var name = m[2].replace(/_/g, ' '), country = TRACK_COUNTRY[name.toLowerCase()];
        return country ? country + ' ' + m[1] + ' - ' + name : name + ' ' + m[1];
    }
    /* Status-bar notice in past mode. '' = no notice (live, or a review). */
    function statusNoticeText(past, review, hasPast) {
        if (!past || review) return '';
        return hasPast ? 'Live values are hidden in past mode.' : 'No past session open. Pick a recording on the Recordings page.';
    }
    function bytesText(n) {
        if (!finite(n) || n < 0) return '-';
        if (n >= 1073741824) return (n / 1073741824).toFixed(2) + ' GB';
        if (n >= 1048576) return (n / 1048576).toFixed(1) + ' MB';
        if (n >= 1024) return Math.round(n / 1024) + ' KB';
        return n + ' B';
    }
    /* slot -> ['A', 'C', ...]: which active sides show that car. */
    Console.prototype.slotLetters = function () {
        var out = {}, self = this;
        this.letters().forEach(function (L) {
            var s = self.side(L);
            if (s && s.slot >= 0) (out[s.slot] = out[s.slot] || []).push(L);
        });
        return out;
    };

    Console.prototype.towerPane = function (p, full) {
        var self = this;
        var box = el('div', 'f1x-scroll'); box.style.top = '0';
        p.body.appendChild(box);
        var cols = full ?
            ['P', 'Driver', 'Team', 'Gap', 'Int', 'Last', 'Best', 'Δ', 'S1', 'S2', 'S3', 'Spd', 'Grd', 'Tyr', 'Fuel', 'Stp', 'Lap', 'Status'] :
            ['P', 'Driver', 'Team', 'Gap', 'Int', 'Last', 'Best', 'S1', 'S2', 'S3', 'Tyr', 'Stp', 'Status'];
        var t = el('table', 'f1x-tw');
        t.innerHTML = '<thead><tr>' + cols.map(function (c, i) { return '<th' + (i === 1 ? ' class="l"' : '') + '>' + c + '</th>'; }).join('') + '</tr></thead>';
        var tb = el('tbody'); t.appendChild(tb); box.appendChild(t);
        var store = {};
        this.refreshers.push(function () {
            var j = self.frame, ca = self.cars();
            p.head.querySelector('.r').textContent = ca.length ? ca.length + ' cars' : '';
            if (!ca.length) { tb.innerHTML = '<tr><td class="l" colspan="' + cols.length + '">Waiting for the game: start a session in F1 Challenge.</td></tr>'; Object.keys(store).forEach(function (k) { delete store[k]; }); return; }
            if (tb.firstChild && !tb.firstChild._h && tb.firstChild.nodeName === 'TR') tb.innerHTML = '';
            var b1 = 999, b2 = 999, b3 = 999, byId = {}, ahead = {}, leader = ca[0];
            ca.forEach(function (c, i) {
                byId[c.id] = c; if (i) ahead[c.id] = ca[i - 1];
                var x2 = s2Of(c);
                if (c.ls1 > 0 && c.ls1 < b1) b1 = c.ls1; if (x2 > 0 && x2 < b2) b2 = x2;
                var x3 = s3Of(c); if (x3 > 0 && x3 < b3) b3 = x3;
            });
            var sideBy = self.slotLetters();
            var raceSession = !!(j && j.ty === 7);
            var lapDown = raceSession ? WB.lapsDown(ca) : {};
            var blue = raceSession ? blueFlags(ca, self.frame ? self.frame.trkl : 0) : {};
            /* rc.9: Stp = completed stops counted from /pits. pt0/p1/p2 are
             * planned pit-window distances, not stops made (live: Raikkonen
             * showed 3 with no stop). */
            var stopCount = {};
            (self.pits || []).forEach(function (q) { if (q && q.done && self.tapeVisible(q)) stopCount[q.id] = (stopCount[q.id] || 0) + 1; });
            diffRows(tb, store, ca.map(function (c) { return c.id; }), function (id) {
                var c = byId[id], s3 = s3Of(c), x2 = s2Of(c), stops = stopCount[id] || 0;
                var pc = c.p <= 3 ? ' p' + c.p : '';
                var ab = (sideBy[c.id] || []).map(function (L) {
                    return '<span class="ab" style="background:' + COLORS[L] + '">' + L + '</span>';
                }).join('');
                /* TAPE2 V1: the human badge (YOU / HUMAN) replaces the old "(friend)" tag; AI copies keep their text. */
                var role = humanBadgeHtml(c, (sideBy[c.id] || [])[0]) + (c.cr === 2 && c.a ? ' <span style="color:#5f7fc4">(friend\'s car, AI driving)</span>' : '');
                var dl = c.bl > 0 && c.ll > 0 ? c.bl - c.ll : null;
                /* 4.4.0-rc.5 task 7: lapped cars carry a +nL tag (laps down). */
                var lapTag = lapDown[c.id] ? ' <span class="lapd" title="Lapped: ' + lapDown[c.id] + ' lap(s) down">+' + lapDown[c.id] + 'L</span>' : '';
                var h = '<td><span class="pos' + pc + '">' + c.p + '</span></td><td class="l" title="' + esc(c.n) + ' · ' + esc(c.cn || '') + '">' + esc(cleanName(c.n)) + role + ab + lapTag + blueTagOf(blue, c.id) + '</td>' +
                    '<td class="team">' + esc(teamOf(c)) + '</td>' +
                    '<td>' + gapTo(c, leader, raceSession) + '</td><td>' + gapTo(c, ahead[c.id], raceSession) + '</td><td>' + tsec(c.ll) + '</td><td>' + tsec(c.bl) + '</td>';
                if (full) h += '<td class="' + (dl === null ? '' : dl > 0.001 ? 'rd' : dl < -0.001 ? 'gn' : '') + '">' + (dl === null ? '' : Math.abs(dl) < 0.001 ? '0.000' : (dl > 0 ? '+' : '') + dl.toFixed(3)) + '</td>';
                h += '<td' + (c.ls1 > 0 && c.ls1 <= b1 + 1e-4 ? ' class="sct"' : '') + '>' + tsec(c.ls1) + '</td>' +
                    '<td' + (x2 > 0 && x2 <= b2 + 1e-4 ? ' class="sct"' : '') + '>' + tsec(x2) + '</td>' +
                    '<td' + (s3 > 0 && s3 <= b3 + 1e-4 ? ' class="sct"' : '') + '>' + (s3 > 0 ? tsec(s3) : '-') + '</td>';
                if (full) h += '<td>' + (c.s ? (c.s * 3.6).toFixed(0) : '-') + '</td><td>' + (c.gp > 0 ? c.gp : '-') + '</td>';
                h += '<td><span class="f1x-tyre" title="' + tyreTitle(c.tc, c.tcn) + '">' + tyreName(c.tc, c.tcn) + '</span></td>';
                if (full) h += '<td>' + (c.fc ? c.fc.toFixed(2) : '-') + '</td>';
                h += '<td>' + (stops || '-') + '</td>';
                if (full) h += '<td>' + lapNo(c.l) + '</td>';
                h += '<td>' + statusCell(c) + '</td>';
                return h;
            }, function (id) {
                var c = byId[id], cl = '';
                if (c.fs === 2 || c.fs === 3 || c.rt) cl += ' dn'; else if (c.dm) cl += ' doomed'; else if (c.pt >= 2) cl += ' pit';
                if (id === self.focusId) cl += ' foc';
                if (j && id === j.ps) cl += ' plr';
                return cl.trim();
            }, function (id, e, dbl) { self.pickCar(id, e, dbl); });
        });
    };

    /* Track map with every car: AIW waypoint line, car dots by position,
     * A/B rings, click a car to focus it. Map-frame coordinates (mx/mz) when
     * present, world coordinates otherwise (same rule as the classic page). */
    Console.prototype.raceMap = function (p) {
        var self = this, base = el('canvas');
        p.body.appendChild(base);
        var st = this.mapState, hits = [], drag = null;
        base.addEventListener('wheel', function (e) {
            e.preventDefault();
            st.zoom = Math.max(0.5, Math.min(8, st.zoom * (e.deltaY < 0 ? 1.15 : 0.87)));
            obj.draw();
        }, { passive: false });
        base.addEventListener('mousedown', function (e) { drag = { x: e.clientX, y: e.clientY, px: st.panX, py: st.panY, moved: false }; });
        window.addEventListener('mousemove', function (e) {
            if (!drag || !base.isConnected) return;
            var dx = e.clientX - drag.x, dy = e.clientY - drag.y;
            if (Math.abs(dx) > 3 || Math.abs(dy) > 3) { drag.moved = true; st.panX = drag.px + dx; st.panY = drag.py + dy; obj.draw(); }
        });
        window.addEventListener('mouseup', function () { setTimeout(function () { drag = null; }, 0); });
        base.addEventListener('dblclick', function () { st.zoom = 1; st.panX = 0; st.panY = 0; obj.draw(); });
        base.addEventListener('click', function (e) {
            if (drag && drag.moved) return;
            var rc = base.getBoundingClientRect(), mx = e.clientX - rc.left, my = e.clientY - rc.top, best = null, bd = 1e9;
            hits.forEach(function (h) {
                var d = (mx - h.x) * (mx - h.x) + (my - h.y) * (my - h.y);
                if (d < (h.r + 10) * (h.r + 10) && d < bd) { bd = d; best = h; }
            });
            if (best && self.topTab === 'tv') { self.tvPickDriver(best.id); return; }
            if (best) self.pickCar(best.id, e, false);
        });
        var obj = {
            draw: function () {
                var c = setupCanvas(base), ctx = c.ctx;
                hits = [];
                var ca = self.cars().filter(function (q) { return q.ot2 === undefined || q.ot2; });
                var wp = [];
                (self.waypoints || []).forEach(function (w) {
                    var x = Array.isArray(w) ? w[0] : w.x, z = Array.isArray(w) ? w[1] : w.z;
                    if (finite(x) && finite(z)) wp.push([x, z]);
                });
                var useTrack = wp.length > 1 || ca.some(function (q) { return Math.abs(q.mx || 0) > 0.01 || Math.abs(q.mz || 0) > 0.01; });
                var pos = function (q) { return useTrack ? [q.mx || 0, q.mz || 0] : [q.px || 0, q.pz || 0]; };
                var minx = Infinity, maxx = -Infinity, minz = Infinity, maxz = -Infinity;
                var acc = function (x, z) { if (x < minx) minx = x; if (x > maxx) maxx = x; if (z < minz) minz = z; if (z > maxz) maxz = z; };
                wp.forEach(function (q) { acc(q[0], q[1]); });
                if (!wp.length) ca.forEach(function (q) { var v = pos(q); acc(v[0], v[1]); });
                ctx.font = '10px ' + PAL.sans;
                if (!finite(minx)) {
                    ctx.fillStyle = PAL.axis; ctx.textAlign = 'center';
                    ctx.fillText('Track map appears when the game is in a session.', c.w / 2, c.h / 2);
                    return;
                }
                if (maxx - minx < 100) { var ex = (100 - (maxx - minx)) / 2; minx -= ex; maxx += ex; }
                if (maxz - minz < 100) { var ez = (100 - (maxz - minz)) / 2; minz -= ez; maxz += ez; }
                var pad = 22, sc = Math.min((c.w - 2 * pad) / (maxx - minx), (c.h - 2 * pad - 14) / (maxz - minz));
                var ox = (c.w - (maxx - minx) * sc) / 2, oz = (c.h - (maxz - minz) * sc) / 2 + 7;
                var view = function (x, z) {
                    return [ox + (x - minx) * sc * st.zoom + st.panX - (st.zoom - 1) * c.w / 2,
                            c.h - (oz + (z - minz) * sc) * st.zoom + st.panY + (st.zoom - 1) * c.h / 2];
                };
                if (wp.length) {
                    ctx.strokeStyle = PAL.track; ctx.lineWidth = 7; ctx.lineJoin = 'round'; ctx.lineCap = 'round';
                    ctx.beginPath();
                    wp.forEach(function (q, i) { var v = view(q[0], q[1]); if (i) ctx.lineTo(v[0], v[1]); else ctx.moveTo(v[0], v[1]); });
                    ctx.closePath(); ctx.stroke();
                    ctx.strokeStyle = PAL.lineDim; ctx.lineWidth = 1;
                    ctx.stroke();
                    var s0 = view(wp[0][0], wp[0][1]);
                    ctx.fillStyle = PAL.start; ctx.fillRect(s0[0] - 1.5, s0[1] - 6, 3, 12);
                }
                var r = Math.max(5, Math.min(9, c.w / 70));
                var sideBy = self.slotLetters();
                /* 4.4.0-rc.6 task 5 (DERIVED lap count): lapped cars are hollow with an L tag. */
                var isRace = !!(self.frame && self.frame.ty === 7);
                var lappedMap = isRace ? WB.lapsDown(ca) : {};
                var blueMap = isRace ? blueFlags(ca, self.frame ? self.frame.trkl : 0) : {};
                ca.slice().reverse().forEach(function (q) {
                    var v = view.apply(null, pos(q));
                    var isLapped = !!lappedMap[q.id];
                    hits.push({ x: v[0], y: v[1], r: r, id: q.id });
                    var col = q.p <= 3 ? POSCOL[q.p - 1] : q.cr === 0 ? '#ffd84a' : q.cr === 2 && !q.a ? '#7fd4ff' : '#c9d4f2';
                    if (q.fs === 2 || q.fs === 3 || q.rt) col = '#55607a';
                    if (sideBy[q.id]) sideBy[q.id].forEach(function (L, k) {
                        ctx.strokeStyle = COLORS[L]; ctx.lineWidth = 3;
                        ctx.beginPath(); ctx.arc(v[0], v[1], r + 4 + k * 4, 0, Math.PI * 2); ctx.stroke();
                    });
                    if (self.topTab === 'tv' && q.id === self.tvAirId) { ctx.strokeStyle = '#e10600'; ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(v[0], v[1], r + 5, 0, Math.PI * 2); ctx.stroke(); }
                    if (self.topTab === 'tv' && q.id === self.tvSelDriver) { ctx.strokeStyle = '#38d6e8'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(v[0], v[1], r + 9, 0, Math.PI * 2); ctx.stroke(); }
                    if (blueMap[q.id]) { ctx.strokeStyle = '#2f7bff'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(v[0], v[1], r + 8, 0, Math.PI * 2); ctx.stroke(); }
                    ctx.fillStyle = isLapped ? '#0a0a0f' : col;
                    ctx.beginPath(); ctx.arc(v[0], v[1], r, 0, Math.PI * 2); ctx.fill();
                    ctx.strokeStyle = q.id === self.focusId ? '#ffffff' : '#000'; ctx.lineWidth = q.id === self.focusId ? 2 : 1; ctx.stroke();
                    if (isLapped) { ctx.strokeStyle = col; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(v[0], v[1], r, 0, Math.PI * 2); ctx.stroke(); }
                    ctx.fillStyle = isLapped ? col : '#000'; ctx.font = 'bold ' + Math.max(7, r) + 'px ' + PAL.sans; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
                    ctx.fillText(String(q.p), v[0], v[1] + 0.5);
                    ctx.fillStyle = '#9fb2e6'; ctx.font = '9px ' + PAL.sans; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom';
                    ctx.fillText((isLapped ? 'L ' : '') + code3(q.n) + (humanRole(q) ? ' ' + humanRole(q) : ''), v[0] + r + 2, v[1] - 2);   /* TAPE2 V1 */
                });
                ctx.textBaseline = 'alphabetic';
                ctx.fillStyle = PAL.axis; ctx.font = '9px ' + PAL.sans; ctx.textAlign = 'left';
                ctx.fillText(useTrack ? 'track coordinates · wheel zoom · drag pan · click a car' : 'world coordinates', 6, c.h - 5);
            }
        };
        obj.isMap = true;   /* TAPE2 V4: the race map is a map (no inset) */
        this.canvases.push(obj);
        return obj;
    };

    Console.prototype.flagHtml = function (d, fc) {
        var sg = d.sg || '', h = '';
        if (d.lapm > 0) {
            var b = [0, d.sc1, d.sc2, d.lapm], cs = (fc && fc.cs) || 0, sect = '';
            for (var i = 0; i < 3; i++) {
                /* A1.1 (owner): three equal cells; each shows its own length, the title keeps the metre range. */
                sect += '<span class="' + (cs === i + 1 ? 'on' : '') + '" title="Sector ' + (i + 1) + ': ' +
                    b[i].toFixed(0) + '–' + b[i + 1].toFixed(0) + ' m (from the track .AIW)"><b>S' + (i + 1) + '</b> ' + (b[i + 1] - b[i]).toFixed(0) + ' m</span>';
            }
            h += '<div class="k">SECTORS</div><div class="v">' + d.lapm.toFixed(0) + ' m</div><div class="w f1x-sect">' + sect + '</div>';
        } else h += '<div class="k">SECTORS</div><div class="v" style="color:#5f7fc4">AIW not read</div>';
        if (sg.length) {
            var cells = '';
            for (var k = 0; k < sg.length; k++) {
                var ch = sg.charAt(k), bg = ch === '0' ? '#0d1d4a' : ch === '2' ? '#f5c400' : '#c0392b';
                cells += '<span style="background:' + bg + (fc && fc.sgi === k ? ';outline:1px solid #fff' : '') + '" title="Marshal zone ' + (k + 1) + ' of ' + sg.length +
                    ' — state ' + ch + (ch === '2' ? ' (YELLOW)' : ch === '0' ? ' (green)' : ' (unmapped)') + '"></span>';
            }
            h += '<div class="k">FLAGS</div><div class="v">' + (d.yc > 0 ? '<span style="color:#f5c400;font-weight:bold">YELLOW ×' + d.yc + '</span>' : '<span style="color:#3ce07a">ALL GREEN</span>') + '</div>' +
                '<div class="w f1x-flagz" title="' + sg.length + ' marshal zones, start line first. Not timing sectors.">' + cells + '</div>';
            /* 4.4.0-rc.5 task 6: the white box is the focus car's zone. Say so, and
             * say when the car is stopped (the box then cannot move). */
            if (fc && Number.isInteger(fc.sgi) && fc.sgi >= 0 && fc.sgi < sg.length) {
                h += '<div class="w f1x-focuszone">White box: ' + esc(code3(fc.n)) + ' in marshal zone ' + (fc.sgi + 1) +
                    (fc.s > 0.5 ? '' : ' (stopped, so the box stays put)') + '</div>';
            }
            if (fc && fc.yf) h += '<div class="w" style="color:#f5c400;font-weight:bold">⚠ ' + esc(code3(fc.n)) + ' IS UNDER A YELLOW FLAG</div>';
        }
        return h;
    };

    Console.prototype.sessionPane = function (p) {
        var self = this, box = el('div', 'f1x-kv');
        p.body.classList.add('pad'); p.body.style.overflow = 'auto'; p.body.style.paddingTop = '6px';
        p.body.appendChild(box);
        this.refreshers.push(function () {
            var d = self.frame;
            if (!d) { box.innerHTML = '<div class="w" style="color:#5f7fc4">No session.</div>'; return; }
            var ca = self.cars(), fc = ca.filter(function (c) { return c.id === self.focusId; })[0];
            var mode = self.trackStatus();
            var tsCol = ['#3ce07a', '#ffd84a', '#ff4136'][mode] || '#3ce07a';
            p.head.querySelector('.r').textContent = sessName(d.ty) + (d.ph > 0 ? ' / ' + phaseName(d.ph) : '');
            var h = '<div class="w" style="text-align:center;margin-bottom:4px"><span class="f1x-ts" style="background:' + tsCol + '">' +
                (['GREEN', 'VSC', 'RED FLAG'][mode] || 'GREEN') + '</span></div>';
            h += '<div class="k">LAP</div><div class="v">' + lapCountText(d.tl, d.ll) + '</div>';
            h += '<div class="k">CARS</div><div class="v">' + (d.vc || ca.length) + '</div>';
            h += '<div class="k">RACE</div><div class="v">' + (d.ac ? 'RUNNING' : 'STANDBY') + '</div>';
            h += self.flagHtml(d, fc);
            h += '<div class="k">TRACK WETNESS</div><div class="v">' + wetText(d) + '</div>';
            h += '<div class="k">AIR / TRACK</div><div class="v">' + (d.at > -900 ? d.at.toFixed(1) + '°C' : '-') + ' / ' + (d.trkt > -900 ? d.trkt.toFixed(1) + '°C' : '-') + '</div>';
            /* rc.16: forecast with uncertainty from the game's timeline (no exact spoilers). */
            var fcst = forecastOf(self.health), fmin = finite(d.st) ? d.st / 60 : null;
            var fnow = fcst ? wxForecastAt(fcst, fmin) : null, fx2 = fcst ? wxForecast(fcst, fmin, wxForecastKey(d, fcst)) : null;
            var rain30 = fx2 && fx2.blocks.length ? Math.max(fx2.blocks[0].rain, fx2.blocks[1] ? fx2.blocks[1].rain : 0) : null;
            h += '<div class="k" title="Forecast, see the Weather page">SKY / FORECAST</div><div class="v">' +
                (fnow ? esc(wxSky(fnow.cond)) + (rain30 !== null ? ' · rain next 30 min ' + rain30 + '%' : '') : 'no forecast for this session') + '</div>';
            h += '<div class="k">AI / MODE</div><div class="v">' + (d.ai || 0) + '% · ' + (d.cm ? 'CHAMP' : 'SINGLE GP') + '</div>';
            if (d.flapid >= 0) {
                var fl = ca.filter(function (c) { return c.id === d.flapid; })[0];
                h += '<div class="k" style="color:#d58cff">FASTEST LAP</div><div class="v" style="color:#d58cff">' + tsec(d.flapt) + ' — ' + esc(fl ? code3(fl.n) : '?') + '</div>';
            }
            if (d.dotdid >= 0) {
                var dd = ca.filter(function (c) { return c.id === d.dotdid; })[0];
                h += '<div class="k" title="Our own composite score, not the game\'s internal calculation.">DRIVER OF DAY</div><div class="v">' + esc(dd ? code3(dd.n) : '?') + ' (score ' + d.dotdsc + ')</div>';
            }
            if (self.coopInfo()) h += '<div class="k">CO-OP</div><div class="v">' + esc(self.coopInfo()) + '</div>';
            box.innerHTML = h;
        });
    };

    Console.prototype.rcPane = function (p, max) {
        var self = this, box = el('div', 'f1x-rc f1x-scroll');
        p.body.appendChild(box);
        this.refreshers.push(function () {
            var d = self.frame, zones = d && d.sg ? d.sg.length : 0, h = '';
            /* only bulletins this page can word; unknown types are not shown or counted */
            var rel = self.incidents.filter(function (e) { return e.ty !== 'POS' && rcMsg(e, zones) && self.tapeVisible(e); });
            p.head.querySelector('.r').textContent = rel.length + ' msg';
            /* 4.4.0-rc.5 task 8: summary rows (fastest lap, stops, last box time, penalties, retirements). */
            var sum = WB.raceControlSummary(d ? d.c : [], self.pace, (self.pits || []).filter(function (q) { return self.tapeVisible(q); }));
            var sh = '<div class="f1x-rcsum"><div class="f1x-mf">' + esc(mfStatus(self.health).text) + '</div><div><b>Fastest lap</b> ' +
                (sum.fastest ? esc(sum.fastest.n) + ' L' + sum.fastest.lap + ' ' + lapText(sum.fastest.t) : '-') + '</div>';
            sum.rows.forEach(function (r) {
                sh += '<div><span>P' + (r.p > 0 ? r.p : '-') + ' ' + esc(r.n) + '</span> <span>stops ' + r.stops +
                    (r.lastBox > 0 ? ' · last box ' + r.lastBox.toFixed(1) + 's' : '') +
                    ' · penalties ' + r.pen + (r.retired ? ' · RETIRED' : '') + '</span></div>';
            });
            sh += '</div>';
            rel.slice().reverse().slice(0, max || 80).forEach(function (e) {
                var m = rcMsg(e, zones);
                /* TAPE2 V1: the driver the bulletin names gets the human badge (YOU / HUMAN) */
                var rcCar = (self.roster || []).filter(function (c) { return c.id === e.id; })[0];
                h += '<div class="' + m.s + '"><span class="lp">L' + (e.lp || 0) + '</span><span>' + m.t + humanBadgeHtml(rcCar, null) + '</span></div>';
            });
            var sig = rel.length + ':' + zones + ':' + sh;
            if (box._sig !== sig) { box._sig = sig; box.innerHTML = sh + (h || '<div><span>No bulletins yet this session.</span></div>'); }
        });
    };

    Console.prototype.pacePane = function (p, nLaps) {
        var self = this, box = el('div', 'f1x-scroll');
        p.body.appendChild(box);
        this.refreshers.push(function () {
            var cars = self.pace.slice().sort(function (a, b) { return (a.p || 999) - (b.p || 999); });
            var sig = JSON.stringify(cars.map(function (c) { return [c.id, c.p, (c.laps || []).length]; })) + self.focusId;
            if (box._sig === sig) return;
            box._sig = sig;
            if (!cars.length) { box.innerHTML = '<div class="f1x-note" style="padding:6px">No completed laps yet.</div>'; return; }
            var maxL = 0, best = 1e9;
            cars.forEach(function (c) { maxL = Math.max(maxL, (c.laps || []).length); (c.laps || []).forEach(function (v) { if (v > 0 && v < best) best = v; }); });
            var from = Math.max(0, maxL - (nLaps || 40));
            p.head.querySelector('.r').textContent = 'laps ' + (from + 1) + '–' + maxL;
            var h = '<table class="f1x-tw"><thead><tr><th class="l">Drv</th>';
            for (var l = from; l < maxL; l++) h += '<th>L' + (l + 1) + '</th>';
            h += '<th>Best</th></tr></thead><tbody>';
            cars.forEach(function (c) {
                var lp = c.laps || [], mn = 1e9;
                lp.forEach(function (v) { if (v > 0 && v < mn) mn = v; });
                h += '<tr data-id="' + c.id + '"' + (c.id === self.focusId ? ' class="foc"' : '') + '><td class="l" title="' + esc(c.n) + '">' + esc(code3(c.n)) + '</td>';
                for (var k = from; k < maxL; k++) {
                    var v = lp[k];
                    h += !(v > 0) ? '<td>-</td>' : '<td class="' + (v <= best + 1e-4 ? 'sct' : v <= mn + 1e-4 ? 'pb' : '') + '">' + lapText(v) + '</td>';
                }
                h += '<td style="color:#9fb2e6">' + (mn < 1e9 ? lapText(mn) : '-') + '</td></tr>';
            });
            box.innerHTML = h + '</tbody></table>';
            Array.prototype.forEach.call(box.querySelectorAll('tr[data-id]'), function (tr) {
                tr.onclick = function (e) { self.pickCar(parseInt(tr.getAttribute('data-id'), 10), e, false); };
                tr.ondblclick = function (e) { self.pickCar(parseInt(tr.getAttribute('data-id'), 10), e, true); };
            });
        });
    };

    /* ---- race pages ---- */

    Console.prototype.view_race = function (host) {
        var g = this.grid(host, 'minmax(0,1.25fr) minmax(0,0.62fr) minmax(0,0.62fr)', 'minmax(0,1.25fr) minmax(0,1fr)');
        var tw = hpanel('Timing Tower'); tw.style.gridRow = '1 / 3'; g.appendChild(tw);
        var mp = panel('Track Map', null); mp.style.gridColumn = '2 / 4'; g.appendChild(mp);
        var ss = hpanel('Session'); g.appendChild(ss);
        var rc = hpanel('Race Control'); g.appendChild(rc);
        this.towerPane(tw, false);
        this.raceMap(mp);
        this.sessionPane(ss);
        this.rcPane(rc, 60);
    };

    Console.prototype.view_timing = function (host) {
        var g = this.grid(host, 'minmax(0,1.35fr) minmax(0,1fr)', 'minmax(0,1.4fr) minmax(0,1fr)');
        var tw = hpanel('Timing Tower'); tw.style.gridRow = '1 / 3'; g.appendChild(tw);
        var pc = hpanel('Race Pace'); g.appendChild(pc);
        var rc = hpanel('Race Control'); g.appendChild(rc);
        this.towerPane(tw, true);
        this.pacePane(pc, 40);
        this.rcPane(rc, 200);
    };

    function dial(ctx, w, h, val, max, unit, red) {
        var cx = w / 2, cy = h / 2 - 2, r = Math.min(w, h) / 2 - 24, a0 = 0.75 * Math.PI, as = 1.5 * Math.PI;
        var a = function (v) { return a0 + Math.max(0, Math.min(1, v / max)) * as; };
        ctx.fillStyle = '#000'; ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = '#1d3fa8'; ctx.lineWidth = 1; ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
        /* segmented scale: 60 ticks, lit up to the value, red past the red line */
        var N = 60;
        for (var s = 0; s < N; s++) {
            var f = s / (N - 1), ang = a0 + f * as, lit = val !== null && f * max <= val + 1e-9;
            var hot = f * max >= red;
            ctx.strokeStyle = hot ? (lit ? '#ff4136' : '#3a1020') : (lit ? '#4f86ff' : '#0d1d4a');
            ctx.lineWidth = s % 5 === 0 ? 3 : 2;
            var r1 = r - (s % 5 === 0 ? 9 : 5), r2 = r + 1;
            ctx.beginPath();
            ctx.moveTo(cx + r1 * Math.cos(ang), cy + r1 * Math.sin(ang));
            ctx.lineTo(cx + r2 * Math.cos(ang), cy + r2 * Math.sin(ang));
            ctx.stroke();
        }
        ctx.fillStyle = '#5f7fc4'; ctx.font = '9px ' + PAL.sans; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        for (var i = 0; i <= 6; i++) {
            var tv = max * i / 6, ta = a(tv);
            ctx.fillText(String(Math.round(max >= 1000 ? tv / 1000 : tv)), cx + (r + 14) * Math.cos(ta), cy + (r + 14) * Math.sin(ta));
        }
        /* LCD readout box in the open lower part of the face */
        var bw = w * 0.52, bh = 26, bx = cx - bw / 2, by = cy + r * 0.32;
        ctx.fillStyle = '#02061a'; ctx.fillRect(bx, by, bw, bh);
        ctx.strokeStyle = '#14307f'; ctx.lineWidth = 1; ctx.strokeRect(bx + 0.5, by + 0.5, bw - 1, bh - 1);
        ctx.fillStyle = val !== null && val >= red ? '#ff7a6a' : '#9fe4ff';
        ctx.font = 'bold 20px ' + PAL.sans; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        ctx.fillText(val === null ? '---' : String(Math.round(val)), cx, by + bh / 2 + 1);
        ctx.fillStyle = '#5f7fc4'; ctx.font = '9px ' + PAL.sans; ctx.textBaseline = 'alphabetic';
        ctx.fillText(unit, cx, by + bh + 11);
    }

    Console.prototype.focusKv = function (fc, d) {
        var RPMF = WB.RPM_FACTOR, ca = this.cars(), h = '';
        var kv = function (k, v, title, style) {
            h += '<div class="k"' + (title ? ' title="' + esc(title) + '"' : '') + '>' + k + '</div><div class="v"' + (style ? ' style="' + style + '"' : '') + '>' + v + '</div>';
        };
        var s3 = s3Of(fc), s2f = s2Of(fc);
        kv('GEAR', fc.g > 0 ? fc.g : fc.g === 0 ? 'N' : 'R');
        kv('LAP', lapNo(fc.l));
        kv('LAST / BEST', tsec(fc.ll) + ' / ' + tsec(fc.bl));
        kv('S1 / S2 / S3', tsec(fc.ls1) + ' / ' + tsec(s2f) + ' / ' + (s3 > 0 ? tsec(s3) : '-'));
        if (fc.secel > 0 && fc.lsn) kv('SECTOR S' + fc.lsn + ' ELAPSED', fc.secel.toFixed(3), 'Elapsed time in the sector this car is in right now.');
        if (fc.lspn) kv('LAST S' + fc.lspn + ' vs PB', (fc.lspb <= 0 ? '' : '+') + fc.lspb.toFixed(3), 'Completed sector time minus the personal best that existed before it.', fc.lspb <= 0 ? 'color:#3ce07a' : 'color:#ff6a6a');
        kv('TYRE', tyreName(fc.tc, fc.tcn), tyreTitle(fc.tc, fc.tcn));
        /* INT_MAX lap total = no lap limit (practice): no laps-left or fuel-needed figure */
        var limited = d.tl > 0 && d.tl < 1000;
        var lapsLeft = limited ? Math.max(0, d.tl - (fc.l || 0)) : 0, need = fc.fc && limited ? fc.fc * lapsLeft : 0;
        kv('FUEL LEFT', fc.fl > 0 ? fc.fl.toFixed(1) + ' L' : '-');
        kv('FUEL/LAP · NEEDED', (fc.fc ? fc.fc.toFixed(2) : '-') + ' · ' + (need > 0 ? need.toFixed(1) : '-') + (limited ? ' (' + lapsLeft + ' laps left)' : ' (no lap limit)'));
        if (fc.nf > 0) kv('NEXT STINT', fc.nf.toFixed(1) + (need > fc.nf ? ' SHORT' : ' OK'), '', need > fc.nf ? 'color:#ff4136' : 'color:#3ce07a');
        if (fc.favg > 0) kv('FUEL AVG / AUTONOMY', fc.favg.toFixed(2) + '/lap · ' + (fc.faut >= 0 ? fc.faut.toFixed(1) + ' laps' : '-'), 'Rolling average over up to 8 completed laps.');
        kv('CALLED TO BOX', fc.cb ? 'YES' : 'no', 'The game\'s own pit-intention flag (call_to_box).', fc.cb ? 'color:#fc0' : '');
        if (fc.boxsoon) kv('BOX SOON?', 'likely — ' + esc(fc.boxwhy), 'Heuristic only, not a game flag.', 'color:#fc0');
        if (fc.topl !== undefined) kv('TOP SPEED lap/prev/sess', (fc.topl * 3.6).toFixed(0) + ' / ' + (fc.toppl * 3.6).toFixed(0) + ' / ' + (fc.tops * 3.6).toFixed(0) + ' km/h', 'Running max of the real speed field (the game max_speed field is a 99999 sentinel).');
        if (fc.gai >= 0 || fc.gbi >= 0) {
            var ah = ca.filter(function (x) { return x.id === fc.gai; })[0], bh = ca.filter(function (x) { return x.id === fc.gbi; })[0];
            kv('RADAR', (ah ? '↑ ' + esc(code3(ah.n)) + ' ' + fc.ga.toFixed(0) + ' m' : '↑ —') + ' &nbsp; ' + (bh ? '↓ ' + esc(code3(bh.n)) + ' ' + fc.gb.toFixed(0) + ' m' : '↓ —'),
                'Distance along the track to the nearest car ahead/behind, lap-corrected. Not a time gap.');
        }
        var D = DMG[fc.dt || 0] || DMG[0];
        kv('DAMAGE', fc.dt ? D.t : 'NONE', D.d, fc.dt ? 'color:' + D.c : '');
        kv('FRONT / REAR WING', (fc.fw ? '<span style="color:#fc0">DETACHED</span>' : 'intact') + ' / ' + (fc.rw ? '<span style="color:#ff4136">DETACHED</span>' : 'intact'));
        kv('ENGINE', fc.ed ? 'DEAD' + (fc.rc === 1 ? ' (ENGINE)' : '') : 'running', '', fc.ed || fc.rc === 1 ? 'color:#ff4136' : '');
        kv('ENG ANGVEL', (fc.eav !== undefined ? fc.eav.toFixed(0) : '-') + ' <span style="color:#5f7fc4">(rpm ' + (fc.r !== undefined ? (fc.r * RPMF).toFixed(0) : '-') + ')</span>',
            'Crankshaft angular velocity (veh+0xA0). Mirrors RPM on a healthy engine; both at 0 = engine truly dead.');
        kv('OIL / WATER', (fc.ot !== undefined ? fc.ot.toFixed(1) + '°' : '-') + ' / ' + (fc.wt !== undefined ? fc.wt.toFixed(1) + '°' : '-'),
            'Per-car vehicle fields. Engine life halves per 2.9 C above 123.43 C oil (engine.ini LifetimeOilTemp).', fc.ot > 123 ? 'color:#ff4136' : fc.ot > 115 ? 'color:#fc0' : '');
        if (fc.id === d.ps && d.otp !== undefined) kv('GAUGE OIL / WATER', (d.otp > -900 ? d.otp.toFixed(1) + '°' : '-') + ' / ' + (d.wtp > -900 ? d.wtp.toFixed(1) + '°' : '-'),
            'As the in-car steering-wheel display reads them (globals 0x7C9174 / 0x7C91B4). Screen-confirmed.');
        var WN = ['FL', 'FR', 'RL', 'RR'];
        if (fc.id === d.ps && d.btmp) kv('BRAKE TEMP', WN.map(function (w, i) { var v = d.btmp[i]; return w + ' ' + (v > -900 ? '<span style="' + (v > 750 ? 'color:#ff4136' : v > 600 ? 'color:#fc0' : '') + '">' + v.toFixed(0) + '</span>' : '-'); }).join(' · '),
            'Brake disc temperature per wheel, C. Wheel order DERIVED from the HUD draw order.');
        if (fc.id === d.ps && d.ttmp) kv('TYRE TEMP', WN.map(function (w, i) { var z = d.ttmp[i] || []; return w + ' ' + [0, 1, 2].map(function (k) { return z[k] > -900 ? z[k].toFixed(0) : '-'; }).join('/'); }).join(' · '),
            'Tyre temperature, C, 3 zones per wheel (same strip the in-car HUD draws).');
        var life = null;
        if (d.trd && fc.id === d.ps) life = d.trd.map(function (v) { return Math.max(0, Math.min(1, v)) * 100; });
        else if (fc.wfl !== undefined) life = [fc.wfl, fc.wfr, fc.wrl, fc.wrr].map(treadLife);
        if (life) {
            var mn = Math.min.apply(null, life);
            kv('TYRE LIFE %', '<span style="' + lifeCol2(mn) + '">' + life.map(function (x) { return x === null ? '-' : x.toFixed(1); }).join(' / ') + '</span>',
                'FL/FR/RL/RR. 100 = fresh, 0 = blowout (raw tread 0.75). 70 and 40 are the light and severe blister call thresholds. Wear, not grip.');
        }
        kv('ENGINE LIFE', fc.wg === 1 ? '<span style="' + lifeCol(fc.lpe) + '">' + lifeTxt(fc.lfe, fc.lpe, fc.ll) + '</span>' : '<span style="color:#5f7fc4">n/a — not simulated</span>',
            'Seconds of engine life left (veh+0x3D70); the countdown runs only for veh+0x32C8==1 (AI).');
        kv('BRK / GBX / SUS', '<span style="' + lifeCol(fc.lpb) + '">' + lifeShort(fc.lfb) + '</span> / <span style="' + lifeCol(fc.lpg) + '">' + lifeShort(fc.lfg) +
            '</span> / <span style="' + lifeCol(fc.lps) + '">' + lifeShort(fc.lfs) + '</span>');
        if (fc.dm) kv('PROGNOSIS', 'WILL RETIRE — ' + esc(doomTag(fc)), doomWhy(fc), 'color:#ff4136');
        kv('CLASSIFICATION', esc(reasonTxt(fc)));
        kv('TRACK LIMITS / PENALTIES', (fc.tl || 0) + ' / ' + (fc.pn || 0));
        return h;
    };

    Console.prototype.view_focus = function (host) {
        var self = this;
        var g = this.grid(host, 'minmax(330px,0.9fr) minmax(0,1.3fr)', 'minmax(0,1fr) minmax(0,1fr)');
        var left = hpanel('Driver Focus'); left.style.gridRow = '1 / 3'; g.appendChild(left);
        var dos = hpanel('Laps & Pit Stops'); g.appendChild(dos);
        var mp = panel('Track Map', null); g.appendChild(mp);
        left.body.classList.add('pad'); left.body.style.overflow = 'auto'; left.body.style.paddingTop = '6px';
        var btns = el('div', 'f1x-btns');
        var toA = el('button', 'A', 'Show on A'), toB = el('button', 'B', 'Show on B'), lap = el('button', '', 'Open Lap Analysis');
        toA.onclick = function () { if (self.focusId !== null) self.pickCar(self.focusId, null, true); };
        toB.onclick = function () { if (self.focusId !== null) self.pickCar(self.focusId, { shiftKey: true }, false); };
        lap.onclick = function () { self.setPage('overview'); };
        var toNew = el('button', '', 'Add as new side');
        toNew.title = 'Add this driver as an extra side (C to F)';
        toNew.onclick = function () {
            if (self.focusId === null) return;
            var L = self.nextFreeLetter();
            if (!L) { self.toast('Six sides at most: remove one first'); return; }
            if (self.addSide(L)) self.pickSideCar(L, self.focusId);
        };
        [toA, toB, toNew, lap].forEach(function (b) { btns.appendChild(b); });
        var who = el('div', 'f1x-h4');
        var dials = el('div', 'f1x-dials');
        var cS = el('canvas'), cR = el('canvas');
        dials.appendChild(cS); dials.appendChild(cR);
        var bars = el('div', 'f1x-bars');
        var kv = el('div', 'f1x-kv');
        kv.style.marginTop = '8px';
        [who, btns, dials, bars, kv].forEach(function (n) { left.body.appendChild(n); });
        var dbox = el('div', 'f1x-scroll'); dbox.style.padding = '0 6px'; dos.body.appendChild(dbox);
        this.raceMap(mp);
        this.pollDriver();
        this.refreshers.push(function () {
            var d = self.frame, ca = self.cars(), fc = ca.filter(function (c) { return c.id === self.focusId; })[0];
            if (!d || !fc) {
                who.textContent = 'No car in focus: pick a driver in the timing tower or on the map.';
                kv.innerHTML = ''; bars.innerHTML = ''; return;
            }
            left.head.querySelector('.r').textContent = 'P' + fc.p + ' ' + code3(fc.n) + humanBadgeText(fc);   /* TAPE2 V1 */
            who.innerHTML = 'P' + fc.p + ' · ' + esc(cleanName(fc.n)) + humanBadgeHtml(fc, null) + ' <span style="color:#5f7fc4;font-weight:normal">· ' + esc(cleanName(fc.cn || '')) +
                (fc.cr === 2 && fc.a ? " · friend's car, AI driving" : fc.cr !== 0 && fc.a ? ' · AI' : '') + '</span>';
            var vf = fc.vf || 0, RPMF = WB.RPM_FACTOR;
            var sz = function (c) { var s = setupCanvas(c); return s; };
            var s1 = sz(cS), s2 = sz(cR);
            dial(s1.ctx, s1.w, s1.h, vf & 1 ? fc.s * 3.6 : null, 360, 'km/h', 330);
            var mr = fc.mr > 0 ? fc.mr * RPMF : 19000;
            dial(s2.ctx, s2.w, s2.h, finite(fc.r) ? fc.r * RPMF : null, mr, 'rpm', mr * 0.92);
            var th = vf & 2 ? Math.max(0, Math.min(100, fc.th * 100)) : null, br = vf & 4 ? Math.max(0, Math.min(100, fc.br * 100)) : null;
            var ib = inputBars(fc);   /* 1.2.2: CO-OP remote car: brake on/off, no throttle */
            bars.innerHTML = '<span title="' + esc(ib.thrTitle) + '">THR</span><div class="t"><i style="width:' + (th || 0) + '%;background:#2fb35c"></i></div><span>' + ib.thrText(th) + '</span>' +
                '<span title="' + esc(ib.brkTitle) + '">BRK</span><div class="t"><i style="width:' + (br || 0) + '%;background:#d23a3a"></i></div><span>' + ib.brkText(br) + '</span>';
            kv.innerHTML = self.focusKv(fc, d);
            var doc = self.driverDoc;
            var sig = doc ? JSON.stringify([doc.n, (doc.laps || []).length, (doc.pits || []).length, (doc.pits || []).map(function (q) { return q.done; })]) : 'none' + self.focusId;
            if (dbox._sig === sig) return;
            dbox._sig = sig;
            if (!doc && self.driverErr) { dbox.innerHTML = '<div class="f1x-note">' + esc(self.driverErr) + '</div>'; return; }
            if (!doc || doc.id !== undefined && doc.id !== self.focusId) { dbox.innerHTML = '<div class="f1x-note">Loading the driver dossier…</div>'; return; }
            var lp = doc.laps || [], pt = doc.pits || [];
            var bl = 1e9, b1 = 1e9, b2 = 1e9, b3 = 1e9;
            lp.forEach(function (q) { if (q.t > 0 && q.t < bl) bl = q.t; if (q.s1 > 0 && q.s1 < b1) b1 = q.s1; if (lapS2(q) > 0 && lapS2(q) < b2) b2 = lapS2(q); if (lapS3(q) > 0 && lapS3(q) < b3) b3 = lapS3(q); });
            var bc = function (v, b) { return v > 0 && v <= b + 1e-4 ? ' class="sct"' : ''; };
            var f3 = function (v) { return v > 0 ? v.toFixed(3) : '-'; };
            var h = '<div class="f1x-h4">LAPS (' + lp.length + ')</div><table class="f1x-tw"><thead><tr><th>Lap</th><th>Time</th><th>S1</th><th>S2</th><th>S3</th><th>Pos</th><th>Tyre</th><th>Fuel</th><th>Tyre life FL/FR/RL/RR</th><th>Pit</th></tr></thead><tbody>';
            for (var i = lp.length - 1; i >= 0; i--) {
                var q = lp[i], lf = (q.lf || []).map(function (v) { return v * 100; });
                h += '<tr><td>' + q.l + '</td><td' + bc(q.t, bl) + '>' + lapText(q.t) + '</td><td' + bc(q.s1, b1) + '>' + f3(q.s1) + '</td><td' + bc(lapS2(q), b2) + '>' + f3(lapS2(q)) + '</td><td' + bc(lapS3(q), b3) + '>' + f3(lapS3(q)) + '</td>' +
                    '<td>' + q.p + '</td><td>' + tyreName(q.tc) + '</td><td>' + (q.f > 0 ? q.f.toFixed(1) : '-') + '</td>' +
                    '<td style="' + lifeCol2(lf.length ? Math.min.apply(null, lf) : null) + '">' + lf.map(function (v) { return v.toFixed(1); }).join(' / ') + '</td><td>' + (q.pit ? 'PIT' : '') + '</td></tr>';
            }
            if (!lp.length) h += '<tr><td colspan="10" class="l">No completed laps yet.</td></tr>';
            h += '</tbody></table><div class="f1x-h4">PIT STOPS (' + pt.length + ')</div><table class="f1x-tw"><thead><tr><th>Lap in</th><th>Lap out</th><th>Stationary</th><th class="l">Tyres</th><th>Fuel in</th><th>Fuel out</th><th>Added</th><th class="l">Repairs</th><th>State</th></tr></thead><tbody>';
            pt.forEach(function (e) {
                var rep = []; if (e.rfw) rep.push('front wing'); if (e.rrw) rep.push('rear wing');
                h += '<tr><td>' + e.li + '</td><td>' + (e.done ? e.lo : '-') + '</td><td>' + (e.done ? e.d.toFixed(2) + ' s' : 'in box') + '</td>' +
                    '<td class="l">' + (e.ti === e.to ? tyreName(e.to) + ' (same)' : tyreName(e.ti) + ' → ' + tyreName(e.to)) + '</td>' +
                    '<td>' + e.fi.toFixed(1) + '</td><td>' + (e.done ? e.fo.toFixed(1) : '-') + '</td><td>' + (e.done ? (e.add > 0.05 ? '+' + e.add.toFixed(1) + ' L' : 'none') : '-') + '</td>' +
                    '<td class="l">' + (rep.length ? rep.join(' + ') : e.dmg ? 'none (damaged)' : 'none') + '</td><td>' + (e.done ? 'complete' : 'IN PROGRESS') + '</td></tr>';
            });
            if (!pt.length) h += '<tr><td colspan="9" class="l">No pit stops yet.</td></tr>';
            h += '</tbody></table><div class="f1x-note">Stationary time is pit-entry to pit-exit (the game exposes no box-only clock), so it includes the pit lane run. ' +
                'Repairs are only observable while the car is in the box.</div>';
            dbox.innerHTML = h;
        });
    };

    /* ---- live gauges, legacy look (console only; /viewer keeps its cards) ---- */
    function lcdBox(label, txt, cls) {
        return '<div class="f1x-lcdbox"><div class="f1x-lcdl">' + label + '</div><div class="f1x-lcd ' + (cls || '') + '">' + txt + '</div></div>';
    }
    function segBar(frac, n, colorOf) {
        var out = '';
        for (var k = 0; k < n; k++) {
            var on = frac !== null && (k + 0.5) / n <= frac;
            out += '<i' + (on ? ' class="on ' + colorOf(k / n) + '"' : '') + '></i>';
        }
        return '<div class="f1x-seg" style="grid-template-columns:repeat(' + n + ',1fr)">' + out + '</div>';
    }

    /* 4.4.0-rc.5 task 11: Engine Live for the focus car (see WB.engineSnapshot). */
    Console.prototype.view_engine = function (host) {
        var self = this, p = hpanel('Engine live · focus car');
        p.body.classList.add('pad');
        p.body.style.overflow = 'auto';
        /* 1.1.0: engine wear chart (player only) under the live table */
        var g = this.grid(host, 'minmax(0,1fr)', 'minmax(0,1.3fr) minmax(0,1fr)');
        g.appendChild(p);
        var wp = hpanel('Engine wear rate · your car');
        g.appendChild(wp);
        this.pchEngineChart(wp);
        this.refreshers.push(function () {
            var j = self.frame;
            var fc = j && self.focusId !== null && self.focusId !== undefined
                ? j.c.filter(function (c) { return c.id === self.focusId; })[0] : null;
            var rows = WB.engineSnapshot(fc, j);
            p.body.innerHTML = '<table class="f1x-eng"><tbody>' + rows.map(function (r) {
                return '<tr><th>' + esc(r.k) + '</th><td>' + esc(r.v) + '</td><td class="f1x-tier">' + esc(r.tier) + '</td></tr>';
            }).join('') + '</tbody></table>';
        });
    };

    /* ---- 1.1.0 player chassis (vanilla game data, your car only) ---------
     * Source: /telemetry "pch" = the game's full-physics object 0x7C7428. AI
     * cars do not simulate these channels; for them only per-axle brake
     * temperature (abt) and wheel spin (aws) exist. Live-proven 2026-10-13
     * (COMMUNITY_CT_INTEGRATION_2026-10-13.md). */
    var PCH_HIST_MAX = 3000, PCH_WN = ['FL', 'FR', 'RL', 'RR'];
    function pchOk(v) { return finite(v) && v > -900 ? v : null; }
    function pchPush(self, j) {
        var p = j && j.pch;
        if (!p || !p.v || !finite(j.st)) return;
        var h = self.pchHist || (self.pchHist = { t: [], ewr: [], rpm: [], oilp: [], rhf: [], rhr: [] });
        var last = h.t.length ? h.t[h.t.length - 1] : null;
        if (last !== null && j.st < last) { for (var k in h) h[k].length = 0; last = null; }
        if (last !== null && j.st === last) return;
        function avg(a, b) { return pchOk(a) !== null && pchOk(b) !== null ? (a + b) / 2 : null; }
        h.t.push(j.st); h.ewr.push(pchOk(p.ewr)); h.rpm.push(pchOk(p.rpm)); h.oilp.push(pchOk(p.oilp));
        h.rhf.push(p.rh ? avg(p.rh[0], p.rh[1]) : null); h.rhr.push(p.rh ? avg(p.rh[2], p.rh[3]) : null);
        if (h.t.length > PCH_HIST_MAX) for (var k2 in h) h[k2].shift();
    }
    function pv(v, dp, unit) { return pchOk(v) !== null ? v.toFixed(dp) + (unit || '') : '-'; }
    function pchTag(word, bg, fg) { return '<span class="f1x-tag" style="background:' + bg + (fg ? ';color:' + fg : '') + '">' + word + '</span>'; }
    function pchRoll(r) {
        if (pchOk(r) === null) return '';
        if (r < 0.7) return pchTag('LOCK', '#ff4040', '#fff');
        if (r > 1.1) return pchTag('SPIN', '#ffd84a');
        return '';
    }
    function pchBar(v, col) {
        var x = pchOk(v) === null ? 0 : Math.max(0, Math.min(1, Math.abs(v)));
        return '<span class="f1x-wxbar"><i style="width:' + (x * 100).toFixed(1) + '%;background:' + col + '"></i></span>';
    }
    function pchMax(a, floor) {
        var m = floor;
        for (var i = 0; i < a.length; i++) if (a[i] !== null && a[i] > m) m = a[i];
        return m;
    }
    Console.prototype.pchEngineChart = function (p) {
        var self = this;
        p.body.style.display = 'flex'; p.body.style.flexDirection = 'column';
        function chart() {
            var b = el('div', 'f1x-wxbox'); b.style.flex = '1 1 0'; b.style.minHeight = '60px';
            var cv = el('canvas'); b.appendChild(cv); p.body.appendChild(b);
            return cv;
        }
        var c1 = chart(), c2 = chart();
        this.canvases.push({ draw: function () {
            var h = self.pchHist || { t: [], ewr: [], rpm: [], oilp: [] };
            var top = Math.ceil(pchMax(h.ewr, 0.5) * 10) / 10;
            wxChart(c1, h, [{ v: h.ewr, c: '#ff5a5a' }], 0, top, [{ t: 'engine life used per second (higher = faster wear)', c: '#ff5a5a' }]);
            wxChart(c2, h, [{ v: h.rpm.map(function (x) { return x === null ? null : x / 1000; }), c: '#7fd4ff' },
                            { v: h.oilp.map(function (x) { return x === null ? null : x / 50; }), c: '#3ce07a' }], 0, 20,
                [{ t: 'rpm x1000', c: '#7fd4ff' }, { t: 'oil pressure / 50', c: '#3ce07a' }]);
        } });
        this.refreshers.push(function () {
            var d = self.frame, ok = d && d.pch && d.pch.v;
            p.head.querySelector('.r').textContent = !ok ? 'no driven session' :
                'now ' + pv(d.pch.ewr, 3, ' s/s') + ' · life left ' + pv(d.pel, 0, ' s');
        });
    };
    Console.prototype.view_chassis = function (host) {
        var self = this;
        var g = this.grid(host, 'minmax(0,1.35fr) minmax(0,1fr)', 'minmax(0,1.15fr) minmax(0,1fr)');
        function at(pn, col, row) { pn.style.gridColumn = col; pn.style.gridRow = row; g.appendChild(pn); return pn; }
        var wp = at(hpanel('Wheels · your car'), '1', '1 / 3');
        var ap = at(hpanel('Driver aids · your input vs what the car used'), '2', '1');
        var cp = at(hpanel('Ride height · aero · damage'), '2', '2');
        [wp, ap].forEach(function (pn) { pn.body.classList.add('pad'); pn.body.style.overflow = 'auto'; });
        var wbox = el('div'), abox = el('div');
        wp.body.appendChild(wbox); ap.body.appendChild(abox);
        cp.body.style.display = 'flex'; cp.body.style.flexDirection = 'column';
        var kv = el('div', 'f1x-kv'); kv.style.flex = '0 0 auto'; kv.style.padding = '4px 6px'; cp.body.appendChild(kv);
        var bx = el('div', 'f1x-wxbox'); bx.style.flex = '1 1 0'; bx.style.minHeight = '60px';
        var cv = el('canvas'); bx.appendChild(cv); cp.body.appendChild(bx);
        this.canvases.push({ draw: function () {
            var h = self.pchHist || { t: [], rhf: [], rhr: [] };
            wxChart(cv, h, [{ v: h.rhf, c: '#7fd4ff' }, { v: h.rhr, c: '#ff9c40' }], 0, Math.max(80, Math.ceil(pchMax(h.rhr, 0) / 10) * 10),
                [{ t: 'front ride height mm', c: '#7fd4ff' }, { t: 'rear mm', c: '#ff9c40' }]);
        } });
        function aiBlock(fc) {
            if (!fc || !fc.abt) return '';
            return '<div class="w" style="color:#5f7fc4;margin-top:6px">AI cars only simulate brake temperature per axle and wheel speed.</div>' +
                '<table class="f1x-eng"><tbody><tr><th>' + esc(fc.n || '') + ' brakes</th><td>front ' + pv(fc.abt[0], 0, '°') + ' · rear ' + pv(fc.abt[1], 0, '°') + '</td></tr>' +
                '<tr><th>Wheel speed</th><td>' + (fc.aws || []).map(function (v, i) { return PCH_WN[i] + ' ' + pv(v, 0); }).join(' · ') + ' rad/s</td></tr></tbody></table>' +
                '<div class="w" style="color:#5f7fc4">Clutch: not available for AI (player only)</div>' +
                '<div class="w" style="color:#5f7fc4">AI tyre temperature: not available from the game (player only)</div>';
        }
        this.refreshers.push(function () {
            var d = self.frame, p = d && d.pch;
            var fc = d && self.focusId !== null && self.focusId !== undefined ? d.c.filter(function (c) { return c.id === self.focusId; })[0] : null;
            if (!d || !p || !p.v) {
                wbox.innerHTML = '<div class="w" style="color:#5f7fc4">No driven session. These channels exist for your car only.</div>' + aiBlock(fc && d && fc.id !== d.ps ? fc : null);
                abox.innerHTML = ''; kv.innerHTML = ''; wbox._h = abox._h = kv._h = null; return;
            }
            function row(name, f) {
                return '<tr><th>' + name + '</th>' + [0, 1, 2, 3].map(function (w) { return '<td>' + f(w) + '</td>'; }).join('') + '</tr>';
            }
            var tt = d.ttmp || [], tr = d.trd || [], bt = d.btmp || [];
            var html = '<table class="f1x-eng"><thead><tr><th></th>' + PCH_WN.map(function (n) { return '<th>' + n + '</th>'; }).join('') + '</tr></thead><tbody>' +
                row('Tyre temp L·C·R °C', function (w) { var z = tt[w] || []; return z.map(function (v) { return pv(v, 0); }).join(' · '); }) +
                row('Ideal temp °C', function (w) { return pv(p.opt[w], 0); }) +
                row('Pressure kPa (set)', function (w) { return pv(p.pr[w], 1) + ' (' + pv(p.prs[w], 0) + ')'; }) +
                row('Grip', function (w) { return pchOk(p.gr[w]) === null ? '-' : (p.gr[w] * 100).toFixed(1) + '%'; }) +
                row('Tread left', function (w) { return finite(tr[w]) ? (tr[w] * 100).toFixed(1) + '%' : '-'; }) +
                row('Camber °', function (w) { return pv(p.cam[w], 2); }) +
                row('Brake temp °C', function (w) { return pv(bt[w], 0); }) +
                row('Brake disc mm', function (w) { return pv(p.disc[w], 2) + (pchOk(p.disc[w]) !== null && pchOk(p.discf[w]) !== null ? ' <span style="color:#5f7fc4">(' + (p.disc[w] - p.discf[w]).toFixed(2) + ' to fail)</span>' : ''); }) +
                row('Suspension mm', function (w) { return pv(p.sus[w], 1); }) +
                row('Tyre squash mm', function (w) { return pv(p.tdf[w], 1); }) +
                row('Ride height mm', function (w) { return pv(p.rh[w], 1); }) +
                row('Wheel speed', function (w) { return pv(p.ws[w], 0, ' rad/s') + ' ' + pchRoll(p.roll[w]); }) +
                row('Damage', function (w) { return ((p.det[w] ? pchTag('DETACHED', '#ff4040', '#fff') : '') + (p.bent[w] ? pchTag('BENT', '#ff9c40') + pv(p.bend[w], 3, ' rad') : '')) || '-'; }) +
                '</tbody></table>' +
                '<div class="w" style="color:#5f7fc4;margin-top:4px">L·C·R = left edge, centre, right edge of the tyre as seen from behind. LOCK = wheel turning under 70% of car speed; SPIN = over 110%.</div>' +
                (fc && fc.id !== d.ps ? aiBlock(fc) : '');
            if (wbox._h !== html) { wbox._h = html; wbox.innerHTML = html; }
            var names = ['Throttle', 'Brake', 'Steering', 'Clutch'];
            var ah = '<table class="f1x-eng"><tbody>' + names.map(function (n, k) {
                var r = p.inr[k], u = p.inu[k], note = '';
                if (pchOk(r) !== null && pchOk(u) !== null) {
                    if (k === 0 && u < r - 0.05) note = pchTag('TC CUT', '#ffd84a');
                    else if (k === 0 && u > r + 0.05) note = pchTag('BLIP', '#7fd4ff');
                    else if (k !== 0 && Math.abs(u - r) > 0.05) note = pchTag('HELP', '#7fd4ff');
                }
                return '<tr><th>' + n + '</th><td>you ' + pchBar(r, '#9aa3ad') + pv(r, 2) + '</td><td>car ' + pchBar(u, '#3ce07a') + pv(u, 2) + '</td><td>' + note + '</td></tr>';
            }).join('') + '</tbody></table>' +
                '<div class="w" style="color:#5f7fc4;margin-top:4px">ABS does not change the brake value: it works on each wheel. Watch the LOCK tags.</div>';
            if (abox._h !== ah) { abox._h = ah; abox.innerHTML = ah; }
            var slip = pchOk(p.rpm) !== null && pchOk(p.crpm) !== null ? Math.abs(p.rpm - p.crpm) : null;
            var kh = '<div class="w">Brake bias value ' + pv(p.bias, 3) + '</div>' +
                '<div class="w">Body drag ' + pv(p.drag, 3) + ' · lift ' + pv(p.lift, 3) + ' · drag now ' + pv(p.dragu, 3) + '</div>' +
                '<div class="w">Floor ' + (p.bot ? pchTag('SCRAPING', '#ff9c40') : 'clear') + ' · fuel ' + pv(p.fuel, 2, ' L') + '</div>' +
                '<div class="w">Engine ' + pv(p.rpm, 0) + ' rpm · clutch ' + pv(p.crpm, 0) + ' rpm' + (slip !== null && slip > 200 ? ' ' + pchTag('CLUTCH SLIP', '#ffd84a') : '') + ' · oil pressure ' + pv(p.oilp, 0) + '</div>' +
                '<div class="w">Compound index ' + (finite(p.cmp) ? p.cmp : '-') + ' (ideal ' + pv(p.opt[0], 0, ' °C') + ')</div>';
            if (kv._h !== kh) { kv._h = kh; kv.innerHTML = kh; }
            cp.head.querySelector('.r').textContent = 'drag and lift change with damage';
        });
    };

    /* ---- rc.15 weather: the session Weather page (Conditions group) ----------
     * Race-engineer view, same look as the Race pages. Plain words only.
     *   CONDITIONS  now: sky, air/track temperature, racing-line / off-line wetness
     *   OUTLOOK     the coming sky changes, minutes from now
     *   TRACK MAP   track wetness on and off the racing line, cars, focus car
     *   FORECAST    sky and air temperature over the session, now marker
     *   TRENDS      temperatures and wetness so far this session
     * Sky words and the PROJECTED rule are the game's own (see wxSky /
     * wxProjected). Every canvas sits in its own position:relative box. */
    var WX_SKYCOL = { Dry: '#3ce07a', Overcast: '#9aa3ad', Raining: '#2f8bff', Monsoon: '#7a5cff' };
    function wxSkyTag(word) {
        var c = WX_SKYCOL[word] || '#55607a';
        return '<span class="f1x-tag" style="background:' + c + (word === 'Raining' || word === 'Monsoon' ? ';color:#fff' : '') + '">' + esc(String(word).toUpperCase()) + '</span>';
    }
    function wxChart(cv, wh, lines, y0, y1, caption) {
        var s = setupCanvas(cv), ctx = s.ctx;
        var pl = 30, pr = 8, pt = 14, pb = 12;
        var pw = Math.max(10, s.w - pl - pr), ph = Math.max(8, s.h - pt - pb);
        var t0 = wh.t.length ? wh.t[0] : 0, t1 = wh.t.length ? wh.t[wh.t.length - 1] : 0;
        if (!(t1 > t0)) t1 = t0 + 1;
        function gx(t) { return pl + (t - t0) / (t1 - t0) * pw; }
        function gy(v) { var c = Math.max(y0, Math.min(y1, v)); return pt + (1 - (c - y0) / (y1 - y0)) * ph; }
        ctx.font = '9px ' + PAL.sans; ctx.textBaseline = 'alphabetic';
        var cx = pl;
        (caption || []).forEach(function (c) {
            ctx.fillStyle = c.c; ctx.textAlign = 'left'; ctx.fillText(c.t, cx, 10);
            cx += ctx.measureText(c.t).width + 10;
        });
        ctx.fillStyle = PAL.axis; ctx.textAlign = 'right';
        ctx.fillText(String(y1), pl - 4, pt + 7);
        ctx.fillText(String(y0), pl - 4, pt + ph);
        ctx.strokeStyle = PAL.grid; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(pl, pt); ctx.lineTo(pl, pt + ph); ctx.lineTo(pl + pw, pt + ph); ctx.stroke();
        ctx.fillStyle = PAL.axis;
        if (wh.t.length) {
            ctx.textAlign = 'left'; ctx.fillText(wxClock(t0), pl, s.h - 2);
            ctx.textAlign = 'right'; ctx.fillText(wxClock(wh.t[wh.t.length - 1]), pl + pw, s.h - 2);
        }
        lines.forEach(function (ln) {
            ctx.strokeStyle = ln.c; ctx.lineWidth = 1.4;
            ctx.beginPath();
            var open = false, n = 0, last = -1;
            for (var i = 0; i < wh.t.length; i++) {
                var v = ln.v[i];
                if (v === null || v === undefined || !isFinite(v)) { open = false; continue; }
                var px = gx(wh.t[i]), py = gy(v);
                if (!open) { ctx.moveTo(px, py); open = true; } else ctx.lineTo(px, py);
                n++; last = i;
            }
            ctx.stroke();
            if (n === 1) { ctx.fillStyle = ln.c; ctx.beginPath(); ctx.arc(gx(wh.t[last]), gy(ln.v[last]), 2.2, 0, Math.PI * 2); ctx.fill(); }
        });
    }
    function wxBar(pct, col) {
        var v = finite(pct) ? Math.max(0, Math.min(100, pct)) : 0;
        return '<span class="f1x-wxbar"><i style="width:' + v.toFixed(1) + '%;background:' + col + '"></i></span>';
    }
    function wxIn(min) { return min < 1 ? 'now' : 'in ' + Math.round(min) + ' min'; }

    /* ---- Batch 4 (API 9): Recordings manager -----------------------------------
     * GET /recordings lists the recordings folder on the game PC (sizes, states, the manager notice; relative names only).
     * Load selects a recording and changes no file. Archive and Delete are POST jobs with the guards of the route:
     * this PC, the X-Telemetry-Action header naming the action, and a JSON body. Delete moves files to the Windows Recycle Bin
     * only, after an in-page dialog (batch 6) that names the item and its size. Every server string reaches the page through esc()
     * (or the toast, which escapes its text); nothing is passed to innerHTML unescaped. */
    Console.prototype.pollSessions = function () {
        var self = this;
        Promise.all([
            this.fetchJson('/recordings'),
            this.fetchJson('/recordings/jobs').catch(function () { return null; })
        ]).then(function (both) {
            var d = both[0], j = both[1];
            self.sessionsErr = '';   /* 1.3.0: a good answer clears the error text */
            self.gone = self.gone || {};
            self.pendingJobs = self.pendingJobs || {};
            if (j && Array.isArray(j.jobs)) self.jobsDoc = j;
            /* A delete/archive job that finished: ok -> its items are gone (hide them now); error -> just refresh. */
            var stillOpen = false;
            Object.keys(self.pendingJobs).forEach(function (id) {
                var hit = (self.jobsDoc && self.jobsDoc.jobs || []).filter(function (x) { return x.job === id; })[0];
                if (hit && hit.state !== 'done') { stillOpen = true; return; }
                if (!hit || (hit.state === 'done' && hit.result === 'ok')) self.pendingJobs[id].keys.forEach(function (k) { self.gone[k] = true; });
                delete self.pendingJobs[id];
            });
            if (d && Array.isArray(d.recordings)) {
                /* Owner 2026-10-13: newest recording first (same as the replay list). Key = date+time in the id. */
                var recKey = function (r) { var m = /(\d{4}-\d{2}-\d{2})[ _](\d{2}-\d{2})/.exec(r.id || ''); return m ? m[1] + m[2] : ''; };
                /* 1.1.2 owner order inside a weekend: Practice 1-4, Qualifying, Warm-up, Race. */
                var rank = function (id) {
                    var sp = String(id).split(' - '), l = (sp.length >= 4 ? sp[sp.length - 2] : (sp.length === 3 ? sp[1] : '')).toLowerCase().replace(/[\s-]/g, '');
                    var m = /^practice(\d)$/.exec(l);
                    if (m) return +m[1];
                    return l === 'qualifying' ? 5 : l === 'warmup' ? 6 : l === 'race' ? 7 : 0;
                };
                var gone = self.gone;
                d.recordings = d.recordings.filter(function (r) { return !gone[r.id]; });
                d.recordings.forEach(function (r) {
                    r.sessions = (r.sessions || []).filter(function (s) { return !gone[r.id + '/' + s.id]; })
                        .sort(function (a, b) { var x = rank(a.id), y = rank(b.id); return x !== y ? x - y : (a.id < b.id ? -1 : a.id > b.id ? 1 : 0); });
                });
                d.recordings.sort(function (a, b) { var x = recKey(a), y = recKey(b); return x < y ? 1 : x > y ? -1 : 0; });
                self.sessionsDoc = d;
            }
            self.invalidate();
            /* While a job we started is still queued or running, look again soon (the list is refreshed when it ends). */
            if (stillOpen && !self._jobTimer) {
                self._jobTimer = setTimeout(function () { self._jobTimer = null; self.pollSessions(); }, 1500);
            }
        }).catch(function (e) {
            /* 1.3.0 (TAPE 3.3): the server message, not a stuck "Loading recordings" text */
            self.sessionsErr = e && e.message ? String(e.message) : 'The recordings did not load.';
            self.invalidate();
        });
    };
    /* POST /recordings/<action>. Resolves to { ok, code, j } and never rejects. */
    Console.prototype.recordingPost = function (action, body) {
        return fetch('/recordings/' + action, { method: 'POST', cache: 'no-store', body: JSON.stringify(body),
            headers: { 'Content-Type': 'application/json', 'X-Telemetry-Action': action } })
            .then(function (r) {
                return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, code: r.status, j: j || {} }; });
            })
            .catch(function () { return { ok: false, code: 0, j: { error: 'No answer from the game PC. Nothing was changed.' } }; });
    };
    /* Batch 6 (1.1.0): in-page dialog for the Recordings page. Replaces window.confirm (no browser popup).
     * opts: { title, html (markup; every value escaped with esc()), radios: [{ value, label, checked }] (optional),
     *         buttons: [{ label, value }] (value "cancel" = Cancel), safe: value of the default button (Enter and initial focus) }.
     * Resolves to { value, choice } (choice = the checked radio value, or null), or to null for Cancel, Esc and a click outside.
     * Enter always takes the safe button; Esc always cancels. Page shortcuts are blocked while the dialog is open. */
    Viewer.prototype.dialog = function (opts) {
        return new Promise(function (resolve) {
            var m = el('div', 'tv-modal'), prev = document.activeElement, done = false;
            var h = '<div><h3>' + esc(opts.title) + '</h3>' + opts.html;
            if (opts.radios) {
                h += '<div role="radiogroup">';
                opts.radios.forEach(function (r) {
                    h += '<label class="tv-dlg-opt"><input type="radio" name="tvdlg" value="' + esc(r.value) + '"' +
                        (r.checked ? ' checked' : '') + '> ' + esc(r.label) + '</label>';
                });
                h += '</div>';
            }
            h += '<div class="tv-dlg-btns">' + opts.buttons.map(function (b) {
                return '<button class="tv-btn' + (b.value === opts.safe ? ' on' : '') + '" data-v="' + esc(b.value) + '">' + esc(b.label) + '</button>';
            }).join('') + '</div></div>';
            m.innerHTML = h;
            function finish(v) {
                if (done) return;
                done = true;
                window.removeEventListener('keydown', onKey, true);
                var rad = m.querySelector('input[type="radio"]:checked');
                m.remove();
                if (prev && prev.focus) prev.focus();
                resolve(v === null || v === 'cancel' ? null : { value: v, choice: rad ? rad.value : null });
            }
            function onKey(e) {
                if (done) return;
                e.stopPropagation();
                if (e.key === 'Escape') { e.preventDefault(); finish(null); }
                else if (e.key === 'Enter') { e.preventDefault(); finish(opts.safe); }
                else if (e.key === 'Tab') {
                    var f = m.querySelectorAll('input, button'), first = f[0], last = f[f.length - 1];
                    if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
                    else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
                }
            }
            window.addEventListener('keydown', onKey, true);
            m.addEventListener('click', function (e) {
                var btn = e.target.closest ? e.target.closest('button[data-v]') : null;
                if (btn) finish(btn.getAttribute('data-v'));
                else if (e.target === m) finish(null);
            });
            document.body.appendChild(m);
            var d = m.querySelector('button[data-v="' + opts.safe + '"]');
            if (d) d.focus();
        });
    };
    /* Batch 6: the page keeps the last refusal or failure in view until the next action or Dismiss. */
    Console.prototype.setNotice = function (text, bad) {
        var self = this, n = text ? { text: String(text), bad: !!bad } : null, ms = n ? noticeHideMs(n.bad) : 0;
        this.sessNotice = n;
        this._sessSig = null;
        this.invalidate();
        /* 1.2.1: a success line hides itself after 6 s; an error stays until Dismiss. */
        if (n && ms) setTimeout(function () { if (self.sessNotice === n) { self.sessNotice = null; self._sessSig = null; self.invalidate(); } }, ms);
    };

    /* 1.1.2: rows the page deleted are hidden once their job reports done, even if the server list is still stale.
     * pendingJobs: job id -> { keys: [...], rec: name }. gone: keys already moved to the Windows Recycle Bin (rec, rec/session, rec/file). */
    Console.prototype.recordingDone = function (res, msg, keys) {
        var self = this;
        this.gone = this.gone || {};
        this.pendingJobs = this.pendingJobs || {};
        if (!res.ok && res.code === 404) {
            /* The item is already gone on the game PC: refresh the list instead of asking the user to reload. */
            this.sessNotice = null;
            (keys || []).forEach(function (k) { self.gone[k] = true; });
            this.setNotice('That item was already moved or is gone. The list was refreshed.', false);
            this.sessionSel = {};
            this._sessSig = null;
            this.pollSessions();
            return;
        }
        if (!res.ok) {
            var why = res.j && res.j.error ? res.j.error : 'Refused. Nothing was changed.';
            var txt = (res.code ? 'Refused (HTTP ' + res.code + '): ' : '') + why;
            this.setNotice(txt, true);
            this.toast(txt);
            return;
        }
        this.setNotice(msg, false);
        this.toast(msg);
        this.sessionSel = {};
        this._sessSig = null;
        if (res.j && res.j.job && keys) this.pendingJobs[res.j.job] = { keys: keys, rec: keys[0] ? keys[0].split('/')[0] : '' };
        this.pollSessions();
    };
    /* Load: selects the recording on the game PC (no file changes) and switches the page to PAST mode. stem = session id. */
    Console.prototype.loadRecording = function (rec, stem) {
        var self = this;
        this.recordingPost('load', { recording: rec }).then(function (res) {
            var msg = '';
            if (res.ok && rec) { self.enterPast(rec, stem); msg = 'Past session opened: ' + pastShortName(stem || rec) + '. Loading does not change any file.'; }
            else if (res.ok) { self.source = 'live'; self.past = null; self.applyMode(); msg = 'Back to live. No file was changed.'; }
            self.recordingDone(res, msg);
        });
    };

    /* Working bytes of one session ("recording/session" key) as listed. */
    Console.prototype.recordingBytes = function (key) {
        var d = this.sessionsDoc, out = 0;
        if (!d) return 0;
        (d.recordings || []).forEach(function (r) {
            (r.sessions || []).forEach(function (s) { if (r.id + '/' + s.id === key) out += s.bytes; });
        });
        return out;
    };
    /* One dialog: the size, then the choice for the working copy. Enter = Archive with "Keep" (the default choice). */
    Console.prototype.archiveSelected = function (keys) {
        var self = this, size = 0, names = {}, recName = [];
        if (!keys.length) return;
        keys.forEach(function (k) { size += self.recordingBytes(k); names[k.split('/')[0]] = true; });
        Object.keys(names).forEach(function (n) { recName.push(n); });
        this.dialog({
            title: 'Archive ' + keys.length + ' session(s)',
            html: '<p>Archive ' + keys.length + ' session(s) of ' + esc(recName.join(', ')) + ' (working size ' + esc(bytesText(size)) +
                ') into the compact permanent format? Archived files are smaller and kept until you delete them.</p>',
            radios: [
                { value: 'keep', label: 'Keep the working copy (default)', checked: true },
                { value: 'recycle', label: 'Move the working copy to the Windows Recycle Bin' }
            ],
            buttons: [{ label: 'Archive', value: 'archive' }, { label: 'Cancel', value: 'cancel' }],
            safe: 'archive'
        }).then(function (r) {
            if (!r) return;
            self.setNotice(null);
            self.recordingPost('archive', { sessions: keys, recycle_working: r.choice === 'recycle' }).then(function (res) {
                /* No rows are hidden after an archive: the archived copy stays listed (only Delete removes rows). */
                self.recordingDone(res, res.ok ? 'Archive queued (' + res.j.job + '). The list shows the result when it is done.' : '', null);
            });
        });
    };
    /* target: recording | session | file. Cancel is the default (Enter and Esc). */
    Console.prototype.deleteItem = function (target, rec, id, bytes) {
        var self = this, body = { target: target, id: id, confirm_bytes: bytes };
        var what = target === 'recording' ? 'the recording ' + recTitle(id) :
            target === 'session' ? 'the ' + (sessionLabel(id) || 'session') + ' of ' + recTitle(rec) : 'a damaged or interrupted file';
        this.dialog({
            title: 'Move to the Windows Recycle Bin',
            html: '<p>Move ' + esc(what) + ' (' + esc(bytesText(bytes)) + ') to the Windows Recycle Bin? You can restore it from there.</p>',
            buttons: [{ label: 'Move to Windows Recycle Bin', value: 'delete' }, { label: 'Cancel', value: 'cancel' }],
            safe: 'cancel'
        }).then(function (r) {
            if (!r) return;
            if (target !== 'recording') body.recording = rec;
            var key = target === 'recording' ? id : rec + '/' + id;
            self.setNotice(null);
            self.recordingPost('delete', body).then(function (res) {
                self.recordingDone(res, res.ok ? 'Delete queued (' + res.j.job + '). Files go to the Windows Recycle Bin; the list updates when it is done.' : '', [key]);
                if (res.ok) {
                    self.jobNames = self.jobNames || {};
                    self.jobNames[res.j.job] = target === 'recording' ? id : rec;   /* recording name for the "Last job" line */
                }
            });
        });
    };

    /* The job line for the page: the newest unfinished job, else the newest job. bad = that job finished with an error. */
    Console.prototype.lastJob = function () {
        var jobs = this.jobsDoc && Array.isArray(this.jobsDoc.jobs) ? this.jobsDoc.jobs.slice() : [];
        if (!jobs.length) return { text: '', bad: false };
        var num = function (x) { return +(String(x.job || '').split('-')[1]) || 0; };
        var open = jobs.filter(function (x) { return x.state !== 'done'; });
        var j = (open.length ? open : jobs).reduce(function (a, b) { return num(b) > num(a) ? b : a; });
        /* 1.1.2: short summary, no file list. Names come from this page's own delete request (jobNames). */
        var nm = (this.jobNames || {})[j.job], of = nm ? " of '" + nm + "'" : '', n = j.moved ? j.moved.length : 0;
        var text;
        if (j.state === 'done' && j.result === 'ok' && j.kind === 'delete') text = 'Last job: moved ' + n + ' file(s)' + of + ' to the Windows Recycle Bin.';
        else if (j.state === 'done' && j.result === 'error') text = 'Last job ' + j.job + ' failed: ' + (j.message || 'nothing else was deleted') + '.';
        else text = 'Last job ' + j.job + ': ' + j.state + '.';
        if (j.not_moved && j.not_moved.length) text += ' ' + j.not_moved.length + ' file(s) were not moved.';
        return { text: text, bad: j.state === 'done' && j.result === 'error' };
    };
    Console.prototype.reviewFailed = function () {
        this.toast('This saved session could not be opened (deleted or not recorded). Back to live in 3 s.');
        setTimeout(function () { location.href = '/'; }, 3000);
    };
    /* ---- 1.2.1 Recordings page: one card per recording, sessions as rows, search and mode filter, multi-select bar.
     * Data: GET /recordings (poll) and GET /recordings/jobs. Actions: POST load / archive / delete only (guards in web_telemetry.c). */
    Console.prototype.view_sessions = function (host) {
        var self = this;
        this.refreshers = [];
        var g = this.grid(host, 'minmax(0,1fr)', 'minmax(0,1fr)');
        var pn = hpanel('Recordings');
        g.appendChild(pn);
        var wrap = el('div', 'f1x-sess');
        wrap.style.cssText = 'position:absolute;inset:17px 0 0 0;display:flex;flex-direction:column;overflow:hidden;padding:6px 8px';
        pn.body.appendChild(wrap);
        /* The toolbar is built once, outside the list, so typing keeps its focus while the list refreshes. */
        var bar = el('div', 'f1x-sess-bar');
        var q = el('input', 'f1x-sess-q');
        q.type = 'search'; q.placeholder = 'Search track, year or session'; q.setAttribute('aria-label', 'Search recordings');
        q.value = this.sessFilter.q;
        var mode = el('select', 'tv-sel'); mode.setAttribute('aria-label', 'Filter by mode');
        [['all', 'All modes'], ['single', 'Single player'], ['coop', 'CO-OP'], ['mp', 'Multiplayer']].forEach(function (o) {
            var op = el('option'); op.value = o[0]; op.textContent = o[1]; mode.appendChild(op);
        });
        mode.value = this.sessFilter.mode;
        q.addEventListener('input', function () { self.sessFilter.q = q.value; self._sessSig = null; self.invalidate(); });
        mode.addEventListener('change', function () { self.sessFilter.mode = mode.value; self._sessSig = null; self.invalidate(); });
        bar.appendChild(q); bar.appendChild(mode);
        var box = el('div', 'f1x-sess-list');
        box.style.cssText = 'flex:1 1 auto;min-height:0;overflow:auto';
        wrap.appendChild(bar); wrap.appendChild(box);
        this.sessionSel = this.sessionSel || {};
        this.pollSessions();
        box.addEventListener('click', function (e) {
            var t = e.target && e.target.closest ? e.target.closest('[data-act]') : null;
            if (!t) return;
            var act = t.getAttribute('data-act'), rec = t.getAttribute('data-rec') || '', id = t.getAttribute('data-id') || '';
            var bytes = +(t.getAttribute('data-bytes') || 0);
            var rid = t.getAttribute('data-rid') || '';
            if (act === 'load' && rid) self.openReview(rid, t.getAttribute('data-name') || '');
            else if (act === 'load' && rec) self.cannotPlay(rec, id);
            else if (act === 'del-rec' && rec) self.deleteItem('recording', '', rec, bytes);
            else if (act === 'del-sess' && rec && id) self.deleteItem('session', rec, id, bytes);
            else if (act === 'del-file' && rec && id) self.deleteItem('file', rec, id, bytes);
            else if (act === 'dismiss-notice') self.setNotice(null);
            else if (act === 'refresh') self.pollSessions();
            else if (act === 'sel-clear') { self.sessionSel = {}; self._sessSig = null; self.invalidate(); }
            else if (act === 'sel-archive') self.archiveSelected(self.selectedKeys().filter(function (k) { var s = self.keyState(k); return s === 'closed' || s === 'recovered'; }));
            else if (act === 'sel-delete') self.deleteSelected(self.selectedKeys());
            else if (act === 'open-past') self.goPast();
            else if (act === 'change') self.goChange();
            else if (act === 'live') self.goLive();
        });
        box.addEventListener('change', function (e) {
            var t = e.target;
            if (t && t.getAttribute && t.getAttribute('data-sel')) { self.sessionSel[t.getAttribute('data-sel')] = t.checked; self._sessSig = null; self.invalidate(); }
        });
        this.refreshers.push(function () {
            var d = self.sessionsDoc, lj = self.lastJob();
            var sig = JSON.stringify([d, self.sessionSel, lj.text, lj.bad, self.review, self.past, self.sessNotice, self.sessFilter, self.source]);
            if (sig === self._sessSig) return;
            self._sessSig = sig;
            box.innerHTML = self.recordingsHtml();
        });
    };

    /* ---- 1.2.2 Replay page: a finished v2 recording plays like a race broadcast (REPLAY_SPEC_2026-10-14 section 3).
     * Pure helpers first (unit-tested in viewer_test.js), then the page. Data: GET /replay/info once, then GET /replay windows
     * of 30 s (at most 2 in flight). Positions are interpolated locally, one frame at a time. ---- */
    var RP_SPAN = 30, RP_LEAD = 10, RP_GAP = 2, RP_FLIGHT = 2;
    var RP_SPEEDS = [1, 2, 4, 8, 16];
    var RP_PALETTE = ['#e10600', '#3da5ff', '#3ce07a', '#ffd84a', '#b38cff', '#ff7ab6', '#ff9c40', '#c9d4ea', '#7fd4ff', '#8fe3a0'];
    var RP_MARK_COL = { pit: '#ffd84a', contact: '#ff4a3d', limits: '#9aa6bf', incident: '#f2f2f2' };
    var RP_DASH = '—';

    function rpFin(v) { return typeof v === 'number' && isFinite(v); }
    /* Window k holds sim time [tFirst + 30k, tFirst + 30(k+1)], the last one clipped to tLast. */
    function rpWindowIndex(t, tFirst) { return rpFin(t) && t > tFirst ? Math.floor((t - tFirst) / RP_SPAN) : 0; }
    function rpWindowRange(k, tFirst, tLast) {
        return { t0: Math.min(tLast, tFirst + k * RP_SPAN), t1: Math.min(tLast, tFirst + (k + 1) * RP_SPAN) };
    }
    /* Windows to request now, in priority order: the window holding t, then the next one once t is within RP_LEAD seconds
     * of the end of its window. has(k) is true for a window that is loaded, loading, failed, or waiting for a retry.
     * At most RP_FLIGHT requests in flight in total. */
    function rpPlanFetch(t, tFirst, tLast, has, inflight) {
        var k = rpWindowIndex(t, tFirst), r = rpWindowRange(k, tFirst, tLast), out = [];
        if (!has(k)) out.push(k);
        if (t >= r.t1 - RP_LEAD && r.t1 < tLast && !has(k + 1)) out.push(k + 1);
        return out.slice(0, Math.max(0, RP_FLIGHT - inflight));
    }
    /* Rows of one car from the loaded windows around window k (the previous one is kept for the look-back at a boundary). */
    function rpRowsFor(win, slot, k) {
        var out = [], a = win[k - 1], b = win[k];
        if (a && a.state === 'ready' && a.cars[slot]) out = out.concat(a.cars[slot]);
        if (b && b.state === 'ready' && b.cars[slot]) out = out.concat(b.cars[slot]);
        return out;
    }
    function rpNum(row, ci, name) {
        var i = ci[name];
        return i === undefined ? null : (rpFin(row[i]) ? row[i] : null);
    }
    /* State of one car at sim time t. rows: sorted by time; ci: column name -> index. null = hidden (outside the rows, or inside
     * a gap longer than RP_GAP seconds). x, z, spd, thr, brk and dist are linear; pos, lap, pit, gear, fin keep the previous row.
     * wrap = lap length in metres (0 = none): dist is interpolated across the line instead of jumping back. */
    function rpCarAt(rows, t, ci, wrap) {
        if (!rows || !rows.length || !rpFin(t) || ci.t === undefined) return null;
        var lo = 0, hi = rows.length - 1;
        if (t < rows[0][ci.t] || t > rows[hi][ci.t]) return null;
        while (lo < hi) { var mid = (lo + hi + 1) >> 1; if (rows[mid][ci.t] <= t) lo = mid; else hi = mid - 1; }
        var a = rows[lo], ta = a[ci.t];
        var b = lo + 1 < rows.length ? rows[lo + 1] : null;
        if (!b || t === ta) return rpRowState(a, null, 0, ci, wrap);
        var dt = b[ci.t] - ta;
        if (dt > RP_GAP) return null;
        if (!(dt > 0)) return rpRowState(a, null, 0, ci, wrap);
        return rpRowState(a, b, (t - ta) / dt, ci, wrap);
    }
    function rpRowState(a, b, u, ci, wrap) {
        var lin = function (name) {
            var va = rpNum(a, ci, name), vb = b ? rpNum(b, ci, name) : null;
            if (va === null || vb === null) return va;
            return va + (vb - va) * u;
        };
        var dist = rpNum(a, ci, 'dist'), db = b ? rpNum(b, ci, 'dist') : null;
        if (dist !== null && db !== null) {
            var step = db - dist;
            if (wrap > 0 && step < -wrap / 2) step += wrap;
            dist = dist + step * u;
            if (wrap > 0) dist = ((dist % wrap) + wrap) % wrap;
        }
        return { x: lin('x'), z: lin('z'), dist: dist, spd: lin('spd'), thr: lin('thr'), brk: lin('brk'),
            pos: rpNum(a, ci, 'pos'), lap: rpNum(a, ci, 'lap'), pit: rpNum(a, ci, 'pit'), gear: rpNum(a, ci, 'gear'), fin: rpNum(a, ci, 'fin') };
    }
    /* Lap length from the outline (closed polyline). 0 when there is no outline. */
    function rpOutlineLength(pts) {
        if (!pts || pts.length < 3) return 0;
        var s = 0;
        for (var i = 0; i < pts.length; i++) {
            var p = pts[i], q = pts[(i + 1) % pts.length];
            s += Math.sqrt((q[0] - p[0]) * (q[0] - p[0]) + (q[1] - p[1]) * (q[1] - p[1]));
        }
        return s;
    }
    /* Race distance in metres (lap count and lap distance). null when unknown. */
    function rpProgress(st, lapLen) {
        if (!st || st.lap === null || st.dist === null || !(lapLen > 0)) return null;
        return (st.lap - 1) * lapLen + st.dist;
    }
    /* Gap text "+1.2": gap in metres divided by the speed in m/s (channel 2 is metres per second). */
    function rpGapText(gapM, speedMs) {
        if (!rpFin(gapM) || !rpFin(speedMs) || speedMs < 1) return RP_DASH;
        return '+' + (Math.max(0, gapM) / speedMs).toFixed(1);
    }
    /* Tower gap column: FIN and PIT win over the gap; the leader says Leader. */
    function rpGapLabel(st, gapM, speedMs) {
        if (!st) return RP_DASH;
        if (st.fin > 0) return 'FIN';
        if (st.pit > 0) return 'PIT';
        if (st.pos === 1) return 'Leader';
        return rpGapText(gapM, speedMs);
    }
    /* Tower order: position ascending, cars without a position last, ties by slot. Items: {slot, pos}. */
    function rpTowerOrder(items) {
        return items.slice().sort(function (a, b) {
            var pa = rpFin(a.pos) ? a.pos : 1e9, pb = rpFin(b.pos) ? b.pos : 1e9;
            return pa - pb || a.slot - b.slot;
        });
    }
    /* Name fallback (the server already sends these; the page repeats them for old or partial data). */
    /* TAPE2 V1: a replay car's role as the shared helper reads it (player = cr 0, remote = cr 2 and human, else AI). */
    function rpHumanCar(car) {
        return { cr: car && car.role === 'player' ? 0 : car && car.role === 'remote' ? 2 : 1, a: car && car.role === 'remote' ? 0 : 1 };
    }
    function rpDisplayName(car) {
        var n = car && typeof car.name === 'string' ? car.name.trim() : '';
        if (n) return n;
        if (car && rpFin(car.num) && car.num > 0) return '#' + car.num;
        return 'Car ' + (car && rpFin(car.slot) ? car.slot : '?');
    }
    /* Three-letter-ish tag for the tower: the last word of the name ("Mika Hakkinen" -> "HAK"); "#7" and "Car 3" stay whole. */
    function rpShortName(name) {
        var s = String(name || '').replace(/\(.*?\)/g, '').trim();
        if (/^(#\d+|Car \d+)$/.test(s)) return s.toUpperCase().slice(0, 6);
        var parts = s.split(/\s+/);
        return (parts[parts.length - 1] || '').slice(0, 3).toUpperCase();
    }
    /* E2E 2026-10-15: the tower name cell. The tag is always shown; the full name follows only when it is not the tag already
     * ("#18" and "Car 3" print once, not "#18#18"). */
    function rpTowerNameHtml(name) {
        var full = String(name || '').trim(), tag = rpShortName(full);
        return '<b>' + esc(tag) + '</b>' + (tag === full.toUpperCase() ? '' : '<span class="fn">' + esc(full) + '</span>');
    }
    /* m:ss.s (or m:ss when whole). Negative and bad input read as 0. */
    function rpClock(sec, whole) {
        if (!rpFin(sec) || sec < 0) sec = 0;
        var m = Math.floor(sec / 60), s = sec - m * 60;
        if (whole) { s = Math.floor(s); return m + ':' + (s < 10 ? '0' : '') + s; }
        var ts = Math.round(s * 10) / 10;
        if (ts >= 60) { m += 1; ts = 0; }
        return m + ':' + (ts < 10 ? '0' : '') + ts.toFixed(1);
    }
    /* Lap time of the car's last completed lap at sim time t (lap events, sorted by t). null when none yet. */
    function rpLastLap(events, slot, t) {
        if (!events || !events.length) return null;
        var lo = 0, hi = events.length - 1, idx = -1;
        while (lo <= hi) { var m = (lo + hi) >> 1; if (events[m].t <= t) { idx = m; lo = m + 1; } else hi = m - 1; }
        for (var i = idx; i >= 0; i--) {
            var e = events[i];
            if (e.type === 'lap' && e.car === slot && rpFin(e.last_lap_s)) return e.last_lap_s;
        }
        return null;
    }
    /* Scrub-bar marks: pit, contact, limits, other incidents. Lap events are ticks, not marks. */
    function rpMarkKind(ev) {
        if (!ev) return null;
        if (ev.type === 'pit') return 'pit';
        if (ev.type === 'incident') return ev.kind === 'contact' ? 'contact' : ev.kind === 'limits' ? 'limits' : 'incident';
        return null;
    }
    function rpMarkText(ev, nameOf) {
        var k = rpMarkKind(ev), who = nameOf(ev.car);
        var lap = rpFin(ev.lap) ? ' (lap ' + ev.lap + ')' : '';
        if (k === 'pit') return 'Pit stop: ' + who + lap;
        if (k === 'contact') return 'Contact: ' + who + ' with ' + nameOf(ev.with);
        if (k === 'limits') return 'Track limits: ' + who + lap;
        return 'Incident: ' + who + lap;
    }
    /* Car colour: the team colour when the team is known (stable hash), else a fixed palette entry by slot. */
    function rpColor(car) {
        var key = car && car.team ? String(car.team) : '';
        if (!key) {
            var sl = car && rpFin(car.slot) ? car.slot : 0;
            return RP_PALETTE[((sl % RP_PALETTE.length) + RP_PALETTE.length) % RP_PALETTE.length];
        }
        var h = 0;
        for (var i = 0; i < key.length; i++) h = (h * 31 + key.charCodeAt(i)) % 65521;
        return RP_PALETTE[h % RP_PALETTE.length];
    }
    /* Normalises GET /replay/info (REPLAY_SPEC 2.1) for the page. */
    function rpInfoFrom(j) {
        var cars = (j.cars || []).map(function (c) {
            return { slot: rpFin(c.slot) ? c.slot : 0, name: typeof c.name === 'string' ? c.name : '',
                team: typeof c.team === 'string' ? c.team : '', num: rpFin(c.num) ? c.num : 0,
                role: c.role === 'player' || c.role === 'remote' ? c.role : 'ai', laps: rpFin(c.laps) ? c.laps : 0 };
        }).sort(function (a, b) { return a.slot - b.slot; });
        var byslot = {}, lead = null;
        cars.forEach(function (c) { byslot[c.slot] = c; if (!lead || c.laps > lead.laps) lead = c; });
        var tf = rpFin(j.t_first) ? j.t_first : 0;
        var tl = rpFin(j.t_last) ? Math.max(tf + 1, j.t_last) : tf + 1;
        var events = (Array.isArray(j.events) ? j.events : []).filter(function (e) { return e && rpFin(e.t); })
            .sort(function (a, b) { return a.t - b.t; });
        var outline = Array.isArray(j.outline) ? j.outline.filter(function (p) { return Array.isArray(p) && rpFin(p[0]) && rpFin(p[1]); }) : [];
        return { tf: tf, tl: tl, cars: cars, byslot: byslot, events: events,
            marks: events.filter(function (e) { return rpMarkKind(e) !== null; }),
            outline: outline, outLen: rpOutlineLength(outline),
            maxLaps: Math.max(1, lead ? lead.laps : 1), leader: lead ? lead.slot : -1 };
    }
    function rpErrText(res) {
        if (!res || res.code === 0) return 'No answer from the server.';
        if (res.j && res.j.error) return String(res.j.error);
        return 'HTTP ' + res.code + '.';
    }

    Console.prototype.rpInit = function () {
        if (!this._rp || this._rp.rid !== this.review) {
            this._rp = { rid: this.review, info: null, infoState: '', tries: 0, msg: '', msgBad: false, win: {}, inflight: 0,
                ci: null, wrap: 0, t: 0, playing: false, speed: 1, follow: null, drag: false, hits: [], cars: [],
                running: false, dirty: true, towerSig: null, dom: null, lastNoticeAt: 0 };
        }
        return this._rp;
    };
    Console.prototype.rpNotice = function (rp, text) {
        var now = Date.now();
        if (now - rp.lastNoticeAt > 4000) { rp.lastNoticeAt = now; this.toast(text); }
    };
    Console.prototype.rpTitle = function () {
        var d = this.sessionsDoc, rid = this.review;
        if (d && d.recordings) {
            for (var i = 0; i < d.recordings.length; i++) {
                var r = d.recordings[i], ss = r.sessions || [];
                for (var j = 0; j < ss.length; j++) {
                    if (ss[j].review_id === rid) {
                        var lab = sessionLabel(ss[j].id);
                        return recTitle(r.id) + (lab ? ' - ' + lab : '');
                    }
                }
            }
        }
        return reviewTitle(rid);
    };

    /* ---- 1.3.0 tape (TAPE_SPEC_2026-10-14 3.1 to 3.4): ONE clock for every page in review. The state lives on this.tape,
     * which is the same object as this._rp, so the Replay page and the transport bar share one clock. The timer below is the
     * only loop of the review clock (requestAnimationFrame does not fire in the Browser pane). Pure rules come first. ---- */
    var TAPE_TICK_MS = 100;
    var TAPE_SEEK_FWD_MAX = 5;
    /* One clock step: dt wall seconds at speed, held at tl, stops at tl. Pure. */
    function tapeStep(t, playing, speed, dt, tl) {
        if (!playing) return { t: t, playing: false };
        var nt = Math.min(tl, t + Math.max(0, dt) * speed);
        return { t: nt, playing: nt < tl };
    }
    /* A seek backwards, or more than 5 s forward, clears the short live histories. Pure. */
    function tapeSeekResets(from, to) { return to < from || to - from > TAPE_SEEK_FWD_MAX; }
    /* A race control or pit item shows only when its sim time (seconds, field t) is at or before the tape. Pure. */
    function tapeShowsAt(item, t) { return !!item && rpFin(item.t) && item.t <= t; }
    function tapeFrameUrl(t) { return '/replay/frame?t=' + (Math.round(t * 1000) / 1000).toFixed(3); }
    function tapeCannotPlayText(reason) { return 'This recording cannot be played: ' + reason + '. Nothing was changed.'; }
    /* One frame request at a time: due while playing or after a seek or step, and only once the recording info has loaded. Pure. */
    function tapeFetchDue(tp) { return !!tp && !!tp.info && !tp.frameInflight && (!!tp.playing || !!tp.needFetch); }

    /* The tape state object: the clock, the Replay page state and the recording info. */
    Console.prototype.tapeNew = function (rid) {
        return { kind: rid ? 'rec' : 'live', rid: rid || null, t: 0, t_first: 0, t_last: 0, playing: false, speed: 1,
            info: null, infoState: '', tries: 0, msg: '', msgBad: false, win: {}, inflight: 0,
            frameInflight: false, needFetch: false, frameMsg: '', frameBad: false, timer: null, last: 0, driverBusy: false,
            ci: null, wrap: 0, follow: null, drag: false, hits: [], cars: [], dirty: true, towerSig: null, dom: null, lastNoticeAt: 0 };
    };
    /* Starts the review of a v2 recording: the recording info (503 = still reading, retried), then the track outline. */
    Console.prototype.tapeInit = function () {
        if (!this.review || !/^v2_[0-9a-f]{16}$/.test(this.review)) return;
        this.tapeStart();
        if (this.tape.infoState !== 'loading' && this.tape.infoState !== 'wait') this.rpLoadInfo(this.tape);
    };
    Console.prototype.tapeStart = function () {
        var self = this;
        if (this.tape.timer) return;
        this.tape.last = 0;
        this.tape.timer = setInterval(function () { self.tapeTick(); }, TAPE_TICK_MS);
    };
    /* Called when /replay/info has loaded: the tape rests at t_first and one frame is fetched, so every page has data at once. */
    Console.prototype.tapeOnInfo = function (tp) {
        tp.t_first = tp.info.tf; tp.t_last = tp.info.tl;
        tp.needFetch = true; tp.dirty = true;
        this.tapeStart();
        this.tapeLoadTrack();
    };
    /* The track outline of the recording for the race map (live /track shape {pts:[[x,z],...]}). */
    Console.prototype.tapeLoadTrack = function () {
        var self = this, tp = this.tape;
        this.tapeTrackOk = true;   /* the outline comes from here, not from the page's fetchTrack hook (Viewer.onFrame) */
        this.fetchLapDoc('/track').then(function (res) {
            if (self.tape !== tp) return;
            if (res.code === 200 && res.j && Array.isArray(res.j.pts)) self.waypoints = res.j.pts;
            self.invalidate();
        });
    };
    /* The one clock: advances the tape while playing, asks for one frame when due, paints the transport bar and the Replay
     * page, and redraws the pages when something changed. */
    /* TAPE2 V3: every side follows the tape. Completed laps = those whose end is at or before the tape time (the newest of
     * them is "Last full lap (auto)"). The lap being driven at the tape time ("Lap in progress") shows its samples up to
     * the tape time, from the recording's own lap (seeded in full by /trace/data, so no request per tick). A shown lap the
     * tape has not reached is dropped, so seeking back hides the later laps again. Review only. */
    Console.prototype.tapeSyncLaps = function () {
        var self = this, t = this.tape.t;
        if (!this.review) return;
        /* the signature covers the tape time and each side's laps (seeds may arrive after the tape stopped) */
        var sig = [t].concat(this.wb.allSides().map(function (s) {
            return s.letter + ':' + (s.laps || []).length + ':' + (s.shown ? s.shown.lap : '-');
        })).join('|');
        if (sig === this._lapSyncSig) return;
        this._lapSyncSig = sig;
        this.wb.allSides().forEach(function (side) {
            var L = side.letter, lap = WB.lapInProgress(side.laps, t);
            side.tapeT = t;
            if (side.shown && side.shown.end > t + 1e-6) { side.shown = null; self.lapMode[L] = 'auto'; }
            side.tapeOpen = lap ? { lap: lap.lap, src: lap.src, recorded: true, start: lap.start, end: t, complete: false,
                length: lap.length, samples: lap.samples.filter(function (q) { return q.t <= t + 1e-6; }) } : null;
        });
        this.cache = {};
        this.invalidate();
    };

    Console.prototype.tapeTick = function () {
        var tp = this.tape, now = Date.now(), dt = tp.last ? Math.min(0.25, Math.max(0, (now - tp.last) / 1000)) : 0;
        tp.last = now;
        if (tp.info) {
            if (tp.playing && typeof document !== 'undefined' && document.hidden) { tp.playing = false; tp.dirty = true; }   /* playback pauses while the tab is hidden */
            if (tp.playing) {
                var w = tp.win[rpWindowIndex(tp.t, tp.info.tf)];
                /* the Replay page holds the clock until its own 30 s window has loaded (its rows drive the map and the tower) */
                var hold = this.view === 'replay' && !(w && w.state === 'ready');
                if (!hold) { var s = tapeStep(tp.t, true, tp.speed, dt, tp.info.tl); tp.t = s.t; tp.playing = s.playing; tp.needFetch = true; }
                tp.dirty = true;
            }
            if (this.view === 'replay') this.rpPump(tp);
            if (tapeFetchDue(tp)) this.tapeFetch();
            this.tapeSyncLaps();   /* TAPE2 V3: the laps follow the tape time (no-op when it has not moved) */
        }
        if (tp.dirty) {
            tp.dirty = false;
            this.tapePaintBar();
            if (this.view === 'replay') this.rpPaint(tp);
            this.invalidate();   /* the pages redraw with the new tape time */
        }
        this.flush();   /* in review the timer is the redraw loop (RAF does not fire in the Browser pane) */
    };
    /* One frame at the tape time: GET /replay/frame (live /telemetry shape plus the tape object), passed to onFrame. Never two in
     * flight; a request made while one is in flight is skipped and the next tick asks for the tape time of that moment. */
    Console.prototype.tapeFetch = function () {
        var self = this, tp = this.tape, t = tp.t;
        tp.needFetch = false; tp.frameInflight = true;
        this.fetchLapDoc(tapeFrameUrl(t)).then(function (res) {
            tp.frameInflight = false;
            if (self.tape !== tp) return;
            if (res.code === 200 && res.j && Array.isArray(res.j.c)) {
                tp.frameMsg = ''; tp.frameBad = false;
                self.onFrame(res.j);
                if (self.view === 'focus') self.pollDriver();
            } else if (res.code === 503) {
                tp.needFetch = true;
                self.rpNotice(tp, 'Preparing the recording (reading its laps)...');
            } else { tp.frameMsg = 'Replay: ' + rpErrText(res); tp.frameBad = true; }
            tp.dirty = true;
            self.invalidate();
        });
    };
    /* A seek backwards or far forward: the Engine and Chassis short histories restart at the new time. Completed laps stay
     * (they are seeded from /trace/data and the workbench is not fed by tape frames). */
    Console.prototype.tapeSeekReset = function () {
        this.pchHist = null;
    };
    /* Race control and pit lists in review show only what the tape has reached (no spoilers). Live, and reviews without
     * recording info, show every item. */
    Console.prototype.tapeVisible = function (item) {
        var tp = this.tape;
        return !(tp && tp.kind === 'rec' && tp.info) || tapeShowsAt(item, tp.t);
    };
    /* Open on a recording that has no review id: the reason, and LIVE stays as it is. */
    Console.prototype.cannotPlay = function (rec, id) {
        var d = this.sessionsDoc, reason = 'no replay data (made before storage v2, or no work file)', s = null;
        ((d && d.recordings) || []).forEach(function (r) {
            if (r.id === rec) (r.sessions || []).forEach(function (q) { if (q.id === id) s = q; });
        });
        if (s && s.state === 'open') reason = 'still recording';
        this.setNotice(tapeCannotPlayText(reason), true);
    };

    /* The transport bar (every page in review, the Replay page's own bar is this one). Built once; shown by refreshChrome. */
    Console.prototype.buildTapeBar = function () {
        var self = this, bar = el('div', 'f1x-tape');
        bar.innerHTML =
            '<div class="f1x-rp-bar">' +
              '<div class="f1x-rp-ctl">' +
                '<button class="tv-btn primary f1x-rp-play" type="button">Play</button>' +
                '<button class="tv-btn f1x-rp-sp" type="button" data-s="1">1x</button>' +
                '<button class="tv-btn f1x-rp-sp" type="button" data-s="2">2x</button>' +
                '<button class="tv-btn f1x-rp-sp" type="button" data-s="4">4x</button>' +
                '<button class="tv-btn f1x-rp-sp" type="button" data-s="8">8x</button>' +
                '<button class="tv-btn f1x-rp-sp" type="button" data-s="16">16x</button>' +
                '<span class="f1x-rp-time">Lap -/-   0:00.0 / 0:00</span>' +
                '<span class="f1x-rp-hint">Space play/pause  |  Left/Right 5 s  |  Shift+Left/Right 30 s  |  Up/Down speed  |  Esc stop following</span>' +
              '</div>' +
              '<div class="f1x-rp-scrubw"><canvas aria-label="Timeline: lap ticks and event marks"></canvas><div class="f1x-rp-tip"></div></div>' +
              '<div class="f1x-rp-msg"></div>' +
            '</div>';
        var q = function (s) { return bar.querySelector(s); };
        var d = this.tapeDom = { root: bar, play: q('.f1x-rp-play'), speeds: bar.querySelectorAll('.f1x-rp-sp'), time: q('.f1x-rp-time'),
            sc: q('.f1x-rp-scrubw canvas'), tip: q('.f1x-rp-scrubw .f1x-rp-tip'), msg: q('.f1x-rp-msg') };
        d.play.onclick = function () { self.rpTogglePlay(self.tape); };
        Array.prototype.forEach.call(d.speeds, function (b) {
            b.onclick = function () { self.tape.speed = +b.getAttribute('data-s') || 1; self.tape.dirty = true; };
        });
        var nameOf = function (slot) { var c = self.tape.info && self.tape.info.byslot[slot]; return c ? rpDisplayName(c) : 'Car ' + slot; };
        d.sc.addEventListener('mousedown', function (e) {
            var tp = self.tape;
            if (!tp.info) return;
            var r = d.sc.getBoundingClientRect(), x = e.clientX - r.left, m = self.rpMarkNear(tp, x, r.width);
            if (m) self.rpSeek(tp, m.t - 3);
            else { tp.drag = true; self.rpSeek(tp, tp.info.tf + x / Math.max(1, r.width) * (tp.info.tl - tp.info.tf)); }
        });
        d.sc.addEventListener('mousemove', function (e) {
            var tp = self.tape;
            if (!tp.info) return;
            var r = d.sc.getBoundingClientRect(), x = e.clientX - r.left, m = self.rpMarkNear(tp, x, r.width);
            if (tp.drag) self.rpSeek(tp, tp.info.tf + x / Math.max(1, r.width) * (tp.info.tl - tp.info.tf));
            if (m) { d.tip.textContent = rpMarkText(m, nameOf); d.tip.style.display = 'block'; d.tip.style.left = x + 'px'; }
            else d.tip.style.display = 'none';
        });
        d.sc.addEventListener('mouseleave', function () { d.tip.style.display = 'none'; });
        if (!this.tapeMouseBound) { this.tapeMouseBound = true; document.addEventListener('mouseup', function () { self.tape.drag = false; }); }
        bar.style.display = 'none';
        return bar;
    };
    /* The bar's text, buttons and scrub, from the tape state. The lap is the leader's lap in the current frame. */
    Console.prototype.tapePaintBar = function () {
        var d = this.tapeDom, tp = this.tape;
        if (!d) return;
        if (!tp.info) {
            d.msg.textContent = tp.msg || (tp.infoState === 'wait' ? 'Preparing the recording (reading its laps)...' : 'Loading the recording...');
            d.msg.className = 'f1x-rp-msg' + (tp.msgBad ? ' bad' : '');
            d.play.disabled = true;
            d.time.textContent = 'Lap -/-   0:00.0 / 0:00';
            return;
        }
        d.play.disabled = false;
        var lead = this.cars()[0], lapTxt = lead && rpFin(lead.l) ? lead.l : RP_DASH;
        d.time.textContent = 'Lap ' + lapTxt + '/' + tp.info.maxLaps + '   ' + rpClock(tp.t - tp.info.tf) + ' / ' + rpClock(tp.info.tl - tp.info.tf, true);
        d.play.textContent = tp.playing ? 'Pause' : (tp.t >= tp.info.tl ? 'Replay' : 'Play');
        Array.prototype.forEach.call(d.speeds, function (b) { b.classList.toggle('on', +b.getAttribute('data-s') === tp.speed); });
        var k = rpWindowIndex(tp.t, tp.info.tf), w = tp.win[k], text = '', bad = false;
        if (tp.frameBad) { text = tp.frameMsg; bad = true; }
        else if (w && w.state === 'error') { text = 'Replay error: ' + w.msg; bad = true; }
        else if (w && w.state === 'wait') text = 'Preparing the recording (reading its laps)...';
        else if (tp.msg) { text = tp.msg; bad = tp.msgBad; }
        d.msg.textContent = text;
        d.msg.className = 'f1x-rp-msg' + (bad ? ' bad' : '');
        this.rpPaintScrub(tp);
    };

    /* The page. Live mode: a note only. Review of a v2 recording: the Replay page (map, tower, follow). Its clock and bar are the tape. */
    Console.prototype.view_replay = function (host) {
        var self = this;
        this.refreshers = [];
        host.innerHTML = '';
        if (!this.review || !/^v2_[0-9a-f]{16}$/.test(this.review)) {
            var note = el('div', 'f1x-rp-live');
            if (this.review) note.innerHTML = '<p><b>This saved session has no replay data.</b></p><p>Replay needs a recording made by this version. Open a recording from the Recordings page.</p>';
            else note.innerHTML = '<p><b>Replay plays a finished recording. Open one on the Recordings page.</b></p>';
            var ob = el('button', 'tv-btn primary', 'Open Recordings');
            ob.onclick = function () { self.setPage('sessions'); };
            note.appendChild(ob);
            host.appendChild(note);
            this.dirty = true;
            return;
        }
        var rp = this.rpInit();
        var g = this.grid(host, 'minmax(0,1fr)', 'minmax(0,1fr)');
        var root = el('div', 'f1x-rp');
        root.innerHTML =
            '<div class="f1x-rp-main">' +
              '<div class="f1x-rp-map"><canvas aria-label="Track map with the cars"></canvas><div class="f1x-rp-title"></div></div>' +
              '<div class="f1x-rp-side">' +
                '<div class="f1x-rp-tower"><table class="f1x-rp-t"><thead><tr><th>P</th><th class="l">Driver</th><th>Lap</th><th>Gap</th></tr></thead><tbody></tbody></table></div>' +
                '<div class="f1x-rp-follow">' +
                  '<div class="f1x-rp-fempty">Click a car on the map, or a row in the tower, to follow it.</div>' +
                  '<div class="f1x-rp-fbody" style="display:none">' +
                    '<div class="f1x-rp-fn"></div><div class="f1x-rp-fc"></div>' +
                    '<div class="f1x-rp-fg"><div>Speed<b class="spd">-</b></div><div>Gear<b class="gear">-</b></div><div>Lap<b class="lap">-</b></div><div>Last lap<b class="last">-</b></div></div>' +
                    '<div class="f1x-rp-pedal"><span>Throttle</span><span class="bar"><i class="thr"></i></span><span>Brake</span><span class="bar brk"><i class="brk"></i></span></div>' +
                  '</div>' +
                '</div>' +
              '</div>' +
            '</div>';
        g.appendChild(root);
        var q = function (s) { return root.querySelector(s); };
        rp.dom = { root: root, mc: q('.f1x-rp-map canvas'), title: q('.f1x-rp-title'), tower: q('.f1x-rp-t tbody'),
            fEmpty: q('.f1x-rp-fempty'), fBody: q('.f1x-rp-fbody'), fName: q('.f1x-rp-fn'), fTeam: q('.f1x-rp-fc'),
            fSpd: q('.f1x-rp-fg .spd'), fGear: q('.f1x-rp-fg .gear'), fLap: q('.f1x-rp-fg .lap'), fLast: q('.f1x-rp-fg .last'),
            fThr: q('.f1x-rp-pedal .thr'), fBrk: q('.f1x-rp-pedal .brk') };
        rp.towerSig = null;
        rp.dirty = true;
        rp.dom.tower.addEventListener('click', function (e) {
            var tr = e.target && e.target.closest ? e.target.closest('tr[data-slot]') : null;
            if (!tr) return;
            var slot = +tr.getAttribute('data-slot');
            rp.follow = rp.follow === slot ? null : slot;
            rp.dirty = true;
        });
        rp.dom.mc.addEventListener('click', function (e) {
            var r = rp.dom.mc.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, best = null, bd = 1e9;
            rp.hits.forEach(function (h) {
                var dd = (x - h.x) * (x - h.x) + (y - h.y) * (y - h.y);
                if (dd <= h.r * h.r && dd < bd) { bd = dd; best = h; }
            });
            if (best) { rp.follow = rp.follow === best.slot ? null : best.slot; rp.dirty = true; }
        });
        if (!rp.resBound) { rp.resBound = true; window.addEventListener('resize', function () { rp.dirty = true; }); }
        if (!rp.info && rp.infoState !== 'loading' && rp.infoState !== 'wait') this.rpLoadInfo(rp);
        this.rpStart(rp);
    };

    /* Tape keys on every page in review (1.3.0): Space play/pause, Left/Right 5 s, Shift 30 s, Up/Down speed, Esc stops following
     * on the Replay page. Returns true when the key was used, so the global page keys do not also run. */
    Console.prototype.rpKey = function (e) {
        var rp = this.tape, k = e.key;
        if (!rp || rp.kind !== 'rec') return false;
        if (e.target && /^(INPUT|SELECT|TEXTAREA)$/.test(e.target.tagName)) return false;
        if (k === 'Escape') {
            if (this.view !== 'replay' || rp.follow === null) return false;
            rp.follow = null; rp.dirty = true; e.preventDefault();
            return true;
        }
        if (!/^(ArrowLeft|ArrowRight|ArrowUp|ArrowDown| )$/.test(k)) return false;
        e.preventDefault();
        if (!rp.info) return true;   /* not loaded yet: the keys wait and the page does not change */
        if (k === ' ') {
            if (e.target && e.target.tagName === 'BUTTON') e.target.blur();   /* a focused button must not also click */
            this.rpTogglePlay(rp);
        }
        else if (k === 'ArrowLeft' || k === 'ArrowRight') this.rpSeek(rp, rp.t + (k === 'ArrowLeft' ? -1 : 1) * (e.shiftKey ? 30 : 5));
        else if (k === 'ArrowUp' || k === 'ArrowDown') {
            var i = RP_SPEEDS.indexOf(rp.speed) + (k === 'ArrowUp' ? 1 : -1);
            rp.speed = RP_SPEEDS[Math.max(0, Math.min(RP_SPEEDS.length - 1, i))];
            rp.dirty = true;
        }
        return true;
    };

    Console.prototype.rpTogglePlay = function (rp) {
        if (!rp.info) return;
        if (!rp.playing && rp.t >= rp.info.tl) rp.t = rp.info.tf;
        rp.playing = !rp.playing;
        rp.dirty = true;
        if (rp.playing) this.rpStart(rp);
    };
    Console.prototype.rpSeek = function (rp, t) {
        if (!rp.info) return;
        var tf = rp.info.tf, from = rp.t;
        rp.t = Math.max(tf, Math.min(rp.info.tl, t));
        if (tapeSeekResets(from, rp.t)) this.tapeSeekReset();
        rp.needFetch = true;   /* one frame at the new time, then the pages follow */
        var k = rpWindowIndex(rp.t, tf), w = rp.win[k];
        if (w && (w.state === 'error' || w.state === 'wait')) delete rp.win[k];   /* seeking into a failed window retries it */
        rp.dirty = true;
    };

    /* The one clock is the tape timer (tapeStart); the Replay page only asks for its clock to run. */
    Console.prototype.rpStart = function () {
        this.tapeStart();
    };
    Console.prototype.rpPump = function (rp) {
        if (!rp.info) return;
        var self = this, now = Date.now(), tf = rp.info.tf, tl = rp.info.tl, k = rpWindowIndex(rp.t, tf);
        Object.keys(rp.win).forEach(function (key) { if (Math.abs(+key - k) > 2) delete rp.win[key]; });
        var want = rpPlanFetch(rp.t, tf, tl, function (kk) {
            var w = rp.win[kk];
            if (!w) return false;
            if (w.state === 'wait') return now < w.retryAt;
            return true;
        }, rp.inflight);
        want.forEach(function (kk) { self.rpLoadWindow(rp, kk); });
    };

    Console.prototype.rpLoadInfo = function (rp) {
        var self = this;
        rp.infoState = 'loading';
        this.fetchLapDoc('/replay/info').then(function (res) {
            if (self.tape !== rp) return;
            if (res.code === 200 && res.j && Array.isArray(res.j.cars)) {
                rp.info = rpInfoFrom(res.j);
                rp.t = rp.info.tf;
                rp.wrap = rp.info.outLen;
                rp.infoState = 'ready';
                rp.msg = ''; rp.msgBad = false;
                self.tapeOnInfo(rp);
            } else if (res.code === 503 && rp.tries++ < 120) {
                rp.infoState = 'wait';
                self.rpNotice(rp, 'Preparing the recording (reading its laps)...');
                setTimeout(function () { if (self.tape === rp && rp.infoState === 'wait') self.rpLoadInfo(rp); }, 1000);
            } else {
                rp.infoState = 'error'; rp.msg = 'Replay: ' + rpErrText(res); rp.msgBad = true;
            }
            rp.dirty = true;
        });
    };
    Console.prototype.rpLoadWindow = function (rp, k) {
        var self = this, r = rpWindowRange(k, rp.info.tf, rp.info.tl);
        rp.win[k] = { state: 'pending' };
        rp.inflight++;
        this.fetchLapDoc('/replay?t0=' + r.t0.toFixed(3) + '&t1=' + r.t1.toFixed(3)).then(function (res) {
            rp.inflight--;
            if (self.tape !== rp) return;
            var j = res.j;
            if (res.code === 200 && j && Array.isArray(j.cars)) {
                var w = { state: 'ready', cars: {} };
                j.cars.forEach(function (c) { w.cars[c.slot] = Array.isArray(c.rows) ? c.rows : []; });
                if (!rp.ci && Array.isArray(j.cols)) { rp.ci = {}; j.cols.forEach(function (n, i) { rp.ci[n] = i; }); }
                rp.win[k] = w;
            } else if (res.code === 503) {
                rp.win[k] = { state: 'wait', retryAt: Date.now() + 1000 };
                self.rpNotice(rp, 'Preparing the recording (reading its laps)...');
            } else {
                rp.win[k] = { state: 'error', msg: rpErrText(res) };
            }
            rp.dirty = true;
        });
    };

    /* Every car at rp.t, in tower order. */
    Console.prototype.rpCars = function (rp) {
        var k = rpWindowIndex(rp.t, rp.info.tf), ci = rp.ci, out = [];
        rp.info.cars.forEach(function (car) {
            var st = ci ? rpCarAt(rpRowsFor(rp.win, car.slot, k), rp.t, ci, rp.wrap) : null;
            out.push({ slot: car.slot, car: car, st: st, pos: st && rpFin(st.pos) ? st.pos : null });
        });
        return rpTowerOrder(out);
    };
    Console.prototype.rpMarkNear = function (rp, x, W) {
        var best = null, bd = 8, span = Math.max(1e-6, rp.info.tl - rp.info.tf);
        rp.info.marks.forEach(function (ev) {
            var dd = Math.abs((ev.t - rp.info.tf) / span * W - x);
            if (dd < bd) { bd = dd; best = ev; }
        });
        return best;
    };

    /* Map, tower and follow panel of the Replay page. The transport bar (clock, buttons, scrub) is painted by tapePaintBar. */
    Console.prototype.rpPaint = function (rp) {
        var d = rp.dom;
        if (!d || !rp.info) return;
        var items = this.rpCars(rp), L = rp.info.outLen, fol = null;
        items.forEach(function (it, n) {
            it.gapM = null;
            var ahead = n > 0 ? items[n - 1] : null;
            if (ahead && ahead.st && it.st) {
                var pa = rpProgress(ahead.st, L), pm = rpProgress(it.st, L);
                if (pa !== null && pm !== null) it.gapM = Math.max(0, pa - pm);
            }
            if (it.slot === rp.follow) fol = it;
        });
        rp.cars = items;
        var tt = this.rpTitle();
        if (d.title.textContent !== tt) d.title.textContent = tt;
        this.rpPaintMap(rp, items, fol);
        this.rpPaintTower(rp, items);
        this.rpPaintFollow(rp, fol);
    };

    Console.prototype.rpPaintMap = function (rp, items, fol) {
        var c = setupCanvas(rp.dom.mc), ctx = c.ctx, W = c.w, H = c.h, pts = rp.info.outline;
        var minx = Infinity, maxx = -Infinity, minz = Infinity, maxz = -Infinity;
        var grow = function (x, z) { if (x < minx) minx = x; if (x > maxx) maxx = x; if (z < minz) minz = z; if (z > maxz) maxz = z; };
        pts.forEach(function (p) { grow(p[0], p[1]); });
        if (!pts.length) items.forEach(function (it) { if (it.st && rpFin(it.st.x) && rpFin(it.st.z)) grow(it.st.x, it.st.z); });
        rp.hits = [];
        if (!isFinite(minx)) {
            ctx.fillStyle = PAL.axis; ctx.font = '12px ' + PAL.sans; ctx.textAlign = 'center';
            ctx.fillText('The track map appears when the first part of the recording has loaded.', W / 2, H / 2);
            return;
        }
        if (maxx - minx < 100) { var ex = (100 - (maxx - minx)) / 2; minx -= ex; maxx += ex; }
        if (maxz - minz < 100) { var ez = (100 - (maxz - minz)) / 2; minz -= ez; maxz += ez; }
        var pad = 26, sc = Math.min((W - 2 * pad) / (maxx - minx), (H - 2 * pad) / (maxz - minz));
        var ox = (W - (maxx - minx) * sc) / 2, oz = (H - (maxz - minz) * sc) / 2;
        /* Same convention as the Session Overview map: z is flipped (north up), one scale for both axes. */
        var tr = { s: sc, x0: ox - minx * sc, y0: H - oz + minz * sc };
        if (fol && fol.st && rpFin(fol.st.x) && rpFin(fol.st.z)) {
            var s2 = sc * 2.4;
            tr = { s: s2, x0: W / 2 - fol.st.x * s2, y0: H / 2 + fol.st.z * s2 };
        }
        var P = function (x, z) { return [tr.x0 + x * tr.s, tr.y0 - z * tr.s]; };
        ctx.lineJoin = 'round'; ctx.lineCap = 'round';
        if (pts.length > 2) {
            ctx.beginPath();
            pts.forEach(function (p, i) { var v = P(p[0], p[1]); if (i) ctx.lineTo(v[0], v[1]); else ctx.moveTo(v[0], v[1]); });
            ctx.closePath();
            ctx.strokeStyle = '#17306e'; ctx.lineWidth = 12; ctx.stroke();
            ctx.strokeStyle = '#5f7fc4'; ctx.lineWidth = 1.5; ctx.stroke();
            var s0 = P(pts[0][0], pts[0][1]);
            ctx.fillStyle = '#ffffff'; ctx.fillRect(s0[0] - 1.5, s0[1] - 6, 3, 12);
        }
        var rank = function (it) {
            if (it.slot === rp.follow) return 3;
            return it.car.role === 'player' ? 2 : it.car.role === 'remote' ? 1 : 0;
        };
        items.slice().sort(function (a, b) { return rank(a) - rank(b) || a.slot - b.slot; }).forEach(function (it) {
            if (!it.st || !rpFin(it.st.x) || !rpFin(it.st.z)) return;
            var car = it.car, v = P(it.st.x, it.st.z), r = car.role === 'ai' ? 8 : 10;
            var num = car.num > 0 ? car.num : car.slot, pit = it.st.pit > 0, fo = it.slot === rp.follow;
            ctx.globalAlpha = pit ? 0.45 : 1;
            if (car.role === 'remote') { ctx.beginPath(); ctx.arc(v[0], v[1], r + 3, 0, Math.PI * 2); ctx.strokeStyle = '#38d6e8'; ctx.lineWidth = 2; ctx.stroke(); }
            if (car.role === 'player') { ctx.beginPath(); ctx.arc(v[0], v[1], r + 3, 0, Math.PI * 2); ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 2; ctx.stroke(); }
            if (fo) { ctx.beginPath(); ctx.arc(v[0], v[1], r + 7, 0, Math.PI * 2); ctx.strokeStyle = '#ffd84a'; ctx.lineWidth = 1.5; ctx.setLineDash([3, 3]); ctx.stroke(); ctx.setLineDash([]); }
            ctx.beginPath(); ctx.arc(v[0], v[1], r, 0, Math.PI * 2); ctx.fillStyle = rpColor(car); ctx.fill();
            ctx.font = 'bold ' + (r > 8 ? 10 : 9) + 'px ' + PAL.sans; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
            ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,0.85)'; ctx.strokeText(String(num), v[0], v[1] + 0.5);
            ctx.fillStyle = '#ffffff'; ctx.fillText(String(num), v[0], v[1] + 0.5);
            if (pit) { ctx.font = 'bold 9px ' + PAL.sans; ctx.fillStyle = '#ffd84a'; ctx.fillText('PIT', v[0], v[1] - r - 8); }
            if (fo) {
                ctx.font = 'bold 11px ' + PAL.sans; ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,0.85)';
                var nm = rpShortName(rpDisplayName(car));
                ctx.strokeText(nm, v[0], v[1] - r - 18); ctx.fillStyle = '#ffffff'; ctx.fillText(nm, v[0], v[1] - r - 18);
            }
            ctx.globalAlpha = 1;
            rp.hits.push({ slot: it.slot, x: v[0], y: v[1], r: r + 6 });
        });
        ctx.textAlign = 'left'; ctx.textBaseline = 'bottom'; ctx.font = '10px ' + PAL.sans; ctx.fillStyle = '#7f93c8';
        ctx.fillText('White ring: you    Cyan ring: CO-OP partner    Click a car to follow it    Esc stops following', 10, H - 6);
        ctx.textBaseline = 'alphabetic';
    };

    Console.prototype.rpPaintTower = function (rp, items) {
        var html = items.map(function (it) {
            var st = it.st, car = it.car, cls = [];
            if (car.role === 'player') cls.push('me');
            if (it.slot === rp.follow) cls.push('fol');
            if (!st) cls.push('out');
            var label = rpGapLabel(st, it.gapM, st ? st.spd : null);
            var name = rpDisplayName(car);
            return '<tr data-slot="' + it.slot + '" class="' + cls.join(' ') + '"><td class="p">' + (st && rpFin(st.pos) ? st.pos : RP_DASH) + '</td>' +
                '<td class="d"><i style="background:' + rpColor(car) + '"></i>' + rpTowerNameHtml(name) +
                humanBadgeHtml(rpHumanCar(car), null, rpColor(car)) + '</td>' +
                '<td>' + (st && rpFin(st.lap) ? st.lap : RP_DASH) + '</td><td class="g' + (label === 'PIT' ? ' pit' : '') + '">' + esc(label) + '</td></tr>';
        }).join('');
        if (html === rp.towerSig) return;
        rp.towerSig = html;
        rp.dom.tower.innerHTML = html;
    };

    Console.prototype.rpPaintFollow = function (rp, it) {
        var d = rp.dom;
        d.fEmpty.style.display = it ? 'none' : '';
        d.fBody.style.display = it ? '' : 'none';
        if (!it) return;
        var st = it.st, car = it.car;
        d.fName.textContent = rpDisplayName(car) + humanBadgeText(rpHumanCar(car));   /* TAPE2 V1 */
        d.fTeam.textContent = (car.team || 'Team not recorded') + ' - ' + (car.role === 'player' ? 'You' : car.role === 'remote' ? 'CO-OP partner' : 'AI driver');
        d.fSpd.textContent = st && rpFin(st.spd) ? String(Math.round(st.spd * 3.6)) + ' km/h' : RP_DASH;
        d.fGear.textContent = st && rpFin(st.gear) ? (st.gear === 0 ? 'N' : String(st.gear)) : RP_DASH;
        d.fLap.textContent = st && rpFin(st.lap) ? String(st.lap) : RP_DASH;
        var last = rpLastLap(rp.info.events, it.slot, rp.t);
        d.fLast.textContent = rpFin(last) ? rpClock(last) : RP_DASH;
        var pct = function (v) { return rpFin(v) ? Math.round(Math.max(0, Math.min(1, v)) * 100) + '%' : '0%'; };
        d.fThr.style.width = pct(st ? st.thr : null);
        d.fBrk.style.width = pct(st ? st.brk : null);
    };

    Console.prototype.rpPaintScrub = function (rp) {
        var c = setupCanvas(this.tapeDom.sc), ctx = c.ctx, W = c.w, H = c.h, tf = rp.info.tf;
        var span = Math.max(1e-6, rp.info.tl - tf), X = function (t) { return (t - tf) / span * W; };
        var barY = 11, barH = 8;
        ctx.fillStyle = '#0a1640'; ctx.fillRect(0, barY, W, barH);
        Object.keys(rp.win).forEach(function (key) {
            var w = rp.win[key];
            if (!w || w.state !== 'ready') return;
            var r = rpWindowRange(+key, tf, rp.info.tl);
            ctx.fillStyle = '#1d3fa8'; ctx.fillRect(X(r.t0), barY, Math.max(1, X(r.t1) - X(r.t0)), barH);
        });
        ctx.fillStyle = '#38d6e8'; ctx.fillRect(0, barY, Math.max(0, Math.min(W, X(rp.t))), barH);
        ctx.strokeStyle = '#c9d4ea'; ctx.lineWidth = 1;
        rp.info.events.forEach(function (ev) {
            if (ev.type !== 'lap' || ev.car !== rp.info.leader) return;
            var x = Math.round(X(ev.t)) + 0.5;
            ctx.beginPath(); ctx.moveTo(x, 1); ctx.lineTo(x, barY - 2); ctx.stroke();
        });
        rp.info.marks.forEach(function (ev) {
            ctx.fillStyle = RP_MARK_COL[rpMarkKind(ev)] || '#ffffff';
            ctx.beginPath(); ctx.arc(X(ev.t), H - 7, 3.5, 0, Math.PI * 2); ctx.fill();
        });
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(Math.round(X(rp.t)) - 1, 0, 2, H);
    };

    /* Replay page CSS (f1x- prefix; colours and fonts as the other console pages). */
    var REPLAY_CSS = [
        '.f1x-tape{flex:none;min-width:0;padding:4px 0 0;border-top:1px solid #14307f}',
        '.f1x-rp{position:relative;display:flex;flex-direction:column;gap:6px;height:100%;min-height:0;min-width:0;font:11px var(--sans,Segoe UI,Arial,sans-serif);color:#dfe7ff}',
        '.f1x-rp-main{flex:1 1 auto;min-height:0;display:flex;gap:6px}',
        '.f1x-rp-map{position:relative;flex:0 0 68%;min-width:0;min-height:260px;background:#02061a;border:1px solid #14307f;overflow:hidden}',
        '.f1x-rp-map canvas{position:absolute;left:0;top:0;width:100%;height:100%;display:block}',
        '.f1x-rp-title{position:absolute;left:10px;top:7px;right:10px;font:bold 12px var(--sans,Segoe UI,Arial,sans-serif);color:#fff;text-shadow:0 1px 2px #000;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;pointer-events:none}',
        '.f1x-rp-side{flex:1 1 32%;min-width:260px;display:flex;flex-direction:column;gap:6px;min-height:0}',
        '.f1x-rp-tower{flex:1 1 auto;min-height:0;overflow:auto;background:#040b26;border:1px solid #14307f}',
        '.f1x-rp-t{border-collapse:collapse;width:100%;font:12px var(--sans,Segoe UI,Arial,sans-serif)}',
        '.f1x-rp-t th{position:sticky;top:0;background:#071066;color:#9fb2e6;font:bold 10px var(--sans,Segoe UI,Arial,sans-serif);text-align:right;padding:4px 6px;letter-spacing:.05em;text-transform:uppercase}',
        '.f1x-rp-t th.l{text-align:left}',
        '.f1x-rp-t td{padding:3px 6px;border-top:1px solid #0d2466;white-space:nowrap;text-align:right;color:#dfe7ff;font-variant-numeric:tabular-nums}',
        '.f1x-rp-t td.p{width:24px;font-weight:bold;color:#fff}',
        '.f1x-rp-t td.d{text-align:left;width:100%;max-width:0;overflow:hidden;text-overflow:ellipsis}',
        '.f1x-rp-t td.d i{display:inline-block;width:3px;height:12px;vertical-align:middle;margin-right:6px}',
        '.f1x-rp-t td.d b{color:#fff;margin-right:6px}',
        '.f1x-rp-t td.d .fn{color:#9fb2e6;font-size:11px}',
        '.f1x-rp-t td.g{font-weight:bold;color:#7fd4ff;min-width:58px}',
        '.f1x-rp-t td.g.pit{color:#ffd84a}',
        '.f1x-rp-t tr{cursor:pointer}',
        '.f1x-rp-t tbody tr:hover td{background:#0b1a5e}',
        '.f1x-rp-t tr.me td{background:#14307f}',
        '.f1x-rp-t tr.me td.p{box-shadow:inset 3px 0 0 #e10600}',
        '.f1x-rp-t tr.fol td{color:#ffd84a}',
        '.f1x-rp-t tr.out td{opacity:.45}',
        '.f1x-rp-follow{flex:0 0 auto;background:#040b26;border:1px solid #14307f;padding:8px 10px;min-height:150px}',
        '.f1x-rp-fempty{color:#7f93c8;font-size:11px;line-height:1.5;margin-top:4px}',
        '.f1x-rp-fn{font:bold 16px var(--sans,Segoe UI,Arial,sans-serif);color:#fff}',
        '.f1x-rp-fc{color:#9fb2e6;font-size:11px;margin-bottom:6px}',
        '.f1x-rp-fg{display:grid;grid-template-columns:repeat(4,1fr);gap:4px;margin-bottom:6px}',
        '.f1x-rp-fg div{background:#02061a;border:1px solid #14307f;padding:3px 5px;color:#9fb2e6;font-size:9px;text-transform:uppercase;letter-spacing:.05em}',
        '.f1x-rp-fg b{display:block;font:bold 14px var(--sans,Segoe UI,Arial,sans-serif);color:#fff;text-transform:none;letter-spacing:0;white-space:nowrap}',
        '.f1x-rp-pedal{display:grid;grid-template-columns:52px 1fr;gap:4px 8px;align-items:center;color:#9fb2e6;font-size:10px}',
        '.f1x-rp-pedal span.bar{position:relative;height:8px;background:#02061a;border:1px solid #14307f;display:block}',
        '.f1x-rp-pedal span.bar i{position:absolute;left:0;top:0;bottom:0;width:0;background:#3ce07a}',
        '.f1x-rp-pedal span.bar.brk i{background:#e10600}',
        '.f1x-rp-bar{flex:0 0 auto;background:#02061a;border:1px solid #14307f;padding:6px 8px;display:flex;flex-direction:column;gap:5px}',
        '.f1x-rp-ctl{display:flex;align-items:center;gap:6px;flex-wrap:wrap}',
        '.f1x-rp-ctl .tv-btn{min-width:44px;justify-content:center}',
        '.f1x-rp .tv-btn.primary,.f1x-rp-live .tv-btn.primary{background:#e10600;border-color:#ff4a3d;color:#fff}',
        '.f1x-rp-time{font:bold 13px var(--mono,Consolas,ui-monospace,monospace);color:#fff;margin-left:8px;white-space:nowrap}',
        '.f1x-rp-hint{margin-left:auto;color:#7f93c8;font-size:10px;white-space:nowrap}',
        '.f1x-rp-scrubw{position:relative}',
        '.f1x-rp-scrubw canvas{width:100%;height:30px;display:block;cursor:pointer}',
        '.f1x-rp-tip{position:absolute;display:none;bottom:34px;transform:translateX(-50%);pointer-events:none;background:#040b26;border:1px solid #38d6e8;color:#dfe7ff;font:11px var(--sans,Segoe UI,Arial,sans-serif);padding:3px 7px;white-space:nowrap;z-index:5}',
        '.f1x-rp-msg{min-height:14px;color:#9fb2e6;font-size:11px}',
        '.f1x-rp-msg.bad{color:#ff8a80}',
        '.f1x-rp-live{padding:18px 14px;font:13px var(--sans,Segoe UI,Arial,sans-serif);color:#dfe7ff;max-width:760px}',
        '.f1x-rp-live p{margin:6px 0}',
        '.f1x-sess-t a.f1x-watch{display:inline-flex;align-items:center;height:22px;padding:0 10px;margin-left:6px;border:1px solid #ff4a3d;border-radius:3px;background:#e10600;color:#fff;font:bold 11px var(--sans,Segoe UI,Arial,sans-serif);text-decoration:none;vertical-align:middle}',
        '.f1x-sess-t a.f1x-watch:hover{background:#ff2a1f}'
    ].join('\n');

    Console.prototype.noticeHtml = function (nt) {
        return '<div class="f1x-notice ' + (nt.bad ? 'bad' : 'ok') + '" title="' + esc(nt.text) + '">' + esc(nt.text) +
            (nt.bad ? ' <button class="tv-btn" data-act="dismiss-notice">Dismiss</button>' : '') + '</div>';
    };
    Console.prototype.recordingsHtml = function () {
        var self = this, d = this.sessionsDoc, nt = this.sessNotice, lj = this.lastJob(), f = this.sessFilter;
        var h = '';
        if (!d) return (nt ? this.noticeHtml(nt) : '') + (this.sessionsErr ? '<div class="f1x-note">Recordings could not be loaded: ' + esc(this.sessionsErr) + '</div>' : '<div class="f1x-note">Loading recordings…</div>');
        /* Page header card: folder total, warning level, the recording-now pill, Refresh. */
        h += '<div class="f1x-card f1x-hdr">' +
            '<span>Folder total <b>' + esc(bytesText(d.total_bytes)) + '</b></span>' +
            '<span class="f1x-wlevel ' + (d.warning ? 'warn' : 'ok') + '">' + esc(d.warning || 'Under the 1 GB warning level') + '</span>' +
            (d.recording ? '<span class="f1x-pill" title="' + esc(String(d.recording.session || '')) + '">Recording now: ' +
                esc(recShortTitle(String(d.recording.session || ''))) + ' (' + Math.round((d.recording.ticks || 0) / 1200) + ' min)</span>'
                : '<span class="f1x-pill idle">Not recording</span>') +
            '<button class="tv-btn" data-act="refresh">Refresh</button></div>';
        if (!d.enabled) h += '<div class="f1x-note bad">Telemetry storage is off on this PC. Nothing is recorded.</div>';
        if (d.notice) h += '<div class="f1x-note">' + esc(d.notice) + '</div>';
        if (d.loaded && !self.isPast()) h += '<div class="f1x-note">Still selected on the game PC: <b>' + esc(recShortTitle(d.loaded)) +
            '</b>. Selecting changes no file; Back to live clears the selection.</div>';
        if (nt) h += this.noticeHtml(nt);
        if (lj.text) h += '<div class="f1x-note' + (lj.bad ? ' bad' : '') + '">' + esc(lj.text) + '</div>';
        var recs = d.recordings || [];
        if (!recs.length) return h + '<div class="f1x-note">No recordings yet. Each session is recorded from its start, automatically.</div>';
        var list = recs.filter(function (r) { return recMatches(r.id, f.q, f.mode); });
        var sel = this.selectedKeys();
        var arch = sel.filter(function (k) { var s = self.keyState(k); return s === 'closed' || s === 'recovered'; });
        if (sel.length) {
            h += '<div class="f1x-selbar"><b>' + sel.length + ' selected:</b> ' +
                '<button class="tv-btn" data-act="sel-archive"' + (arch.length ? '' : ' disabled title="Only closed or recovered sessions can be archived"') + '>Archive</button>' +
                ' <span class="f1x-sep">-</span> <button class="tv-btn" data-act="sel-delete" title="Move the selected sessions to the Windows Recycle Bin">Move to Windows Recycle Bin</button>' +
                ' <span class="f1x-sep">-</span> <button class="tv-btn" data-act="sel-clear">Clear</button></div>';
        }
        if (!list.length) return h + '<div class="f1x-note">No recording matches this search. Clear the search or choose All modes.</div>';
        list.forEach(function (r) { h += self.recCardHtml(r, d.loaded); });
        return h;
    };
    Console.prototype.recCardHtml = function (r, loaded) {
        var self = this, sess = (r.sessions || []).slice(), isOn = loaded === r.id;
        var hasOpen = sess.some(function (s) { return s.state === 'open'; });
        sess.sort(function (a, b) {
            var x = sessionRank(sessionLabel(a.id)), y = sessionRank(sessionLabel(b.id));
            return x !== y ? x - y : (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
        });
        var delAttr = isOn ? ' disabled title="Back to live first: this recording is open in PAST mode"' :
            hasOpen ? ' disabled title="A session is recording now. Delete the finished sessions one by one."' :
            ' title="Move this whole recording to the Windows Recycle Bin"';
        var h = '<section class="f1x-card' + (isOn ? ' on' : '') + '">' +
            '<header class="f1x-card-h"><div><h3 title="' + esc(r.id) + '">' + esc(recTitle(r.id)) + '</h3><small>' + esc(recSummary(r)) + '</small></div>' +
            '<div class="f1x-card-b"><button class="tv-btn' + (isOn ? ' on' : '') + '" data-act="load" data-rec="' + esc(r.id) + '"' +
            ((r.sessions || []).filter(function (q) { return q.review_id; }).slice(0, 1).map(function (q) {
                return ' data-rid="' + esc(q.review_id) + '" data-name="' + esc(recTitle(r.id) + ' - ' + (sessionLabel(q.id) || 'Session')) + '"';
            })[0] || '') + '>Open</button>' +
            ' <button class="f1x-link" data-act="del-rec" data-rec="' + esc(r.id) + '" data-bytes="' + (+r.bytes || 0) + '"' + delAttr + '>Delete...</button></div></header>';
        if (!sess.length && !(r.damaged || []).length && !(r.partial || []).length) {
            h += '<div class="f1x-note">No sessions in this recording.</div></section>';
            return h;
        }
        h += '<table class="f1x-sess-t"><tbody>';
        sess.forEach(function (s) {
            var key = r.id + '/' + s.id, open = s.state === 'open', b = stateBadge(s.state), lab = s.type_name || sessionLabel(s.id) || 'Session (type not in name)';
            h += '<tr' + (open ? ' class="open"' : '') + '><td class="c">' +
                (open || isOn ? '' : '<input type="checkbox" data-sel="' + esc(key) + '"' + (self.sessionSel[key] ? ' checked' : '') + ' aria-label="Select ' + esc(lab) + '">') +
                '</td><td class="n" title="' + esc(s.id) + '">' + esc(lab) +
                '<details class="f1x-det"><summary>Details</summary><div>Name: ' + esc(s.id) + (s.archived ? ' · archived' : '') + '</div></details></td>' +
                '<td class="s"><span class="f1x-badge ' + b.cls + '" title="' + esc(b.tip) + '">' + esc(b.text) + '</span>' +
                '<span class="f1x-size">' + esc(bytesText(s.bytes)) + '</span></td>' +
                '<td class="a"><button class="f1x-link" data-act="load" data-rec="' + esc(r.id) + '" data-id="' + esc(s.id) + '"' +
                (s.review_id ? ' data-rid="' + esc(s.review_id) + '" data-name="' + esc(recTitle(r.id) + ' - ' + lab) + '"' : '') +
                (open ? ' title="Recording now: it can be viewed after the session ends"' : '') + '>Open</button>' +
                (s.review_id && !open ? ' <a class="tv-btn primary f1x-watch" href="/?session=' + esc(s.review_id) + '&page=replay" title="Watch this recording on the Replay page">Watch</a>' : '') +
                (open ? '' : ' <button class="f1x-link" data-act="del-sess" data-rec="' + esc(r.id) + '" data-id="' + esc(s.id) + '" data-bytes="' + (+s.bytes || 0) + '"' +
                    (isOn ? ' disabled title="Back to live first"' : ' title="Move this session to the Windows Recycle Bin"') + '>Delete...</button>') +
                '</td></tr>';
        });
        (r.damaged || []).concat(r.partial || []).forEach(function (f) {
            var dmg = f.format === 'damaged', fb = stateBadge(dmg ? 'damaged' : 'archive_interrupted');
            h += '<tr><td class="c"></td><td class="n" title="' + esc(f.file) + '">' + (dmg ? 'Damaged file' : 'Interrupted archive') +
                '<details class="f1x-det"><summary>Details</summary><div>Name: ' + esc(f.file) + '</div></details></td>' +
                '<td class="s"><span class="f1x-badge ' + fb.cls + '" title="' + esc(fb.tip) + '">' + esc(fb.text) + '</span>' +
                '<span class="f1x-size">' + esc(bytesText(f.bytes)) + '</span></td>' +
                '<td class="a"><button class="f1x-link" data-act="del-file" data-rec="' + esc(r.id) + '" data-id="' + esc(f.file) + '" data-bytes="' + (+f.bytes || 0) +
                '" title="Move this file to the Windows Recycle Bin"' + (isOn ? ' disabled' : '') + '>Delete...</button></td></tr>';
        });
        return h + '</tbody></table></section>';
    };
    /* Selected keys ("recording/session") that still exist and are not the open session. */
    Console.prototype.selectedKeys = function () {
        var self = this, out = [], d = this.sessionsDoc;
        if (!d) return out;
        (d.recordings || []).forEach(function (r) {
            (r.sessions || []).forEach(function (s) {
                var k = r.id + '/' + s.id;
                if (self.sessionSel[k] && s.state !== 'open') out.push(k);
            });
        });
        return out;
    };
    Console.prototype.keyState = function (key) {
        var st = '', d = this.sessionsDoc;
        ((d && d.recordings) || []).forEach(function (r) {
            (r.sessions || []).forEach(function (s) { if (r.id + '/' + s.id === key) st = s.state; });
        });
        return st;
    };
    /* Several sessions at once: one dialog naming the count and size, then one delete request per session (each a job). */
    Console.prototype.deleteSelected = function (keys) {
        var self = this, size = 0;
        var items = keys.map(function (k) {
            var i = k.indexOf('/'), b = self.recordingBytes(k);
            size += b;
            return { key: k, rec: k.slice(0, i), id: k.slice(i + 1), bytes: b };
        });
        if (!items.length) return;
        this.dialog({
            title: 'Move to the Windows Recycle Bin',
            html: '<p>Move ' + items.length + ' session(s) (' + esc(bytesText(size)) + ') to the Windows Recycle Bin? You can restore it from there.</p>',
            buttons: [{ label: 'Move to Windows Recycle Bin', value: 'delete' }, { label: 'Cancel', value: 'cancel' }],
            safe: 'cancel'
        }).then(function (r) {
            if (!r) return;
            self.setNotice(null);
            var results = [];
            items.reduce(function (p, x) {
                return p.then(function () {
                    return self.recordingPost('delete', { target: 'session', recording: x.rec, id: x.id, confirm_bytes: x.bytes })
                        .then(function (res) { results.push({ x: x, res: res }); });
                });
            }, Promise.resolve()).then(function () {
                var queued = 0, why = '';
                self.gone = self.gone || {};
                self.pendingJobs = self.pendingJobs || {};
                self.jobNames = self.jobNames || {};
                results.forEach(function (o) {
                    if (o.res.ok) {
                        queued++;
                        if (o.res.j && o.res.j.job) {
                            self.pendingJobs[o.res.j.job] = { keys: [o.x.key], rec: o.x.rec };
                            self.jobNames[o.res.j.job] = o.x.rec;
                        }
                    } else if (o.res.code === 404) self.gone[o.x.key] = true;
                    else if (!why) why = o.res.j && o.res.j.error ? o.res.j.error : 'Refused. Nothing was changed.';
                });
                self.sessionSel = {};
                self._sessSig = null;
                if (why) self.setNotice('Queued ' + queued + ' of ' + items.length + '. Refused: ' + why, true);
                else self.setNotice('Delete queued for ' + queued + ' session(s). Files go to the Windows Recycle Bin; the list updates when it is done.', false);
                self.pollSessions();
            });
        });
    };

    Console.prototype.view_weather = function (host) {
        if (this.review) { host.appendChild(el('div', 'f1x-note', 'Weather is not recorded in this recording.')); return; }
        var g = this.grid(host, 'minmax(0,0.8fr) minmax(0,1.25fr) minmax(0,0.95fr)', 'minmax(0,1fr) minmax(0,1fr)');
        function at(pn, col, row) { pn.style.gridColumn = col; pn.style.gridRow = row; g.appendChild(pn); return pn; }
        var now = at(hpanel('Conditions'), '1', '1');
        var out = at(hpanel('Outlook'), '1', '2');
        var map = at(hpanel('Track Map'), '2', '1 / 3');
        var fc = at(hpanel('Forecast'), '3', '1');
        var tr = at(hpanel('Trends'), '3', '2');
        this.weatherPaneNow(now);
        this.weatherPaneOutlook(out);
        this.weatherPaneMap(map);
        this.weatherPaneForecast(fc);
        this.weatherPaneHistory(tr);
    };

    Console.prototype.weatherPaneNow = function (p) {
        var self = this, box = el('div', 'f1x-kv');
        p.body.classList.add('pad'); p.body.style.overflow = 'auto'; p.body.style.paddingTop = '6px';
        p.body.appendChild(box);
        this.refreshers.push(function () {
            var d = self.frame;
            if (!d) { box.innerHTML = '<div class="w" style="color:#5f7fc4">No session.</div>'; return; }
            var n = weatherNow(d, null);
            p.head.querySelector('.r').textContent = sessName(d.ty) + (finite(d.st) ? ' · ' + wxClock(d.st) : '');
            var fcst = forecastOf(self.health), fmin = finite(d.st) ? d.st / 60 : null;
            var fnow = fcst ? wxForecastAt(fcst, fmin) : null;
            var fx2 = fcst ? wxForecast(fcst, fmin, wxForecastKey(d, fcst)) : null;
            var wet = (n.onwPct || 0) > 0 || (n.offwPct || 0) > 0;
            var h = '<div class="w" style="text-align:center;margin-bottom:6px">' + (fnow ? wxSkyTag(wxSky(fnow.cond)) + ' ' : '') +
                '<span class="f1x-ts" style="background:' + (wet ? '#2f6fe0;color:#fff' : '#3ce07a') + '">' + (wet ? 'WET TRACK' : 'DRY TRACK') + '</span></div>';
            function kv(k, v) { h += '<div class="k">' + k + '</div><div class="v">' + v + '</div>'; }
            kv('AIR', fx(n.at, 1, ' °C'));
            kv('TRACK', fx(n.trkt, 1, ' °C') + (n.delta === null ? '' : ' <span style="color:#5f7fc4">(' + (n.delta > 0 ? '+' : '') + n.delta.toFixed(1) + ' vs air)</span>'));
            kv('RACING LINE', wxBar(n.onwPct, '#2f8bff') + ' ' + (n.onwPct === null ? '-' : n.onwPct.toFixed(0) + '% wet'));
            kv('OFF LINE', wxBar(n.offwPct, '#2f8bff') + ' ' + (n.offwPct === null ? '-' : n.offwPct.toFixed(0) + '% wet'));
            if (fnow) {
                kv('SKY NOW', esc(wxSky(fnow.cond)));
                if (fx2 && fx2.blocks.length) {
                    var b0 = fx2.blocks[0], b1 = fx2.blocks[1] || b0;
                    var peak = fx2.blocks.reduce(function (m, b) { return b.rain > m.rain ? b : m; }, b0);
                    kv('RAIN NEXT 15 MIN', '<span style="color:' + (b0.rain >= 40 ? '#7fd4ff' : '#dfe7ff') + '">' + b0.rain + '%</span> <span style="color:#5f7fc4">' + wxRainWord(b0.rain) + '</span>');
                    kv('RAIN 15-30 MIN', '<span style="color:' + (b1.rain >= 40 ? '#7fd4ff' : '#dfe7ff') + '">' + b1.rain + '%</span> <span style="color:#5f7fc4">' + wxRainWord(b1.rain) + '</span>');
                    kv('OUTLOOK', peak.rain >= 40 ? 'rain ' + wxRainWord(peak.rain) + ' around +' + peak.from + '-' + peak.to + ' min' : 'mostly dry in the coming ' + fx2.blocks[fx2.blocks.length - 1].to + ' min');
                    kv('FORECAST ISSUED', 'session ' + wxClock(fx2.issued * 60) + ' <span style="color:#5f7fc4">· updates every 10 min · may be wrong</span>');
                }
            } else {
                kv('FORECAST', '<span style="color:#5f7fc4">not available</span>');
            }
            box.innerHTML = h;
        });
    };

    /* Forecast table: time blocks from now, most likely sky, rain chance, air
     * range. Never the game's exact timeline (rc.16: no spoilers). */
    Console.prototype.weatherPaneOutlook = function (p) {
        var self = this, box = el('div', 'f1x-scroll'); box.style.top = '0';
        p.body.appendChild(box);
        this.refreshers.push(function () {
            var d = self.frame, f = forecastOf(self.health), fmin = d && finite(d.st) ? d.st / 60 : 0;
            if (!f) {
                p.head.querySelector('.r').textContent = '';
                var msg = '<div class="f1x-note" style="padding:6px">No forecast for this session. A forecast exists when Changing Weather is set to Selected Season.</div>';
                if (box._sig !== msg) { box._sig = msg; box.innerHTML = msg; }
                return;
            }
            var fx2 = wxForecast(f, fmin, wxForecastKey(d, f));
            var sig = JSON.stringify(fx2);
            if (box._sig === sig) return;
            box._sig = sig;
            p.head.querySelector('.r').textContent = 'issued ' + wxClock(fx2.issued * 60);
            var h = '<table class="f1x-tw"><thead><tr><th class="l">Time from now</th><th class="l">Most likely</th><th>Rain chance</th><th>Air</th></tr></thead><tbody>';
            if (!fx2.blocks.length) h += '<tr><td class="l" colspan="4">No forecast beyond this point of the session.</td></tr>';
            fx2.blocks.forEach(function (b) {
                h += '<tr><td class="l">+' + b.from + ' to +' + b.to + ' min</td><td class="l">' + wxSkyTag(b.sky) + '</td><td>' +
                    '<span class="f1x-wxbar" style="width:44px"><i style="width:' + b.rain + '%;background:#2f8bff"></i></span>' + b.rain + '%</td><td>' +
                    b.tLo + '-' + b.tHi + ' °C</td></tr>';
            });
            h += '</tbody></table><div class="f1x-note" style="padding:2px 6px">A forecast, not a promise: it gets less certain further ahead and is revised every 10 minutes.</div>';
            box.innerHTML = h;
        });
    };

    Console.prototype.weatherPaneMap = function (p) {
        var self = this, cv = el('canvas');
        p.body.appendChild(cv);
        var st = this.wxMapState || (this.wxMapState = { zoom: 1, panX: 0, panY: 0, follow: false });
        cv.addEventListener('wheel', function (e) {   /* zoom about the cursor */
            e.preventDefault();
            var rc = cv.getBoundingClientRect(), mx = e.clientX - rc.left - rc.width / 2, my = e.clientY - rc.top - rc.height / 2;
            var z0 = st.zoom, z1 = Math.max(0.5, Math.min(40, z0 * (e.deltaY < 0 ? 1.2 : 1 / 1.2)));
            if (st.follow) { mx = 0; my = 0; }
            st.panX = mx - (mx - st.panX) * z1 / z0; st.panY = my - (my - st.panY) * z1 / z0; st.zoom = z1;
            obj.draw();
        }, { passive: false });
        var drag = null;
        cv.addEventListener('mousedown', function (e) { drag = { x: e.clientX, y: e.clientY, px: st.panX, py: st.panY, moved: false }; e.preventDefault(); });
        window.addEventListener('mousemove', function (e) {
            if (!drag || !cv.isConnected) return;
            var dx = e.clientX - drag.x, dy = e.clientY - drag.y;
            if (Math.abs(dx) > 3 || Math.abs(dy) > 3) { drag.moved = true; st.follow = false; st.panX = drag.px + dx; st.panY = drag.py + dy; obj.draw(); }
        });
        window.addEventListener('mouseup', function (e) {
            if (drag && !drag.moved && e.target === cv) { st.follow = !st.follow; if (st.follow && st.zoom < 4) st.zoom = 6; obj.draw(); }
            drag = null;
        });
        cv.addEventListener('dblclick', function () { st.zoom = 1; st.panX = 0; st.panY = 0; st.follow = false; obj.draw(); });

        var obj = { draw: function () {
            var s = setupCanvas(cv), ctx = s.ctx;
            var pts = [];
            (self.waypoints || []).forEach(function (q) {
                var x = Array.isArray(q) ? q[0] : q.x, z = Array.isArray(q) ? q[1] : q.z;
                if (finite(x) && finite(z)) pts.push([x, z]);
            });
            var lines = wxTrackLines(self.trackDoc);
            var minx = Infinity, maxx = -Infinity, minz = Infinity, maxz = -Infinity;
            function acc(a) { if (a[0] < minx) minx = a[0]; if (a[0] > maxx) maxx = a[0]; if (a[1] < minz) minz = a[1]; if (a[1] > maxz) maxz = a[1]; }
            pts.forEach(acc);
            ctx.font = '10px ' + PAL.sans;
            if (!finite(minx)) {
                ctx.fillStyle = PAL.axis; ctx.textAlign = 'center';
                ctx.fillText('Track map appears when the game is in a session.', s.w / 2, s.h / 2);
                return;
            }
            if (maxx - minx < 100) { var ex = (100 - (maxx - minx)) / 2; minx -= ex; maxx += ex; }
            if (maxz - minz < 100) { var ez = (100 - (maxz - minz)) / 2; minz -= ez; maxz += ez; }
            var d = self.frame, on = d && finite(d.onw) ? d.onw : 0, off = d && finite(d.offw) ? d.offw : 0;
            var pad = 22, top = 18;
            var sc = Math.min((s.w - 2 * pad) / (maxx - minx), (s.h - 2 * pad - top) / (maxz - minz));
            var ox = (s.w - (maxx - minx) * sc) / 2, oz = (s.h - top - (maxz - minz) * sc) / 2;
            var pxm = sc * st.zoom;
            var fcar = self.cars().filter(function (q) { return q.id === self.focusId; })[0];
            if (st.follow && fcar && (Math.abs(fcar.mx || 0) > 0.01 || Math.abs(fcar.mz || 0) > 0.01)) {
                st.panX = -(ox + (fcar.mx - minx) * sc - s.w / 2) * st.zoom;
                st.panY = -(s.h - (oz + (fcar.mz - minz) * sc) - s.h / 2) * st.zoom;
            }
            function view(x, z) {
                var u = ox + (x - minx) * sc, v = s.h - (oz + (z - minz) * sc);
                return [s.w / 2 + (u - s.w / 2) * st.zoom + st.panX, s.h / 2 + (v - s.h / 2) * st.zoom + st.panY];
            }
            function stroke(list, style, width) {
                ctx.strokeStyle = style; ctx.lineWidth = width; ctx.lineJoin = 'round'; ctx.lineCap = 'round';
                ctx.beginPath();
                list.forEach(function (a, k) { var v = view(a[0], a[1]); if (k) ctx.lineTo(v[0], v[1]); else ctx.moveTo(v[0], v[1]); });
                ctx.closePath(); ctx.stroke();
            }
            /* track surface = off-line wetness; racing line band = racing-line wetness */
            stroke(pts, wxMapWet(off), Math.max(8, 14 * pxm));
            if (lines) stroke(lines.wl, wxMapWet(on), Math.max(3, 3 * pxm));
            var s0 = view(pts[0][0], pts[0][1]); ctx.fillStyle = PAL.start; ctx.fillRect(s0[0] - 1.5, s0[1] - 6, 3, 12);
            self.cars().filter(function (q) { return q.ot2 === undefined || q.ot2; }).slice().reverse().forEach(function (q) {
                if (!(Math.abs(q.mx || 0) > 0.01 || Math.abs(q.mz || 0) > 0.01)) return;
                var v = view(q.mx, q.mz), foc = q.id === self.focusId, r = foc ? 6 : 4;
                var col = q.p <= 3 ? POSCOL[q.p - 1] : q.cr === 0 ? '#ffd84a' : '#c9d4f2';
                if (q.fs === 2 || q.fs === 3 || q.rt) col = '#55607a';
                ctx.fillStyle = col; ctx.beginPath(); ctx.arc(v[0], v[1], r, 0, Math.PI * 2); ctx.fill();
                ctx.strokeStyle = foc ? '#fff' : '#000'; ctx.lineWidth = foc ? 2 : 1; ctx.stroke();
                if (foc || q.p <= 3) {
                    ctx.fillStyle = '#9fb2e6'; ctx.font = (foc ? 'bold ' : '') + '9px ' + PAL.sans; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom';
                    ctx.fillText(code3(q.n) + (humanRole(q) ? ' ' + humanRole(q) : ''), v[0] + r + 2, v[1] - 2);   /* TAPE2 V1 */
                }
            });
            ctx.textBaseline = 'middle'; ctx.textAlign = 'left'; ctx.font = '9px ' + PAL.sans;
            ctx.fillStyle = '#000'; ctx.fillRect(0, 0, s.w, top);
            var lx = 8;
            [[wxMapWet(on), 'racing line ' + (on * 100).toFixed(0) + '% wet'], [wxMapWet(off), 'off line ' + (off * 100).toFixed(0) + '% wet']].forEach(function (it) {
                ctx.fillStyle = it[0]; ctx.fillRect(lx, 5, 16, 8);
                ctx.fillStyle = '#9fb2e6'; ctx.fillText(it[1], lx + 20, 9);
                lx += 20 + ctx.measureText(it[1]).width + 16;
            });
            ctx.textBaseline = 'alphabetic'; ctx.fillStyle = PAL.axis;
            ctx.fillText(st.follow ? 'following ' + (fcar ? code3(fcar.n) : 'focus car') + ' · click to stop' : 'click: follow focus car · wheel zoom · drag pan · double-click reset', 6, s.h - 5);
        } };
        obj.isMap = true;   /* TAPE2 V4: the Weather map is a map (no inset) */
        this.canvases.push(obj);
    };

    /* Rain chance per block (bars) and the air temperature range (band). */
    Console.prototype.weatherPaneForecast = function (p) {
        var self = this;
        p.body.style.display = 'flex'; p.body.style.flexDirection = 'column';
        var top = el('div', 'f1x-note'); top.style.margin = '3px 6px'; top.style.flex = '0 0 auto';
        var box = el('div', 'f1x-wxbox'); box.style.flex = '1 1 auto'; box.style.minHeight = '90px';
        var cv = el('canvas'); box.appendChild(cv);
        p.body.appendChild(top); p.body.appendChild(box);
        var obj = { draw: function () {
            var f = forecastOf(self.health), d = self.frame;
            if (!f || !d || !finite(d.st)) return;
            var fx2 = wxForecast(f, d.st / 60, wxForecastKey(d, f));
            if (!fx2 || !fx2.blocks.length) return;
            var s = setupCanvas(cv), ctx = s.ctx, bl = fx2.blocks, t1 = bl[bl.length - 1].to;
            var tmin = Infinity, tmax = -Infinity;
            bl.forEach(function (b) { tmin = Math.min(tmin, b.tLo); tmax = Math.max(tmax, b.tHi); });
            tmin -= 1; tmax += 1;
            var pl = 30, pr = 30, pt = 14, pb = 14, pw = Math.max(10, s.w - pl - pr), ph = Math.max(8, s.h - pt - pb);
            function gx(m) { return pl + m / t1 * pw; }
            function gy(v) { return pt + (1 - v / 100) * ph; }
            function gt(v) { return pt + (1 - (v - tmin) / (tmax - tmin)) * ph; }
            ctx.font = '9px ' + PAL.sans; ctx.textBaseline = 'alphabetic';
            ctx.strokeStyle = PAL.grid; ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(pl, pt); ctx.lineTo(pl, pt + ph); ctx.lineTo(pl + pw, pt + ph); ctx.lineTo(pl + pw, pt); ctx.stroke();
            ctx.save(); ctx.setLineDash([2, 3]); ctx.beginPath(); ctx.moveTo(pl, gy(50)); ctx.lineTo(pl + pw, gy(50)); ctx.stroke(); ctx.restore();
            bl.forEach(function (b) {
                var x0 = gx(b.from) + 2, x1 = gx(b.to) - 2;
                ctx.fillStyle = 'rgba(47,139,255,' + (0.3 + 0.6 * b.rain / 100) + ')';
                ctx.fillRect(x0, gy(b.rain), Math.max(1, x1 - x0), gy(0) - gy(b.rain));
                ctx.fillStyle = '#dfe7ff'; ctx.textAlign = 'center';
                ctx.fillText(b.rain + '%', (x0 + x1) / 2, Math.min(gy(b.rain) - 3, gy(0) - 3));
                /* air temperature: range whisker with a mid tick, on the right axis */
                var xm = (x0 + x1) / 2, ya = gt(b.tHi), yb = gt(b.tLo), ym = gt((b.tLo + b.tHi) / 2);
                ctx.strokeStyle = '#ff9c40'; ctx.lineWidth = 1.5;
                ctx.beginPath(); ctx.moveTo(xm, ya); ctx.lineTo(xm, yb); ctx.moveTo(xm - 4, ya); ctx.lineTo(xm + 4, ya); ctx.moveTo(xm - 4, yb); ctx.lineTo(xm + 4, yb); ctx.stroke();
                ctx.fillStyle = '#ff9c40'; ctx.fillRect(xm - 2.5, ym - 1, 5, 2);
            });
            ctx.fillStyle = '#7fd4ff'; ctx.textAlign = 'left'; ctx.fillText('rain chance %', pl, 10);
            ctx.fillStyle = '#ff9c40'; ctx.textAlign = 'right'; ctx.fillText('air °C range', pl + pw, 10);
            ctx.fillStyle = PAL.axis; ctx.textAlign = 'right'; ctx.fillText('100', pl - 4, pt + 7); ctx.fillText('0', pl - 4, pt + ph);
            ctx.textAlign = 'left'; ctx.fillText(String(tmax), pl + pw + 4, pt + 7); ctx.fillText(String(tmin), pl + pw + 4, pt + ph);
            ctx.fillText('now', pl, s.h - 2); ctx.textAlign = 'right'; ctx.fillText('+' + t1 + ' min', pl + pw, s.h - 2);
        } };
        this.canvases.push(obj);
        this.refreshers.push(function () {
            var f = forecastOf(self.health), d = self.frame;
            var fx2 = f && d && finite(d.st) ? wxForecast(f, d.st / 60, wxForecastKey(d, f)) : null;
            p.head.querySelector('.r').textContent = fx2 ? 'issued ' + wxClock(fx2.issued * 60) : 'not available';
            box.style.display = fx2 && fx2.blocks.length ? '' : 'none';
            var html = fx2 ? 'Rain chance and air temperature ahead. Less certain further out; revised every 10 minutes.' :
                'No forecast for this session (Changing Weather must be Selected Season).';
            if (top._h !== html) { top._h = html; top.textContent = html; }
        });
    };

    Console.prototype.weatherPaneHistory = function (p) {
        var self = this;
        p.body.style.display = 'flex'; p.body.style.flexDirection = 'column';
        function chart() {
            var b = el('div', 'f1x-wxbox'); b.style.flex = '1 1 0'; b.style.minHeight = '60px';
            var cv = el('canvas'); b.appendChild(cv); p.body.appendChild(b);
            return cv;
        }
        var c1 = chart(), c2 = chart();
        var obj = { draw: function () {
            var wh = weatherHistory(self.weather);
            wxChart(c1, wh, [{ v: wh.air, c: '#7fd4ff' }, { v: wh.track, c: '#ff9c40' }], 0, 60, [{ t: 'air °C', c: '#7fd4ff' }, { t: 'track °C', c: '#ff9c40' }]);
            wxChart(c2, wh, [{ v: wh.on, c: '#3ce07a' }, { v: wh.off, c: '#ffd84a' }], 0, 100, [{ t: 'racing line % wet', c: '#3ce07a' }, { t: 'off line % wet', c: '#ffd84a' }]);
        } };
        this.canvases.push(obj);
        this.refreshers.push(function () {
            var wh = weatherHistory(self.weather);
            p.head.querySelector('.r').textContent = wh.t.length ? 'session so far' : 'starts when the session runs';
        });
    };

    Console.prototype.view_live = function (host) {
        var g = this.grid(host, 'minmax(0,1fr) minmax(0,1fr)', 'minmax(0,1fr)');
        g.style.gridTemplateColumns = 'repeat(auto-fill,minmax(360px,1fr))';   /* 2 to 3 per row */
        g.style.gridTemplateRows = 'auto';
        g.style.alignItems = 'start';
        var self = this;
        this.liveGrid = g; this.liveSig = null;
        this.liveCards = {};
        this.updateLive();
    };

    Console.prototype.updateLive = function () {
        if (!this.liveCards) return;
        var j = this.frame, self = this;
        var sig = this.letters().join('');
        if (sig !== this.liveSig) {   /* side set changed: rebuild one card per side */
            this.liveSig = sig; this.liveGrid.innerHTML = ''; this.liveCards = {};
            this.letters().forEach(function (L) {
                var p = hpanel('Live gauges · ' + L);
                p.body.classList.add('pad');
                p.body.style.overflow = 'auto';
                self.liveGrid.appendChild(p);
                self.liveCards[L] = p;
            });
        }
        this.letters().forEach(function (L) {
            var p = self.liveCards[L], side = self.side(L);
            var car = j && side ? j.c.filter(function (c) { return c.id === side.slot; })[0] : null;
            if (!car) { p.body.innerHTML = '<div class="f1x-note">No live row for side ' + L + '. Pick a car in the ' + L + ' box above.</div>'; return; }
            var vf = car.vf || 0, vf6 = car.vf6 || 0;
            var kmh = vf & 1 ? car.s * 3.6 : null;
            var thr = vf & 2 ? car.th * 100 : null, brk = vf & 4 ? car.br * 100 : null;
            var lat = vf & 32 ? car.glat / G : null, lon = vf & 64 ? -car.glon / G : null;
            var rpm = finite(car.rpm) ? car.rpm : null, mr = finite(car.max_rpm) && car.max_rpm > 0 ? car.max_rpm : 19000;
            var gear = car.g > 0 ? String(car.g) : car.g === 0 ? 'N' : 'R';
            var rpmCol = function (f) { return f > 0.9 ? 'red' : f > 0.75 ? 'amb' : 'grn'; };
            var gx = lat !== null ? Math.max(4, Math.min(96, 50 - lat / 3 * 50)) : 50;
            var gy = lon !== null ? Math.max(4, Math.min(96, 50 - lon / 3 * 50)) : 50;
            var ttc = Array.isArray(car.ttc) ? car.ttc : null;
            var tShow = ttc && ((vf6 & 4) || ((vf6 & 256) && self.research));
            var tyres = '';
            WHEELS.forEach(function (w, k) {
                var t = tShow ? ttc[k] : null;
                tyres += lcdBox(w, t === null || t === undefined || !finite(t) ? '-' : Math.round(t) + '&deg;', 'sm');
            });
            var steer = steerLabel(car), ib = inputBars(car);   /* 1.2.2: CO-OP remote inputs */
            p.body.innerHTML = '<div class="f1x-lg">' +
                '<div class="f1x-lgrow">' +
                    lcdBox('SPEED km/h', kmh !== null ? kmh.toFixed(0) : '---', 'big') +
                    lcdBox('GEAR', gear, 'gear') +
                    lcdBox('RPM', rpm !== null ? WB.formatRpmValue(rpm) : '----', '') +
                '</div>' +
                segBar(rpm !== null ? Math.min(1, rpm / mr) : null, 30, rpmCol) +
                '<div class="f1x-lgrow2"><div class="f1x-lgbars">' +
                    '<div class="f1x-lbl" title="' + esc(ib.thrTitle) + '">' + ib.thrLabel + '</div>' + segBar(thr !== null ? thr / 100 : null, 24, function () { return 'grn'; }) +
                    '<div class="f1x-lbl" title="' + esc(ib.brkTitle) + '">' + ib.brkLabel + '</div>' + segBar(brk !== null ? brk / 100 : null, 24, function () { return 'red'; }) +
                '</div>' +
                '<div class="f1x-gg"><i class="ax h"></i><i class="ax v"></i><i class="ring"></i>' +
                    '<b style="left:' + gx.toFixed(1) + '%;top:' + gy.toFixed(1) + '%;background:' + COLORS[L] + '"></b><span>3 g</span></div>' +
                '</div>' +
                '<div class="f1x-lgrow">' + tyres + '</div>' +
                '<div class="f1x-note">P' + car.p + ' · lap ' + lapNo(car.l) + ' · ' + WB.roleOf(car) +
                    ' · Lat ' + fx(lat, 2, 'g') + ' ' + latWord(lat) + ' · Long ' + fx(lon, 2, 'g') + ' · Steer ' + steer +
                    ' · sample age ' + fx(j.sample_age_ms, 0, 'ms') + '</div>' +
                '</div>';
        });
    };

    /* 1.2.0 A1.6 (owner): page-styled dropdowns. Every visible <select> in the console gets a themed button and list;
     * the <select> stays in the DOM (hidden) as the single source of truth, so every existing onchange handler and
     * every .value read keeps working. Picking an item sets select.value and fires 'change'. The label follows
     * programmatic changes (mutation observer + 400 ms sync). Keyboard: Enter/Space/ArrowDown open, arrows move,
     * Enter picks, Escape closes. Design: Claude. */
    var DD_CSS = [
        '.f1x-dd{position:relative;display:inline-flex;vertical-align:middle;max-width:250px}',
        '.f1x-dd-b{display:inline-flex;align-items:center;gap:6px;height:22px;max-width:250px;padding:0 6px 0 8px;border:1px solid #2a43a8;background:#071066;color:#dfe7ff;font:11px var(--sans,Segoe UI,Arial,sans-serif);cursor:pointer;text-align:left;white-space:nowrap}',
        '.f1x-dd-b:hover{border-color:#38d6e8}',
        '.f1x-dd-b:focus{outline:1px solid #38d6e8;outline-offset:0}',
        '.f1x-dd-b[disabled]{opacity:.5;cursor:not-allowed}',
        '.f1x-dd-t{overflow:hidden;text-overflow:ellipsis;flex:1 1 auto;min-width:0}',
        '.f1x-dd-c{flex:0 0 auto;width:0;height:0;border-left:4px solid transparent;border-right:4px solid transparent;border-top:5px solid #7fd4ff}',
        '.f1x-dd-m{position:fixed;z-index:9999;min-width:160px;max-width:360px;max-height:60vh;overflow:auto;background:#040b26;border:1px solid #38d6e8;box-shadow:0 6px 18px rgba(0,0,0,.6);padding:2px 0;font:11px var(--sans,Segoe UI,Arial,sans-serif)}',
        '.f1x-dd-o{padding:4px 10px;color:#dfe7ff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;cursor:pointer}',
        '.f1x-dd-o.sel{background:#14307f;color:#fff;font-weight:bold}',
        '.f1x-dd-o.act{background:#1d3fa8;color:#fff}',
        '.f1x-dd-o.dis{color:#5f7fc4;cursor:default}',
        '.f1x-dd-g{padding:5px 10px 2px;color:#9fb2e6;font:bold 9px var(--sans,Segoe UI,Arial,sans-serif);letter-spacing:.06em;text-transform:uppercase}',
        '.f1x-bar .f1x-dd-b{height:19px;font-size:10px}'
    ].join('\n');

    function ddLabel(sel) {
        var o = sel.options && sel.selectedIndex >= 0 ? sel.options[sel.selectedIndex] : null;
        return o ? o.text : '';
    }

    var ddAll = [], ddTimer = null;

    function ddEnhance(sel) {
        if (sel.__dd || sel.multiple || sel.size > 1 || sel.getAttribute('data-native') === '1') return;
        if (sel.style.display === 'none' || !sel.parentNode) return;
        var wrap = document.createElement('span'); wrap.className = 'f1x-dd';
        var btn = document.createElement('button'); btn.type = 'button'; btn.className = 'f1x-dd-b';
        btn.setAttribute('aria-haspopup', 'listbox');
        var txt = document.createElement('span'); txt.className = 'f1x-dd-t';
        var car = document.createElement('span'); car.className = 'f1x-dd-c';
        btn.appendChild(txt); btn.appendChild(car); wrap.appendChild(btn);
        sel.parentNode.insertBefore(wrap, sel.nextSibling);
        sel.style.display = 'none';
        var menu = null, act = -1, items = [];
        var sync = function () {
            var t = ddLabel(sel);
            if (txt.textContent !== t) txt.textContent = t;
            var ti = sel.title || sel.getAttribute('aria-label') || '';
            if (btn.title !== ti) btn.title = ti;
            if (ti) btn.setAttribute('aria-label', ti);
            btn.disabled = !!sel.disabled;
            /* a select moved elsewhere by page code: keep the button next to it */
            if (sel.isConnected && sel.nextSibling !== wrap && sel.parentNode) sel.parentNode.insertBefore(wrap, sel.nextSibling);
            if (!sel.isConnected && wrap.parentNode) wrap.parentNode.removeChild(wrap);
        };
        var outside = function (e) { if (menu && !menu.contains(e.target) && !wrap.contains(e.target)) close(); };
        var close = function () {
            if (!menu) return;
            if (menu.parentNode) menu.parentNode.removeChild(menu);
            menu = null; items = []; act = -1;
            document.removeEventListener('mousedown', outside, true);
            btn.setAttribute('aria-expanded', 'false');
        };
        var mark = function (i) {
            items.forEach(function (it, k) { it.el.classList.toggle('act', k === i); });
            act = i;
            if (items[i] && items[i].el.scrollIntoView) items[i].el.scrollIntoView({ block: 'nearest' });
        };
        var pick = function (i) {
            var it = items[i];
            if (!it || it.dis) return;
            close();
            if (sel.value !== it.v) { sel.value = it.v; sel.dispatchEvent(new Event('change', { bubbles: true })); }
            sync(); btn.focus();
        };
        var open = function () {
            if (menu || sel.disabled) return;
            menu = document.createElement('div'); menu.className = 'f1x-dd-m'; menu.setAttribute('role', 'listbox');
            items = [];
            var add = function (o) {
                var d = document.createElement('div');
                d.className = 'f1x-dd-o' + (o.selected ? ' sel' : '') + (o.disabled ? ' dis' : '');
                d.setAttribute('role', 'option'); d.textContent = o.text; d.title = o.text;
                var k = items.length;
                d.onmousedown = function (e) { e.preventDefault(); pick(k); };
                d.onmousemove = function () { if (act !== k) mark(k); };
                items.push({ el: d, v: o.value, dis: o.disabled });
                menu.appendChild(d);
            };
            Array.prototype.forEach.call(sel.children, function (ch) {
                if (ch.tagName === 'OPTGROUP') {
                    var g = document.createElement('div'); g.className = 'f1x-dd-g'; g.textContent = ch.label; menu.appendChild(g);
                    Array.prototype.forEach.call(ch.children, add);
                } else if (ch.tagName === 'OPTION') add(ch);
            });
            document.body.appendChild(menu);
            var r = btn.getBoundingClientRect(), mh = menu.offsetHeight, vh = window.innerHeight;
            menu.style.minWidth = Math.max(160, r.width) + 'px';
            menu.style.left = Math.max(0, Math.min(r.left, window.innerWidth - menu.offsetWidth - 4)) + 'px';
            menu.style.top = (r.bottom + mh + 4 > vh && r.top > mh ? r.top - mh - 2 : r.bottom + 2) + 'px';
            btn.setAttribute('aria-expanded', 'true');
            document.addEventListener('mousedown', outside, true);
            mark(Math.max(0, sel.selectedIndex));
        };
        btn.onclick = function () { if (menu) close(); else open(); };
        btn.onkeydown = function (e) {
            var k = e.key;
            if (!menu) {
                if (k === 'ArrowDown' || k === 'Enter' || k === ' ') { e.preventDefault(); open(); }
                return;
            }
            if (k === 'Escape') { e.preventDefault(); close(); }
            else if (k === 'ArrowDown') { e.preventDefault(); mark(Math.min(items.length - 1, act + 1)); }
            else if (k === 'ArrowUp') { e.preventDefault(); mark(Math.max(0, act - 1)); }
            else if (k === 'Enter' || k === ' ') { e.preventDefault(); pick(act); }
            else if (k === 'Tab') close();
        };
        btn.onblur = function () { setTimeout(function () { if (menu && !menu.contains(document.activeElement)) close(); }, 150); };
        sel.addEventListener('change', sync);
        if (typeof MutationObserver === 'function')
            new MutationObserver(sync).observe(sel, { childList: true, subtree: true, attributes: true, characterData: true });
        sel.__dd = { sync: sync, close: close };
        ddAll.push(sel);
        sync();
    }

    function ddAttach(root) {
        if (typeof document === 'undefined' || !root || !root.querySelectorAll) return;
        if (!document.getElementById('f1x-dd-css')) {
            var st = document.createElement('style'); st.id = 'f1x-dd-css'; st.textContent = DD_CSS;
            document.head.appendChild(st);
        }
        var scan = function () { Array.prototype.forEach.call(root.querySelectorAll('select'), ddEnhance); };
        scan();
        if (typeof MutationObserver === 'function') {
            var pend = false;
            new MutationObserver(function () {
                if (pend) return;
                pend = true;
                setTimeout(function () { pend = false; scan(); }, 0);
            }).observe(root, { childList: true, subtree: true });
        }
        if (!ddTimer) ddTimer = setInterval(function () {
            ddAll = ddAll.filter(function (s) { s.__dd.sync(); return s.isConnected; });
        }, 400);
    }

    function mountConsole(root, opts) { var c = new Console(root, opts); ddAttach(root); return c; }

    function mount(root, opts) { return new Viewer(root, opts); }

    return {
        VERSION: VERSION,
        PRODUCT_VERSION: PRODUCT_VERSION,
        inputBars: inputBars, steerLabel: steerLabel,
        latWord: latWord, wetText: wetText, tyreName: tyreName, tyreTitle: tyreTitle,
        mfStatus: mfStatus, forecastOf: forecastOf, blueFlags: blueFlags, blueTagOf: blueTagOf,
        weatherHistory: weatherHistory, weatherNow: weatherNow, weatherForecastInfo: weatherForecastInfo, wxClock: wxClock,
        wxTrackLines: wxTrackLines, wxWetColor: wxWetColor, wxGripRefs: wxGripRefs,
        wxGdb: wxGdb, wxModelRefs: wxModelRefs,
        wxForecastAt: wxForecastAt, wxSky: wxSky, wxNextRainChange: wxNextRainChange, wxProjected: wxProjected, wxForecast: wxForecast, wxNormCdf: wxNormCdf, wxRainWord: wxRainWord,
        wxProfileFrac: wxProfileFrac, wxSpotWetness: wxSpotWetness, wxDistToLine: wxDistToLine, wxTrailColor: wxTrailColor, wxGarage: wxGarage, wxMapWet: wxMapWet,
        canonicalLegacy: canonicalLegacy, layoutFileFor: layoutFileFor, smoothPair: SMOOTH_PAIR,
        s2Of: s2Of, s3Of: s3Of, bestS2Of: bestS2Of, lapNo: lapNo,
        tvdLines: tvdLines, TVD_FIXED: TVD_FIXED, tvdAngleText: tvdAngleText, tvdCircuit: tvdCircuit, tvdAngleKey: tvdAngleKey, tvdPitText: tvdPitText, tvdDrivers: tvdDrivers, tvdKeyAction: tvdKeyAction, tvdBody: tvdBody, tvdResultText: tvdResultText, TVD_CSS: TVD_CSS,
        tvdGapText: tvdGapText, tvdCutBlock: tvdCutBlock, tvdVerifyText: tvdVerifyText, TVD_DASH: TVD_DASH,
        tvdPositionValue: tvdPositionValue, tvdHotkeyAllowed: tvdHotkeyAllowed,
        COLORS: COLORS,
        derive: derive,
        valueAt: valueAt,
        brakingZones: brakingZones,
        cornerMetrics: cornerMetrics,
        ggEnvelope: ggEnvelope,
        lapStats: lapStats,
        lapWear: lapWear,
        correlate: correlate,
        miniSectors: miniSectors,
        sectorTimes: sectorTimes,
        exportDoc: exportDoc,
        officialLapPick: officialLapPick,
        lapShownTime: lapShownTime,
        partialLapTitle: partialLapTitle,
        layoutSpecText: layoutSpecText,
        teamOf: teamOf,
        exportFileName: exportFileName,
        lapsetCsvHeader: lapsetCsvHeader,
        pickLaps: pickLaps,
        joinOfficial: joinOfficial,
        lapsetCsv: lapsetCsv,
        lapsetJson: lapsetJson,
        lapProblem: lapProblem,
        tempColor: tempColor,
        Viewer: Viewer,
        Console: Console,
        setPalette: setPalette,
        sessName: sessName,
        rcMsg: rcMsg,
        reasonTxt: reasonTxt,
        nearestOnScreen: nearestOnScreen,
        gapTo: gapTo,
        mount: mount,
        mountConsole: mountConsole,
        reviewUrl: reviewUrl,
        reviewTitle: reviewTitle,
        bytesText: bytesText,
        recTitle: recTitle, recParts: recParts, sessionLabel: sessionLabel, recShortTitle: recShortTitle,
        lapCountText: lapCountText, headerTrackText: headerTrackText, trackDisplayName: trackDisplayName, statusNoticeText: statusNoticeText,
        pastBannerText: pastBannerText, pastShortName: pastShortName, slotSourceText: slotSourceText,
        recModeKey: recModeKey, recMatches: recMatches, sessionRank: sessionRank, recKey: recKey,
        recSummary: recSummary, stateBadge: stateBadge, noticeHideMs: noticeHideMs, liveLineText: liveLineText,
        NOTICE_HIDE_MS: NOTICE_HIDE_MS,
        /* 1.2.2 Replay page (pure helpers) */
        RP_SPAN: RP_SPAN, RP_LEAD: RP_LEAD, RP_GAP: RP_GAP, RP_FLIGHT: RP_FLIGHT, RP_SPEEDS: RP_SPEEDS,
        rpWindowIndex: rpWindowIndex, rpWindowRange: rpWindowRange, rpPlanFetch: rpPlanFetch, rpRowsFor: rpRowsFor,
        rpCarAt: rpCarAt, rpOutlineLength: rpOutlineLength, rpProgress: rpProgress, rpGapText: rpGapText,
        rpGapLabel: rpGapLabel, rpTowerOrder: rpTowerOrder, rpDisplayName: rpDisplayName, rpShortName: rpShortName,
        rpClock: rpClock, rpLastLap: rpLastLap, rpMarkKind: rpMarkKind, rpMarkText: rpMarkText, rpColor: rpColor,
        rpInfoFrom: rpInfoFrom, REPLAY_CSS: REPLAY_CSS,
        TAPE_TICK_MS: TAPE_TICK_MS, TAPE_SEEK_FWD_MAX: TAPE_SEEK_FWD_MAX, tapeStep: tapeStep, tapeSeekResets: tapeSeekResets,
        tapeShowsAt: tapeShowsAt, tapeFrameUrl: tapeFrameUrl, tapeCannotPlayText: tapeCannotPlayText, tapeFetchDue: tapeFetchDue,
        humanRole: humanRole, humanBadgeHtml: humanBadgeHtml, humanBadgeText: humanBadgeText, lapLabel: lapLabel,
        frameMapPos: frameMapPos, rpHumanCar: rpHumanCar, rpTowerNameHtml: rpTowerNameHtml, coopSummary: coopSummary
    };
});
