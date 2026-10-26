"""
teleport_car.py - Visually teleport AI cars in F1 Challenge Online.exe

Usage:
    python tools/teleport_car.py list
    python tools/teleport_car.py teleport <name> player          # beside player
    python tools/teleport_car.py teleport <name> <x> <y> <z>    # exact coords
    python tools/teleport_car.py teleport <name> player ahead    # 30m ahead of player

Requirements:
    - Game running (F1 Challenge Online.exe)
    - frida: pip install frida

Confirmed recipe (Heidfeld, Kimi, Salo, Pedro, Manning — all proven 2026-07-22):
    - Works reliably on ANY retired car that hasn't been collision-decoupled yet.
    - Poll every 5ms for 3s: AF=1, RF=0, FS=0, RC=0, PitState=1 + physics XYZ + scene node XYZ
    - Re-read nodeIdx each tick (scene manager can reassign on activation)
    - Physics and scene node coords are identical world-space (1:1 mapping confirmed)
    - After session ends game reverts AF=0, but scene node stays frozen at teleport position

Scene graph:
    - Base struct embedded at 0x5EA2C0 (NOT a pointer — IDA shows dword but call uses LEA)
    - nodeTable = *(0x5EA2C0 + 344),  dim = *(0x5EA2C0 + 356)  (observed dim=52)
    - entry = nodeTable + 28*(nodeIdx + subIdx*dim)
    - entry layout: X(f) Y(f) Z(f) rotX(f) rotY(f) rotZ(f) pad(u32) = 28 bytes
    - nodeIdx = vehicle+0x48A4,  subIdx = vehicle+0x48A8
"""

import frida
import subprocess
import sys
import time


VEHICLE_COUNT_ADDR  = 0x005C329C
VEHICLE_ARRAY_ADDR  = 0x007C54F0
SCENE_BASE_ADDR     = 0x005EA2C0
STRIDE              = 0x48C0

OFF_PHYS_X   = 0x1C
OFF_PHYS_Y   = 0x20
OFF_PHYS_Z   = 0x24
OFF_AF       = 0x45F4
OFF_RF       = 0x4410
OFF_FS       = 0x4064
OFF_RC       = 0x4068
OFF_NODE_IDX = 0x48A4
OFF_NODE_SUB = 0x48A8
OFF_NAME     = 0x4620


def get_game_pid():
    r = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq F1 Challenge Online.exe", "/FO", "CSV"],
        capture_output=True, text=True
    )
    for line in r.stdout.splitlines()[1:]:
        parts = line.strip('"').split('","')
        if parts and parts[0] == "F1 Challenge Online.exe":
            return int(parts[1])
    return None


def run_script(pid, js, timeout=5):
    session = frida.attach(pid)
    results = []

    script = session.create_script(js)

    def on_msg(msg, data):
        if msg["type"] == "send":
            results.append(msg["payload"])
        elif msg["type"] == "error":
            results.append("ERR: " + msg["description"])

    script.on("message", on_msg)
    script.load()
    time.sleep(timeout)
    session.detach()
    return results


