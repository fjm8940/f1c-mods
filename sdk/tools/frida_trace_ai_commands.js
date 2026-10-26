// Frida script: trace AI command-queue dispatch for one selected vehicle.
// Answers the open question in mods/ai_pit_retire/README.md: which command
// code (a2) actually fires ActiveFlag=0 during a natural retirement, and
// does it happen through AICommandMessageQueueDispatcher_00424cd0's own
// bottom dedup-queue block (line 60: if(!a2 && FinishState==2) ActiveFlag=0)
// or through case 4/5's sub_4A46F0 call?
//
// Usage: frida -p <pid> -l tools/frida_trace_ai_commands.js
//   rpc.exports.pickCar(index)  — select AI car (1-based, 0=player BLOCKED)
//   rpc.exports.status()
//
// Hooks:
//   0x00424cd0 AICommandMessageQueueDispatcher(this, a2, a3) — thiscall,
//     this in ECX, a2/a3 on stack. Logs every call for the selected car.
//   0x00429ee0 CommandQueueResponseHandler(this) — thiscall, this in ECX
//     only. Logs every call for the selected car (fires once per enqueued
//     command per case_15/dedup path, per the decompiled source).

var PLAYER_PTR = new NativePointer('0x007C54E8');
var VEHICLE_COUNT = new NativePointer('0x005C329C');
var STRIDE = 0x48C0;

var OFF_FINISHSTATE = 0x4064;
var OFF_RETIREDFLAG = 0x4410;
var OFF_ACTIVEFLAG = 0x45F4;
var OFF_REASONCODE = 0x4068;
var OFF_4544 = 0x4544;

var NAMES = ['Matias', 'M.Schumacher', 'Hakkinen', 'Coulthard', 'Barrichello',
    'R.Schumacher', 'TesterA', 'Raikkonen', 'Trulli', 'Frentzen',
    'Hill', 'Irvine', 'Montoya', 'Fisichella', 'Manning',
    'Heidfeld', 'Salo', 'Sarrazin', 'Alesi', 'De La Rosa', 'Webber', 'Alonso'];

var selectedCar = null;   // null = log for ALL cars (broad mode)
var selectedIndex = -1;

function getBase() {
    try { return PLAYER_PTR.readPointer(); }
    catch (e) { return ptr(0); }
}

function getVehicle(index) {
    var base = getBase();
    if (base.isNull()) return null;
    var count = VEHICLE_COUNT.readU32();
    if (index < 0 || index >= count) return null;
    return base.add(index * STRIDE);
}

function nameFor(vehPtr) {
    var base = getBase();
    if (base.isNull()) return '?';
    var diff = vehPtr.sub(base).toInt32();
    if (diff % STRIDE !== 0) return '?(unaligned)';
    var idx = diff / STRIDE;
    if (idx < 0 || idx >= NAMES.length) return '?(idx' + idx + ')';
    return NAMES[idx];
}

function snap(veh) {
    return 'FS=' + veh.add(OFF_FINISHSTATE).readS32() +
           ' RC=' + veh.add(OFF_REASONCODE).readS32() +
           ' Retired=' + veh.add(OFF_RETIREDFLAG).readU8() +
           ' Active=' + veh.add(OFF_ACTIVEFLAG).readU8() +
           ' f4544=' + veh.add(OFF_4544).readS32();
}

// Broad mode (selectedCar==null): log for ALL cars, but only on a CHANGE
// of a2 per car (this dispatcher is called every frame per car, mostly
// with the same idle a2 -- naive logging would flood).
var lastA2 = {};

Interceptor.attach(ptr('0x00424cd0'), {
    onEnter: function (args) {
        var vehPtr = this.context.ecx;
        if (selectedCar && !vehPtr.equals(selectedCar)) return;
        var a2 = args[0].toInt32();
        var a3 = args[1].toInt32();
        var key = vehPtr.toString();
        if (!selectedCar) {
            if (lastA2[key] === a2) return;
            lastA2[key] = a2;
        }
        send('[dispatch] ' + nameFor(vehPtr) + ' a2(cmd)=' + a2 + ' a3(param)=' + a3 + ' | ' + snap(vehPtr));
    }
});

Interceptor.attach(ptr('0x00429ee0'), {
    onEnter: function (args) {
        var vehPtr = this.context.ecx;
        if (selectedCar && !vehPtr.equals(selectedCar)) return;
        send('[response-handler] ' + nameFor(vehPtr) + ' | ' + snap(vehPtr));
    }
});

Interceptor.attach(ptr('0x004a46f0'), {
    onEnter: function (args) {
        var vehPtr = this.context.ecx;
        if (selectedCar && !vehPtr.equals(selectedCar)) return;
        var a2 = args[0].toInt32();
        var a3 = args[1].toInt32();
        send('[sub_4A46F0 state-transition] ' + nameFor(vehPtr) + ' arg1=' + a2 + ' arg2=' + a3 + ' | ' + snap(vehPtr));
    }
});

rpc.exports = {
    pickCar: function (index) {
        if (index === 0) return 'BLOCKED: index 0 is player car. Use 1+';
        var veh = getVehicle(index);
        if (!veh) return 'invalid index (count=' + VEHICLE_COUNT.readU32() + ')';
        selectedCar = veh;
        selectedIndex = index;
        send('[pick] car ' + index + ' at ' + veh + ' | ' + snap(veh));
        return 'selected car ' + index + ' at ' + veh;
    },
    status: function () {
        if (!selectedCar) return 'no car selected';
        return snap(selectedCar);
    }
};

send('=== AI command-queue tracer loaded ===');
send('Usage: pickCar(<index>), then cause that car to crash. Watch for [dispatch]/[response-handler]/[sub_4A46F0] lines.');
