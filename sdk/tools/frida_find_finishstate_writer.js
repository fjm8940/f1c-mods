// Finds the actual instruction that writes FinishState (veh+0x4064) for one
// selected vehicle. Static analysis found ZERO writers of this field across
// the whole 7,138-function corpus (skeleton_facts_all.json extraction
// missed it) -- this uses MemoryAccessMonitor (page-guard) to catch the
// write live, no need to guess which function first.
//
// Usage: frida -p <pid> -l tools/frida_find_finishstate_writer.js
//   rpc.exports.pickCar(index)  -- select AI car (1-based, 0=player BLOCKED)
//   rpc.exports.arm()           -- arm the page-guard watch (re-arms after each hit)

var PLAYER_PTR = new NativePointer('0x007C54E8');
var VEHICLE_COUNT = new NativePointer('0x005C329C');
var STRIDE = 0x48C0;
var OFF_FINISHSTATE = 0x4064;

var NAMES = ['Matias', 'M.Schumacher', 'Hakkinen', 'Coulthard', 'Barrichello',
    'R.Schumacher', 'TesterA', 'Raikkonen', 'Trulli', 'Frentzen',
    'Hill', 'Irvine', 'Montoya', 'Fisichella', 'Manning',
    'Heidfeld', 'Salo', 'Sarrazin', 'Alesi', 'De La Rosa', 'Webber', 'Alonso'];

var selectedCar = null;
var selectedIndex = -1;
var watchAddr = null;
var hitCount = 0;

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

function symbolize(addr) {
    var sym = DebugSymbol.fromAddress(addr);
    return addr + ' ' + (sym ? sym.toString() : '(no symbol)');
}

function armWatch() {
    if (!selectedCar) {
        send('[error] pick a car first');
        return;
    }
    watchAddr = selectedCar.add(OFF_FINISHSTATE);
    var pageStart = watchAddr.and(ptr(0xFFFFF000));
    MemoryAccessMonitor.enable([{ base: pageStart, size: 0x1000 }], {
        onAccess: function (details) {
            if (details.operation !== 'write') return;
            // Only care if the faulting address's page contains our target;
            // MemoryAccessMonitor is page-granular, so double check we're
            // near the real offset (best-effort; still very informative).
            hitCount++;
            send('[WRITE #' + hitCount + '] to page containing FinishState. ' +
                 'Instruction: ' + symbolize(details.from) +
                 ' | thread=' + details.threadId);
            // Re-arm so we keep catching subsequent writes on this page.
            MemoryAccessMonitor.enable([{ base: pageStart, size: 0x1000 }], arguments.callee ? undefined : undefined);
        }
    });
    send('[armed] watching page 0x' + pageStart + ' containing FinishState @ ' + watchAddr);
}

rpc.exports = {
    pickCar: function (index) {
        if (index === 0) return 'BLOCKED: index 0 is player car. Use 1+';
        var veh = getVehicle(index);
        if (!veh) return 'invalid index (count=' + VEHICLE_COUNT.readU32() + ')';
        selectedCar = veh;
        selectedIndex = index;
        var name = index < NAMES.length ? NAMES[index] : ('idx' + index);
        send('[pick] ' + name + ' at ' + veh);
        return 'selected ' + name + ' at ' + veh;
    },
    arm: function () {
        armWatch();
        return 'armed';
    },
    status: function () {
        return { selectedIndex: selectedIndex, watchAddr: watchAddr ? watchAddr.toString() : null, hitCount: hitCount };
    }
};

send('=== FinishState writer finder loaded ===');
send('Usage: pickCar(<index>), then arm(), then cause that car to crash/retire.');
