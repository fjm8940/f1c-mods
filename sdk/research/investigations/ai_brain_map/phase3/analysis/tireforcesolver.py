# tireforcesolver.py -- D1.2: Python model of TireForceSolver (0x4E8CE0), read-only research code.
#
# Derived from functions/TireForceSolver_004e8ce0.c plus capstone disassembly of the whole
# function (phase3/analysis/d1_static.py; raw log %TEMP%\f1c_analysis\d1_static.log). `this` = veh+0xC
# (confirmed by the call in AIComputePassScore: `lea ecx, [esi+0xC]`, esi = vehicle base), so
# this[N] -> veh + 0xC + 4N. The 0x59e440 constant is 0.0, 0x59e46c is 4.0, 0x59e6fc is 128.0.
#
# Semantics (asm-faithful; the five `sub_5895A2` `_frnd` returns are STORED after the call, so the
# table index uses the rounded value while the interpolation fraction uses the raw value --
# DROPPED_ST0_SCAN "TireForceSolver" row, review 11):
#
#   sel  = this[3903]                                  # veh+0x3D08
#   seed = |a4| * sel                                  # a4 = wp+0x70
#   r    = frnd(seed); i = (int) r
#   if i < 0: v8 = this[3904]                          # veh+0x3D0C
#   else: i = min(i, 13); v8 = this[3904+i] + (this[3905+i]-this[3904+i]) * (seed - i)
#   v8 = min(v8, 128)
#   lo = v8 - 4 (or 0 if v8 < 4); hi = v8 + 4 (or 8 if v8 < 4)
#   repeat:
#     F(w):                                            # force at speed w
#       w2 = w*w
#       A  = w2*this[3740] - (w2*a2 + a3[1])*this[3397]   # 3740=veh+0x3A7C, 3397=veh+0x3520
#       B  = (w2*a4 - a3[0])*this[3397]                   # a3[0]=wp+0x78, a3[1]=wp+0x7C
#       raw = A * (this[3871] if B >= 0 else this[3855])  # 3871=veh+0x3C88, 3855=veh+0x3C48
#       j = (int) frnd(raw); table = [3872..] if B >= 0 else [3856..]   # 0x3C8C / 0x3C4C
#       if j < 0: v = table[0]
#       else: j = min(j, 13); v = table[j] + (table[j+1]-table[j]) * (raw - j)
#       F = v - B if B >= 0 else v + B
#     F_lo, F_hi = F(lo), F(hi)
#     if F_hi * F_lo <= 0:                             # bracket
#        return 0.5*(hi-lo)+lo if (F_lo - F_hi) == 0
#        else F_lo/(F_lo-F_hi)*(hi-lo)+lo
#     if F_lo >= 0:
#        if F_lo < F_hi: return 128.0
#        hi = min(128, hi + a5)                        # a5 = 16.0
#     else:
#        if F_lo < F_hi: hi = min(128, hi + a5)
#        else:
#          if lo <= 0: return 0.0
#          hi = lo; lo = max(0, lo - a5)
#
# a2 = wp+0x74; a3 points at wp+0x78 so a3[0] = wp+0x78, a3[1] = wp+0x7C; a4 = wp+0x70; a5 = 16.0.
# The interval-step gates were resolved from the asm (0x4E8FDB..0x4E909C); the Hex-Rays `v38|v39`
# condition equals `F_lo < F_hi` and both branches agree with the flag paths.
#
# Required vehicle data (this = veh+0xC): this[3397] can be taken from snapshot extra.v3520;
# this[3740]=0x3A7C, this[3855]=0x3C48, this[3856]=0x3C4C, this[3871]=0x3C88, this[3872]=0x3C8C,
# this[3903]=0x3D08 and tables [3856..3870], [3872..3886], [3904..3918] are all present in
# cars_static.jsonl (raw read veh 0x3A7C..0x3D50 every 60 s), so MISSING is empty for the
# recorded schema. No recorder request is needed for D1.
import math

TABLE_LEN = 15

