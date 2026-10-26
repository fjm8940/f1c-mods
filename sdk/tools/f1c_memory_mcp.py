"""Internal implementation. Public entry point: tools/f1c_mod.py memory-mcp.

No write/injection/debugger capability. IPC accepts only fixed semantic reads.
"""
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
import time
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
TARGET = 'F1 Challenge Online.exe'
SHA256 = '1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003'
ACCESS = 0x0400 | 0x0010  # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
MAX_MESSAGE = 2 * 1024 * 1024
OPS = {'health', 'drivers', 'safety_car', 'trace', 'positions'}
# Previously reviewed runtime recipes and lifecycle/AI accessors; not commands.
FIELDS = {'orientation_up_x': (0x71c, 'f'), 'orientation_up_dot': (0x720, 'f'), 'orientation_up_z': (0x724, 'f'),
          'world_x': (0x34e0, 'f'), 'world_z': (0x34e8, 'f'), 'role': (0x32c8, 'i'), 'rank_raw': (0x4060, 'i'),
          'speed_ms': (0x4440, 'f'), 'speed_cap_ms': (0x4538, 'f'),
          'passing_norm': (0x44cc, 'f'), 'aggression': (0x44f8, 'f'),
          'interaction_target': (0x45fc, 'I'), 'lateral_request': (0x4420, 'f'),
          'finish': (0x4064, 'i'), 'retired': (0x4410, 'B'),
          'ai_control': (0x45f4, 'B'), 'pit_status': (0x3450, 'i'),
          'laps': (0x403c, 'i'), 'line_time': (0x4098, 'f'), 'best_lap': (0x409c, 'f'),
          'line_race_clock': (0x40b8, 'f'),
          'lap_dist': (0x4044, 'f'), 'lap_dist_player': (0x4048, 'f')}
LIMITATION = ('PassingNorm is tuning, not an overtake-intent Boolean. An interaction '
              'target may concern yielding; no target does not prove no intention. '
              'Unchanged simulation time may mean pause/inactivity; pause is not proven.')


def orientation_state(d):
    v=[d.get(k) for k in ('orientation_up_x','orientation_up_dot','orientation_up_z')]
    if any(x is None or not math.isfinite(x) for x in v) or not .90<=sum(x*x for x in v)<=1.10 or abs(v[1])>1.05:
        return 'unknown'
    return 'overturned' if v[1]<=-.25 else 'side/tilt' if v[1]<.5 else 'upright'


def utc():
    return datetime.now(timezone.utc).isoformat()


def validate_target(base, digest):
    if base != 0x400000 or digest.upper() != SHA256:
        raise ValueError('Unsupported executable hash/base; read refused')


def select_process(matches):
    if len(matches) != 1:
        raise ValueError('Game not found' if not matches else 'Multiple matching games; refusing ambiguous target')
    return matches[0]


def sid():
    import win32api, win32security
    token = win32security.OpenProcessToken(win32api.GetCurrentProcess(), 8)
    try:
        return win32security.ConvertSidToStringSid(win32security.GetTokenInformation(token, 1)[0])
    finally:
        token.Close()


def pipe_name():
    return r'\\.\pipe\F1C.ReadOnly.v13.' + sid()


def validate(request):
    if not isinstance(request, dict) or set(request) - {'op', 'samples', 'interval', 'slot'}:
        raise ValueError('Only op/samples/interval/slot are accepted')
    if request.get('op') not in OPS:
        raise ValueError('Unsupported read operation')
    if request['op']=='positions':
        if set(request)-{'op','slot'} or ('slot' in request and (type(request['slot']) is not int or not 0<=request['slot']<128)):
            raise ValueError('positions accepts only an integer slot0..127')
        return request
    if request['op'] != 'trace' and set(request) != {'op'}:
        raise ValueError('This operation accepts no parameters')
    if request['op'] == 'trace':
        n, interval = request.get('samples', 3), request.get('interval', 0.5)
        if type(n) is not int or not 2 <= n <= 10:
            raise ValueError('samples must be an integer from 2 to 10')
        if type(interval) not in (int, float) or not math.isfinite(interval) or not 0.25 <= interval <= 1:
            raise ValueError('interval must be 0.25 to 1 seconds')
    return request


