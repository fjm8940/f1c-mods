/*
 * part_detach_watch.js — log the part-detach chain at the source.
 *
 * Run with:  python tools/mod_pipeline/watch_run.py tools/part_detach_watch.js
 *
 * Hooks (all read-only, no writes):
 *   0x004ED250 RepairServiceStateDispatcher  __thiscall(this, partIdx)
 *                *this = vehicle ptr; partIdx is what we could never recover
 *                from polling. 0-3 wheel, 4/5 wing, 6-13 bodywork.
 *   0x0041AFD0 sub_41AFD0                    __stdcall(veh, partIdx)
 *                the commentary emitter for the same partIdx.
 *   0x004144F0 DriverDataTableLookupAndTimeConversion __thiscall(buf, eventId, driverId, ...)
 *                EVERY commentary event, logged before it reaches the 1-deep
 *                queue head -- so nothing is lost to same-frame overwrites or
 *                to priority starvation (114 pri=20 beat 222 pri=40 last run).
 *
 * Emits CSV rows consumed by watch_run.py.
 */

const VEH_BASE   = 0x5EC6D8;
const VEH_STRIDE = 0x48C0;
const NAME_OFF   = 0x4620;

const F_REPAIR = 0x4ED250;
const F_EMIT   = 0x41AFD0;
const F_EVENT  = 0x4144F0;

// partIdx -> meaning (DERIVED for the 4/5 front-vs-rear split; that is the
// open question this agent exists to settle)
const PARTS = {
  0: "wheel0", 1: "wheel1", 2: "wheel2", 3: "wheel3",
  4: "wingA(4)", 5: "wingB(5)",
  6: "body6", 7: "body7", 8: "body8", 9: "body9",
  10: "body10", 11: "body11", 12: "body12", 13: "body13"
};

// event ids emitted by sub_41AFD0
const EVENTS = {
  106: "player wheel0", 107: "player wheel1", 109: "player wheel2",
  110: "player wheel3", 108: "player wingA", 111: "player wingB",
  114: "PWS_HIT10", 115: "PWS_HIT08",
  220: "PWS_AIHIT08(dead)", 221: "PWS_AIHIT07 AI wheel",
  222: "PWS_AIHIT01 AI wingA", 223: "PWS_AIHIT02 AI wingB"
};

const t0 = Date.now();
function ts() { return ((Date.now() - t0) / 1000).toFixed(3); }

function slotOf(veh) {
  if (veh === 0) return -1;
  const d = veh - VEH_BASE;
  if (d < 0 || d % VEH_STRIDE !== 0) return -1;
  const s = d / VEH_STRIDE;
  return (s >= 0 && s < 104) ? s : -1;
}

function nameOf(veh) {
  try {
    const s = ptr(veh).add(NAME_OFF).readCString();
    return (s || "?").replace(/,/g, " ");
  } catch (e) { return "?"; }
}

function vehDesc(veh) {
  const s = slotOf(veh);
  return s + "," + (s >= 0 ? nameOf(veh) : "?" ) ;
}

send("CSV_HEADER:time,hook,slot,driver,partIdx,partName,eventId,eventName,carConditions,reasonCode,finishState");

function fieldsOf(veh) {
  try {
    const p = ptr(veh);
    return p.add(0x3D6C).readS32() + "," +
           p.add(0x4068).readS32() + "," +
           p.add(0x4064).readS32();
  } catch (e) { return ",,"; }
}

// --- RepairServiceStateDispatcher: __thiscall(this, partIdx) ---
Interceptor.attach(ptr(F_REPAIR), {
  onEnter(args) {
    try {
      const self = this.context.ecx;
      const veh = self.readU32();            // *this = vehicle ptr
      const partIdx = this.context.esp.add(4).readS32();
      send("CSV:" + ts() + ",REPAIR," + vehDesc(veh) + "," +
           partIdx + "," + (PARTS[partIdx] || "?") + ",,," + fieldsOf(veh));
    } catch (e) { send("LOG:repair hook error " + e); }
  }
});

// --- sub_41AFD0: __stdcall(veh, partIdx) ---
Interceptor.attach(ptr(F_EMIT), {
  onEnter(args) {
    try {
      const esp = this.context.esp;
      const veh = esp.add(4).readU32();
      const partIdx = esp.add(8).readS32();
      send("CSV:" + ts() + ",EMIT," + vehDesc(veh) + "," +
           partIdx + "," + (PARTS[partIdx] || "?") + ",,," + fieldsOf(veh));
    } catch (e) { send("LOG:emit hook error " + e); }
  }
});

// --- DriverDataTableLookupAndTimeConversion: __thiscall(buf, eventId, driverId, ...) ---
Interceptor.attach(ptr(F_EVENT), {
  onEnter(args) {
    try {
      const esp = this.context.esp;
      const eid = esp.add(4).readS32();
      const drv = esp.add(8).readS32();
      send("CSV:" + ts() + ",EVENT,," + drv + ",,," +
           eid + "," + (EVENTS[eid] || "") + ",,,");
    } catch (e) { send("LOG:event hook error " + e); }
  }
});

send("LOG:part_detach_watch attached — REPAIR/EMIT/EVENT hooks live");

rpc.exports = {
  poke: function () { return 0; },   // read-only agent
  dump: function (slot, start, end, step) {
    const veh = VEH_BASE + slot * VEH_STRIDE;
    const out = [];
    for (let o = start; o < end; o += step) {
      out.push(o + "=" + ptr(veh).add(o).readS32());
    }
    return out.join(" ");
  }
};
