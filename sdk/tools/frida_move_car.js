// Frida script: real-time AI car coordinate manipulation
// Usage: frida -p <pid> -l tools/frida_move_car.js
//
// SAFE VERSION — writes ONLY to +0x1C (physics) and +0x34D4 (camera/world)
// Both confirmed by IDA. Does NOT touch player car.
//
// RPC API:
//   rpc.exports.pickCar(index)     — select AI car (1-based, 0=player BLOCKED)
//   rpc.exports.nudge(dx,dy,dz)    — shift car by delta, hold position
//   rpc.exports.release()           — stop holding, car returns to physics
//   rpc.exports.getPos()            — read current position of selected car
//   rpc.exports.listCars()          — show first 5 cars with positions
//   rpc.exports.status()            — check state

var PLAYER_PTR = new NativePointer('0x007C54E8');
var VEHICLE_COUNT = new NativePointer('0x005C329C');
var GAME_STATE = new NativePointer('0x007D00EC');

var STRIDE = 0x48C0;
var PHYS_X = 0x1C, PHYS_Y = 0x20, PHYS_Z = 0x24;
var WORLD_X = 0x34D4, WORLD_Y = 0x34D8, WORLD_Z = 0x34DC;

var selectedCar = null;
var selectedIndex = -1;
var holdX = 0, holdY = 0, holdZ = 0;
var worldOffsetX = 0, worldOffsetY = 0, worldOffsetZ = 0;
var holding = false;
var writeInterval = null;
var writeCount = 0;

// Original bytes at 0x4EF7C0 for recovery if needed
var ORIG_BYTES_4EF7C0 = [0x83,0xEC,0x30,0x8B,0x44,0x24,0x38,0x8B,
                         0x10,0x53,0x56,0x8B,0xF1,0x8D,0x9E,0xE0];

function getBase() {
    try { return PLAYER_PTR.readPointer(); }
    catch(e) { return ptr(0); }
}

function getVehicle(index) {
    var base = getBase();
    if (base.isNull()) return null;
    var count = VEHICLE_COUNT.readU32();
    if (index < 0 || index >= count) return null;
    return base.add(index * STRIDE);
}

function readPos(veh) {
    return {
        x: veh.add(PHYS_X).readFloat(),
        y: veh.add(PHYS_Y).readFloat(),
        z: veh.add(PHYS_Z).readFloat()
    };
}

function startHolding() {
    if (writeInterval) return;
    writeInterval = setInterval(function() {
        if (!selectedCar || !holding) return;
        // Write physics position
        selectedCar.add(PHYS_X).writeFloat(holdX);
        selectedCar.add(PHYS_Y).writeFloat(holdY);
        selectedCar.add(PHYS_Z).writeFloat(holdZ);
        // Write world/camera position (same coords)
        selectedCar.add(WORLD_X).writeFloat(holdX + worldOffsetX);
        selectedCar.add(WORLD_Y).writeFloat(holdY + worldOffsetY);
        selectedCar.add(WORLD_Z).writeFloat(holdZ + worldOffsetZ);
        writeCount++;
    }, 1);
}

function stopHolding() {
    if (writeInterval) {
        clearInterval(writeInterval);
        writeInterval = null;
    }
    holding = false;
}

// Restore 0x4EF7C0 if it was corrupted by a previous Frida session
function restorePathTracking() {
    var addr = ptr('0x004EF7C0');
    var first = addr.readU8();
    if (first === 0xE9) {
        send('[WARN] 0x4EF7C0 has stale JMP trampoline, restoring original bytes');
        Memory.protect(addr, 16, 'rwx');
        for (var i = 0; i < ORIG_BYTES_4EF7C0.length; i++) {
            addr.add(i).writeU8(ORIG_BYTES_4EF7C0[i]);
        }
        send('[WARN] restored');
    }
}

// Auto-restore on load
restorePathTracking();

// Position monitor
setInterval(function() {
    if (!selectedCar) return;
    var p = readPos(selectedCar);
    var gs = GAME_STATE.readU32();
    send('[car' + selectedIndex + '] X=' + p.x.toFixed(1) + ' Y=' + p.y.toFixed(1) + ' Z=' + p.z.toFixed(1) +
         ' | GS=' + gs + ' | ' + (holding ? 'HOLDING (' + writeCount + ' writes)' : 'free'));
}, 2000);

rpc.exports = {
    pickCar: function(index) {
        if (index === 0) return 'BLOCKED: index 0 is player car. Use 1+';
        var veh = getVehicle(index);
        if (!veh) return 'invalid index (count=' + VEHICLE_COUNT.readU32() + ')';
        selectedCar = veh;
        selectedIndex = index;
        var p = readPos(veh);
        // Capture world position offset relative to physics
        worldOffsetX = veh.add(WORLD_X).readFloat() - p.x;
        worldOffsetY = veh.add(WORLD_Y).readFloat() - p.y;
        worldOffsetZ = veh.add(WORLD_Z).readFloat() - p.z;
        send('[pick] car ' + index + ' at ' + veh + ' pos=(' + p.x.toFixed(1) + ',' + p.y.toFixed(1) + ',' + p.z.toFixed(1) + ')');
        send('[pick] world-phys offset=(' + worldOffsetX.toFixed(2) + ',' + worldOffsetY.toFixed(2) + ',' + worldOffsetZ.toFixed(2) + ')');
        return 'selected car ' + index;
    },

    nudge: function(dx, dy, dz) {
        if (!selectedCar) return 'pick a car first';
        var p = readPos(selectedCar);
        holdX = p.x + dx;
        holdY = p.y + dy;
        holdZ = p.z + dz;
        holding = true;
        writeCount = 0;
        startHolding();
        return 'holding at (' + holdX.toFixed(1) + ',' + holdY.toFixed(1) + ',' + holdZ.toFixed(1) + ')';
    },

    release: function() {
        stopHolding();
        return 'released (' + writeCount + ' total writes)';
    },

    getPos: function() {
        if (!selectedCar) return null;
        return readPos(selectedCar);
    },

    listCars: function() {
        var base = getBase();
        if (base.isNull()) return 'no vehicles';
        var count = Math.min(VEHICLE_COUNT.readU32(), 8);
        var result = [];
        var player = PLAYER_PTR.readPointer();
        for (var i = 0; i < count; i++) {
            var veh = base.add(i * STRIDE);
            var p = {
                x: veh.add(PHYS_X).readFloat(),
                y: veh.add(PHYS_Y).readFloat(),
                z: veh.add(PHYS_Z).readFloat()
            };
            var tag = veh.equals(player) ? 'PLAYER' : 'AI';
            result.push('[' + i + '] ' + tag + ' X=' + p.x.toFixed(1) + ' Z=' + p.z.toFixed(1));
        }
        return result.join('\n');
    },

    status: function() {
        return {
            selectedIndex: selectedIndex,
            holding: holding,
            holdPos: holding ? {x:holdX,y:holdY,z:holdZ} : null,
            writeCount: writeCount
        };
    }
};

send('=== Car Mover v5 loaded (SAFE: +0x1C and +0x34D4 only, no player) ===');
send('Usage: pickCar(1), nudge(30,0,0), release(), listCars()');