def compare(before, after):
    """Compare trusted snapshot envelopes, refusing cross-process/session diffs."""
    if not before.get('ok') or not after.get('ok'):
        return {'comparable': False, 'reason': 'Both observations must be successful'}
    if before.get('identity') != after.get('identity'):
        return {'comparable': False, 'reason': 'Process identity changed'}
    a, b = before.get('sim_time'), after.get('sim_time')
    if a is None or b is None or b < a:
        return {'comparable': False, 'reason': 'Simulation unavailable or reset'}
    old = {d['slot']: d for d in before.get('drivers', [])}
    changes = []
    for driver in after.get('drivers', []):
        previous = old.get(driver['slot'])
        if not previous or previous['name'] != driver['name']:
            changes.append({'slot': driver['slot'], 'identity_changed': True})
        else:
            delta = {k: {'before': previous.get(k), 'after': v} for k, v in driver.items()
                     if previous.get(k) != v}
            if delta:
                changes.append({'slot': driver['slot'], 'changes': delta})
    removed = sorted(set(old) - {d['slot'] for d in after.get('drivers', [])})
    return {'comparable': True, 'simulation_advanced': b > a, 'sim_delta': b-a,
            'driver_changes': changes, 'removed_slots': removed,
            'heartbeat_before': before.get('heartbeat'), 'heartbeat_after': after.get('heartbeat')}


def position_inventory(read, block, base, stride, slots, target_slot=None):
    """Fixed, bounded vehicle-domain inventory; no caller-supplied addresses.

    Offsets include the +0xC physics-owner adjustment. Raw retired slots are
    evidence for offline discovery, NOT a claim that all floats are positions.
    """
    if stride != 0x48c0 or not 1 <= slots <= 128 or len(block) != stride*slots:
        raise ValueError('Unsupported vehicle inventory layout')
    from f1c_vehicle_object_graph import scene_inventory, spatial_inventory, retired_part_pools
    rows = []
    rejected_target = None
    raw_count = 0
    for slot in range(slots):
        b = block[slot*stride:(slot+1)*stride]
        def unpack(off, fmt):
            return struct.unpack_from('<'+fmt, b, off)
        name = b[0x4620:0x4660].split(b'\0')[0].decode('latin1')
        role, finish = unpack(0x32c8, 'i')[0], unpack(0x4064, 'i')[0]
        retired, ai = b[0x4410], b[0x45f4]
        if not name or any(ord(c)<32 for c in name) or role not in (-1,0,1,2) or finish not in range(4) or retired>1 or ai>1:
            if slot == target_slot:
                rejected_target = dict(slot=slot, vehicle_address=hex(base+slot*stride),
                    name=name, role=role, finish=finish, retired=retired, ai_control=ai,
                    validated_driver=False, reason='Normal driver inventory validation rejected this slot; raw diagnostic only, no pointer traversal.',
                    vehicle_bytes_hex=b.hex(), vehicle_sha256=hashlib.sha256(b).hexdigest())
                # Native replay role3 is not a normal running driver. Only an
                # explicit, named, self-owned and bounded identity may expose
                # the existing read-only scene walk for physics/render comparison.
                if (role == 3 and name and all(ord(c)>=32 for c in name)
                        and finish in range(4) and retired<=1 and ai<=1
                        and unpack(0,'I')[0] == base+slot*stride
                        and 0<=unpack(0x4898,'i')[0]<4096):
                    rejected_target['reason']='Replay-role diagnostic only; not a validated running driver. Bounded scene traversal enabled for explicit self-owned identity.'
                    rejected_target['replay_scene_graph']=scene_inventory(read,b)
            continue
        def vec(off, count=3):
            return [v if math.isfinite(v) else None for v in unpack(off, str(count)+'f')]
        row = dict(slot=slot, name=name, vehicle_address=hex(base+slot*stride), role=role,
                   finish=finish, retired=retired, ai_control=ai,
                   pit_status=unpack(0x3450,'i')[0], team=unpack(0x48a4,'i')[0], bay=unpack(0x48a8,'i')[0],
                   body_world=vec(0x34e0), body_rotation_matrix=vec(0x34bc,9),
                   body_euler=vec(0x34ec), transformed_point_world=vec(0x3b6c),
                   transformed_point_local=vec(0x3b60))
        try:
            table = struct.unpack('<I',read(0x5ea2c0+0x158,4))[0]
            width = struct.unpack('<I',read(0x5ea2c0+0x164,4))[0]
            if not 1<=width<=52 or not 0<=row['team']<width or not 0<=row['bay']<3 or not table:
                raise ValueError('Invalid garage table or assignment')
            address=table+28*(row['team']+row['bay']*width)
            if address>0xffffffff-28:
                raise ValueError('Garage address overflow')
            garage=struct.unpack('<6f',read(address,24))
            if not all(math.isfinite(v) and abs(v)<1e7 for v in garage) or not any(garage[:3]):
                raise ValueError('Invalid garage pose')
            row.update(garage_address=hex(address), garage_world=list(garage[:3]),garage_euler=list(garage[3:]))
            if all(v is not None for v in row['body_world']):
                row['body_garage_distance_m']=math.dist(row['body_world'],garage[:3])
        except (ValueError,struct.error) as exc:
            row['garage_unavailable']=str(exc)
        if retired or finish in (2,3) or slot==target_slot:
            raw_count+=1
            if raw_count>40:
                raise ValueError('Retired raw inventory exceeds bounded response')
            row['vehicle_bytes_hex']=b.hex()
            row['vehicle_sha256']=hashlib.sha256(b).hexdigest()
        if (target_slot is None and retired) or slot==target_slot:
            row['scene_graph']=scene_inventory(read,b)
            row['part_pools']=retired_part_pools(read,{n['address'] for n in row['scene_graph']['nodes']})
        rows.append(row)
    return {'rows':rows, 'rejected_target':rejected_target, 'spatial':spatial_inventory(read), 'coverage':'Named valid slots including role -1; full terminal vehicle bytes; retired scene branches and global spatial entries. Explicit rejected target is raw diagnostic only. Completeness/error fields apply. Loose-object originating driver remains unverified. Raw bytes are not typed coordinates.'}


