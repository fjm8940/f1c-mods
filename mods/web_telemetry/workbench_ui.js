/* workbench_ui.js — Web Telemetry workbench model + legacy renderer (4.4.0-rc.1).
 *
 * Canonical sidecar beside web_telemetry.dll, served read-only at
 * /workbench.js and mounted by the product page at /workbench. It consumes
 * the existing read-only surfaces (/telemetry JSON via SSE, /track,
 * /trace/export?fmt=json, /trace/open, /trace/import) and never writes game
 * state. The STOP 4 review page under legacy_dossier/stop4_draft/ loads this
 * same file with a synthetic feed for offline review.
 *
 * Design rules implemented here (STOP 3R section 6, all accepted):
 *  - A/B sides select the player or any AI; identity labels and fixed colors.
 *  - Default Chassis Overview: track/driving line, speed vs distance,
 *    G-G scatter, RPM vs distance; Inputs view: throttle/brake/gear/steer
 *    (steering only where valid).
 *  - Live partial-lap drawing with segment breaks; no lines across gaps.
 *  - LIVE / PARTIAL / COMPLETED / FROZEN states; freeze pins display without
 *    stopping capture; resume rebinds to newest data.
 *  - Lap crossing keeps history and the previous completed lap as reference.
 *  - Linked distance cursor across charts and the track map; the cursor delta
 *    between A and B uses the viewer's own distance->time interpolation
 *    (PCODE_SEMANTICS_2026-10-09 section 2c, Data_management #14).
 *  - Zoom/pan/reset per chart and map; explicit missing-data reasons.
 *
 * Old-viewer parity (2026-10-09, FRONTEND_PARITY_2026-10-09.md):
 *  - Full-screen layout engine driven by the 23 legacy f1_telemetry layout
 *    INIs (embedded below by gen_workbench_layouts.py; grammar in
 *    legacy_dossier/LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md). Default layout is
 *    Chassis Overview (2x2: track map, G-G, velocity, engine rpm).
 *  - The legacy "time" x channel is drawn against lap distance (m) by default
 *    (owner screenshots); a toggle switches to lap time (s).
 *  - Channels without a live or import source draw an "unavailable" panel
 *    with a plain-language reason; nothing is invented.
 *  - Lap strip per side (open lap, completed laps with times, imports,
 *    fastest full lap highlighted, reference pick); dtime is the viewer's
 *    incremental time difference against the reference lap.
 *
 * RPM: the live /telemetry field `c.r` (and `c.mr`) is engine angular speed
 * in rad/s (finding engine-rpm-live-scale-20261009). radPerSecToRpm() applies
 * x 9.549296585513721 and every "RPM" label here goes through formatRpm();
 * the raw value is never labeled RPM. A converted trace channel (`rpm`, from
 * /trace/data, /trace/open or a legacy import) is already display RPM and
 * must not be converted again.
 */
