/*
 * engine_temp_poll.js — sample candidate engine-lifetime / temperature fields over
 * REAL time, to separate continuously-varying physics state (temperature, wear)
 * from static setup values.
 *
 * Motivated by SeasonData/Vehicles/<team>/engine.ini:
 *   LifetimeEngineRPM=(15742.0,355.3)  base RPM for lifetime, range where it halves
 *   LifetimeOilTemp=(123.43,2.90)      base oil temp, range where lifetime halves
 *   LifetimeAvg=8924                   average engine lifetime in SECONDS
 *   LifetimeVar=2947.89                lifetime random variance
 *   OptimumOilTemp=105.6
 * so per-car oil temp, water temp and a lifetime counter should all exist in memory.
 *
 * Do NOT sample with a busy-wait inside a Frida script — that yields byte-identical
 * readings. Poll on a timer from a persistent process instead.
 */
const PTR_ARRAY = ptr('0x007C54F0');
const LIVE_OFF = 0x32C8;
const CAND = [0x3D10, 0x3D14, 0x3D0C, 0x3984, 0x424C];
const OFF = { rpm: 0x39A0, speed: 0x4440, name: 0x4620, cc: 0x3D6C,
              rsn: 0x4068, act: 0x45F4, angvel: 0x00A0 };

send('CSV_HEADER,time,slot,driver,rpm,kmh,angvel,cc,rsn,act,' +
     CAND.map(o => 'x' + o.toString(16)).join(','));

const t0 = Date.now();
const ts = () => ((Date.now() - t0) / 1000).toFixed(2);

function nameOf(v) {
  try { return (v.add(OFF.name).readCString(20) || '?').split('(')[0].trim().replace(/,/g, ' '); }
  catch (e) { return '?'; }
}

setInterval(function () {
  for (let i = 0; i < 24; i++) {
    let v;
    try {
      v = PTR_ARRAY.add(i * 4).readPointer();
      if (v.isNull() || v.add(LIVE_OFF).readInt() === -1) continue;
    } catch (e) { continue; }
    const row = [
      'CSV', ts(), i, nameOf(v),
      v.add(OFF.rpm).readFloat().toFixed(0),
      (v.add(OFF.speed).readFloat() * 3.6).toFixed(0),
      v.add(OFF.angvel).readFloat().toFixed(0),
      v.add(OFF.cc).readInt(), v.add(OFF.rsn).readInt(), v.add(OFF.act).readInt()
    ].concat(CAND.map(o => v.add(o).readFloat().toFixed(3)));
    send(row.join(','));
  }
}, 2000);