def restart_service_inventory(read, block, stride, slots):
    """Fixed exact-target observations, not a write contract or repair command.

    Native004F1890 proves the player fuel mirror. Native004ED960/004F18C0
    and0043DA10 identify separate AI/player tyre-state candidates. Their full
    preservation semantics remain under investigation; raw values are retained.
    """
    if stride!=0x48c0 or not 1<=slots<=128 or len(block)!=stride*slots:
        raise ValueError('Unsupported service inventory layout')
    player=read(0x7c7428,0x1460)
    if len(player)!=0x1460:raise ValueError('Incomplete player service physics read')
    def finite(data,offset,fmt):
        value=struct.unpack_from('<'+fmt,data,offset)[0]
        return value if not isinstance(value,float) or math.isfinite(value) else None
    rows=[]
    for slot in range(slots):
        v=block[slot*stride:(slot+1)*stride]
        role=finite(v,0x32c8,'i')
        name=v[0x4620:0x4660].split(b'\0')[0].decode('latin1')
        if role not in (0,1,2) or not name or any(ord(c)<32 for c in name):continue
        rows.append({'slot':slot,'name':name,'role':role,
            'fuel':finite(v,0x3984,'f'),'fuel_capacity':finite(v,0x3988,'f'),
            'compound_index':finite(v,0x397c,'i'),
            'wheel_condition_candidates':[finite(v,0x363c+248*i,'f') for i in range(4)],
            'penalty_count':finite(v,0x4070,'i'),
            'penalty_pairs':[list(struct.unpack_from('<ii',v,0x4074+8*i)) for i in range(4)]})
    wheels=[]
    for i in range(4):
        start=652+1124*i
        wheels.append({'wheel':i,'condition_candidate':finite(player,start+916,'d'),
            'temperature_candidates':[finite(player,start+844+8*j,'d') for j in range(4)]})
    return {'vehicles':rows,'player_physics':{'address':'0x7c7428','bytes':len(player),
        'raw_hex':player.hex(),'sha256':hashlib.sha256(player).hexdigest(),
        'fuel_mirror':finite(player,0x19c,'d'),'compound_index':finite(player,5212,'i'),
        'wheels':wheels},
        'evidence':'Exact-target bounded read. Fuel ABI reviewed; wheel fields are static native-source candidates, not a complete tyre/damage restoration contract.'}