(function (root, factory) {
    if (typeof module === 'object' && module.exports) module.exports = factory();
    else root.Stop4Workbench = factory();
})(typeof self !== 'undefined' ? self : this, function () {
    'use strict';

    var RPM_FACTOR = 9.549296585513721; /* engine-rpm-live-scale-20261009 */

    /* Budgets mirror the server-side bounds (STOP 3R sections 5.1-5.2) and are
     * asserted by the STOP 4 test plan. Remote clients pay ~310 KB/s each at
     * the measured 31,016 B /telemetry frame and 10 Hz SSE (22 cars). */
    /* C1 (2026-10-14): a completed lap waits this many sim seconds for the game's
     * last-lap time (frame field ll) before it is recorded as official_time null. */
    var OFFICIAL_WAIT_S = 8;

    var LIMITS = {
        livePointsPerCar: 3200,     /* mirrors the server TRACE_SAMPLES ring (4.4.0 all-cars) */
        drawnPoints: 1200,          /* drawn polyline subsample cap */
        completedLapsKept: 6,       /* TRACE_LAPS */
        historyPoints: 262144,      /* global browser budget across all 104 slots */
        tracedCars: 2,              /* TRACE_CARS */
        sseHz: 10,                  /* SSE_UPDATE_MS = 100 */
        captureHz: 20,              /* design target */
        telemetryResponseBytes: 262144,
        traceResponseBytes: 1572864,
        compareMaxPoints: 1000,
        trackMaxPoints: 3000,
        clientTrackPoints: 500,     /* live driving-line ring in the page */
        maxSseClients: 16,
        remoteBytesPerSecMeasured: 31016 * 10, /* 310,160 B/s at 22 cars */
        remoteBitsPerSecMeasured: 31016 * 10 * 8 /* ~2.48 Mbit/s per client */
    };

    /* /telemetry vf validity bits: 1 speed, 2 throttle, 4 brake, 8 steering
     * (AI only), 16 tread, 32 lateral acceleration, 64 longitudinal acceleration. */
    var VF = { SPEED: 1, THROTTLE: 2, BRAKE: 4, STEER: 8, TREAD: 16, ACCEL: 32, GLON: 64 };

    /* f1c.trace/1 sample flags (TRF_* in web_telemetry.c). */
    var TRF = {
        DIST: 1, SPEED: 2, THROTTLE: 4, BRAKE: 8, STEER: 16,
        GLAT: 32, GLON: 64, GVERT: 128, YAW: 256, YAWRATE: 512,
        TREAD: 1024, SEGMENT: 2048, RPM: 4096, GEAR: 8192, TRACK: 16384,
        /* 4.3.0: tyre temp (FACT player / RESEARCH AI when TTEMP_RX) and the
         * RESEARCH raw candidates (meaning and unit unproven). */
        TTEMP: 32768, RX_SU: 65536, RX_LD: 131072, RX_SL: 262144, RX_FO: 524288,
        TTEMP_RX: 1048576
    };
    var TRF_MAX = 2097151;

    /* 4.3.0 /telemetry vf6 bits (car_telemetry_t v6 block). */
    var VF6 = { GVERT: 1, YAW: 2, TTEMP: 4, TCARC: 8, RX_SU: 16, RX_LD: 32,
                RX_SL: 64, RX_FO: 128, TTEMP_RX: 256 };

    function quad(a) {
        return Array.isArray(a) && a.length >= 4 && a.slice(0, 4).every(Number.isFinite)
            ? a.slice(0, 4) : null;
    }
    /* Live tread per car, FL,FR,RL,RR normalized 0..1: the player uses the
     * frame-level thermal block (trd); AI cars the raw 1.0..0.75 field. */
    function liveTread(j, car) {
        if (!((car.vf || 0) & VF.TREAD)) return null;
        var local = car.cr === 0 || (car.cr === undefined && !car.a);
        if (local && Array.isArray(j.trd)) return quad(j.trd);
        if (!car.a) return null; /* a remote human's tread is not on this PC */
        var raw = [car.wfl, car.wfr, car.wrl, car.wrr];
        if (!raw.every(Number.isFinite)) return null;
        return raw.map(function (v) { return Math.max(0, Math.min(1, (v - 0.75) * 4)); });
    }
    function liveResearch(car, vf6) {
        var rx = Array.isArray(car.rx) && car.rx.length === 16 ? car.rx : null;
        if (!rx) return null;
        var out = {
            su: (vf6 & VF6.RX_SU) ? quad(rx.slice(0, 4)) : null,
            ld: (vf6 & VF6.RX_LD) ? quad(rx.slice(4, 8)) : null,
            sl: (vf6 & VF6.RX_SL) ? quad(rx.slice(8, 12)) : null,
            fo: (vf6 & VF6.RX_FO) ? quad(rx.slice(12, 16)) : null
        };
        return (out.su || out.ld || out.sl || out.fo) ? out : null;
    }
    function traceResearch(s, f) {
        var rx = Array.isArray(s.rx) && s.rx.length === 16 ? s.rx : null;
        if (!rx) return null;
        var out = {
            su: (f & TRF.RX_SU) ? quad(rx.slice(0, 4)) : null,
            ld: (f & TRF.RX_LD) ? quad(rx.slice(4, 8)) : null,
            sl: (f & TRF.RX_SL) ? quad(rx.slice(8, 12)) : null,
            fo: (f & TRF.RX_FO) ? quad(rx.slice(12, 16)) : null
        };
        return (out.su || out.ld || out.sl || out.fo) ? out : null;
    }

    /* One palette for every side (A and B unchanged; C to F are the extra
     * sides of the multi-driver comparison). The viewer reads WB.COLORS. */
    var COLORS = { A: '#3da5ff', B: '#ff9c40', C: '#3ce07a', D: '#ff5ad2', E: '#ffe14a', F: '#b38cff' };
    var LETTERS = ['A', 'B', 'C', 'D', 'E', 'F'];
    var MAX_SIDES = 6;

    /* Canvas palette for the legacy-layout renderer. The defaults are the
     * /workbench look; the 4.4.0 console (telemetry_viewer.js) swaps in the
     * f1_telemetry.exe palette through setTheme(). titleRight puts the graph
     * title in the top-right corner like the old viewer, legend top-left. */
    var THEME_DEFAULT = {
        title: '#c9d4e0', unavailable: '#d9a55b', frame: '#2a3442', axis: '#8fa3b8',
        minor: '#141c28', grid: '#1e2836', zero: '#3a4a5e', cursor: '#e6edf3',
        track: '#3a4656', marker: '#ffffff', ref: '#9aa5b1',
        font: 'system-ui,Segoe UI,sans-serif', titleFont: 'bold 11px system-ui,Segoe UI,sans-serif',
        titleRight: false
    };
    var THEME = {};
    function setTheme(t) {
        var k;
        for (k in THEME_DEFAULT) THEME[k] = THEME_DEFAULT[k];
        if (t) for (k in t) if (Object.prototype.hasOwnProperty.call(THEME_DEFAULT, k)) THEME[k] = t[k];
        return THEME;
    }
    setTheme(null);

    /* Plain-language note for each per-sample validity code (notes panel). */
    var PLAIN_MISSING = {
        'speed not valid': 'Speed was not valid in some samples; those points are left blank.',
        'throttle not valid': 'Throttle was not valid in some samples; those points are left blank.',
        'brake not valid': 'Brake was not valid in some samples; those points are left blank.',
        'acceleration not valid': 'G-forces were not valid in some samples; those points are left blank.'
    };

    function radPerSecToRpm(radPerSec) { return radPerSec * RPM_FACTOR; }

    function formatRpm(rawRadPerSec) {
        if (!Number.isFinite(rawRadPerSec)) return '\u2014';
        var rpm = radPerSecToRpm(rawRadPerSec);
        if (!Number.isFinite(rpm)) return '\u2014';
        var text = Math.round(rpm).toString();
        return text.replace(/\B(?=(\d{3})+(?!\d))/g, ',');
    }

    function formatRpmValue(rpm) { /* converted RPM (trace channel) input */
        if (!Number.isFinite(rpm)) return '\u2014';
        var text = Math.round(rpm).toString();
        return text.replace(/\B(?=(\d{3})+(?!\d))/g, ',');
    }

    /* Viewer parity: time at a distance by linear interpolation on a lap's
     * distance->time curve (PCODE_SEMANTICS_2026-10-09 section 2c,
     * Data_management #14): scan while distance <= target; k = i-1;
     * f = (target - d[k]) / (d[i] - d[k]) when |d[i]-d[k]| > 0.0001 else 1;
     * result = t[k] + f * (t[i] - t[k]). Points may carry null distances. */
    function timeAtDistance(points, target) {
        if (!Number.isFinite(target)) return null;
        var prev = null;
        for (var i = 0; i < points.length; i++) {
            var p = points[i];
            if (!Number.isFinite(p.dist) || !Number.isFinite(p.t)) { prev = null; continue; }
            if (p.dist === target) return p.t;
            if (p.dist > target) {
                if (!prev || p.seg || !(p.t > prev.t) || !(p.dist > prev.dist)) return null;
                var dd = p.dist - prev.dist;
                var f = dd > 0.0001 ? (target - prev.dist) / dd : 1;
                if (f < 0) f = 0;
                if (f > 1) f = 1;
                return prev.t + f * (p.t - prev.t);
            }
            prev = p;
        }
        return null; /* never extrapolate past a partial lap */
    }

    /* ------------------------------------------------------------------ model */

    function CarSide(letter) {
        this.letter = letter;
        this.color = COLORS[letter] || '#cccccc';
        this.slot = -1;
        this.label = '(none)';
        this.tapeT = null;      /* TAPE2 V3: review tape time (null = live, no filter) */
        this.tapeOpen = null;   /* TAPE2 V3: the lap in progress at tapeT, samples up to tapeT */
        this.notes = [];
        this.noteCounts = {};
        this.steerWhy = '';
        this.reset(null, null);
        this.state = 'WAITING';
    }

    CarSide.prototype.reset = function (slot, meta) {
        this.slot = (slot === undefined || slot === null) ? -1 : slot; /* null >= 0 is true in JS */
        /* C1: roster identity for exports (driver name, car, team). Kept across a
         * rewind reset (slot given, meta null); cleared only by an empty reset. */
        if (meta) this.meta = { n: meta.n || '', cn: meta.cn || '', tn: meta.tn || '' };
        else if (slot === undefined || slot === null) this.meta = null;
        this.pendingOfficial = null;  /* C1: completed lap waiting for frame ll */
        this.lastLl = null;           /* C1: car.ll seen on the previous frame */
        if (meta) {
            /* CO-OP (4.4.0-rc.1): cr is the controller mode (0 local human,
             * 1 AI, 2 remote). A remote car is simulated on the other PC. */
            this.label = (meta.n || 'car ' + slot) +
                (meta.cn ? ' \u00b7 ' + meta.cn : '') +
                (meta.cr === 2 ? (meta.a ? " \u00b7 friend's car, AI driving" : ' \u00b7 remote human') :
                 ' \u00b7 ' + roleOf(meta));
        } else {
            this.label = '(none)';
        }
        this.points = [];          /* open lap */
        this.laps = [];            /* completed laps, oldest first, bounded */
        this.reference = null;     /* last completed lap of this side */
        this.pendingLap = null;    /* kept when the open lap rolls over */
        this.crossings = 0;
        this.startSeen = false;
        this.openLap = null;
        this.lapStart = null;
        this.interrupted = false;
        this.dropped = 0;          /* samples dropped past the ring */
        this.frozenSnapshot = null;
        this.hadHole = false;
        this.shown = null;         /* null = open lap; else a laps[] entry */
        this.state = this.slot >= 0 ? 'LIVE' : 'WAITING';
    };

    /* Notes are de-duplicated: each distinct reason is listed once with a
     * repeat count (noteCounts), oldest dropped past 8 distinct reasons. */
    CarSide.prototype.note = function (reason) {
        if (this.noteCounts[reason]) { this.noteCounts[reason]++; return; }
        this.noteCounts[reason] = 1;
        this.notes.push(reason);
        if (this.notes.length > 8) delete this.noteCounts[this.notes.shift()];
    };

    /* Missing-data reasons from the validity bits, mirroring unavailable[] on
     * the server path. Never re-labels a zero; missing stays missing. */
    CarSide.prototype.missingReasons = function (car) {
        var reasons = [];
        var vf = car.vf || 0;
        if (!(vf & VF.SPEED)) reasons.push('speed not valid');
        if (!(vf & VF.THROTTLE)) reasons.push('throttle not valid');
        if (!(vf & VF.BRAKE)) reasons.push('brake not valid');
        if (!(vf & VF.ACCEL)) reasons.push('acceleration not valid');
        return reasons;
    };

    /* Ingest one /telemetry frame for this side. Bounded; never interpolates
     * across pauses, backwards travel or teleports; a lap crossing rotates the
     * ring and keeps the previous completed lap as the reference. */
    CarSide.prototype.pushSample = function (j, car, trackLen) {
        if (!car) { this.note('The selected car was not in the latest game frame.'); return; }
        var t = j.st;
        if (!Number.isFinite(t)) return;
        var prev = this.points.length ? this.points[this.points.length - 1] : null;
        if (prev && t === prev.t) return; /* pause/repeated SSE packet */
        if (prev && t < prev.t) {
            var label = this.label;
            this.reset(this.slot, null); this.label = label;
            this.note('Simulation time rewound; the old timeline was cleared.');
            prev = null;
        }
        /* 4.4.0-rc.4 (task 1): the game's lap counter is 0 for the whole first
         * racing lap. Accept lap 0 while the car is moving or its track distance
         * is advancing, so grid standstill and a stopped or retired car are still
         * not recorded. No phase test: a live 2026-10-12 race reported ph 5 while
         * running (finding web-telemetry-recorder-lap0-phase-not-2-20261012). */
        var vf0 = car.vf || 0;
        var raceLap0 = car.l === 0 && (
            ((vf0 & VF.SPEED) && car.s > 0) ||
            (prev && prev.dist !== null && Number.isFinite(car.tp2) && car.tp2 > prev.dist));
        if (!(car.p > 0) || car.ot2 === 0 || car.pt >= 2 || car.fs > 0 || car.rt ||
            !(car.l > 0 || raceLap0) || !(trackLen > 0) || !Number.isFinite(car.tp2) ||
            car.tp2 < 0 || car.tp2 > trackLen || j.sample_age_ms > 1000) {
            this.interrupted = true; this.hadHole = true; this.updateState(); return;
        }
        var vf = car.vf || 0;
        var sample = {
            t: t,
            dist: Number.isFinite(car.tp2) ? car.tp2 : null,
            speed: (vf & VF.SPEED) ? car.s : null,
            rpmRaw: Number.isFinite(car.r) ? car.r : null,      /* rad/s */
            rpm: Number.isFinite(car.r) ? radPerSecToRpm(car.r) : null,
            maxRpm: Number.isFinite(car.mr) ? radPerSecToRpm(car.mr) : null,
            gear: Number.isFinite(car.g) ? car.g : null,
            throttle: (vf & VF.THROTTLE) ? car.th : null,
            brake: (vf & VF.BRAKE) ? car.br : null,
            steer: (vf & VF.STEER) ? car.steer : null,
            glat: (vf & VF.ACCEL) ? car.glat : null,
            glon: (vf & VF.GLON) ? car.glon : null,
            x: Number.isFinite(car.px) ? car.px : null,
            z: Number.isFinite(car.pz) ? car.pz : null,
            tx: Number.isFinite(car.mx) ? car.mx : null,
            tz: Number.isFinite(car.mz) ? car.mz : null,
            lap: car.l,
            seg: this.interrupted,
            /* 4.3.0 viewer channels */
            gvert: ((car.vf6 || 0) & VF6.GVERT) && Number.isFinite(car.gv) ? car.gv : null,
            yaw: ((car.vf6 || 0) & VF6.YAW) && Number.isFinite(car.yaw) ? car.yaw : null,
            tread: liveTread(j, car),
            ttemp: ((car.vf6 || 0) & (VF6.TTEMP | VF6.TTEMP_RX)) ? quad(car.ttc) : null,
            ttempResearch: !((car.vf6 || 0) & VF6.TTEMP) && !!((car.vf6 || 0) & VF6.TTEMP_RX),
            tcarc: ((car.vf6 || 0) & VF6.TCARC) ? quad(car.tcc) : null,
            rx: liveResearch(car, car.vf6 || 0)
        };
        var missing = this.missingReasons(car);
        for (var mi = 0; mi < missing.length; mi++)
            this.note(PLAIN_MISSING[missing[mi]] || missing[mi]);
        this.steerWhy = (vf & VF.STEER) ? '' : (car.steerwhy || 'steering not valid for this car');

        var interrupted = this.interrupted;
        this.interrupted = false;
        if (this.openLap === null) this.openLap = car.l;
        if (prev) {
            var dt = t - prev.t;
            var dd = (sample.dist !== null && prev.dist !== null)
                ? sample.dist - prev.dist : 0;
            /* Lap crossing first: the wrap is a large negative delta and must
             * not be mistaken for a reverse/teleport break. */
            var crossing = !interrupted && car.l >= prev.lap && car.l <= prev.lap + 1 && trackLen > 0 && prev.dist !== null &&
                sample.dist !== null && dt > 0 && dt <= 1.0 &&
                prev.dist >= trackLen - 50 && sample.dist <= 50 &&
                trackLen - prev.dist + sample.dist > 0 && trackLen - prev.dist + sample.dist <= 50;
            /* 1.2.0 (owner CO-OP live): a remote car's distance is projected on the AI line, so near the line it can jump from
             * a few metres short of the lap length to a few metres past 0 by more than the 50 m window. The game's lap counter
             * going up by one together with a wrap of more than half a lap is a crossing too (not a reverse break). */
            if (!crossing && !interrupted && trackLen > 0 && car.l === prev.lap + 1 && prev.dist !== null && sample.dist !== null &&
                dt > 0 && dt <= 1.0 && prev.dist - sample.dist > trackLen * 0.5) crossing = true;
            if (crossing) {
                this.completeLap(prev, sample, trackLen);
            } else {
                var broke = false;
                if (!(dt > 0) || dt > 1.0) broke = true;      /* pause/rewind */
                if (sample.dist !== null && prev.dist !== null) {
                    if (dd < -2.0) broke = true;               /* reverse */
                    if (dd > 50.0) broke = true;               /* teleport/reset */
                }
                if (broke) {
                    sample.seg = true;
                    this.hadHole = true;
                    this.note('The line has gaps where the game paused, rewound or reset the car; gaps are not bridged.');
                }
                if (car.l < prev.lap || car.l > prev.lap + 1) {
                    this.points = []; this.startSeen = false; this.lapStart = t;
                    this.openLap = car.l;
                    sample.seg = true; this.hadHole = true;
                }
                if (car.l !== this.openLap && !(car.l === this.openLap + 1 &&
                    prev.dist >= trackLen - 50 && sample.dist >= trackLen - 50)) {
                    sample.seg = true; this.hadHole = true;
                }
            }
        }

        this.points.push(sample);
        if (this.points.length > LIMITS.livePointsPerCar) {
            this.points.shift();
            this.dropped++;
            this.hadHole = true;
        }
        this.resolveOfficial(car, t);
        this.lastLl = Number.isFinite(car.ll) ? car.ll : null;
        this.updateState();
    };

    /* C1 (2026-10-14): the game's own lap time (frame ll) arrives a few frames
     * after the crossing. The lap keeps official_time undefined until ll changes
     * from its value before the crossing (or OFFICIAL_WAIT_S sim seconds pass,
     * then official_time is null). Only the viewer's export reads it. */
    CarSide.prototype.resolveOfficial = function (car, t) {
        var p = this.pendingOfficial;
        if (!p) return;
        var ll = Number.isFinite(car.ll) ? car.ll : null;
        if (ll !== null && ll > 0 && ll !== p.ll0) {
            p.lap.official_time = ll;
            this.pendingOfficial = null;
        } else if (t - p.since >= OFFICIAL_WAIT_S) {
            p.lap.official_time = null;
            this.pendingOfficial = null;
        }
    };

    CarSide.prototype.completeLap = function (prev, sample, trackLen) {
        var crossingTime = prev.t + (sample.t - prev.t) *
            (trackLen - prev.dist) / (trackLen - prev.dist + sample.dist);
        var lap = {
            lap: this.openLap + 1,   /* 4.4.0-rc.2: openLap counts from 0 on the grid; label 1-based */
            samples: this.points, /* ownership transfers; the next lap gets a fresh array */
            start: this.startSeen ? this.lapStart : this.points[0].t,
            end: crossingTime,
            complete: this.startSeen && !this.hadHole && !this.dropped,
            length: trackLen,
            /* 1.2.5 (DVOA 2026-10-18): both line crossings seen here, so the game's lap time belongs to this lap */
            timed: !!this.startSeen
        };
        /* 1.2.5: why a lap is partial (chip tooltip); a partial lap asks the server ring for its clean copy */
        if (!lap.complete) {
            lap.why = !this.startSeen ? 'the page opened during this lap' :
                this.dropped ? 'the oldest samples of this lap were dropped' :
                'the live feed paused or skipped during this lap';
            lap.repair = true;
        }
        this.pendingLap = lap;                 /* rotates only when kept */
        if (this.pendingOfficial) this.pendingOfficial.lap.official_time = null; /* previous lap never answered */
        this.pendingOfficial = { lap: lap, ll0: this.lastLl, since: sample.t };
        this.laps.push(lap);
        trimLaps(this.laps);
        this.reference = lap;                  /* survives further crossings */
        this.crossings++;
        this.points = [];
        this.hadHole = false;
        this.dropped = 0;
        this.startSeen = true;
        this.openLap++;
        this.lapStart = crossingTime;
        sample.seg = false;
    };

    /* 1.2.5: replace a partial lap held here with the server ring's copy of the same lap when that copy is clean
     * (the DLL samples every car at the game rate; this page only sees the frames that reached it). The lap keeps
     * its label and game time. Returns true when the lap was replaced. */
    CarSide.prototype.repairLap = function (lap, doc) {
        if (this.historySource) return this.historySource.repairLap(lap, doc);
        if (!lap || this.laps.indexOf(lap) < 0) return false;
        var scratch = new CarSide(this.letter);
        if (!scratch.loadTrace(doc) || !scratch.laps.length) return false;
        var fresh = scratch.laps[0];
        if (!lapIsFull(fresh)) return false;
        lap.samples = fresh.samples;
        lap.start = fresh.start;
        lap.end = fresh.end;
        lap.complete = true;
        lap.timed = true;
        lap.repaired = true;
        delete lap.why;
        lap.repair = false;
        this.updateState();
        return true;
    };

    CarSide.prototype.updateState = function () {
        if (this.frozenSnapshot) { this.state = 'FROZEN'; return; }
        if (this.slot < 0) { this.state = 'WAITING'; return; }
        if (this.points.length > 0)
            this.state = this.hadHole ? 'PARTIAL' : 'LIVE';
        else
            this.state = this.laps.length ? 'COMPLETED' : 'WAITING';
    };

    CarSide.prototype.freeze = function () {
        this.frozenSnapshot = {
            points: this.points.slice(),
            laps: this.laps.slice(),
            reference: this.reference,
            shown: this.shown,
            state: this.state
        };
        this.state = 'FROZEN';
    };

    CarSide.prototype.resume = function () {
        this.frozenSnapshot = null;
        if (this.historySource) delete this.state;
        else this.updateState(); /* rebinds display to the newest data */
    };

    /* TAPE2 V3 (pure): the laps finished by tape time t (end <= t), and the lap being driven at t (start <= t < end). */
    function lapsUpTo(laps, t) {
        return (laps || []).filter(function (l) { return Number.isFinite(l.end) && l.end <= t + 1e-6; });
    }
    function lapInProgress(laps, t) {
        return (laps || []).filter(function (l) {
            return Number.isFinite(l.start) && Number.isFinite(l.end) && l.start <= t && t < l.end;
        })[0] || null;
    }

    CarSide.prototype.displayPoints = function () {
        var shown = this.frozenSnapshot ? this.frozenSnapshot.shown : this.shown;
        if (shown) return shown.samples;
        /* TAPE2 V3: in a review the lap in progress at the tape time (its samples up to that time) is shown when no completed lap is */
        if (!this.frozenSnapshot && this.tapeOpen) return this.tapeOpen.samples;
        return this.frozenSnapshot ? this.frozenSnapshot.points : this.points;
    };
    CarSide.prototype.displayLaps = function () {
        var base = this.frozenSnapshot ? this.frozenSnapshot.laps : this.laps;
        /* TAPE2 V3: a review with a tape time lists only the laps the tape has finished (null = live, nothing hidden) */
        return this.tapeT !== null && !this.frozenSnapshot ? lapsUpTo(base, this.tapeT) : base;
    };

    /* f1c.trace/1 sample conversion shared by loadTrace and seedOpen.
     * Returns {pts, hole} or null when the document is unusable. */
    function traceDocPoints(doc) {
        if (!doc || doc.schema !== 'f1c.trace/1' || !Array.isArray(doc.samples) ||
            !doc.samples.length || doc.samples.length > LIMITS.livePointsPerCar ||
            doc.samples.some(function (s) { return !s || !Number.isFinite(s.t) ||
                !Number.isInteger(s.f) || s.f < 0 || s.f > TRF_MAX; }))
            return null;
        var pts = [];
        var hole = false;
        for (var i = 0; i < doc.samples.length && i < LIMITS.livePointsPerCar; i++) {
            var s = doc.samples[i];
            var f = s.f || 0;
            var seg = !!(f & TRF.SEGMENT) || (i > 0 && s.t <= doc.samples[i - 1].t);
            if (seg) hole = true;
            pts.push({
                t: s.t,
                dist: (f & TRF.DIST) && Number.isFinite(s.d) ? s.d : null,
                speed: (f & TRF.SPEED) ? s.s : null,
                rpmRaw: null,
                rpm: (f & TRF.RPM) && Number.isFinite(s.rpm) ? s.rpm : null,   /* already RPM */
                maxRpm: null,
                gear: (f & TRF.GEAR) && Number.isFinite(s.ge) ? s.ge : null,
                throttle: (f & TRF.THROTTLE) ? s.th : null,
                brake: (f & TRF.BRAKE) ? s.br : null,
                steer: (f & TRF.STEER) ? s.st : null,
                glat: (f & TRF.GLAT) ? s.gl : null,
                glon: (f & TRF.GLON) ? s.go : null,
                gvert: (f & TRF.GVERT) && Number.isFinite(s.gv) ? s.gv : null,
                yaw: (f & TRF.YAW) && Number.isFinite(s.y) ? s.y : null,
                tread: (f & TRF.TREAD) && Array.isArray(s.tr) ? s.tr : null,
                ttemp: (f & TRF.TTEMP) ? quad(s.tt) : null,
                ttempResearch: !!(f & TRF.TTEMP_RX),
                tcarc: null,
                rx: traceResearch(s, f),
                x: null, z: null,
                tx: (f & TRF.TRACK) && Number.isFinite(s.tx) ? s.tx : null,
                tz: (f & TRF.TRACK) && Number.isFinite(s.tz) ? s.tz : null,
                seg: seg,
                missing: []
            });
        }
        return { pts: pts, hole: hole };
    }

    /* f1c.trace/1 JSON lap import (already bounded server-side). Imported RPM
     * is stored by the recorder in display RPM: never convert it again. */
    CarSide.prototype.loadTrace = function (doc) {
        var conv = traceDocPoints(doc);
        if (!conv) return false;
        var pts = conv.pts, hole = conv.hole;
        this.label = (doc.driver || 'imported') + (doc.car ? ' \u00b7 ' + doc.car : '') +
            (doc.ai ? ' \u00b7 AI' : '') + ' \u00b7 legacy import';
        this.points = pts;
        this.laps = [];
        this.reference = null;
        this.crossings = 0;
        this.hadHole = hole;
        this.frozenSnapshot = null;
        this.shown = null;
        if (pts.length) {
            var imp = {
                lap: Number.isFinite(doc.lap) ? doc.lap : 1, src: 'import',
                samples: pts, start: pts[0].t, end: pts[pts.length - 1].t,
                length: Number.isFinite(doc.lap_len) ? doc.lap_len : 0
            };
            imp.complete = (doc.source === 'live' ? !!(doc.flags & 1) && !(doc.flags & 6) :
                doc.partial === false) && !doc.open && !doc.truncated && !hole;
            /* A live trace records interpolated start/crossing boundaries that
             * can fall between its first and last sampled timestamps. Preserve
             * those canonical boundaries for complete imports when consistent;
             * legacy and partial documents keep their sample-derived range. */
            /* 1.2.4: an export marks the game's official time (lap_time_src "game"); it is kept as the lap's official time,
             * as for a live lap, and the boundaries use the derived time when the file has it. */
            var gameTime = doc.lap_time_src === 'game' && Number.isFinite(doc.lap_time) && doc.lap_time > 0;
            if (gameTime) imp.official_time = doc.lap_time;
            var spanTime = gameTime ? doc.derived_lap_time : doc.lap_time;
            /* 1.2.5: a recorded lap (review) carries the same start_sim + derived_lap_time boundaries */
            if (imp.complete && (doc.source === 'live' || doc.source === 'recording') &&
                Number.isFinite(doc.start_sim) && Number.isFinite(spanTime) &&
                spanTime > 0) {
                var timedEnd = doc.start_sim + spanTime;
                if (Number.isFinite(timedEnd) && doc.start_sim <= pts[0].t + 0.001 &&
                    timedEnd > pts[0].t && timedEnd + 0.001 >= pts[pts.length - 1].t) {
                    imp.start = doc.start_sim;
                    imp.end = timedEnd;
                }
            }
            this.laps.push(imp);
            this.shown = imp;
        }
        this.updateState();
        if (hole) this.note('The imported lap has gaps in its recording; gaps are drawn as holes.');
        return true;
    };

    /* Merge the server's open lap with frames received while the HTTP request
     * was in flight. The shared history owns this data; views never own it. */
    CarSide.prototype.seedOpen = function (doc) {
        if (!doc || doc.open !== true) return false;
        if (this.historySource) return this.historySource.seedOpen(doc);
        var conv = traceDocPoints(doc);
        if (!conv || !conv.pts.length) return false;
        if (this.openLap !== null && this.openLap !== doc.lap && this.points.length) return false;
        var existing = this.points;
        var first = existing.length ? existing[0].t : Infinity;
        if (existing.length && conv.pts[0].t >= first) return false;
        var end = conv.pts[conv.pts.length - 1].t;
        var tail = existing.filter(function (p) { return p.t > end; });
        this.points = conv.pts.concat(tail).slice(-LIMITS.livePointsPerCar);
        /* Live crossing detection needs the native zero-based lap counter. */
        this.points.forEach(function (p) { p.lap = doc.lap; });
        this.openLap = Number.isFinite(doc.lap) ? doc.lap : 0;
        this.lapStart = Number.isFinite(doc.start_sim) ? doc.start_sim : conv.pts[0].t;
        this.startSeen = !!(doc.flags & 1);
        this.hadHole = conv.hole;
        this.interrupted = conv.hole;
        this.updateState();
        if (conv.hole) this.note('The open lap loaded from the server has gaps; gaps are not bridged.');
        return true;
    };

    /* Every car in the frame is recorded from the moment the page opens
     * (bounded per car by LIMITS), so changing the A/B selection only
     * switches which history is shown; it never erases collected laps
     * (owner report 2026-10-09: two laps lost on a driver change). */
    function Workbench() {
        this.hist = {};          /* slot -> CarSide, one per car seen */
        this.sides = { A: new CarSide('A'), B: new CarSide('B') };  /* letter -> side; A and B always exist */
        this.trackLen = 0;
        this.trackKey = '';
        this.sessionGeneration = null;
        this.cursor = null;      /* distance in metres */
        this.cursorT = null;     /* lap time (s) when the x axis is time */
        this.refLap = null;      /* {letter, lap} picked in the lap strip */
        this.notes = [];
        this.frames = 0;
    }

    Workbench.prototype.reset = function () {
        this.hist = {};
        var self = this;   /* extra sides keep their letter, blank until a car is picked */
        LETTERS.forEach(function (L) { if (self.sides[L]) self.sides[L] = new CarSide(L); });
        this.cursor = null;
        this.cursorT = null;
        this.refLap = null;
        this.notes = [];
    };

    /* a and b are aliases of sides.A and sides.B (existing code and /workbench). */
    Object.defineProperty(Workbench.prototype, 'a', {
        get: function () { return this.sides.A; },
        set: function (s) { this.sides.A = s; }, enumerable: true, configurable: true });
    Object.defineProperty(Workbench.prototype, 'b', {
        get: function () { return this.sides.B; },
        set: function (s) { this.sides.B = s; }, enumerable: true, configurable: true });

    /* Active sides in letter order: A, B, then any extra sides C to F. */
    Workbench.prototype.allSides = function () {
        var out = [], self = this;
        LETTERS.forEach(function (L) { if (self.sides[L]) out.push(self.sides[L]); });
        return out;
    };
    Workbench.prototype.sideOf = function (letter) { return this.sides[letter] || null; };
    /* Adds an extra side (C to F). Returns the side, or null when the letter
     * is invalid, already present (the existing side is returned) or the
     * six-side limit is reached. A and B are never removed. */
    Workbench.prototype.addSide = function (letter) {
        if (LETTERS.indexOf(letter) < 0) return null;
        if (this.sides[letter]) return this.sides[letter];
        if (this.allSides().length >= MAX_SIDES) return null;
        this.sides[letter] = new CarSide(letter);
        return this.sides[letter];
    };
    /* Removes an extra side. A and B cannot be removed (returns false). The
     * recorded history of the car stays in hist (bounded), only the view goes. */
    Workbench.prototype.removeSide = function (letter) {
        if (letter === 'A' || letter === 'B' || !this.sides[letter]) return false;
        delete this.sides[letter];
        if (this.refLap && this.refLap.letter === letter) this.refLap = null;
        return true;
    };

    Workbench.prototype.history = function (slot, meta) {
        var h = this.hist[slot];
        /* Retired AI may report a=0 while its controller role stays AI. */
        var identity = meta ? JSON.stringify([meta.n, meta.cn, meta.cr === 2]) : null;
        if (h && identity !== null && h.identity !== identity) {
            h.reset(slot, meta);
            h.notes = []; h.noteCounts = {};
            this.allSides().forEach(function (view) {
                if (view.historySource === h) {
                    view.shown = null; view.frozenSnapshot = null; delete view.state;
                }
            });
            this.refLap = null;
        }
        if (!h) {
            h = new CarSide('A');
            h.reset(slot, meta || null);
            h.note('Older laps load from the server when available; the first live lap here can be partial.');
            this.hist[slot] = h;
        } else if (meta && h.label === '(none)') {
            var tmp = new CarSide('A'); tmp.reset(slot, meta); h.label = tmp.label;
        }
        if (identity !== null) h.identity = identity;
        return h;
    };

    Workbench.prototype.select = function (letter, slot, meta) {
        if (LETTERS.indexOf(letter) < 0) return;
        if (!this.sides[letter] && !this.addSide(letter)) return;
        var side;
        if (Number.isInteger(slot) && slot >= 0 && slot < 104) {
            var history = this.hist[slot] || this.history(slot, meta);
            side = Object.create(history);
            side.historySource = history;
            side.shown = null; side.frozenSnapshot = null;
        } else side = new CarSide(letter);
        side.letter = letter; side.color = COLORS[letter];
        this.sides[letter] = side;
    };

    /* An import gets its own side object so it never overwrites a car's
     * recorded history. */
    /* 4.4.0-rc.6 task 4 (Batch 2i): add completed laps from /trace/data documents to
     * this side's history. loadTrace replaces history, so each document goes through a
     * scratch side. Only complete laps; a lap number already held is skipped. */
    CarSide.prototype.seedLaps = function (docs) {
        if (this.historySource) return this.historySource.seedLaps(docs);
        var self = this, added = 0;
        (docs || []).forEach(function (doc) {
            if (!doc || doc.open) return;
            var scratch = new CarSide(self.letter);
            if (!scratch.loadTrace(doc) || !scratch.laps.length) return;
            var lap = scratch.laps[0];
            lap.recorded = true;   /* TAPE2 V2: a lap seeded from the server is a recorded lap ("L<n>"), not an import */
            if (self.laps.some(function (q) { return q.lap === lap.lap; })) return;
            self.laps.push(lap);
            added++;
        });
        if (!added) return 0;
        self.laps.sort(function (a, b) { return a.lap - b.lap; });
        trimLaps(self.laps);
        if (!self.shown) self.shown = self.laps[self.laps.length - 1];
        if (!self.reference) self.reference = self.laps[self.laps.length - 1];
        self.updateState();
        return added;
    };

    /* 1.2.3 (DVOA report 2026-10-17): an exported f1c.lapset/1 file loads every lap it holds into one side (1.2.2 and
     * older refused the page's own export). Open laps are skipped; at most 60 laps. Returns the number of laps loaded. */
    Workbench.prototype.importSetInto = function (letter, docs) {
        var side = null, n = 0, list = (Array.isArray(docs) ? docs : []).filter(function (d) {
            return d && d.schema === 'f1c.trace/1' && !d.open;
        }).slice(0, 60);
        for (var i = 0; i < list.length; i++) {
            var s = new CarSide(letter);
            if (!s.loadTrace(list[i]) || !s.laps.length) continue;
            n++;
            if (!side) side = s;
            else if (!side.laps.some(function (q) { return q.lap === s.laps[0].lap; })) side.laps.push(s.laps[0]);
        }
        if (!side) return 0;
        side.laps.sort(function (a, b) { return a.lap - b.lap; });
        side.shown = fastestLap(side.laps) || side.laps[side.laps.length - 1];
        side.points = side.shown.samples;
        side.updateState();
        if (!this.sides[letter] && !this.addSide(letter)) return 0;
        this.sides[letter] = side;
        return side.laps.length;
    };

    Workbench.prototype.importInto = function (letter, doc) {
        var side = new CarSide(letter);
        if (!side.loadTrace(doc)) return false;
        if (!this.sides[letter] && !this.addSide(letter)) return false;
        this.sides[letter] = side;
        return true;
    };

    Workbench.prototype.frame = function (j) {
        if (!j || !Array.isArray(j.c)) return;
        var trackKey = j.trk || '';
        var contextChanged = this.trackKey && trackKey && (trackKey !== this.trackKey ||
            (j.lapm > 0 && this.trackLen > 0 && Math.abs(j.lapm - this.trackLen) > 1));
        if (contextChanged || j.session_generation !== this.sessionGeneration) {
            /* A generation change clears history; the first frame only adopts
             * the generation so a selection made before the first frame stays. */
            if (this.sessionGeneration !== null) this.reset();
            this.sessionGeneration = j.session_generation;
        }
        this.trackLen = Number.isFinite(j.lapm) && j.lapm > 0 ? j.lapm : 0;
        this.trackKey = trackKey;
        this.frames++;
        var side, car, i, present = {};
        for (i = 0; i < j.c.length; i++) {
            car = j.c[i];
            if (!Number.isInteger(car.id) || car.id < 0 || car.id >= 104 || !(car.p > 0)) continue;
            present[car.id] = true;
            this.history(car.id, car).pushSample(j, car, this.trackLen);
        }
        var total = 0, oldest = [];
        Object.keys(this.hist).forEach(function (key) {
            var h = this.hist[key];
            if (!present[key]) { h.interrupted = true; h.hadHole = true; h.updateState(); }
            total += h.points.length;
            h.laps.forEach(function (lap) { total += lap.samples.length; oldest.push({h:h,lap:lap}); });
        }, this);
        if (total > LIMITS.historyPoints) {
            oldest.sort(function (a,b) { return a.lap.end-b.lap.end; });
            for (i=0;i<oldest.length && total>LIMITS.historyPoints;i++) {
                var entry=oldest[i], index=entry.h.laps.indexOf(entry.lap);
                if (index<0) continue;
                entry.h.laps.splice(index,1); total-=entry.lap.samples.length;
                if (entry.h.reference===entry.lap) entry.h.reference=null;
                if (entry.h.pendingLap===entry.lap) entry.h.pendingLap=null;
                entry.h.note('Old completed laps were removed to keep browser memory bounded.');
            }
        }
        var sel = this.allSides();
        for (i = 0; i < sel.length; i++) {
            side = sel[i];
            if (side.slot >= 0 && !j.c.some(function (c) { return c.id === side.slot; }))
                side.note('selected car missing from frame');
        }
    };

    /* ------------------------------------------------------------- layouts */

    /* LAYOUTS:BEGIN generated by gen_workbench_layouts.py; do not edit */
    var LAYOUTS = [{"file":"overview.ini","name":"Overview","title":"*00_Overview","desc":"Chassis Overview","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.5]},{"x":"lat accel","y":"long accel","flags":["DOTS","NOLINES"],"title":null,"sub":null,"bmp":null,"range":[-6.0,6.0,-2.0,4.0],"auto":false,"rect":[0.0,0.4,0.5,1.0]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.4,1.0,0.0,0.5]},{"x":"time","y":"engine rpm","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,7000.0,20000.0],"auto":false,"rect":[0.4,1.0,0.5,1.0]}]},{"file":"time_diff.ini","name":"Time Diff","title":"00_Time Diff","desc":"Incremental Time Difference","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,1.0]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.4,1.0,0.7,1.0]},{"x":"time","y":"dtime","flags":[],"title":"Incremental Time Difference","sub":null,"bmp":null,"range":[-5.0,100.0,-3.0,6.0],"auto":false,"rect":[0.4,1.0,0.0,0.7]}]},{"file":"chassis_gears.ini","name":"Gears","title":"04_Gears","desc":"Engine and Gearbox Information","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,1.0]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.4,1.0,0.0,0.25]},{"x":"time","y":"engine rpm","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,12000.0,19000.0],"auto":false,"rect":[0.4,1.0,0.25,0.5]},{"x":"time","y":"gear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,8.0],"auto":false,"rect":[0.4,1.0,0.5,0.75]},{"x":"time","y":"rear wheel speed differential","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,1.0,0.75,1.0]}]},{"file":"track.ini","name":"Track","title":"05_Track","desc":"Track View","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,1.0,0.0,1.0]}]},{"file":"basic.ini","name":"Basic","title":"Basic","desc":"Track and Velocity Trace","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,1.0]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.4,1.0,0.0,1.0]}]},{"file":"dampers_all.ini","name":"Dampers","title":"Dampers","desc":"[Advanced] - Damper Velocities","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"damper velocity front lh","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-100.0,100.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"damper velocity front rh","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-100.0,100.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"damper velocity rear lh","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-100.0,100.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"damper velocity rear rh","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-100.0,100.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"dampers_all_filtered.ini","name":"Dampers (filtered)","title":"Dampers (filtered)","desc":"[Advanced] - Damper Velocities (Smoothed)","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"filter[damper velocity front lh,20]","flags":[],"title":"damper velocity front lh","sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"filter[damper velocity front rh,20]","flags":[],"title":"damper velocity front rh","sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"filter[damper velocity rear lh,20]","flags":[],"title":"damper velocity rear lh","sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"filter[damper velocity rear rh,20]","flags":[],"title":"damper velocity rear rh","sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"chassis_gg.ini","name":"G-G","title":"G-G","desc":"[Advanced] - Friction Circle","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"lat accel","y":"long accel","flags":["DOTS","NOLINES"],"title":null,"sub":null,"bmp":null,"range":[-6.0,6.0,-2.0,4.0],"auto":false,"rect":[0.4,1.0,0.0,1.0]}]},{"file":"chassis_gxyz.ini","name":"G-xyz","title":"G-xyz","desc":"[Advanced] - Chassis Accelerations XYZ","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"long accel","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-2.0,4.0],"auto":false,"rect":[0.4,1.0,0.0,0.333]},{"x":"time","y":"lat accel","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-5.0,5.0],"auto":false,"rect":[0.4,1.0,0.333,0.666]},{"x":"time","y":"vert accel","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,1.0],"auto":false,"rect":[0.4,1.0,0.666,1.0]}]},{"file":"chassis_inputs.ini","name":"Inputs","title":"Inputs","desc":"Driver Inputs","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.5]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.5,1.0]},{"x":"time","y":"throttle","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,10.0,130.0],"auto":false,"rect":[0.4,1.0,0.0,0.2]},{"x":"time","y":"brake","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,130.0],"auto":false,"rect":[0.4,1.0,0.2,0.4]},{"x":"time","y":"steering","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-130.0,130.0],"auto":false,"rect":[0.4,1.0,0.4,0.6]},{"x":"time","y":"gear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,8.0],"auto":false,"rect":[0.4,1.0,0.6,0.8]},{"x":"time","y":"clutch","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,130.0],"auto":false,"rect":[0.4,1.0,0.8,1.0]}]},{"file":"chassis_rideheight.ini","name":"Ride Heights","title":"Ride Heights","desc":"[Advanced] - Front and Rear Ride Height","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"rear ride height","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-5.0,80.0],"auto":false,"rect":[0.4,1.0,0.5,1.0]},{"x":"time","y":"front ride height","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-5.0,80.0],"auto":false,"rect":[0.4,1.0,0.0,0.5]}]},{"file":"chassis_rideheight_filtered.ini","name":"Ride Heights (filtered)","title":"Ride Heights (filtered)","desc":"[Advanced] - Front and Rear Ride Height Smoothed","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"filter[rear ride height,20]","flags":[],"title":"Rear Ride Height","sub":null,"bmp":null,"range":[-5.0,100.0,-5.0,80.0],"auto":false,"rect":[0.4,1.0,0.5,1.0]},{"x":"time","y":"filter[front ride height,20]","flags":[],"title":"Front Ride Height","sub":null,"bmp":null,"range":[-5.0,100.0,-5.0,80.0],"auto":false,"rect":[0.4,1.0,0.0,0.5]}]},{"file":"chassis_slip.ini","name":"Slip Angle","title":"Slip Angle","desc":"[Advanced] - Chassis Slip Angle","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"lat accel","y":"long accel","flags":["DOTS","NOLINES"],"title":null,"sub":null,"bmp":null,"range":[-5.0,5.0,-2.0,5.0],"auto":false,"rect":[0.4,1.0,0.5,1.0]},{"x":"time","y":"yaw","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-20.0,20.0],"auto":false,"rect":[0.4,1.0,0.0,0.5]}]},{"file":"susp_deflection_all.ini","name":"Susp-All","title":"Susp-All","desc":"[Advanced] - Suspension Travel","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl suspension deflection","flags":[],"title":"FL suspension deflection","sub":"measured at damper","bmp":null,"range":[-5.0,100.0,-10.0,30.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"fr suspension deflection","flags":[],"title":"FR suspension deflection","sub":"measured at damper","bmp":null,"range":[-5.0,100.0,-10.0,30.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rl suspension deflection","flags":[],"title":"RL suspension deflection","sub":"measured at damper","bmp":null,"range":[-5.0,100.0,-10.0,30.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"rr suspension deflection","flags":[],"title":"RR suspension deflection","sub":"measured at damper","bmp":null,"range":[-5.0,100.0,-10.0,30.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"tyre_temp_all.ini","name":"Tyre-All","title":"Tyre-All","desc":"[Advanced] - Tyre Temperatures (Inside/Middle/Outside)","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl tyre crown temp","flags":[],"title":"FL Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.0,0.25]},{"x":"time","y":"fl tyre camber temp","flags":[],"title":"FL Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.25,0.5]},{"x":"time","y":"fr tyre crown temp","flags":[],"title":"FR Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.0,0.25]},{"x":"time","y":"fr tyre camber temp","flags":[],"title":"FR Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.25,0.5]},{"x":"time","y":"rl tyre crown temp","flags":[],"title":"RL Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.5,0.75]},{"x":"time","y":"rl tyre camber temp","flags":[],"title":"RL Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.75,1.0]},{"x":"time","y":"rr tyre crown temp","flags":[],"title":"RR Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.5,0.75]},{"x":"time","y":"rr tyre camber temp","flags":[],"title":"RR Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.75,1.0]},{"x":"null","y":"null","flags":[],"title":null,"sub":null,"bmp":"planview.jpg","range":[0.0,1.0,0.0,1.0],"auto":false,"rect":[0.35,0.65,0.4,0.6]}]},{"file":"tyre_temp_front.ini","name":"Tyre-Front","title":"Tyre-Front","desc":"[Advanced] - Front Tyre Temperatures (Inside/Middle/Outside)","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl tyre crown temp","flags":[],"title":"FL Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"fr tyre crown temp","flags":[],"title":"FR Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"fl tyre camber temp","flags":[],"title":"FL Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"fr tyre camber temp","flags":[],"title":"FR Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"tyre_temp_rear.ini","name":"Tyre-Rear","title":"Tyre-Rear","desc":"[Advanced] - Rear Tyre Temperatures (Inside/Middle/Outside)","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"rl tyre crown temp","flags":[],"title":"RL Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"rr tyre crown temp","flags":[],"title":"RR Tyre Crown Temperature","sub":"Caused By Over Pressure","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rl tyre camber temp","flags":[],"title":"RL Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"rr tyre camber temp","flags":[],"title":"RR Tyre Camber Temperature","sub":"caused by camber","bmp":null,"range":[-5.0,100.0,-10.0,10.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"tyre_temp.ini","name":"Tyre-Temp","title":"Tyre-Temp","desc":"[Advanced] - Tyre Temperatures (Averaged)","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl tyre average temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,20.0,160.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"fr tyre average temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,20.0,160.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rl tyre average temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,20.0,160.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"rr tyre average temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,20.0,160.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"tyre_wear.ini","name":"Tyre-Wear","title":"Tyre-Wear","desc":"[Advanced] - Tyre Wear","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl tyre wear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.9,1.01],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"fr tyre wear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.9,1.01],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rl tyre wear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.9,1.01],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"rr tyre wear","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.9,1.01],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"track_conditions.ini","name":"Weather","title":"Weather","desc":"Weather","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,1.0]},{"x":"time","y":"air temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.0,50.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"track temp","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,0.0,50.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rain","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,110.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"track dampness","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-10.0,110.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"wheels_slip.ini","name":"Wheel-Spin","title":"Wheel-Spin","desc":"[Advanced] - Wheel Spin","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-1876.376,1876.376,-1500.0,1500.0],"auto":true,"rect":[0.0,0.4,0.0,0.7]},{"x":"time","y":"velocity","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-25.0,300.0],"auto":false,"rect":[0.0,0.4,0.7,1.0]},{"x":"time","y":"fl wheelspin","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,1.0],"auto":false,"rect":[0.4,0.7,0.0,0.5]},{"x":"time","y":"fr wheelspin","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,1.0],"auto":false,"rect":[0.7,1.0,0.0,0.5]},{"x":"time","y":"rl wheelspin","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,1.0],"auto":false,"rect":[0.4,0.7,0.5,1.0]},{"x":"time","y":"rr wheelspin","flags":[],"title":null,"sub":null,"bmp":null,"range":[-5.0,100.0,-1.0,1.0],"auto":false,"rect":[0.7,1.0,0.5,1.0]}]},{"file":"_revert_layout.ini","name":"current layout","title":"current layout","desc":"current layout","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-934.9476,505.713,-205.5585,2777.782],"auto":true,"rect":[0.0,1.0,0.0,1.0]}]},{"file":"_single_layout.ini","name":"single graph layout","title":"single graph layout","desc":"single graph layout","graphs":[{"x":"x","y":"z","flags":["NOGRID","NOXAXIS","NOYAXIS"],"title":null,"sub":null,"bmp":null,"range":[-934.9476,505.713,-205.5585,2777.782],"auto":true,"rect":[0.0,1.0,0.0,1.0]}]}];
    /* LAYOUTS:END */

    var DEFAULT_LAYOUT = 'overview.ini';
    var G_MS2 = 9.8;   /* m/s^2 per g; same factor as the legacy .lap import */
    var WHEELS = ['fl', 'fr', 'rl', 'rr'];

    /* ---- 1.2.5 (DVOA report 2026-10-18): user layouts ("My layouts") ----
     * A user layout has the shape of the legacy layouts above, is built from a small spec (track map, G-G, up to
     * USER_GRAPH_MAX channel graphs) and lives in this browser's storage (key wb.userLayouts). Files: the legacy
     * f1_telemetry .ini text (shared with the old viewer) and f1c.layout/1 JSON. Nothing is sent to the game PC. */
    var USER_LAYOUT_KEY = 'wb.userLayouts';
    var USER_LAYOUT_MAX = 40;
    var USER_GRAPH_MAX = 8;
    var USER_NAME_MAX = 40;
    var MAP_RANGE = [-1876.376, 1876.376, -1500.0, 1500.0];
    /* The channels a user graph can show: every name channelInfo() draws from a sample. Group order = editor order. */
    var CHANNEL_CATALOG = [
        ['Speed and engine', ['velocity', 'engine rpm', 'gear']],
        ['Driver inputs', ['throttle', 'brake', 'steering']],
        ['Chassis', ['lat accel', 'long accel', 'vert accel', 'yaw']],
        ['Tyres', ['fl tyre wear', 'fr tyre wear', 'rl tyre wear', 'rr tyre wear',
                   'fl tyre average temp', 'fr tyre average temp', 'rl tyre average temp', 'rr tyre average temp']],
        ['Lap', ['dtime']]
    ];
    var userLayoutCache = null;

    function userStore() {
        try { return typeof localStorage !== 'undefined' ? localStorage : null; } catch (e) { return null; }
    }
    function catalogHas(name) {
        return CHANNEL_CATALOG.some(function (g) { return g[1].indexOf(name) >= 0; });
    }
    /* Pure. [{group, name, label, unit}] in editor order. */
    function channelCatalog() {
        var out = [];
        CHANNEL_CATALOG.forEach(function (g) {
            g[1].forEach(function (n) {
                var info = channelInfo(n);
                out.push({ group: g[0], name: n, label: info.label, unit: info.unit || '' });
            });
        });
        return out;
    }
    function cleanLayoutName(name) {
        var s = String(name === undefined || name === null ? '' : name).replace(/[\u0000-\u001f"]/g, ' ').replace(/\s+/g, ' ').trim();
        return (s || 'My layout').slice(0, USER_NAME_MAX);
    }
    /* Pure. spec {name, map, gg, graphs:[{y, smooth}]} -> a layout object (legacy shape), or null when it has no graph.
     * Unknown channels are dropped; at most USER_GRAPH_MAX channel graphs. The map and G-G share a left column. */
    function buildUserLayout(spec, file) {
        spec = spec || {};
        var ys = (Array.isArray(spec.graphs) ? spec.graphs : []).filter(function (g) {
            return g && typeof g.y === 'string' && catalogHas(g.y);
        }).slice(0, USER_GRAPH_MAX).map(function (g) { return { y: g.y, smooth: !!g.smooth && g.y !== 'gear' && g.y !== 'dtime' }; });
        var map = !!spec.map, gg = !!spec.gg;
        if (!ys.length && !map && !gg) return null;
        var left = map || gg, x0 = left && ys.length ? 0.4 : 0, graphs = [];
        var lw = ys.length ? 0.4 : 1;
        if (map) graphs.push({ x: 'x', y: 'z', flags: ['NOGRID', 'NOXAXIS', 'NOYAXIS'], title: null, sub: null, bmp: null,
            range: MAP_RANGE.slice(), auto: true, rect: [0, lw, 0, gg ? 0.6 : 1] });
        if (gg) graphs.push({ x: 'lat accel', y: 'long accel', flags: ['DOTS', 'NOLINES'], title: null, sub: null, bmp: null,
            range: [-6, 6, -2, 4], auto: false, rect: [0, lw, map ? 0.6 : 0, 1] });
        ys.forEach(function (g, i) {
            var h = 1 / ys.length;
            graphs.push({ x: 'time', y: g.smooth ? 'filter[' + g.y + ',20]' : g.y, flags: [], title: null, sub: null, bmp: null,
                range: [-5, 100, 0, 1], auto: true,
                rect: [x0, 1, Math.round(i * h * 1000) / 1000, Math.round((i + 1) * h * 1000) / 1000] });
        });
        var name = cleanLayoutName(spec.name);
        return { file: file || ('user:' + Date.now().toString(36) + Math.floor(Math.random() * 1296).toString(36)),
            name: name, title: name, desc: name, user: true,
            spec: { name: name, map: map, gg: gg, graphs: ys }, graphs: graphs };
    }
    /* Pure. The spec of any layout (built-in or user): the map, the G-G and its time graphs, for "Customize". Graphs whose
     * channel the editor does not offer are left out (they have no source in this viewer). */
    function layoutSpec(layout) {
        if (!layout) return null;
        if (layout.spec) return JSON.parse(JSON.stringify(layout.spec));
        var spec = { name: cleanLayoutName((layout.name || 'Layout') + ' (mine)'), map: false, gg: false, graphs: [] };
        (layout.graphs || []).forEach(function (g) {
            if (g.x === 'x' && g.y === 'z') { spec.map = true; return; }
            if (g.x === 'lat accel' && g.y === 'long accel') { spec.gg = true; return; }
            if (g.x !== 'time' && g.x !== 'distance') return;
            var m = /^filter\[(.+),\s*\d+\]$/.exec(String(g.y || '').toLowerCase().trim());
            var y = m ? m[1] : String(g.y || '').toLowerCase().trim();
            if (catalogHas(y) && spec.graphs.length < USER_GRAPH_MAX) spec.graphs.push({ y: y, smooth: !!m });
        });
        return spec;
    }
    function validUserLayout(l) {
        return !!l && typeof l.file === 'string' && /^user:[a-z0-9]{1,24}$/.test(l.file) && !!l.spec;
    }
    /* The saved user layouts (rebuilt from their specs, so a stored entry can never inject a graph shape). */
    function userLayouts() {
        if (userLayoutCache) return userLayoutCache;
        var list = [], st = userStore();
        try {
            var raw = st ? JSON.parse(st.getItem(USER_LAYOUT_KEY) || '[]') : [];
            (Array.isArray(raw) ? raw : []).slice(0, USER_LAYOUT_MAX).forEach(function (l) {
                if (!validUserLayout(l)) return;
                var b = buildUserLayout(l.spec, l.file);
                if (b) list.push(b);
            });
        } catch (e) { list = []; }
        userLayoutCache = list;
        return list;
    }
    /* Saves (adds, or replaces by file) one user layout. Returns the stored layout, or null when it is empty or the
     * list is full. */
    function saveUserLayout(spec, file) {
        var list = userLayouts().slice(), lay = buildUserLayout(spec, file);
        if (!lay) return null;
        var i = list.map(function (l) { return l.file; }).indexOf(lay.file);
        if (i >= 0) list[i] = lay;
        else if (list.length >= USER_LAYOUT_MAX) return null;
        else list.push(lay);
        writeUserLayouts(list);
        return lay;
    }
    function deleteUserLayout(file) {
        var list = userLayouts().filter(function (l) { return l.file !== file; });
        writeUserLayouts(list);
        return list.length;
    }
    function writeUserLayouts(list) {
        userLayoutCache = list;
        var st = userStore();
        try {
            if (st) st.setItem(USER_LAYOUT_KEY, JSON.stringify(list.map(function (l) { return { file: l.file, spec: l.spec }; })));
        } catch (e) { /* private mode or full storage: the list lives until the page closes */ }
    }
    function allLayouts() { return LAYOUTS.concat(userLayouts()); }

    /* Pure. A layout as legacy f1_telemetry .ini text (CRLF), the format of the 23 built-in layouts. */
    function layoutToIni(layout) {
        var L = ['*' + cleanLayoutName(layout.name), cleanLayoutName(layout.desc || layout.name), String(layout.graphs.length)];
        function n(v) { return String(Math.round(v * 1000) / 1000); }
        layout.graphs.forEach(function (g) {
            L.push('');
            L.push('"' + g.x + '","' + g.y + '","' + (g.flags || []).join(' ') +
                (g.title ? ' TITLE=' + g.title + (g.sub ? '|' + g.sub : '') + '#' : '') + '"');
            L.push(g.range.map(n).join(',') + ',' + (g.auto ? '#TRUE#' : '#FALSE#'));
            L.push(g.rect.map(n).join(','));
        });
        return L.join('\r\n') + '\r\n';
    }
    /* Pure. Legacy .ini text -> a user-layout spec (same grammar as gen_workbench_layouts.py), or null. Only the map,
     * the G-G and channel graphs this viewer can draw are kept; the others are listed in spec.skipped. */
    function iniToSpec(text, fallbackName) {
        var lines = String(text || '').split(/\r?\n/);
        if (lines.length < 3) return null;
        var count = parseInt(lines[2], 10);
        if (!(count > 0) || count > 64) return null;
        var body = lines.slice(3).filter(function (s) { return s.trim(); });
        var graphs = [];
        for (var g = 0; g < count && 3 * g + 2 < body.length; g++) {
            var parts = body[3 * g].match(/"([^"]*)"/g);
            if (!parts || parts.length < 2) return null;
            graphs.push({ x: parts[0].slice(1, -1).toLowerCase().trim(), y: parts[1].slice(1, -1).toLowerCase().trim() });
        }
        if (!graphs.length) return null;
        var title = String(lines[0] || '').replace(/^\*/, '').replace(/^\d+_/, '').trim();
        var spec = layoutSpec({ name: 'x', graphs: graphs });
        spec.name = cleanLayoutName(title || fallbackName || 'Imported layout');
        spec.skipped = graphs.filter(function (q) {
            if ((q.x === 'x' && q.y === 'z') || (q.x === 'lat accel' && q.y === 'long accel')) return false;
            var m = /^filter\[(.+),\s*\d+\]$/.exec(q.y);
            return !((q.x === 'time' || q.x === 'distance') && catalogHas(m ? m[1] : q.y));
        }).map(function (q) { return q.y; });
        return spec;
    }
    /* Pure. f1c.layout/1 JSON (the editor's own export) -> spec, or null. */
    function jsonToSpec(doc) {
        if (!doc || doc.schema !== 'f1c.layout/1' || !doc.spec) return null;
        var lay = buildUserLayout(doc.spec, 'user:tmp');
        return lay ? lay.spec : null;
    }
    function layoutToJson(layout) {
        return JSON.stringify({ schema: 'f1c.layout/1', spec: layout.spec || layoutSpec(layout) }, null, 1);
    }

    function findLayout(file) {
        var all = allLayouts();   /* 1.2.5: the built-in layouts, then My layouts */
        for (var i = 0; i < all.length; i++)
            if (all[i].file === file) return all[i];
        return LAYOUTS[0] || null;
    }

    function lapTimeText(sec) {
        if (!Number.isFinite(sec) || sec <= 0) return '—';
        var m = Math.floor(sec / 60), s = sec - m * 60;
        return m + ':' + (s < 10 ? '0' : '') + s.toFixed(3);
    }

    /* A lap counts as a full timed lap only when it starts near the line and
     * has no recording gap; otherwise it is labeled partial and never fastest. */
    function lapIsFull(lap) {
        if (!lap || !lap.complete || !lap.samples || lap.samples.length < 2) return false;
        var first = lap.samples[0];
        if (first.dist === null || first.dist > 150) return false;
        for (var i = 1; i < lap.samples.length; i++) if (lap.samples[i].seg) return false;
        return lap.end > lap.start;
    }
    function lapSeconds(lap) { return lap ? lap.end - lap.start : NaN; }
    /* 1.2.4: the time that ranks laps (fastest): the game's official time when known, as the lap chips show it. */
    function lapRankTime(lap) {
        return lap && typeof lap.official_time === 'number' && isFinite(lap.official_time) && lap.official_time > 0 ?
            lap.official_time : lapSeconds(lap);
    }
    function lapStripSignature(side) {
        var shown = side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown;
        return side.letter + ':' + side.slot + ':' + side.state + ':' + side.label + ':' +
            side.displayLaps().map(function (l) { return [l.lap,l.start,l.end,l.complete,l.official_time].join(','); }).join(';') +
            ':' + (shown ? shown.lap + ',' + shown.start : 'open');
    }
    /* 1.2.5 (DVOA 2026-10-18): a lap can be the fastest when it is full, or when the page saw both line crossings and
     * the game's own lap time arrived. A pause in the live feed (background tab, Wi-Fi) leaves holes in the drawing,
     * but it does not change the game's time, so it no longer hides the fastest lap. */
    function lapIsRanked(lap) {
        if (lapIsFull(lap)) return true;
        return !!lap && lap.timed === true && typeof lap.official_time === 'number' && isFinite(lap.official_time) &&
            lap.official_time > 0 && lap.end > lap.start && !!lap.samples && lap.samples.length >= 2;
    }
    function fastestLap(laps) {
        var best = null;
        (laps || []).forEach(function (l) {
            if (lapIsRanked(l) && (!best || lapRankTime(l) < lapRankTime(best))) best = l;
        });
        return best;
    }
    /* 1.2.5: the server ring lap (/trace/status entry) that is the same lap as a lap held here: same start crossing
     * when this page saw it, else the same end crossing. Pure. */
    function serverLapFor(lap, statusLaps) {
        var best = null, bestD = 0.5;
        (statusLaps || []).forEach(function (l) {
            if (!l || !(l.flags & 1) || !Number.isFinite(l.start) || !Number.isFinite(l.t)) return;
            var d = lap.timed ? Math.abs(l.start - lap.start) : Math.abs(l.start + l.t - lap.end);
            if (d <= bestD) { best = l; bestD = d; }
        });
        return best;
    }

    /* 1.2.3 (DVOA report 2026-10-17): drop the oldest laps past the limit, but never the fastest full lap (the server
     * ring keeps the same lap), so an early fastest lap stays on the lap strip. */
    function trimLaps(laps) {
        while (laps.length > LIMITS.completedLapsKept) {
            var best = fastestLap(laps);
            laps.splice(laps[0] === best && laps.length > 1 ? 1 : 0, 1);
        }
    }

    /* DM#14 interpolation with a moving index: same formula as
     * timeAtDistance(), O(n) over a monotone query sequence. */
    function makeTimeLookup(points) {
        var pts = points;
        var k = 0;
        return function (target) {
            if (!pts.length) return null;
            if (k > 0 && pts[k - 1].dist > target) k = 0;
            if (!Number.isFinite(target)) return null;
            while (k < pts.length && (!Number.isFinite(pts[k].dist) || pts[k].dist < target)) k++;
            if (k >= pts.length) return null;
            if (pts[k].dist === target) return Number.isFinite(pts[k].t) ? pts[k].t : null;
            if (k === 0) return null;
            var a = pts[k - 1], b = pts[k], dd = b.dist - a.dist;
            if (b.seg || !Number.isFinite(a.dist) || !Number.isFinite(a.t) ||
                !Number.isFinite(b.t) || !(b.t > a.t) || !(dd > 0)) return null;
            var f = dd > 0.0001 ? (target - a.dist) / dd : 1;
            if (f < 0) f = 0;
            if (f > 1) f = 1;
            return a.t + f * (b.t - a.t);
        };
    }

    /* Incremental time difference (legacy "dtime"): lap time of `pts` minus
     * lap time of `ref` at the same distance, both measured from the first
     * distance they share. Positive = slower than the reference. */
    function dtimeSeries(pts, ref) {
        var out = [];
        if (!pts || !ref || !pts.length || !ref.length) return out;
        function firstDist(a) {
            for (var i = 0; i < a.length; i++) if (Number.isFinite(a[i].dist)) return a[i].dist;
            return null;
        }
        function lastDist(a) {
            for (var i = a.length - 1; i >= 0; i--) if (Number.isFinite(a[i].dist)) return a[i].dist;
            return null;
        }
        var d0 = Math.max(firstDist(pts), firstDist(ref));
        var dEnd = lastDist(ref);
        if (!Number.isFinite(d0) || !Number.isFinite(dEnd)) return out;
        var tS0 = timeAtDistance(pts, d0), tR0 = timeAtDistance(ref, d0);
        if (tS0 === null || tR0 === null) return out;
        var look = makeTimeLookup(ref);
        for (var i = 0; i < pts.length; i++) {
            var p = pts[i];
            if (p.dist === null || p.dist < d0 || p.dist > dEnd) { out.push(null); continue; }
            var tr = look(p.dist);
            out.push(tr === null || p.seg ? null : (p.t - tS0) - (tr - tR0));
        }
        return out;
    }

    /* Legacy channel name -> live/import source. `get` reads a workbench
     * sample; `reason` explains a channel with no source (never invented). */
    function channelInfo(name) {
        var n = String(name || '').toLowerCase().trim();
        var m = /^filter\[(.+),\s*(\d+)\]$/.exec(n);
        if (m) {
            var base = channelInfo(m[1]);
            var copy = {};
            Object.keys(base).forEach(function (k) { copy[k] = base[k]; });
            copy.filter = parseInt(m[2], 10);
            copy.label = base.label + ' (smoothed ' + copy.filter + ')';
            return copy;
        }
        function src(label, unit, get, extra) {
            var o = { name: n, label: label, unit: unit, get: get, reason: null, fixed: null, sym: false, filter: 0 };
            if (extra) Object.keys(extra).forEach(function (k) { o[k] = extra[k]; });
            return o;
        }
        function none(label, reason) {
            label = label.charAt(0).toUpperCase() + label.slice(1);
            return { name: n, label: label, unit: '', get: null, reason: reason, fixed: null, sym: false, filter: 0 };
        }
        switch (n) {
        case 'time': return src('Lap time', 's', function (p) { return p.t; });
        case 'distance': return src('Lap distance', 'm', function (p) { return p.dist; });
        case 'velocity': return src('Velocity', 'km/h', function (p) { return p.speed === null ? null : p.speed * 3.6; });
        case 'engine rpm': return src('Engine RPM', 'rpm', function (p) { return p.rpm; });
        case 'gear': return src('Gear', '', function (p) { return p.gear; });
        case 'throttle': return src('Throttle', '%', function (p) { return p.throttle === null ? null : p.throttle * 100; }, { fixed: [-5, 105] });
        case 'brake': return src('Brake', '%', function (p) { return p.brake === null ? null : p.brake * 100; }, { fixed: [-5, 105] });
        case 'steering': return src('Steering', '% input', function (p) { return p.steer === null ? null : p.steer * 100; }, { sym: true, steering: true });
        case 'lat accel': return src('Lateral G', 'g', function (p) { return p.glat === null ? null : p.glat / G_MS2; });
        case 'long accel': return src('Longitudinal G', 'g', function (p) { return p.glon === null ? null : p.glon / G_MS2; });
        case 'vert accel': return src('Vertical G', 'g', function (p) { return p.gvert === null || p.gvert === undefined ? null : p.gvert / G_MS2; },
            { emptyWhy: 'Vertical G is only in recorded or imported laps, not in the live feed.' });
        case 'yaw': return src('Yaw', 'deg', function (p) { return p.yaw === null || p.yaw === undefined ? null : p.yaw * 180 / Math.PI; },
            { emptyWhy: 'Yaw is only in recorded or imported laps, not in the live feed.' });
        case 'dtime': return src('Incremental time difference', 's', null, { dtime: true });
        case 'clutch': return none('Clutch', 'Clutch is not read from the game yet.');
        }
        var w = /^(fl|fr|rl|rr) (.+)$/.exec(n);
        if (w && w[2] === 'tyre wear') {
            var idx = WHEELS.indexOf(w[1]);
            return src(w[1].toUpperCase() + ' tyre wear', '% left', function (p) {
                return p.tread && Number.isFinite(p.tread[idx]) ? p.tread[idx] * 100 : null;
            }, { emptyWhy: 'Tyre wear is only in recorded or imported laps, not in the live feed.' });
        }
        var tt = /^(fl|fr|rl|rr) tyre (average|crown|camber) temp$/.exec(n);
        if (tt) {
            var ti = WHEELS.indexOf(tt[1]);
            if (tt[2] === 'average')
                return src(tt[1].toUpperCase() + ' tyre average temp', 'C', function (p) {
                    return p.ttemp && Number.isFinite(p.ttemp[ti]) ? p.ttemp[ti] : null;
                }, { emptyWhy: 'Tyre temperature is not in this sample (no valid temperature bit).' });
            return none(n, 'Zone temperatures are not in the per-sample feed. Live zone values: Tyres card (focus car); graphs: use the tyre average temp pages.');
        }
        if (/tyre .*temp/.test(n))
            return none(n, 'Tyre temperatures are not recorded per sample yet.');
        if (/wheelspin|wheel speed/.test(n))
            return none(n, 'Wheel speeds are not captured yet.');
        if (/ride height|damper|suspension/.test(n))
            return none(n, 'Suspension values are not proven in the game data yet (see CHANNEL_LEDGER).');
        /* rc.11 weather: weather is a session value and now has its own console
         * page; the legacy per-driver graphs point there instead of drawing. */
        if (/air temp|track temp|rain|dampness/.test(n))
            return none(n, 'Weather is a session value: see the Weather page (Conditions menu).');
        return none(n, 'This channel is not captured by the live feed or the lap format yet.');
    }

    function niceStep(span, count) {
        var raw = span / Math.max(1, count);
        var mag = Math.pow(10, Math.floor(Math.log10(raw)));
        var r = raw / mag;
        return (r <= 1 ? 1 : r <= 2 ? 2 : r <= 5 ? 5 : 10) * mag;
    }
    function ticks(lo, hi, count) {
        if (!(hi > lo)) return [];
        var step = niceStep(hi - lo, count), out = [];
        for (var v = Math.ceil(lo / step) * step; v <= hi + step * 1e-9; v += step)
            out.push(Math.abs(v) < step * 1e-9 ? 0 : v);
        return out;
    }
    function tickText(v, step) {
        var dec = step >= 1 ? 0 : Math.min(3, Math.ceil(-Math.log10(step)));
        return v.toFixed(dec);
    }

    /* ----------------------------------------------------------------- render */

    var STYLE = [
        '.wbx{display:flex;flex-direction:column;gap:5px;box-sizing:border-box;padding:6px 8px;min-height:420px}',
        '.wbx .wb-bar{display:flex;flex-wrap:wrap;gap:6px;align-items:center}',
        '.wbx .wb-bar select,.wbx .wb-bar button,.wbx .wb-chip{background:#21283a;color:#d7e0ea;border:1px solid #38445a;border-radius:4px;padding:2px 7px;font:12px system-ui,Segoe UI,sans-serif;cursor:pointer}',
        '.wbx .wb-status{margin-left:auto;color:#67d9a0;font-size:12px}',
        '.wbx .wb-laps{display:flex;flex-direction:column;gap:3px;font-size:12px}',
        '.wbx .wb-laprow{display:flex;flex-wrap:wrap;gap:4px;align-items:center}',
        '.wbx .wb-side{font-weight:bold;min-width:220px;max-width:320px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
        '.wbx .wb-chip.shown{outline:2px solid #e6edf3}',
        '.wbx .wb-chip.fast{background:#4b2a6b;border-color:#a970ff;color:#f2e6ff}',
        '.wbx .wb-chip.partial{color:#8494a7;font-style:italic}',
        '.wbx .wb-chip.ref{border-color:#c7d0da;background:#3a4250}',
        '.wbx .wb-flag{margin-left:4px;opacity:.55}.wbx .wb-chip.ref .wb-flag{opacity:1}',
        '.wbx .wb-stage{position:relative;flex:1;min-height:300px}',
        '.wbx .wb-g{position:absolute;box-sizing:border-box;padding:2px}',
        '.wbx .wb-g canvas{display:block;width:100%;height:100%;background:#0b1018;border:1px solid #263041;border-radius:4px}',
        '.wbx .wb-readout,.wbx .wb-reasons{background:#101722;border:1px solid #263041;border-radius:4px;padding:4px 8px;font-size:12px;color:#b9c6d4}',
        '.wbx .wb-reasons div{margin:1px 0}',
        /* 4.4.0 batch 1: fill mode (console) sizes to the parent instead of the window. */
        '.wbx.wb-fill{height:100%;min-height:0;padding:2px;gap:3px}',
        '.wbx.wb-fill .wb-stage{flex:1 1 0;min-height:0;overflow:hidden}',
        '.wbx.wb-fill .wb-readout{flex:none}',
        '.wbx.wb-fill .wb-reasons{flex:none;max-height:20%;overflow:auto}'
    ].join('\n');

    function mount(rootEl, wb, options) {
        if (!rootEl || typeof document === 'undefined') return null;
        var opts = options || {};
        var store = (typeof localStorage !== 'undefined') ? localStorage : null;
        function pref(key, dflt) {
            try { return (store && store.getItem('wb.' + key)) || dflt; } catch (e) { return dflt; }
        }
        function setPref(key, v) { try { if (store) store.setItem('wb.' + key, v); } catch (e) { /* private mode */ } }

        var layoutFile = opts.layout || (opts.view === 'inputs' ? 'chassis_inputs.ini' : pref('layout', DEFAULT_LAYOUT));
        if (!findLayout(layoutFile) || findLayout(layoutFile).file !== layoutFile) layoutFile = DEFAULT_LAYOUT;
        var xMode = pref('xmode', 'dist') === 'time' ? 'time' : 'dist';
        var xZoom = null;          /* shared [lo, hi] for every series graph */
        var mapViews = {};         /* per graph index: {cx, cz, scale} */
        var waypoints = null;
        if (opts.waypoints) {
            waypoints = Array.isArray(opts.waypoints) ? opts.waypoints
                : (Array.isArray(opts.waypoints.pts) ? opts.waypoints.pts : null);
        }
        var graphs = [];           /* {g, box, canvas, kind, info} */
        var cvs = {};
        var unavailable = {};      /* reason -> [channel labels] for the notes panel */
        var lapSig = '';

        function el(tag, cls, text) {
            var n = document.createElement(tag);
            if (cls) n.className = cls;
            if (text !== undefined) n.textContent = text;
            return n;
        }

        if (!document.getElementById('wb-parity-style')) {
            var st = el('style'); st.id = 'wb-parity-style'; st.textContent = STYLE;
            document.head.appendChild(st);
        }
        rootEl.innerHTML = '';
        rootEl.classList.add('wbx');
        rootEl.style.display = 'flex';
        rootEl.style.padding = '6px 8px';
        /* opts.fill (console, 4.4.0 batch 1): the root takes 100% of its parent and
         * the stage shrinks to what is left; no window-height fit. /workbench is unchanged. */
        if (opts.fill) { rootEl.classList.add('wb-fill'); rootEl.style.padding = '2px'; }

        var bar = el('div', 'wb-bar');
        var aSel = el('select'); var bSel = el('select');
        aSel.title = 'A side: player or any AI'; bSel.title = 'B side: player or any AI';
        var layoutSel = el('select'); layoutSel.title = 'Legacy viewer layout';
        allLayouts().forEach(function (l) {   /* 1.2.5: My layouts too */
            var o = el('option', null, l.desc + (l.desc !== l.name ? '  (' + l.name + ')' : ''));
            o.value = l.file; layoutSel.appendChild(o);
        });
        var xSel = el('select'); xSel.title = 'X axis for "time" graphs';
        [['dist', 'X: distance (m)'], ['time', 'X: lap time (s)']].forEach(function (p) {
            var o = el('option', null, p[1]); o.value = p[0]; xSel.appendChild(o);
        });
        var aBtn = el('button', null, 'Freeze A'); var bBtn = el('button', null, 'Freeze B');
        var importBtn = el('button', null, 'Import lap file…');
        importBtn.title = 'Legacy .lap/.spt (converted by the server) or f1c.trace/1 JSON; goes to B, or A when A is empty';
        var file = el('input'); file.type = 'file'; file.accept = '.json,.lap,.spt'; file.style.display = 'none';
        var resetBtn = el('button', null, 'Reset zoom');
        resetBtn.title = 'Also: double-click a graph. Wheel = zoom, drag = pan.';
        var status = el('span', 'wb-status', '');
        [el('b', null, 'A'), aSel, el('b', null, 'B'), bSel, layoutSel, xSel, aBtn, bBtn,
         importBtn, file, resetBtn, status].forEach(function (n) { bar.appendChild(n); });
        rootEl.appendChild(bar);

        var lapsEl = el('div', 'wb-laps');
        rootEl.appendChild(lapsEl);
        var stage = el('div', 'wb-stage');
        rootEl.appendChild(stage);
        var readout = el('div', 'wb-readout', 'cursor: —');
        rootEl.appendChild(readout);
        var reasonsEl = el('div', 'wb-reasons', '');
        rootEl.appendChild(reasonsEl);

        function fitHeight() {
            if (opts.fill || typeof window === 'undefined') return;
            var top = rootEl.getBoundingClientRect().top;
            rootEl.style.height = Math.max(420, window.innerHeight - top - 4) + 'px';
        }

        /* ---------------------------------------------------------- toolbar */

        function carOptions() {
            var out = [['-1', '(none)']];
            var ld = lapsDown(opts.roster);
            rosterOrder(opts.roster).forEach(function (c) {
                out.push([String(c.id), carLabel(c) + (ld[c.id] ? ' · +' + ld[c.id] + 'L' : '')]);
            });
            return out;
        }
        function fillSelect(sel, side) {
            sel.innerHTML = '';
            carOptions().forEach(function (pair) {
                var o = el('option', null, pair[1]); o.value = pair[0]; sel.appendChild(o);
            });
            sel.value = String(side.slot >= 0 ? side.slot : -1);
        }
        fillSelect(aSel, wb.a); fillSelect(bSel, wb.b);
        layoutSel.value = layoutFile;
        xSel.value = xMode;

        function pick(letter, sel) {
            var id = parseInt(sel.value, 10);
            var meta = (opts.roster || []).filter(function (c) { return c.id === id; })[0];
            wb.select(letter, id, meta);
            render();
        }
        aSel.onchange = function () { pick('A', aSel); };
        bSel.onchange = function () { pick('B', bSel); };
        function freezeToggle(side, btn) {
            if (side.state === 'FROZEN') { side.resume(); btn.textContent = 'Freeze ' + side.letter; }
            else { side.freeze(); btn.textContent = 'Resume ' + side.letter; }
            render();
        }
        aBtn.onclick = function () { freezeToggle(wb.a, aBtn); };
        bBtn.onclick = function () { freezeToggle(wb.b, bBtn); };
        layoutSel.onchange = function () { setLayout(layoutSel.value); };
        xSel.onchange = function () {
            xMode = xSel.value === 'time' ? 'time' : 'dist';
            setPref('xmode', xMode);
            xZoom = null; wb.cursorT = null;
            render();
        };
        importBtn.onclick = function () { file.click(); };
        file.onchange = function () {
            var f = file.files && file.files[0];
            if (!f) return;
            if (f.size > 16 * 1024 * 1024) {
                status.textContent = 'import refused: file exceeds 16 MB'; file.value = ''; return;
            }
            var letter = wb.a.slot < 0 && !wb.a.laps.length ? 'A' : 'B';
            function adopt(doc) {
                if (wb.importInto(letter, doc)) {
                    (letter === 'A' ? aSel : bSel).value = '-1';
                    if (wb.refLap && wb.refLap.letter === letter) wb.refLap = null;
                    status.textContent = 'imported into ' + letter + ' (recorded cars are kept; pick one again to show it)';
                    render();
                } else status.textContent = 'import refused: not an f1c.trace/1 document';
            }
            if (/\.(lap|spt)$/i.test(f.name)) {
                /* Legacy binary: convert server-side through the C decoder
                 * (POST /trace/import/legacy) so units/parity live in one place. */
                var fr = new FileReader();
                fr.onload = function () {
                    fetch('/trace/import/legacy?car=' + letter, {
                        method: 'POST', body: fr.result,
                        headers: { 'Content-Type': 'application/octet-stream' }
                    })
                        .then(function (r) { return r.json(); })
                        .then(function (doc) {
                            if (doc && doc.error) { status.textContent = 'import refused: ' + doc.error; return; }
                            adopt(doc);
                        })
                        .catch(function (e) { status.textContent = 'import failed: ' + e.message; });
                };
                fr.readAsArrayBuffer(f);
                file.value = '';
                return;
            }
            var reader = new FileReader();
            reader.onload = function () {
                try {
                    var doc = JSON.parse(String(reader.result));
                    if (doc && doc.schema === 'f1c.trace/1') adopt(doc);
                    else if (doc && doc.laps) {
                        wb.importInto('A', doc.laps[0]);
                        if (doc.laps[1]) wb.importInto('B', doc.laps[1]);
                        render();
                    } else status.textContent = 'import refused: not an f1c.trace/1 document';
                } catch (e) { status.textContent = 'import refused: ' + e.message; }
            };
            reader.readAsText(f, 'utf-8');
            file.value = '';
        };
        resetBtn.onclick = function () { xZoom = null; mapViews = {}; render(); };

        /* --------------------------------------------------------- lap strip */

        function refSamples(side) {
            if (wb.refLap) {
                var s = wb.refLap.lap.samples;
                return s === side.displayPoints() ? null : s;
            }
            var reference = side.frozenSnapshot ? side.frozenSnapshot.reference : side.reference;
            if (reference && reference.samples !== side.displayPoints())
                return reference.samples;
            return null;
        }

        function renderLaps() {
            var sig = wb.allSides().map(lapStripSignature).join('|') + '|' +
                (wb.refLap ? wb.refLap.letter + wb.refLap.lap.lap + wb.refLap.lap.start : '-');
            if (sig === lapSig) return;
            lapSig = sig;
            lapsEl.innerHTML = '';
            wb.allSides().forEach(function (side) {
                var row = el('div', 'wb-laprow');
                var name = el('span', 'wb-side', side.letter + ' · ' + side.label + ' · ' + side.state);
                name.style.color = side.color;
                name.title = name.textContent;
                row.appendChild(name);
                var shown = side.frozenSnapshot ? side.frozenSnapshot.shown : side.shown;
                var laps = side.displayLaps();
                function show(lap) {
                    if (side.frozenSnapshot) side.frozenSnapshot.shown = lap;
                    else side.shown = lap;
                    render();
                }
                var fast = fastestLap(laps);
                if (side.slot >= 0) {
                    var open = el('button', 'wb-chip' + (shown ? '' : ' shown'),
                        side.state === 'FROZEN' ? 'Open lap (frozen)' : 'Open lap (live)');
                    open.title = 'Show the lap being driven now';
                    open.onclick = function () { show(null); };
                    row.appendChild(open);
                }
                laps.slice().reverse().forEach(function (lap) {
                    var full = lapIsFull(lap);
                    var isRef = wb.refLap && wb.refLap.lap === lap;
                    var chip = el('button', 'wb-chip' + (shown === lap ? ' shown' : '') +
                        (lap === fast ? ' fast' : '') + (full ? '' : ' partial') + (isRef ? ' ref' : ''));
                    chip.appendChild(document.createTextNode(
                        (lap.src === 'import' ? 'Import ' : '') + 'L' + lap.lap + ' ' +
                        (full ? '≈' + lapTimeText(lapSeconds(lap)) : 'partial')));
                    chip.title = (full ? 'Lap time from 10 Hz samples (±0.1 s); official times are in the Laps tab.'
                        : 'Partial lap: recording started mid-lap or has a gap; not timed.') +
                        (lap === fast ? ' Fastest full lap of this side.' : '') +
                        ' Click to show; click the flag to use as reference.';
                    var flag = el('span', 'wb-flag', '⚑');
                    flag.title = 'Use as the reference lap (dashed grey; used by dtime and the delta)';
                    chip.appendChild(flag);
                    chip.onclick = function (ev) {
                        if (ev.target === flag) {
                            wb.refLap = isRef ? null : { letter: side.letter, lap: lap };
                        } else { show(shown === lap ? null : lap); return; }
                        render();
                    };
                    row.appendChild(chip);
                });
                if (side.slot < 0 && !side.laps.length)
                    row.appendChild(el('span', 'wb-chip partial', side.letter === 'B'
                        ? 'empty — pick a car in the B menu or import a lap file to compare'
                        : 'empty — pick a car in the A menu'));
                lapsEl.appendChild(row);
            });
        }

        /* ------------------------------------------------------------ stage */

        function graphKind(g) {
            if (g.x === 'null' || g.bmp) return 'image';
            if (g.x === 'x' && g.y === 'z') return 'map';
            if (g.x === 'time' || g.x === 'distance') return 'series';
            return 'scatter';
        }

        function setLayout(fileName) {
            var l = findLayout(fileName);
            if (!l) return;
            layoutFile = l.file;
            layoutSel.value = layoutFile;
            setPref('layout', layoutFile);
            stage.innerHTML = '';
            graphs = []; cvs = {}; mapViews = {};
            l.graphs.forEach(function (g, i) {
                if (graphKind(g) === 'image') return;   /* B6: the viewer bitmap is never served; no box over the graphs */
                var r = g.rect.length === 4 ? g.rect : [0, 1, 0, 1];
                var box = el('div', 'wb-g');
                box.style.left = (r[0] * 100) + '%';
                box.style.width = ((r[1] - r[0]) * 100) + '%';
                box.style.top = (r[2] * 100) + '%';
                box.style.height = ((r[3] - r[2]) * 100) + '%';
                var c = el('canvas');
                box.appendChild(c);
                stage.appendChild(box);
                var kind = graphKind(g);
                var gr = { g: g, i: i, box: box, canvas: c, kind: kind,
                           info: kind === 'image' ? null : channelInfo(g.y),
                           xinfo: kind === 'scatter' ? channelInfo(g.x) : null };
                graphs.push(gr);
                var key = kind === 'map' ? 'track' : kind === 'scatter' ? 'gg' : (gr.info && gr.info.name) || ('g' + i);
                if (!cvs[key]) cvs[key] = c;
                attach(gr);
            });
            render();
        }

        /* ------------------------------------------------------------ data */

        function xOf(p, t0) {
            if (xMode === 'dist') return p.dist;
            return t0 === null ? null : p.t - t0;
        }
        function firstT(pts) { return pts.length ? pts[0].t : null; }

        /* Series values for one side, filtered when the channel asks for it. */
        function valuesFor(info, pts, side) {
            var vals;
            if (info.dtime) vals = dtimeSeries(pts, refSamples(side));
            else vals = pts.map(function (p) { var v = info.get(p); return Number.isFinite(v) ? v : null; });
            if (info.filter > 1) {
                var out = [], win = [], sum = 0;
                for (var i = 0; i < vals.length; i++) {
                    if (pts[i].seg || vals[i] === null) { win = []; sum = 0; }
                    if (vals[i] === null) { out.push(null); continue; }
                    win.push(vals[i]); sum += vals[i];
                    if (win.length > info.filter) sum -= win.shift();
                    out.push(sum / win.length);
                }
                vals = out;
            }
            return vals;
        }

        function fullX(sets) {
            var lo = Infinity, hi = -Infinity;
            sets.forEach(function (s) {
                var t0 = firstT(s.pts);
                s.pts.forEach(function (p) {
                    var x = xOf(p, t0);
                    if (Number.isFinite(x)) { if (x < lo) lo = x; if (x > hi) hi = x; }
                });
            });
            if (xMode === 'dist') {
                lo = 0;
                hi = Math.max(Number.isFinite(hi) ? hi : 0, wb.trackLen || 0) || 1000;
            } else if (!Number.isFinite(lo)) { lo = 0; hi = 100; }
            else lo = Math.min(0, lo);
            if (hi - lo < 1) hi = lo + 1;
            return [lo, hi];
        }

        function displaySets() {
            var sets = [];
            wb.allSides().forEach(function (side) {
                var pts = side.displayPoints();
                if (pts.length) sets.push({ side: side, pts: pts, color: side.color, dash: null });
            });
            return sets;
        }
        function refSet() {
            if (!wb.refLap) return null;
            return { side: wb.sideOf(wb.refLap.letter) || wb.a, pts: wb.refLap.lap.samples,
                     color: THEME.ref, dash: [5, 4], ref: true };
        }

        /* ------------------------------------------------------------ draw */

        function sizeCanvas(c) {
            var dpr = (typeof window !== 'undefined' && window.devicePixelRatio) || 1;
            var w = Math.max(50, Math.floor(c.clientWidth * dpr));
            var h = Math.max(40, Math.floor(c.clientHeight * dpr));
            if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
            var ctx = c.getContext('2d');
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            ctx.clearRect(0, 0, w, h);
            return { ctx: ctx, w: w / dpr, h: h / dpr };
        }

        function plotBox(gr, s) {
            var f = gr.g.flags;
            var noY = f.indexOf('NOYAXIS') >= 0, noX = f.indexOf('NOXAXIS') >= 0;
            return { l: noY ? 6 : 50, r: 10, t: 18, b: noX ? 6 : 22, w: s.w, h: s.h };
        }

        function titleOf(gr) {
            if (gr.kind === 'map') return 'Track map';
            if (gr.kind === 'scatter') return 'G-G: ' + gr.info.label + ' vs ' + gr.xinfo.label;
            var t = gr.g.title || gr.info.label;
            return t + (gr.info.unit ? ' (' + gr.info.unit + ')' : '');
        }

        function drawFrameText(ctx, gr, s, extra) {
            ctx.fillStyle = THEME.title; ctx.font = THEME.titleFont;
            ctx.textAlign = THEME.titleRight ? 'right' : 'left'; ctx.textBaseline = 'alphabetic';
            ctx.fillText(titleOf(gr) + (extra || ''), THEME.titleRight ? s.w - 8 : 6, 13);
            ctx.textAlign = 'left';
        }

        function drawUnavailable(ctx, s, lines) {
            ctx.fillStyle = THEME.unavailable; ctx.font = '12px ' + THEME.font;
            ctx.textAlign = 'center';
            /* 1.2.0: wrap each line to the graph width so a long reason never runs off the canvas */
            var maxW = Math.max(60, s.w - 16), out = [];
            lines.forEach(function (ln) {
                var words = String(ln).split(' '), cur = '';
                words.forEach(function (w) {
                    var t = cur ? cur + ' ' + w : w;
                    if (cur && ctx.measureText(t).width > maxW) { out.push(cur); cur = w; } else cur = t;
                });
                out.push(cur);
            });
            out.forEach(function (ln, i) {
                ctx.fillText(ln, s.w / 2, s.h / 2 + (i - (out.length - 1) / 2) * 16);
            });
            ctx.textAlign = 'left';
        }

        function drawGrid(ctx, b, x0, x1, y0, y1, flags, xUnit, yUnit) {
            var W = b.w - b.l - b.r, H = b.h - b.t - b.b;
            var grid = flags.indexOf('NOGRID') < 0;
            var minor = flags.indexOf('MINORGRID') >= 0;
            ctx.strokeStyle = THEME.frame; ctx.lineWidth = 1;
            ctx.strokeRect(b.l + 0.5, b.t + 0.5, W, H);
            ctx.font = '10px ' + THEME.font; ctx.fillStyle = THEME.axis;
            var xt = ticks(x0, x1, Math.max(2, Math.floor(W / 80)));
            var xs = xt.length > 1 ? xt[1] - xt[0] : 1;
            var yt = ticks(y0, y1, Math.max(2, Math.floor(H / 32)));
            var ys = yt.length > 1 ? yt[1] - yt[0] : 1;
            function gx(v) { return b.l + (v - x0) / (x1 - x0) * W; }
            function gy(v) { return b.t + (y1 - v) / (y1 - y0) * H; }
            if (grid) {
                if (minor) {
                    ctx.strokeStyle = THEME.minor;
                    ctx.beginPath();
                    ticks(x0, x1, Math.max(4, Math.floor(W / 20))).forEach(function (v) { ctx.moveTo(Math.round(gx(v)) + 0.5, b.t); ctx.lineTo(Math.round(gx(v)) + 0.5, b.t + H); });
                    ticks(y0, y1, Math.max(4, Math.floor(H / 10))).forEach(function (v) { ctx.moveTo(b.l, Math.round(gy(v)) + 0.5); ctx.lineTo(b.l + W, Math.round(gy(v)) + 0.5); });
                    ctx.stroke();
                }
                ctx.strokeStyle = THEME.grid;
                ctx.beginPath();
                xt.forEach(function (v) { ctx.moveTo(Math.round(gx(v)) + 0.5, b.t); ctx.lineTo(Math.round(gx(v)) + 0.5, b.t + H); });
                yt.forEach(function (v) { ctx.moveTo(b.l, Math.round(gy(v)) + 0.5); ctx.lineTo(b.l + W, Math.round(gy(v)) + 0.5); });
                ctx.stroke();
                if (y0 < 0 && y1 > 0) {
                    ctx.strokeStyle = THEME.zero;
                    ctx.beginPath(); ctx.moveTo(b.l, Math.round(gy(0)) + 0.5); ctx.lineTo(b.l + W, Math.round(gy(0)) + 0.5); ctx.stroke();
                }
            }
            if (flags.indexOf('NOYAXIS') < 0) {
                ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
                yt.forEach(function (v) { ctx.fillText(tickText(v, ys), b.l - 4, gy(v)); });
            }
            if (flags.indexOf('NOXAXIS') < 0) {
                ctx.textAlign = 'center'; ctx.textBaseline = 'top';
                xt.forEach(function (v, i) {
                    var txt = tickText(v, xs) + (i === xt.length - 1 && xUnit ? ' ' + xUnit : '');
                    ctx.fillText(txt, Math.min(b.w - 18, gx(v)), b.t + H + 4);
                });
            }
            ctx.textAlign = 'left'; ctx.textBaseline = 'alphabetic';
            return { gx: gx, gy: gy, W: W, H: H };
        }

        function legend(ctx, s, sets) {
            ctx.font = '10px ' + THEME.font;
            if (THEME.titleRight) {
                ctx.textAlign = 'left';
                var lx = 8;
                sets.forEach(function (set) {
                    var txt = set.ref ? 'ref' : set.side.letter;
                    ctx.fillStyle = set.color;
                    ctx.fillText(txt, lx, 13);
                    lx += ctx.measureText(txt).width + 8;
                });
                return;
            }
            ctx.textAlign = 'right';
            var x = s.w - 8;
            sets.slice().reverse().forEach(function (set) {
                var txt = set.ref ? 'ref' : set.side.letter;
                ctx.fillStyle = set.color;
                ctx.fillText(txt, x, 13);
                x -= ctx.measureText(txt).width + 8;
            });
            ctx.textAlign = 'left';
        }

        function seriesRange(gr) {
            var info = gr.info, r = gr.g.range;
            if (info.fixed) return info.fixed.slice();
            var lo = Infinity, hi = -Infinity;
            gr.data.forEach(function (d) {
                d.vals.forEach(function (v) { if (v !== null) { if (v < lo) lo = v; if (v > hi) hi = v; } });
            });
            var have = Number.isFinite(lo);
            if (info.sym) {
                var m = have ? Math.max(Math.abs(lo), Math.abs(hi)) : 1;
                m = m > 0 ? m * 1.1 : 1;
                return [-m, m];
            }
            if (gr.g.auto || r.length < 4) {
                if (!have) return [0, 1];
                var pad = (hi - lo) * 0.05 || 1;
                return [lo - pad, hi + pad];
            }
            /* INI range, widened (never clipped) to the data. */
            return [have ? Math.min(r[2], lo) : r[2], have ? Math.max(r[3], hi) : r[3]];
        }

        function cursorPoint(pts) {
            if (!pts.length) return null;
            var best = null, bd = Infinity, t0 = firstT(pts);
            for (var i = 0; i < pts.length; i++) {
                var p = pts[i];
                var d = xMode === 'dist'
                    ? (p.dist === null || wb.cursor === null ? Infinity : Math.abs(p.dist - wb.cursor))
                    : (wb.cursorT === null ? Infinity : Math.abs(p.t - t0 - wb.cursorT));
                if (d < bd) { bd = d; best = p; }
            }
            return best;
        }
        function cursorX() { return xMode === 'dist' ? wb.cursor : wb.cursorT; }

        function drawSeries(gr) {
            var s = sizeCanvas(gr.canvas), ctx = s.ctx, info = gr.info;
            if (!info.get && !info.dtime) {
                drawFrameText(ctx, gr, s);
                drawUnavailable(ctx, s, ['Unavailable', info.reason]);
                (unavailable[info.reason] = unavailable[info.reason] || []).push(info.label);
                return;
            }
            var sets = displaySets();
            var rs = refSet();
            if (rs && !info.dtime) sets.push(rs);
            gr.data = sets.map(function (set) {
                return { set: set, vals: valuesFor(info, set.pts, set.side), t0: firstT(set.pts) };
            });
            var xr = xZoom || fullX(sets);
            var yr = seriesRange(gr);
            var b = plotBox(gr, s);
            var m = drawGrid(ctx, b, xr[0], xr[1], yr[0], yr[1], gr.g.flags,
                xMode === 'dist' ? 'm' : 's', info.unit);
            gr.xr = xr; gr.b = b;
            var lines = gr.g.flags.indexOf('NOLINES') < 0;
            var dots = gr.g.flags.indexOf('DOTS') >= 0;
            var wide = gr.g.flags.indexOf('WIDELINES') >= 0;
            ctx.save();
            ctx.beginPath(); ctx.rect(b.l, b.t, m.W, m.H); ctx.clip();
            var any = false;
            gr.data.forEach(function (d) {
                var pts = d.set.pts, vals = d.vals;
                var step = Math.max(1, Math.ceil(pts.length / LIMITS.drawnPoints));
                ctx.strokeStyle = d.set.color; ctx.fillStyle = d.set.color;
                ctx.lineWidth = wide ? 2.5 : 1.4;
                ctx.setLineDash(d.set.dash || []);
                ctx.beginPath();
                var open = false;
                for (var i = 0; i < pts.length; i++) {
                    var p = pts[i], v = vals[i];
                    if (p.seg) open = false;
                    var x = xOf(p, d.t0);
                    if (!Number.isFinite(v) || !Number.isFinite(x)) { open = false; continue; }
                    if (i % step !== 0 && !p.seg) continue;
                    any = true;
                    var px = m.gx(x), py = m.gy(v);
                    if (dots) ctx.fillRect(px - 1, py - 1, 2, 2);
                    if (!lines) continue;
                    if (!open) { ctx.moveTo(px, py); open = true; } else ctx.lineTo(px, py);
                }
                if (lines) ctx.stroke();
            });
            ctx.setLineDash([]);
            var cx = cursorX();
            if (cx !== null && cx >= xr[0] && cx <= xr[1]) {
                ctx.strokeStyle = THEME.cursor; ctx.lineWidth = 1;
                ctx.beginPath(); ctx.moveTo(Math.round(m.gx(cx)) + 0.5, b.t); ctx.lineTo(Math.round(m.gx(cx)) + 0.5, b.t + m.H); ctx.stroke();
                gr.data.forEach(function (d) {
                    var p = cursorPoint(d.set.pts);
                    if (!p) return;
                    var v = d.vals[d.set.pts.indexOf(p)];
                    if (v === null || v === undefined) return;
                    ctx.fillStyle = d.set.color;
                    ctx.beginPath(); ctx.arc(m.gx(xOf(p, d.t0)), m.gy(v), 3, 0, Math.PI * 2); ctx.fill();
                });
            }
            ctx.restore();
            drawFrameText(ctx, gr, s);
            legend(ctx, s, sets);
            if (!any) {
                var why = [];
                if (info.dtime) why.push('Needs a reference lap: finish a lap, or pick one with the ⚑ flag in the lap strip.');
                else if (info.steering) wb.allSides().forEach(function (side) {
                    if (side.slot >= 0 && side.steerWhy) why.push(side.letter + ': ' + side.steerWhy);
                });
                else if (info.emptyWhy) why.push(info.emptyWhy);
                if (!why.length) why.push(sets.length ? 'No valid samples for this channel yet.' : 'No data yet: pick a car or import a lap.');
                drawUnavailable(ctx, s, why);
                if (sets.length && (info.dtime || info.steering || info.emptyWhy))
                    why.forEach(function (w) { (unavailable[w] = unavailable[w] || []).push(info.label); });
            }
        }

        function drawScatter(gr) {
            var s = sizeCanvas(gr.canvas), ctx = s.ctx;
            var xi = gr.xinfo, yi = gr.info;
            if (!xi.get || !yi.get) {
                drawFrameText(ctx, gr, s);
                drawUnavailable(ctx, s, ['Unavailable', (xi.reason || yi.reason)]);
                return;
            }
            var sets = displaySets();
            /* Symmetric, equal axes (friction circle): limit from the INI
             * ranges and the data, rounded up to a whole g. */
            var lim = 0, r = gr.g.range;
            r.forEach(function (v) { if (Number.isFinite(v)) lim = Math.max(lim, Math.abs(v)); });
            if (!(lim > 0)) lim = 3;
            sets.forEach(function (set) {
                set.pts.forEach(function (p) {
                    var a = xi.get(p), c = yi.get(p);
                    if (Number.isFinite(a)) lim = Math.max(lim, Math.abs(a));
                    if (Number.isFinite(c)) lim = Math.max(lim, Math.abs(c));
                });
            });
            lim = Math.ceil(lim);
            var full = { l: 46, r: 10, t: 18, b: 30, w: s.w, h: s.h };
            var side = Math.max(20, Math.min(full.w - full.l - full.r, full.h - full.t - full.b));
            var ox = full.l + (full.w - full.l - full.r - side) / 2;
            var b = { l: ox, r: full.w - ox - side, t: full.t, b: full.h - full.t - side, w: full.w, h: full.h };
            var m = drawGrid(ctx, b, -lim, lim, -lim, lim, [], 'g', 'g');
            /* axis zero cross */
            ctx.strokeStyle = THEME.zero;
            ctx.beginPath();
            ctx.moveTo(m.gx(0) + 0.5, b.t); ctx.lineTo(m.gx(0) + 0.5, b.t + m.H);
            ctx.stroke();
            ctx.fillStyle = THEME.axis; ctx.font = '10px ' + THEME.font;
            ctx.textAlign = 'center';
            ctx.fillText(xi.label + ' (g) →', b.l + m.W / 2, b.t + m.H + 26);
            ctx.save();
            ctx.translate(b.l - 34, b.t + m.H / 2); ctx.rotate(-Math.PI / 2);
            ctx.fillText(yi.label + ' (g) →', 0, 0);
            ctx.restore();
            ctx.textAlign = 'left';
            var any = false;
            sets.forEach(function (set) {
                ctx.fillStyle = set.color;
                var step = Math.max(1, Math.ceil(set.pts.length / LIMITS.drawnPoints));
                for (var i = 0; i < set.pts.length; i += step) {
                    var p = set.pts[i], a = xi.get(p), c = yi.get(p);
                    if (!Number.isFinite(a) || !Number.isFinite(c)) continue;
                    any = true;
                    ctx.fillRect(m.gx(a) - 1, m.gy(c) - 1, 2, 2);
                }
                var cp = (wb.cursor !== null || wb.cursorT !== null) ? cursorPoint(set.pts) : null;
                if (cp && Number.isFinite(xi.get(cp)) && Number.isFinite(yi.get(cp))) {
                    ctx.strokeStyle = THEME.cursor; ctx.lineWidth = 1.5;
                    ctx.beginPath(); ctx.arc(m.gx(xi.get(cp)), m.gy(yi.get(cp)), 5, 0, Math.PI * 2); ctx.stroke();
                }
            });
            drawFrameText(ctx, gr, s);
            legend(ctx, s, sets);
            if (!any) drawUnavailable(ctx, s, [sets.length ? 'No valid G samples yet.' : 'No data yet: pick a car or import a lap.']);
        }

        function drawMap(gr) {
            var s = sizeCanvas(gr.canvas), ctx = s.ctx;
            var b = { l: 6, r: 6, t: 18, b: 6 };
            var W = s.w - b.l - b.r, H = s.h - b.t - b.b;
            var sets = displaySets();
            var rs = refSet(); if (rs) sets.push(rs);
            var minX = Infinity, maxX = -Infinity, minZ = Infinity, maxZ = -Infinity;
            function acc(x, z) {
                if (!Number.isFinite(x) || !Number.isFinite(z)) return;
                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
            }
            if (waypoints) waypoints.forEach(function (p) { acc(p[0], p[1]); });
            sets.forEach(function (set) { set.pts.forEach(function (p) { acc(p.x, p.z); }); });
            var haveGeo = Number.isFinite(minX);
            if (!haveGeo) { minX = 0; maxX = 1; minZ = 0; maxZ = 1; }
            var fit = Math.min(W / ((maxX - minX) || 1), H / ((maxZ - minZ) || 1)) * 0.92;
            var view = mapViews[gr.i];
            if (!view) view = { cx: (minX + maxX) / 2, cz: (minZ + maxZ) / 2, scale: fit };
            gr.view = view; gr.mapBox = { l: b.l, t: b.t, W: W, H: H };
            function mx(x) { return b.l + W / 2 + (x - view.cx) * view.scale; }
            function mz(z) { return b.t + H / 2 - (z - view.cz) * view.scale; }
            gr.mx = mx; gr.mz = mz;
            ctx.save();
            ctx.beginPath(); ctx.rect(b.l, b.t, W, H); ctx.clip();
            if (waypoints && waypoints.length) {
                ctx.strokeStyle = THEME.track; ctx.lineWidth = 6; ctx.lineJoin = 'round';
                ctx.beginPath();
                waypoints.forEach(function (p, i) {
                    if (i === 0) ctx.moveTo(mx(p[0]), mz(p[1])); else ctx.lineTo(mx(p[0]), mz(p[1]));
                });
                ctx.closePath();
                ctx.stroke();
            }
            sets.forEach(function (set) {
                var pts = set.pts;
                ctx.strokeStyle = set.color; ctx.lineWidth = 1.5;
                ctx.setLineDash(set.dash || []);
                ctx.beginPath();
                var open = false;
                var step = Math.max(1, Math.ceil(pts.length / LIMITS.clientTrackPoints));
                for (var i = 0; i < pts.length; i++) {
                    var p = pts[i];
                    if (p.seg) open = false;
                    if (!Number.isFinite(p.x) || !Number.isFinite(p.z)) { open = false; continue; }
                    if (i % step !== 0 && !p.seg) continue;
                    if (!open) { ctx.moveTo(mx(p.x), mz(p.z)); open = true; } else ctx.lineTo(mx(p.x), mz(p.z));
                }
                ctx.stroke();
                ctx.setLineDash([]);
                if (set.ref) return;
                var last = pts[pts.length - 1];
                if (!set.side.shown && last && last.x !== null && last.z !== null) {
                    ctx.fillStyle = set.color;
                    ctx.beginPath(); ctx.arc(mx(last.x), mz(last.z), 3.5, 0, Math.PI * 2); ctx.fill();
                }
                var cp = (wb.cursor !== null || wb.cursorT !== null) ? cursorPoint(pts) : null;
                if (cp && cp.x !== null && cp.z !== null) {
                    ctx.fillStyle = THEME.marker; ctx.strokeStyle = set.color; ctx.lineWidth = 2.5;
                    ctx.beginPath(); ctx.arc(mx(cp.x), mz(cp.z), 6, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
                }
            });
            ctx.restore();
            /* TAPE2 V7: the cars of the sides at their current frame position (the viewer's shared helper) */
            if (opts.frameBalls) opts.frameBalls(ctx, function (x, z) { return [mx(x), mz(z)]; });
            drawFrameText(ctx, gr, s);
            legend(ctx, s, sets);
            if (!haveGeo) drawUnavailable(ctx, s, ['No track line yet.',
                'Imported laps carry map-frame coordinates only; live cars draw here.']);
        }

        function drawImage(gr) {
            var s = sizeCanvas(gr.canvas), ctx = s.ctx;
            ctx.fillStyle = THEME.title; ctx.font = THEME.titleFont;
            ctx.fillText('Plan view image', 6, 13);
            drawUnavailable(ctx, s, ['Unavailable', 'The viewer bitmap (' + (gr.g.bmp || 'image') + ') is not served by the workbench.']);
        }

        /* ---------------------------------------------------------- readout */

        function fmt(v, d, unit) { return v === null || !Number.isFinite(v) ? '—' : v.toFixed(d) + (unit || ''); }

        function renderReadout() {
            if (wb.cursor === null && wb.cursorT === null) {
                readout.textContent = 'Cursor: move over a graph or the track map. Wheel = zoom, drag = pan, double-click = reset.';
                return;
            }
            var rows = [];
            var head = xMode === 'dist' ? 'd=' + Math.round(wb.cursor) + ' m' : 't=' + fmt(wb.cursorT, 2, ' s');
            wb.allSides().forEach(function (side) {
                var pts = side.displayPoints();
                var p = cursorPoint(pts);
                if (!p) return;
                rows.push(side.letter + ': ' + (p.dist === null ? '' : Math.round(p.dist) + ' m ') +
                    fmt(p.speed === null ? null : p.speed * 3.6, 0, ' km/h') + ' · ' +
                    (p.rpm === null ? '—' : formatRpmValue(p.rpm)) + ' rpm · gear ' +
                    (p.gear === null ? '—' : p.gear) + ' · thr ' +
                    fmt(p.throttle === null ? null : p.throttle * 100, 0, '%') + ' · brk ' +
                    fmt(p.brake === null ? null : p.brake * 100, 0, '%'));
            });
            /* DM#14: t2 - t1 at the cursor distance, each lap timed from the
             * first distance both laps share. */
            var pa = wb.a.displayPoints();
            var d = xMode === 'dist' ? wb.cursor : (cursorPoint(pa) || {}).dist;
            /* DM#14 per extra side: t(L) - t(A) at the cursor distance, each lap
             * timed from the first distance both laps share. */
            wb.allSides().forEach(function (sb) {
                var pb = sb.displayPoints();
                if (sb.letter === 'A' || !(Number.isFinite(d) && pa.length && pb.length)) return;
                var fa = pa.filter(function (p) { return p.dist !== null; });
                var fb = pb.filter(function (p) { return p.dist !== null; });
                if (!fa.length || !fb.length) return;
                var d0 = Math.max(fa[0].dist, fb[0].dist);
                var ta = timeAtDistance(pa, d), tb = timeAtDistance(pb, d);
                var ta0 = timeAtDistance(pa, d0), tb0 = timeAtDistance(pb, d0);
                if (d >= d0 && ta !== null && tb !== null && ta0 !== null && tb0 !== null) {
                    ta -= ta0; tb -= tb0;
                    rows.push('Δ t2−t1 (' + sb.letter + '−A) = ' + (tb - ta >= 0 ? '+' : '') + (tb - ta).toFixed(3) + ' s');
                }
            });
            readout.textContent = 'Cursor ' + head + ' · ' + (rows.length ? rows.join('   |   ') : 'no sample at cursor');
        }

        function renderNotes() {
            reasonsEl.innerHTML = '';
            wb.allSides().forEach(function (side) {
                var line = el('div');
                var head = el('b', null, side.letter + ': ');
                head.style.color = side.color;
                line.appendChild(head);
                var text;
                if (side.slot < 0 && !side.laps.length && !side.points.length)
                    text = side.letter === 'B'
                        ? 'Empty. Pick a car in the B menu, or import a lap file, to compare against A.'
                        : 'Empty. Pick a car in the A menu.';
                else text = side.notes.length ? side.notes.map(function (n) {
                    return n + (side.noteCounts[n] > 1 ? ' (×' + side.noteCounts[n] + ')' : '');
                }).join('  ·  ') : 'No problems seen.';
                if (side.dropped) text += '  ·  ' + side.dropped + ' oldest samples dropped (display ring is ' + LIMITS.livePointsPerCar + ').';
                line.appendChild(document.createTextNode(text));
                reasonsEl.appendChild(line);
            });
            Object.keys(unavailable).forEach(function (why) {
                var uniq = unavailable[why].filter(function (v, i, a) { return a.indexOf(v) === i; });
                reasonsEl.appendChild(el('div', null, 'Not shown — ' + uniq.join(', ') + ': ' + why));
            });
        }

        /* ----------------------------------------------------------- render */

        function render() {
            fitHeight();
            unavailable = {};
            renderLaps();
            graphs.forEach(function (gr) {
                if (!gr.canvas.clientWidth) return;
                if (gr.kind === 'map') drawMap(gr);
                else if (gr.kind === 'scatter') drawScatter(gr);
                else if (gr.kind === 'image') drawImage(gr);
                else drawSeries(gr);
            });
            var layout = findLayout(layoutFile);
            status.textContent = (opts.label || 'DRAFT') + ' · ' + (wb.trackKey || 'no track') +
                (wb.trackLen ? ' ' + Math.round(wb.trackLen) + ' m' : '') +
                ' · ' + (layout ? layout.desc : '') + ' · frames ' + wb.frames;
            renderReadout();
            renderNotes();
            aBtn.textContent = (wb.a.state === 'FROZEN' ? 'Resume' : 'Freeze') + ' A';
            bBtn.textContent = (wb.b.state === 'FROZEN' ? 'Resume' : 'Freeze') + ' B';
        }

        var pending = false;
        function scheduleRender() {
            if (pending) return;
            pending = true;
            /* TAPE2 V4: a timer, not requestAnimationFrame: a host that never runs the frame callback (the Browser pane)
             * left pending set and the legacy layouts never redrew after the first hover. */
            setTimeout(function () { pending = false; render(); }, 16);
        }

        /* ------------------------------------------------------ interaction */

        function setCursorFromX(x) {
            if (xMode === 'dist') { wb.cursor = x; wb.cursorT = null; return; }
            wb.cursorT = x;
            var pts = wb.a.displayPoints().length ? wb.a.displayPoints() : wb.b.displayPoints();
            var p = cursorPoint(pts);
            wb.cursor = p ? p.dist : null;
        }

        function attach(gr) {
            var c = gr.canvas;
            function local(ev) {
                var r = c.getBoundingClientRect();
                return { x: ev.clientX - r.left, y: ev.clientY - r.top };
            }
            var drag = null;
            if (gr.kind === 'series') {
                function xAt(px) {
                    if (!gr.xr || !gr.b) return null;
                    var W = gr.b.w - gr.b.l - gr.b.r;
                    return gr.xr[0] + (px - gr.b.l) / W * (gr.xr[1] - gr.xr[0]);
                }
                c.addEventListener('mousedown', function (ev) {
                    drag = { x: local(ev).x, xr: (gr.xr || [0, 1]).slice(), moved: false };
                });
                c.addEventListener('mousemove', function (ev) {
                    var l = local(ev);
                    if (drag && (ev.buttons & 1)) {
                        var W = gr.b.w - gr.b.l - gr.b.r;
                        var dx = (l.x - drag.x) / W * (drag.xr[1] - drag.xr[0]);
                        if (Math.abs(l.x - drag.x) > 3) drag.moved = true;
                        if (drag.moved) xZoom = [drag.xr[0] - dx, drag.xr[1] - dx];
                    }
                    var x = xAt(l.x);
                    if (x !== null) { setCursorFromX(x); if (opts.onCursor) opts.onCursor(wb.cursor); }
                    scheduleRender();
                });
                c.addEventListener('mouseup', function () { drag = null; });
                c.addEventListener('mouseleave', function () { drag = null; wb.cursor = null; wb.cursorT = null; if (opts.onCursor) opts.onCursor(null); scheduleRender(); });
                c.addEventListener('wheel', function (ev) {
                    ev.preventDefault();
                    var x = xAt(local(ev).x);
                    var cur = gr.xr || [0, 1];
                    if (x === null) return;
                    var f = ev.deltaY > 0 ? 1.25 : 0.8;
                    var lo = x - (x - cur[0]) * f, hi = x + (cur[1] - x) * f;
                    var full = fullX(displaySets());
                    if (hi - lo >= (xMode === 'dist' ? 20 : 0.5) && hi - lo <= (full[1] - full[0]) * 1.5) xZoom = [lo, hi];
                    if (hi - lo > full[1] - full[0]) xZoom = null;
                    scheduleRender();
                }, { passive: false });
                c.addEventListener('dblclick', function () { xZoom = null; scheduleRender(); });
            } else if (gr.kind === 'map') {
                c.addEventListener('mousedown', function (ev) {
                    var v = gr.view;
                    drag = { x: ev.clientX, y: ev.clientY, view: v ? { cx: v.cx, cz: v.cz, scale: v.scale } : null };
                });
                c.addEventListener('mouseup', function () { drag = null; });
                c.addEventListener('mouseleave', function () { drag = null; });
                c.addEventListener('mousemove', function (ev) {
                    if (drag && drag.view && (ev.buttons & 1)) {
                        var k = drag.view.scale || 1;
                        mapViews[gr.i] = { cx: drag.view.cx - (ev.clientX - drag.x) / k,
                                           cz: drag.view.cz + (ev.clientY - drag.y) / k, scale: drag.view.scale };
                        scheduleRender();
                        return;
                    }
                    /* Hover: nearest A (else B) sample on the map sets the linked cursor. */
                    if (!gr.mx) return;
                    var l = local(ev), best = null, bd = 144;
                    wb.allSides().forEach(function (side) {
                        if (best) return;
                        side.displayPoints().forEach(function (p) {
                            if (p.x === null || p.z === null) return;
                            var dx = gr.mx(p.x) - l.x, dz = gr.mz(p.z) - l.y, d2 = dx * dx + dz * dz;
                            if (d2 < bd) { bd = d2; best = { p: p, pts: side.displayPoints() }; }
                        });
                    });
                    if (best) {
                        wb.cursor = best.p.dist;
                        wb.cursorT = best.pts.length ? best.p.t - best.pts[0].t : null;
                        if (opts.onCursor) opts.onCursor(wb.cursor);
                        scheduleRender();
                    }
                });
                c.addEventListener('wheel', function (ev) {
                    ev.preventDefault();
                    var v = gr.view; if (!v) return;
                    mapViews[gr.i] = { cx: v.cx, cz: v.cz, scale: v.scale * (ev.deltaY > 0 ? 0.85 : 1.18) };
                    scheduleRender();
                }, { passive: false });
                c.addEventListener('dblclick', function () { delete mapViews[gr.i]; scheduleRender(); });
            }
        }

        if (typeof window !== 'undefined') window.addEventListener('resize', scheduleRender);
        setLayout(layoutFile);

        return {
            render: scheduleRender,
            canvas: cvs,
            layouts: LAYOUTS,
            setLayout: setLayout,
            setXMode: function (mode) { xSel.value = mode; xSel.onchange(); },
            refreshSelection: function () { fillSelect(aSel, wb.a); fillSelect(bSel, wb.b); lapSig = ''; },
            setRoster: function (roster) {
                opts.roster = roster; fillSelect(aSel, wb.a); fillSelect(bSel, wb.b); lapSig = '';
            },
            setWaypoints: function (doc) {
                waypoints = doc ? (Array.isArray(doc) ? doc : doc.pts || null) : null;
                mapViews = {};
                render();
            }
        };
    }

    /* 4.4.0-rc.6 (Batch 2h task C, owner: pit garages follow CAR NUMBER order):
     * selector order = race number ascending, veh+0x4710 (CONFIRMED). The old
     * gar_team/gar_bay keys (rc.4) are NOT garage order and are no longer used.
     * Cars without a valid number (num < 0) follow in slot order. Race position
     * is a label only. Pure. */
    function rosterOrder(list) {
        function key(c) {
            return Number.isInteger(c.num) && c.num >= 0 ? c.num : 100000 + c.id;
        }
        return (list || []).slice().sort(function (a, b) {
            return key(a) - key(b) || a.id - b.id;
        });
    }
    /* Unique label per car: #race number first, so duplicate roster names
     * (seen live: Sato slots 3 and 11, Coulthard 17 and 19) stay distinct. */
    /* rc.9: the game clears AI control on retired cars (live: Coulthard and
     * Lasee retired, a 0, rt 1, fs 2), so a 0 alone does not mean player. */
    function roleOf(c) {
        if (c && c.a) return 'AI';
        if (c && (c.rt || c.fs > 0)) return 'retired';
        return 'player';
    }
    function carLabel(c) {
        var num = Number.isInteger(c.num) && c.num >= 0 ? '#' + c.num : '#?';
        return num + ' ' + c.n + (c.cn ? ' · ' + c.cn : '') + ' · ' + roleOf(c) +
            ' · P' + (c.p > 0 ? c.p : '-');
    }

    /* 4.4.0-rc.5 (task 2, pure): default sides. A car is a candidate only when
     * it is running: classified (p > 0), on track (ot2), not retired (rt) and
     * finish_state 0 (fs), the same rule as C trace_live_car. A = the player when
     * running, else the race leader (lowest p). B = the friend (co-op), else the
     * first other running AI car. Empty states come back as notes, never as a
     * retired or stopped car. */
    function isRunning(c) {
        return !!c && c.p > 0 && !!c.ot2 && !c.rt && !c.fs;
    }
    function pickDefaultSides(list) {
        var run = (list || []).filter(isRunning).sort(function (a, b) { return a.p - b.p; });
        var notes = { A: null, B: null };
        var pl = run.filter(function (c) { return c.cr === 0; })[0] || run[0] || null;
        var friend = run.filter(function (c) { return c.cr === 2 && !c.a; })[0];
        var ai = friend || run.filter(function (c) { return c.a && c !== pl; })[0] || null;
        if (!pl) notes.A = 'No running car: the player is retired, out of the session or not yet classified. Pick a car from the list.';
        if (!ai) notes.B = 'No other running car to compare. Pick a car from the list.';
        return { A: pl, B: ai, notes: notes };
    }

    /* 4.4.0-rc.5 (task 8, pure): Race Control summary. Fastest lap = minimum
     * completed lap time over /laps (t > 0). Stops per car = /pits events for
     * that car, or, when none, runs of pitted laps in /laps. Penalties = the
     * frame counter pn (penalties). Retired = frame rt === 1. Mod-only pit
     * failures are NOT in this summary (mechfail source still OPEN). */
    function raceControlSummary(cars, pace, pits) {
        var out = { fastest: null, rows: [], pitEvents: (pits || []).length };
        var stopsById = {}, lastBox = {};
        (pits || []).forEach(function (e) {
            stopsById[e.id] = (stopsById[e.id] || 0) + 1;
            if (e.boxdur > 0) lastBox[e.id] = e.boxdur;
        });
        (pace || []).forEach(function (d) {
            var runs = 0, prev = 0;
            (d.laps || []).forEach(function (l) {
                if (l.t > 0 && (!out.fastest || l.t < out.fastest.t))
                    out.fastest = { t: l.t, id: d.id, n: d.n, cn: d.cn || '', lap: l.l };
                if (l.pit && !prev) runs++;
                prev = l.pit ? 1 : 0;
            });
            if (!stopsById[d.id] && runs) stopsById[d.id] = runs;
        });
        (cars || []).forEach(function (c) {
            out.rows.push({
                id: c.id, n: c.n, cn: c.cn || '', p: c.p,
                stops: stopsById[c.id] || 0,
                lastBox: lastBox[c.id] || 0,
                pen: c.pn || 0,
                retired: c.rt === 1
            });
        });
        out.rows.sort(function (a, b) { return (a.p || 999) - (b.p || 999); });
        return out;
    }

    /* 4.4.0-rc.5 (task 7, pure): lapped cars. Laps down = leader lap minus the
     * car's lap counter (c.l), for running classified cars that are not retired
     * or finished. Returns { carId: lapsDown } for cars at least one lap down. */
    /* V6 (TAPE2): a car is lapped against the car at P1 (its lap count), not the highest lap in the frame:
     * a CO-OP remote copy whose lap counter runs ahead must not make the field "+2L" and the leader never
     * gets a count. Fallback (deviation, kept for the blue-flag test that has no P1 car): with no running P1
     * the highest lap is the reference, as before. */
    function lapsDown(cars) {
        var run = (cars || []).filter(function (c) {
            return c && c.p > 0 && Number.isFinite(c.l) && !c.rt && !c.fs;
        });
        var lead = run.filter(function (c) { return c.p === 1; })[0];
        var ref = lead ? lead.l : run.reduce(function (m, c) { return Math.max(m, c.l); }, 0);
        var out = {};
        if (!(ref > 0)) return out;
        run.forEach(function (c) {
            if (lead && c.id === lead.id) return;
            if (ref - c.l >= 1) out[c.id] = ref - c.l;
        });
        return out;
    }

    /* TAPE2 V4: does a legacy layout draw a track map (a graph with x = x and y = z)? Layouts without one get the
     * viewer's map inset. An unknown file counts as no map (the inset is then shown). */
    function layoutHasMap(file) {
        var name = file || DEFAULT_LAYOUT;
        var lay = allLayouts().filter(function (l) { return l.file === name; })[0];   /* 1.2.5: My layouts too */
        return !!lay && lay.graphs.some(function (g) { return g.x === 'x' && g.y === 'z'; });
    }

    /* Engine Live: the AI countdown is vehicle lfe; the player's separate
     * global lifetime is frame.pel (SDK G_EngineLifeSeconds). Do not confuse
     * either with the max-relative lpe estimate. */
    function fmtHms(sec) {
        if (!(sec > 0)) return '-';
        var h = Math.floor(sec / 3600), m = Math.floor(sec % 3600 / 60), s = Math.floor(sec % 60);
        return h + ':' + (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
    }
    function engineSnapshot(car, frame) {
        if (!car) return [{ k: 'Focus car', v: 'none selected', tier: '' }];
        var rows = [{ k: 'Car', v: (car.n || '?') + ' (' + roleOf(car) + ')', tier: '' }];
        rows.push({ k: 'RPM / rev limit', v: Math.round(car.rpm || 0) + ' / ' + Math.round(car.max_rpm || 0) + ' rpm', tier: 'FACT' });
        rows.push({ k: 'Rev limit (max_rpm)', v: car.max_rpm > 0 ? Math.round(car.max_rpm) + ' rpm' : 'not sent', tier: 'FACT (owner test: player rev cut ~17590, max_rpm 17600)' });
        var player = car.cr === 0;
        rows.push({ k: 'Engine status', v: car.ed ? 'stopped' : (car.rpm > 0 ? 'running' : 'not running'), tier: 'DERIVED (RPM / stopped flag)' });
        rows.push({ k: 'Gear / limiter', v: String(car.g) + ' / ' + (car.rl ? 'on' : 'off'), tier: 'FACT' });
        rows.push({ k: 'Throttle / brake', v: Math.round((car.th || 0) * 100) + '% / ' + Math.round((car.br || 0) * 100) + '%', tier: 'FACT when valid' });
        rows.push({ k: 'Crankshaft speed', v: Number.isFinite(car.eav) ? car.eav.toFixed(0) + ' rad/s' : '-', tier: 'FACT (vehicle field)' });
        rows.push({ k: 'Fuel remaining', v: Number.isFinite(car.fl) ? car.fl.toFixed(1) + ' L' : '-', tier: 'FACT (vehicle field)' });
        if (player) {
            rows.push({ k: 'Engine life left', v: fmtHms(frame && frame.pel), tier: 'FACT (player global countdown)' });
            rows.push({ k: 'Oil temp (HUD)', v: frame && frame.otp > 0 ? frame.otp.toFixed(1) + ' C' : '-', tier: 'FACT (HUD, player)' });
            rows.push({ k: 'Water temp (HUD)', v: frame && frame.wtp > 0 ? frame.wtp.toFixed(1) + ' C' : '-', tier: 'FACT (HUD, player)' });
        } else {
            rows.push({ k: 'Engine life left', v: fmtHms(car.lfe), tier: 'FACT (AI vehicle countdown)' });
            rows.push({ k: 'Oil / water (vehicle)', v: (Number.isFinite(car.ot) ? car.ot.toFixed(1) : '-') + ' / ' + (Number.isFinite(car.wt) ? car.wt.toFixed(1) : '-') + ' C', tier: 'RESEARCH (not HUD calibrated)' });
        }
        rows.push({ k: 'Lifetime vs session max', v: Number.isFinite(car.lpe) ? car.lpe.toFixed(1) + '%' : '-', tier: 'DERIVED (not engine health)' });
        rows.push({ k: 'HP / Nm / boost', v: 'not available in game data', tier: 'OPEN' });
        return rows;
    }

    return {
        roleOf: roleOf,
        RPM_FACTOR: RPM_FACTOR,
        engineSnapshot: engineSnapshot,
        fmtHms: fmtHms,
        lapsDown: lapsDown,
        lapsUpTo: lapsUpTo,
        lapInProgress: lapInProgress,
        layoutHasMap: layoutHasMap,
        raceControlSummary: raceControlSummary,
        pickDefaultSides: pickDefaultSides,
        rosterOrder: rosterOrder,
        carLabel: carLabel,
        LIMITS: LIMITS,
        COLORS: COLORS,
        LETTERS: LETTERS,
        MAX_SIDES: MAX_SIDES,
        VF: VF,
        VF6: VF6,
        TRF: TRF,
        LAYOUTS: LAYOUTS,
        allLayouts: allLayouts,
        userLayouts: userLayouts,
        saveUserLayout: saveUserLayout,
        deleteUserLayout: deleteUserLayout,
        buildUserLayout: buildUserLayout,
        layoutSpec: layoutSpec,
        channelCatalog: channelCatalog,
        layoutToIni: layoutToIni,
        iniToSpec: iniToSpec,
        jsonToSpec: jsonToSpec,
        layoutToJson: layoutToJson,
        USER_GRAPH_MAX: USER_GRAPH_MAX,
        resetUserLayoutCache: function () { userLayoutCache = null; },
        THEME: THEME,
        setTheme: setTheme,
        radPerSecToRpm: radPerSecToRpm,
        formatRpm: formatRpm,
        formatRpmValue: formatRpmValue,
        timeAtDistance: timeAtDistance,
        makeTimeLookup: makeTimeLookup,
        dtimeSeries: dtimeSeries,
        channelInfo: channelInfo,
        lapTimeText: lapTimeText,
        lapIsFull: lapIsFull,
        lapStripSignature: lapStripSignature,
        fastestLap: fastestLap,
        lapIsRanked: lapIsRanked,
        serverLapFor: serverLapFor,
        trimLaps: trimLaps,
        ticks: ticks,
        CarSide: CarSide,
        Workbench: Workbench,
        mount: mount
    };
});