def list_cars(pid):
    js = """
var BASE = ptr(VEHICLE_ARRAY_ADDR).readPointer();
var count = ptr(VEHICLE_COUNT_ADDR).readU32();
var dim = ptr(SCENE_BASE_ADDR).add(89*4).readU32();
var nodeTable = ptr(ptr(SCENE_BASE_ADDR).add(86*4).readU32());

send('idx | AF RF FS | phys_z | scene_z | name');
send('----+----------+--------+---------+----------------------------');
for (var i = 0; i < Math.min(count, 24); i++) {
    var v = BASE.add(i * STRIDE);
    var af = v.add(OFF_AF).readU8();
    var rf = v.add(OFF_RF).readU8();
    var fs = v.add(OFF_FS).readU32();
    var pz = v.add(OFF_PHYS_Z).readFloat();
    var ni = v.add(OFF_NODE_IDX).readU32();
    var si = v.add(OFF_NODE_SUB).readU32();
    var entry = nodeTable.add(28 * (ni + si * dim));
    var sz = entry.add(8).readFloat();
    var nb = v.add(OFF_NAME).readByteArray(24);
    var arr = new Uint8Array(nb);
    var name = '';
    for (var j = 0; j < arr.length && arr[j] != 0; j++) name += String.fromCharCode(arr[j]);
    send(('  ' + i).slice(-3) + ' | ' + af + '  ' + rf + '  ' + fs + ' | ' +
         pz.toFixed(0).padStart(7) + ' | ' + sz.toFixed(1).padStart(7) + ' | ' + name);
}
""".replace("VEHICLE_ARRAY_ADDR", hex(VEHICLE_ARRAY_ADDR)) \
   .replace("VEHICLE_COUNT_ADDR", hex(VEHICLE_COUNT_ADDR)) \
   .replace("SCENE_BASE_ADDR", hex(SCENE_BASE_ADDR)) \
   .replace("STRIDE", hex(STRIDE)) \
   .replace("OFF_AF", hex(OFF_AF)) \
   .replace("OFF_RF", hex(OFF_RF)) \
   .replace("OFF_FS", hex(OFF_FS)) \
   .replace("OFF_PHYS_Z", hex(OFF_PHYS_Z)) \
   .replace("OFF_NODE_IDX", hex(OFF_NODE_IDX)) \
   .replace("OFF_NODE_SUB", hex(OFF_NODE_SUB)) \
   .replace("OFF_NAME", hex(OFF_NAME))

    for line in run_script(pid, js, timeout=3):
        print(line)


def teleport(pid, target_name, tx, ty, tz, hold_secs=3.0):
    """
    Teleport car matching target_name fragment to (tx, ty, tz).
    hold_secs: how long to hold all state + physics + scene simultaneously.
    """
    js = f"""
var BASE = ptr({hex(VEHICLE_ARRAY_ADDR)}).readPointer();
var count = ptr({hex(VEHICLE_COUNT_ADDR)}).readU32();
var dim = ptr({hex(SCENE_BASE_ADDR)}).add(89*4).readU32();
var nodeTable = ptr(ptr({hex(SCENE_BASE_ADDR)}).add(86*4).readU32());

var targetIdx = -1;
var TARGET = {repr(target_name.lower())};
for (var i = 0; i < Math.min(count, 24); i++) {{
    var v = BASE.add(i * {hex(STRIDE)});
    var nb = v.add({hex(OFF_NAME)}).readByteArray(24);
    var arr = new Uint8Array(nb);
    var name = '';
    for (var j = 0; j < arr.length && arr[j] != 0; j++) name += String.fromCharCode(arr[j]);
    if (name.toLowerCase().indexOf(TARGET) >= 0) {{
        targetIdx = i;
        send('found: car[' + i + '] = ' + name);
        break;
    }}
}}

if (targetIdx < 0) {{
    send('ERROR: no car matching "' + TARGET + '"');
}} else {{
    var car = BASE.add(targetIdx * {hex(STRIDE)});
    send('AF=' + car.add({hex(OFF_AF)}).readU8() + ' RF=' + car.add({hex(OFF_RF)}).readU8() + ' nodeIdx=' + car.add({hex(OFF_NODE_IDX)}).readU32() + '/' + car.add({hex(OFF_NODE_SUB)}).readU32());

    var TX = {tx};
    var TY = {ty};
    var TZ = {tz};
    send('target: (' + TX.toFixed(2) + ',' + TY.toFixed(2) + ',' + TZ.toFixed(2) + ')');

    // Hold everything simultaneously every tick for hold_secs
    // Re-read nodeIdx each tick in case scene manager reassigns on activation
    var iters = 0;
    var TOTAL = {int(hold_secs * 200)};
    var interval = setInterval(function() {{
        var cni = car.add({hex(OFF_NODE_IDX)}).readU32();
        var csi = car.add({hex(OFF_NODE_SUB)}).readU32();
        var cEntry = nodeTable.add(28 * (cni + csi * dim));

        car.add({hex(OFF_AF)}).writeU8(1);
        car.add({hex(OFF_RF)}).writeU8(0);
        car.add({hex(OFF_FS)}).writeU32(0);
        car.add({hex(OFF_RC)}).writeU32(0);
        car.add(0x4040).writeU32(1);        // PitState=1
        car.add({hex(OFF_PHYS_X)}).writeFloat(TX);
        car.add({hex(OFF_PHYS_Y)}).writeFloat(TY);
        car.add({hex(OFF_PHYS_Z)}).writeFloat(TZ);
        cEntry.writeFloat(TX);
        cEntry.add(4).writeFloat(TY);
        cEntry.add(8).writeFloat(TZ);
        iters++;

        if (iters >= TOTAL) {{
            clearInterval(interval);
            var fni = car.add({hex(OFF_NODE_IDX)}).readU32();
            var fsi = car.add({hex(OFF_NODE_SUB)}).readU32();
            var fEntry = nodeTable.add(28 * (fni + fsi * dim));
            send('done: AF=' + car.add({hex(OFF_AF)}).readU8() + ' scene_z=' + fEntry.add(8).readFloat().toFixed(2) + ' phys_z=' + car.add({hex(OFF_PHYS_Z)}).readFloat().toFixed(2));
        }}
    }}, 5);
}}
"""
    for line in run_script(pid, js, timeout=hold_secs + 2):
        print(line)