def session_context(read):
    # Exact-target native context used by RF's final restart gate. No writes.
    def value(address,fmt):
        data=read(address,struct.calcsize('<'+fmt))
        return struct.unpack('<'+fmt,data)[0]
    return {'game_state':value(0x7c569c,'i'),'session_type':value(0x7c56a4,'i'),
            'race_phase':value(0x7c56a8,'i'),'pause_flag':value(0x7c56ac,'B'),
            'player_out':value(0x7ce258,'i'),'player_vehicle':hex(value(0x7c54e8,'I')),
            'vehicle_count':value(0x5c329c,'i'),'replay_mode':value(0x8ab6e4,'i'),
            'active_vehicle_pointers':[hex(x) for x in struct.unpack('<104I',read(0x7c54f0,416))]}


class NativeReader:
    def capture(self, positions=False, slot=None):
        import psutil
        import win32api, win32process
        from safety_car_status import observe
        matches = []
        for p in psutil.process_iter(['pid', 'name']):
            if (p.info['name'] or '').lower() == TARGET.lower():
                matches.append(p)
        process = select_process(matches)
        identity = {'pid': process.pid, 'created': process.create_time()}
        handle = win32api.OpenProcess(ACCESS, False, process.pid)
        try:
            modules = win32process.EnumProcessModules(handle)
            path = Path(win32process.GetModuleFileNameEx(handle, modules[0]))
            validate_target(modules[0], hashlib.sha256(path.read_bytes()).hexdigest())
            k = ctypes.WinDLL('kernel32', use_last_error=True)
            k.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                           ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
            k.ReadProcessMemory.restype = ctypes.c_int
            def read(address, size):
                buf = ctypes.create_string_buffer(size)
                count = ctypes.c_size_t()
                if not k.ReadProcessMemory(int(handle), address, buf, size, ctypes.byref(count)) or count.value != size:
                    raise ValueError('Incomplete read: Windows error ' + str(ctypes.get_last_error()))
                return buf.raw
            layout = json.loads((ROOT/'tools/sdk/f1c_sdk.json').read_text())['vehicle_array']
            base, stride, slots = int(layout['base'], 0), int(layout['stride'], 0), layout['slots']
            # One contiguous array read reduces skew. Bound by simulation markers;
            # no suspended threads, locks, injection or claim of full atomicity.
            for _ in range(3):
                before = struct.unpack('<d', read(0x8afea0, 8))[0]
                block = read(base, stride * slots)
                inventory = position_inventory(read,block,base,stride,slots,slot) if positions else None
                if inventory is not None and slot is not None:
                    inventory['restart_service']=restart_service_inventory(read,block,stride,slots)
                context = session_context(read)
                after = struct.unpack('<d', read(0x8afea0, 8))[0]
                if math.isfinite(before) and before == after:
                    break
            else:
                raise ValueError('Simulation advanced during all three read attempts; retry')
            drivers = []
            for slot in range(slots):
                offset = slot * stride
                if struct.unpack_from('<i', block, offset+0x32c8)[0] == -1:
                    continue
                d = {'slot': slot, 'name': block[offset+0x4620:offset+0x4660].split(b'\0')[0].decode('latin1')}
                for field, (pos, fmt) in FIELDS.items():
                    value = struct.unpack_from('<'+fmt, block, offset+pos)[0]
                    d[field] = None if isinstance(value, float) and not math.isfinite(value) else value
                d['orientation'] = orientation_state(d)
                d['speed_kmh'] = None if d['speed_ms'] is None else d['speed_ms']*3.6
                d['speed_cap_kmh'] = None if d['speed_cap_ms'] is None else d['speed_cap_ms']*3.6
                drivers.append(d)
            addresses = {base+d['slot']*stride: d for d in drivers}
            for d in drivers:
                pointer = d.pop('interaction_target')
                target = addresses.get(pointer)
                d['target_present'] = bool(pointer)
                d['target_resolved'] = target is not None
                d['target_slot'] = target['slot'] if target else None
                d['target_name'] = target['name'] if target else None
            try:
                status = observe()
            except ValueError as exc:
                status = {'available': False, 'error': str(exc)}
            # Mapping has no owner PID. Its attribution is explicitly unverified.
            status['owner_pid_verified'] = False
            return {'identity': identity, 'exe_sha256': SHA256, 'sim_time': after,
                    'session_context':context,
                    'position_inventory': inventory,
                    'drivers': drivers, 'safety_car': status, 'heartbeat': status.get('heartbeat'),
                    'coherence': 'Simulation markers equal; external reads are not atomic'}
        finally:
            handle.Close()