# vehicle-relative offsets of the fields read (this = veh+0xC)
VEH = {
    3397: 0x3520,
    3740: 0x3A7C,
    3855: 0x3C48, 3856: 0x3C4C,
    3871: 0x3C88, 3872: 0x3C8C,
    3903: 0x3D08, 3904: 0x3D0C,
}
# this[N] -> N is 3872+entry for the tables; required entries are read table[i] and table[i+1]
REQUIRED = (
    [dict(id='T%d' % n, hex='0x%X' % VEH[n], group='tire_solver', note='scalar') for n in sorted(VEH)]
    + [dict(id='T%d' % n, hex='0x%X' % (0xC + 4 * n), group='tire_solver', count=TABLE_LEN, note='15-float table')
       for n in (3856, 3872, 3904)]
    + [dict(id='wp_112', hex='wp+0x70', group='waypoint', note='a4 (seed speed input)'),
       dict(id='wp_116', hex='wp+0x74', group='waypoint', note='a2'),
       dict(id='wp_120', hex='wp+0x78', group='waypoint', note='a3[0]'),
       dict(id='wp_124', hex='wp+0x7C', group='waypoint', note='a3[1]')]
)
MISSING = ()  # all of the above exist in cars_static.jsonl + snapshot extra (RECORDER_REQUESTS A5 done)


def frnd(x):
    """sub_5895A2 = MSVC _frnd: round to nearest integer, ties to even (FPU default control word)."""
    return float(round(x))


def _table_value(tbl, raw):
    """Index+interp as in the asm: index from frnd(raw), fraction from raw."""
    r = frnd(raw)
    i = int(r)
    if i < 0:
        return tbl[0]
    if i >= TABLE_LEN - 1:
        i = TABLE_LEN - 2
    frac = raw - i
    a, b = tbl[i], tbl[i + 1]
    return a + (b - a) * frac


def _seed(v8):
    v8 = min(v8, 128.0)
    lo = v8 - 4.0
    if lo >= 0.0:
        hi = v8 + 4.0
    else:
        lo, hi = 0.0, 8.0
    return lo, hi


def tireforcesolver(wp112, wp116, wp120, wp124, T, step=16.0, max_iter=64):
    """T is a dict {field: value}; table fields (3856/3872/3904) are lists of 15 floats.

    Returns the solved speed (m/s). No field is written; this is the value AIComputePassScore
    multiplies into the corner-speed target."""
    t = lambda n: T[n]

    seed = abs(wp112) * t(3903)
    v8 = _table_value(t(3904), seed)
    lo, hi = _seed(v8)
    a2, a3_0, a3_1, a4 = wp116, wp120, wp124, wp112

    def force(w):
        w2 = w * w
        a = w2 * t(3740) - (w2 * a2 + a3_1) * t(3397)
        b = (w2 * a4 - a3_0) * t(3397)
        raw = a * (t(3871) if b >= 0.0 else t(3855))
        v = _table_value(t(3872) if b >= 0.0 else t(3856), raw)
        return v - b if b >= 0.0 else v + b

    for _ in range(max_iter):
        f_lo, f_hi = force(lo), force(hi)
        if f_hi * f_lo <= 0.0:
            if f_lo - f_hi == 0.0:
                return 0.5 * (hi - lo) + lo
            return f_lo / (f_lo - f_hi) * (hi - lo) + lo
        if f_lo >= 0.0:
            if f_lo < f_hi:
                return 128.0
            hi = min(128.0, hi + step)
        else:
            if f_lo < f_hi:
                hi = min(128.0, hi + step)
            else:
                if lo <= 0.0:
                    return 0.0
                hi = lo
                lo = max(0.0, lo - step)
    return hi


def _selftest():
    import struct
    # synthetic spread tables; force is monotone enough to bracket a root
    T = {3397: 600.0, 3740: 1.5, 3855: 0.001, 3871: 0.001, 3903: 240.0,
         3856: [0.0, 0.02, 0.05, 0.09, 0.15, 0.23, 0.33, 0.45, 0.6, 0.8, 1.05, 1.4, 1.9, 2.6, 3.6],
         3872: [0.0, 0.02, 0.05, 0.09, 0.15, 0.23, 0.33, 0.45, 0.6, 0.8, 1.05, 1.4, 1.9, 2.6, 3.6],
         3904: [255.0, 125.0, 110.0, 95.0, 80.0, 70.0, 62.0, 55.0, 48.0, 42.0, 36.0, 30.0, 25.0, 20.0, 15.0]}
    val = tireforcesolver(0.004, 0.0005, 0.014, -9.8, T)
    real = 0.0042283604852855206
    print('tireforcesolver selftest: v=%.6f (finite=%s) seed=%.3f'
          % (val, isinstance(val, float) and not math.isnan(val), abs(real) * 240.0))
    print('MISSING: %d' % len(MISSING))


if __name__ == '__main__':
    _selftest()