def resolve_position(pid, pos_arg, offset_x=10.0, offset_z=0.0):
    """Read player physics position. Returns (x, y, z) offset by offset_x/z."""
    if pos_arg in ("player", "ahead"):
        js = f"""
var BASE = ptr({hex(VEHICLE_ARRAY_ADDR)}).readPointer();
var px = BASE.add({hex(OFF_PHYS_X)}).readFloat();
var py = BASE.add({hex(OFF_PHYS_Y)}).readFloat();
var pz = BASE.add({hex(OFF_PHYS_Z)}).readFloat();
send(px.toFixed(4) + ',' + py.toFixed(4) + ',' + pz.toFixed(4));
"""
        results = run_script(pid, js, timeout=2)
        for r in results:
            if "," in r and "ERR" not in r:
                parts = r.split(",")
                return float(parts[0]) + offset_x, float(parts[1]), float(parts[2]) + offset_z
    return None


def teleport_all_retired(pid, ty=-1000.0, hold_secs=3.0):
    """Single Frida attach, one setInterval handles all cars sequentially per tick."""
    js = f"""
var BASE = ptr({hex(VEHICLE_ARRAY_ADDR)}).readPointer();
var count = ptr({hex(VEHICLE_COUNT_ADDR)}).readU32();
var dim = ptr({hex(SCENE_BASE_ADDR)}).add(89*4).readU32();
var nodeTable = ptr(ptr({hex(SCENE_BASE_ADDR)}).add(86*4).readU32());

var retired = [];
for (var i = 0; i < Math.min(count, 24); i++) {{
    var v = BASE.add(i * {hex(STRIDE)});
    var fs = v.add({hex(OFF_FS)}).readU32();
    var rf = v.add({hex(OFF_RF)}).readU8();
    if (fs !== 2 && rf !== 1) continue;
    var nb = v.add({hex(OFF_NAME)}).readByteArray(24);
    var arr = new Uint8Array(nb);
    var name = '';
    for (var j = 0; j < arr.length && arr[j] !== 0; j++) name += String.fromCharCode(arr[j]);
    var origFs = fs > 0 ? fs : 2;
    var origRc = v.add({hex(OFF_RC)}).readU32();
    retired.push({{idx: i, name: name, car: BASE.add(i * {hex(STRIDE)}), origFs: origFs, origRc: origRc}});
}}

send('found ' + retired.length + ' retired cars');
if (retired.length === 0) {{
    send('nothing to teleport');
}} else {{
    var TY = {ty};
    var TELEPORT_TICKS = {int(hold_secs * 200)};
    var POST_TICKS = 400;
    var TOTAL = TELEPORT_TICKS + POST_TICKS;
    var iters = 0;

    retired.forEach(function(r) {{ send('  target: [' + r.idx + '] ' + r.name); }});

    var interval = setInterval(function() {{
        var phase = iters < TELEPORT_TICKS ? 'teleport' : 'retire';

        for (var ri = 0; ri < retired.length; ri++) {{
            var r = retired[ri];
            var car = r.car;
            var cni = car.add({hex(OFF_NODE_IDX)}).readU32();
            var csi = car.add({hex(OFF_NODE_SUB)}).readU32();
            var cEntry = nodeTable.add(28 * (cni + csi * dim));

            if (phase === 'teleport') {{
                car.add({hex(OFF_AF)}).writeU8(1);
                car.add({hex(OFF_RF)}).writeU8(0);
                car.add({hex(OFF_FS)}).writeU32(0);
                car.add({hex(OFF_RC)}).writeU32(0);
                car.add(0x4040).writeU32(1);
            }} else {{
                car.add({hex(OFF_AF)}).writeU8(0);
                car.add({hex(OFF_RF)}).writeU8(1);
                car.add({hex(OFF_FS)}).writeU32(r.origFs);
                car.add({hex(OFF_RC)}).writeU32(r.origRc);
            }}
            car.add(0x4058).writeU32(200);
            car.add(0x4060).writeU32(200);
            car.add(0x403c).writeS32(-1);
            car.add({hex(OFF_PHYS_X)}).writeFloat(0.0);
            car.add(0x20).writeFloat(TY);
            car.add({hex(OFF_PHYS_Z)}).writeFloat(0.0);
            cEntry.writeFloat(0.0);
            cEntry.add(4).writeFloat(TY);
            cEntry.add(8).writeFloat(0.0);
        }}

        iters++;

        if (iters === TELEPORT_TICKS) {{
            send('  switching to re-retire phase');
        }}

        if (iters >= TOTAL) {{
            clearInterval(interval);
            for (var ri = 0; ri < retired.length; ri++) {{
                var r = retired[ri];
                var car = r.car;
                car.add({hex(OFF_AF)}).writeU8(0);
                car.add({hex(OFF_RF)}).writeU8(1);
                car.add({hex(OFF_FS)}).writeU32(r.origFs);
                car.add({hex(OFF_RC)}).writeU32(r.origRc);
                car.add(0x4058).writeU32(200);
                car.add(0x4060).writeU32(200);
                car.add(0x403c).writeS32(-1);
                var fni = car.add({hex(OFF_NODE_IDX)}).readU32();
                var fsi = car.add({hex(OFF_NODE_SUB)}).readU32();
                var fEntry = nodeTable.add(28 * (fni + fsi * dim));
                send('  done: [' + r.idx + '] ' + r.name + ' scene_y=' + fEntry.add(4).readFloat().toFixed(0) + ' phys_y=' + car.add(0x20).readFloat().toFixed(0));
            }}
            send('all ' + retired.length + ' cars teleported');
        }}
    }}, 5);
}}
"""
    total_time = hold_secs + 2.0 + 2.0
    for line in run_script(pid, js, timeout=total_time + 2):
        print(line)