class Service:
    def __init__(self, reader=None):
        self.reader = reader or NativeReader()
        self.previous = None
        self.inversions = {}
        self.inversion_wall = None

    def snapshot(self, positions=False, slot=None):
        started = time.monotonic()
        result = {'read_only': True, 'captured_at_utc': utc(), 'ok': False,
                  'pid': None, 'sim_time': None, 'heartbeat': None}
        try:
            result.update(self.reader.capture(positions=True,slot=slot) if positions else self.reader.capture())
            result['ok'] = True
            result['pid'] = result['identity']['pid']
            delta = compare(self.previous or {}, result)
            result['freshness'] = ('advancing' if delta.get('simulation_advanced') else
                                   'unchanged_pause_or_inactivity_possible' if delta.get('comparable') else
                                   'first_sample_or_new_session')
            now=time.monotonic()
            dt=delta.get('sim_delta',0)
            if not delta.get('comparable') or self.inversion_wall is None or now-self.inversion_wall>2:
                self.inversions={};dt=0
            self.inversion_wall=now
            for d in result.get('drivers',[]):
                key=(d['slot'],d['name']); old=self.inversions.get(key,{})
                good=d.get('orientation')=='overturned' and 0<=d.get('speed_ms',999)<2 and d.get('pit_status')==0
                x,z=d.get('world_x'),d.get('world_z')
                progress=old and x is not None and z is not None and (x-old['x'])**2+(z-old['z'])**2<=4
                held=old.get('held',0)+min(dt,.25) if good and progress else 0
                d['observed_inverted_seconds']=held
                d['suspected_incapacitated']=good and held>=3
                d['orientation_history_limit']='Observed samples only; gaps over 2s reset confirmation. Broker telemetry supplies continuous history.'
                if good and x is not None and z is not None:self.inversions[key]={'held':held,'x':x,'z':z}
                else:self.inversions.pop(key,None)
            self.previous = dict(result)
        except Exception as exc:
            result.update(error=str(exc), freshness='unavailable')
            self.previous = None
        result['captured_at_utc'] = utc()
        result['capture_duration_ms'] = round((time.monotonic()-started)*1000, 2)
        result['evidence_age_seconds'] = 0
        result['limitation'] = LIMITATION
        return result

    def dispatch(self, request):
        validate(request)
        if request['op'] == 'trace':
            samples = []
            for i in range(request.get('samples', 3)):
                if i:
                    time.sleep(request.get('interval', .5))
                samples.append(self.snapshot())
            return {'read_only': True, 'samples': samples, 'comparison': compare(samples[0], samples[-1])}
        result = self.snapshot(positions=request['op']=='positions',slot=request.get('slot'))
        result['reader_elevated'] = bool(ctypes.windll.shell32.IsUserAnAdmin())
        result['reader_pid'] = os.getpid()
        if request['op'] == 'health':
            result.pop('drivers', None)
        if request['op'] == 'safety_car':
            result['safety_car_candidates'] = [d for d in result.pop('drivers', [])
                                              if 'mayland' in d['name'].lower() or 'safety' in d['name'].lower()]
            result['candidate_limitation'] = 'Name-based candidates only; SC vehicle identity is not authoritative'
        return result


def receive(handle, timeout):
    import win32file, pywintypes
    deadline = time.monotonic()+timeout
    while time.monotonic() < deadline:
        try:
            _, data = win32file.ReadFile(handle, MAX_MESSAGE)
            if not data or len(data) >= MAX_MESSAGE:
                raise ValueError('Empty/oversized IPC message')
            return json.loads(data)
        except pywintypes.error as exc:
            if exc.winerror not in (232, 233, 536):
                raise
            time.sleep(.02)
    raise TimeoutError('Reader IPC timeout')


def send(handle, result):
    import win32file
    data = json.dumps(result, allow_nan=False).encode()
    if len(data) >= MAX_MESSAGE:
        raise ValueError('Response exceeds size limit')
    _, written = win32file.WriteFile(handle, data)
    if written != len(data):
        raise ValueError('Incomplete IPC write')


def request(payload):
    validate(payload)
    return exchange(payload)


