// watch_tire_wear.js — poll player vehicle struct for changing floats during tire spin
// Run: frida -n "F1 Challenge Online.exe" -l tools/watch_tire_wear.js

var PLAYER_VEH_PTR = ptr(0x007C54E8);

// candidates from struct analysis
var CANDIDATES = {
    "WearAccum_3D64": 0x3D64,
    "TireFL_8CC":     0x08CC,
    "TireFR_9F4":     0x09F4,
    "TireRL_B1C":     0x0B1C,
    "TireRR_C44":     0x0C44,
    // scan window around 0x3D64
    "Wear_3D60": 0x3D60,
    "Wear_3D68": 0x3D68,
    "Wear_3D6C": 0x3D6C,
    "Wear_3D70": 0x3D70,
    "Wear_3D58": 0x3D58,
    "Wear_3D54": 0x3D54,
    // scan around 0x8CC stride 0x128 (per-tire)
    "T0_8CC":  0x08CC, "T1_9F4":  0x09F4, "T2_B1C":  0x0B1C, "T3_C44":  0x0C44,
    // wear channels from docs
    "WC0_1EC": 0x01EC, "WC1_1F0": 0x01F0, "WC2_1F4": 0x01F4, "WC3_1F8": 0x01F8,
};

var prev = {};

function readF32(base, off) {
    try { return base.add(off).readFloat(); } catch(e) { return null; }
}

var baseSnap = null;

setInterval(function() {
    var vehPtr = PLAYER_VEH_PTR.readPointer();
    if (vehPtr.isNull()) return;

    if (!baseSnap) {
        // first run: snapshot all values
        baseSnap = {};
        for (var k in CANDIDATES) {
            baseSnap[k] = readF32(vehPtr, CANDIDATES[k]);
        }
        console.log("[SNAP] baseline captured at " + vehPtr);
        return;
    }

    for (var k in CANDIDATES) {
        var cur = readF32(vehPtr, CANDIDATES[k]);
        var old = prev[k] !== undefined ? prev[k] : baseSnap[k];
        if (cur !== null && old !== null && Math.abs(cur - old) > 0.0001) {
            console.log("[CHANGE] " + k + " (+" + CANDIDATES[k].toString(16) + "): " +
                        old.toFixed(5) + " -> " + cur.toFixed(5) +
                        "  (delta " + (cur - old).toFixed(5) + ")");
        }
        prev[k] = cur;
    }
}, 250);

console.log("[watch_tire_wear] polling every 250ms — do a burnout now");