def main():
    pid = get_game_pid()
    if not pid:
        print("Game not running")
        sys.exit(1)
    print(f"Attached to PID {pid}")

    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)

    cmd = sys.argv[1]

    if cmd == "list":
        list_cars(pid)

    elif cmd == "teleport":
        if len(sys.argv) < 4:
            print("Usage: teleport <name> <x> <y> <z>  OR  teleport <name> player  OR  teleport <name> <target_name>")
            sys.exit(1)

        name = sys.argv[2]

        if sys.argv[3] == "player":
            ahead = len(sys.argv) > 4 and sys.argv[4] == "ahead"
            pos = resolve_position(pid, "player", offset_x=10.0, offset_z=30.0 if ahead else 0.0)
            if not pos:
                print("Could not read player position")
                sys.exit(1)
            tx, ty, tz = pos
            print(f"Teleporting to player (+10m X{', +30m Z ahead' if ahead else ''}): ({tx:.1f}, {ty:.1f}, {tz:.1f})")
            teleport(pid, name, tx, ty, tz)

        elif len(sys.argv) == 6:
            tx, ty, tz = float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
            teleport(pid, name, tx, ty, tz)

        else:
            print("Usage: teleport <name> <x> <y> <z>  OR  teleport <name> player")
            sys.exit(1)

    elif cmd == "teleport_retired":
        hold = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
        teleport_all_retired(pid, ty=-1000.0, hold_secs=hold)

    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)


if __name__ == "__main__":
    main()