def exchange(payload):
    """Internal wire exchange; server independently validates every payload."""
    import win32file, win32pipe
    name = pipe_name()
    win32pipe.WaitNamedPipe(name, 2000)
    # SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION: server can identify only.
    handle = win32file.CreateFile(name, 0xc0000000, 0, None, 3, 0x00110000, None)
    try:
        win32pipe.SetNamedPipeHandleState(handle, 2 | 1, None, None)
        send(handle, payload)
        result = receive(handle, 25)
        for sample in result.get('samples', [result]):
            if 'captured_at_utc' in sample:
                sample['evidence_age_seconds'] = max(0, (datetime.now(timezone.utc)-
                    datetime.fromisoformat(sample['captured_at_utc'])).total_seconds())
        return result
    finally:
        handle.Close()


def serve_reader():
    import win32api, win32file, win32pipe, win32security, pywintypes
    if not ctypes.windll.shell32.IsUserAnAdmin():
        raise ValueError('Reader requires elevation; use memory-mcp start-reader')
    owner = sid()
    # Owner + SYSTEM only, local clients only, medium integrity readable IPC.
    descriptor = win32security.ConvertStringSecurityDescriptorToSecurityDescriptor(
        'D:P(A;;GA;;;SY)(A;;GA;;;'+owner+')S:(ML;;NW;;;ME)', 1)
    security = pywintypes.SECURITY_ATTRIBUTES()
    security.SECURITY_DESCRIPTOR = descriptor
    handle = win32pipe.CreateNamedPipe(pipe_name(), 3 | 0x00080000, 4 | 2 | 1 | 8,
                                      1, MAX_MESSAGE, MAX_MESSAGE, 1000, security)
    service = Service()
    expiry = time.monotonic()+8*3600
    try:
        while time.monotonic() < expiry:
            try:
                win32pipe.ConnectNamedPipe(handle, None)
            except pywintypes.error as exc:
                if exc.winerror == 536:
                    time.sleep(.03)
                    continue
                if exc.winerror == 232:
                    win32pipe.DisconnectNamedPipe(handle)
                    continue
                if exc.winerror != 535:
                    raise
            try:
                payload = receive(handle, 2)
                win32security.ImpersonateNamedPipeClient(handle)
                try:
                    token = win32security.OpenThreadToken(win32api.GetCurrentThread(), 8, True)
                    try:
                        client = win32security.ConvertSidToStringSid(win32security.GetTokenInformation(token, 1)[0])
                    finally:
                        token.Close()
                    if client != owner:
                        raise ValueError('Client identity rejected')
                finally:
                    win32security.RevertToSelf()
                response = service.dispatch(payload)
                send(handle, response)
                # Wait boundedly for the client to consume the response/disconnect.
                deadline = time.monotonic()+2
                while time.monotonic() < deadline:
                    try:
                        win32pipe.PeekNamedPipe(handle, 0)
                    except pywintypes.error:
                        break
                    time.sleep(.02)
            except Exception as exc:
                # Malformed/unauthorized/broken clients receive no privileged data.
                print(type(exc).__name__ + ': ' + str(exc), file=sys.stderr, flush=True)
                try:
                    send(handle, {'ok': False, 'read_only': True, 'freshness': 'unavailable',
                                  'error': 'Request rejected', 'captured_at_utc': utc()})
                    time.sleep(.1)
                except Exception:
                    pass
            finally:
                win32pipe.DisconnectNamedPipe(handle)
    finally:
        handle.Close()


def serve_mcp():
    from mcp.server.fastmcp import FastMCP
    from mcp.types import ToolAnnotations
    server = FastMCP('F1C read-only memory', instructions=LIMITATION, log_level='ERROR')
    annotations = ToolAnnotations(readOnlyHint=True, destructiveHint=False, idempotentHint=True, openWorldHint=False)
    def call(op, **kwargs):
        try:
            return request({'op': op, **kwargs})
        except Exception as exc:
            return {'ok': False, 'read_only': True, 'error': str(exc),
                    'action': 'Run canonical memory-mcp start-reader once per session',
                    'freshness': 'unavailable', 'captured_at_utc': utc()}
    @server.tool(annotations=annotations)
    def f1c_health() -> dict:
        """Check elevated-reader access, supported game, sim time and heartbeat."""
        return call('health')
    @server.tool(annotations=annotations)
    def f1c_drivers() -> dict:
        """Read all active driver speeds/caps, rank and interaction fields; not mental intentions."""
        return call('drivers')
    @server.tool(annotations=annotations)
    def f1c_positions(slot: int | None = None) -> dict:
        """Read named excluded/retired slots, garage poses and bounded raw terminal vehicle structures. Does not enumerate detached colliders."""
        return call('positions',**({'slot':slot} if slot is not None else {}))
    @server.tool(annotations=annotations)
    def f1c_safety_car() -> dict:
        """Read published SC status and name-matched SC speed/cap candidates."""
        return call('safety_car')
    @server.tool(annotations=annotations)
    def f1c_trace(samples: int = 3, interval: float = .5) -> dict:
        """Capture 2..10 observations, 0.25..1s apart, with a first/last comparison."""
        return call('trace', samples=samples, interval=interval)
    @server.tool(annotations=annotations)
    def f1c_compare(before: dict, after: dict) -> dict:
        """Compare two prior successful snapshots; process/session changes refuse comparison."""
        return compare(before, after)
    server.run(transport='stdio')


