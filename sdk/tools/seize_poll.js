/*
 * seize_poll.js — continuously poll the transient heavy-impact flag veh+0x4574.
 *
 * The flag sets on impact and clears once the car settles, so sampling after the
 * fact always reads 0. This polls at ~200Hz and reports every transition with the
 * context needed to tell an engine seize from a generic heavy hit:
 *   - speed immediately BEFORE the transition (proxy for impulse)
 *   - rpm / throttle / gear after, to see whether the engine actually died
 *   - carConditions + wing flags, to correlate with what detached
 *
 * Run:  python tools/mod_pipeline/watch_run.py tools/seize_poll.js --out seize.csv
 */
const PTR_ARRAY = ptr('0x007C54F0');
const LIVE_OFF = 0x32C8;
const OFF = {
  seize: 0x4574, speed: 0x4440, rpm: 0x39A0, thr: 0x357C, gear: 0x39FC,
  cc: 0x3D6C, rsn: 0x4068, fw: 0x0D14, rw: 0x0D58, act: 0x45F4,
  ret: 0x4410, fin: 0x4064, name: 0x4620, hits: 0x12B4, lap: 0x403C
};

const t0 = Date.now();
const ts = () => ((Date.now() - t0) / 1000).toFixed(3);

send('CSV_HEADER,time,event,slot,driver,seize,kmh_now,kmh_prev,kmh_peak,rpm,thr,gear,cc,reason,fwFlag,rwFlag,act,ret,fin,hits,lap');

function nameOf(v) {
  try { return (v.add(OFF.name).readCString(24) || '?').split('(')[0].trim().replace(/,/g, ' '); }
  catch (e) { return '?'; }
}

const prev = {};      // slot -> last seize value
const speedHist = {}; // slot -> recent speeds (ring)
const peak = {};      // slot -> peak speed seen in last ~1s

setInterval(function () {
  for (let i = 0; i < 24; i++) {
    let v;
    try {
      v = PTR_ARRAY.add(i * 4).readPointer();
      if (v.isNull() || v.add(LIVE_OFF).readInt() === -1) continue;
    } catch (e) { continue; }

    let s;
    try { s = v.add(OFF.seize).readU8(); } catch (e) { continue; }
    const kmh = v.add(OFF.speed).readFloat() * 3.6;

    if (!speedHist[i]) { speedHist[i] = []; peak[i] = 0; }
    speedHist[i].push(kmh);
    if (speedHist[i].length > 200) speedHist[i].shift();   // ~1s at 200Hz
    peak[i] = Math.max.apply(null, speedHist[i]);

    if (prev[i] === undefined) { prev[i] = s; continue; }

    if (s !== prev[i]) {
      // speed just before the transition = max over the recent window
      const before = speedHist[i].length > 1
        ? speedHist[i][speedHist[i].length - 2] : kmh;
      send([
        'CSV', ts(), s ? 'SEIZE_SET' : 'SEIZE_CLEAR', i, nameOf(v), s,
        kmh.toFixed(1), before.toFixed(1), peak[i].toFixed(1),
        v.add(OFF.rpm).readFloat().toFixed(0),
        v.add(OFF.thr).readFloat().toFixed(2),
        v.add(OFF.gear).readFloat().toFixed(0),
        v.add(OFF.cc).readInt(), v.add(OFF.rsn).readInt(),
        v.add(OFF.fw).readU8(), v.add(OFF.rw).readU8(),
        v.add(OFF.act).readInt(), v.add(OFF.ret).readU8(),
        v.add(OFF.fin).readInt(), v.add(OFF.hits).readInt(),
        v.add(OFF.lap).readInt()
      ].join(','));
      prev[i] = s;
    }
  }
}, 5);

send('CSV,0.000,POLLER_READY,,,,,,,,,,,,,,,,,,');