def main(args):
    action = args.action
    slot=getattr(args,'slot',None)
    if slot is not None:validate({'op':action,'slot':slot})
    if action == 'serve':
        return serve_mcp()
    if action == 'reader':
        import traceback
        log_dir = Path(os.environ['LOCALAPPDATA'])/'F1CReadOnly'
        log_dir.mkdir(exist_ok=True)
        with (log_dir/('reader-'+str(os.getpid())+'.log')).open('a', encoding='utf-8') as log:
            sys.stderr = log
            try:
                return serve_reader()
            except Exception:
                traceback.print_exc(file=log)
                raise
    if action == 'start-reader':
        import subprocess
        try:
            health = request({'op': 'health'})
            if health.get('reader_elevated'):
                print('Elevated reader already available, PID '+str(health['reader_pid']))
                return
        except Exception:
            pass
        # Fixed command, no request-controlled shell fragments. Pythonw keeps it hidden.
        pythonw = Path(sys.executable).with_name('pythonw.exe')
        entry = ROOT/'tools/f1c_mod.py'
        def quote(value):
            return "'"+str(value).replace("'", "''")+"'"
        command = ('Start-Process -Verb RunAs -WindowStyle Hidden -FilePath '+quote(pythonw)+
                   ' -ArgumentList '+quote('"'+str(entry)+'" memory-mcp reader'))
        subprocess.run(['powershell', '-NoProfile', '-Command', command], check=True, creationflags=0x08000000)
        print('Hidden reader launch requested; session lasts up to 8 hours.')
        return
    if action == 'test':
        import subprocess
        subprocess.run([sys.executable, str(ROOT/'tools/sdk/tests/test_memory_mcp.py')], check=True)
        return
    if action == 'verify':
        import asyncio
        asyncio.run(verify_mcp())
        return
    print(json.dumps(request({'op': action,**({'slot':slot} if slot is not None else {})}), indent=2))


async def verify_mcp():
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    params = StdioServerParameters(command=sys.executable,
                                  args=[str(ROOT/'tools/f1c_mod.py'), 'memory-mcp', 'serve'])
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            listed = (await session.list_tools()).tools
            names = [t.name for t in listed]
            assert set(names) == {'f1c_health', 'f1c_drivers', 'f1c_positions', 'f1c_safety_car', 'f1c_trace', 'f1c_compare'}
            assert all(t.annotations.readOnlyHint for t in listed)
            results = {}
            for name in names:
                arguments = {'before': {}, 'after': {}} if name == 'f1c_compare' else {}
                result = await session.call_tool(name, arguments)
                assert not result.isError, name
                results[name] = result.model_dump(mode='json')
            decoded = {name: json.loads(value['content'][0]['text']) for name, value in results.items()}
            assert decoded['f1c_health'].get('reader_elevated'), 'Elevated IPC not available'
            trace = decoded['f1c_trace']
            if decoded['f1c_drivers'].get('ok'):
                assert trace['samples'] and all(s['ok'] for s in trace['samples'])
            invalid = await session.call_tool('f1c_trace', {'samples': 999})
            assert not json.loads(invalid.content[0].text).get('ok')
            rejected = exchange({'op': 'unsupported-operation'})
            assert rejected.get('ok') is False and 'drivers' not in rejected
            recovered = request({'op': 'health'})
            assert recovered.get('reader_elevated')
            print(json.dumps({'tools': names, 'results': results,
                              'unsupported_wire_operation_rejected': True,
                              'connection_after_rejection': True}, indent=2))
