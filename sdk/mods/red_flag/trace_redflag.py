"""
trace_redflag.py — Monitor for automatic red flag trigger conditions.

Polls all vehicles at 10Hz and evaluates 4 trigger criteria:

  1. 5+ retirements in 60s in same sector (excludes pit/garage)
  2. 5+ retirements in 3 minutes
  3. Crash > 300 km/h with heavy damage (carConditions >= 2)
  4. 25%+ grid at <= 30 km/h for 30s (race session only)

Offsets verified live 2026-07-23.

Usage:
  python trace_redflag.py              # run until Ctrl+C
  python trace_redflag.py --hz 20      # 20Hz polling
"""

import ctypes
import struct
import time
import sys
from collections import deque
from datetime import datetime

k32 = ctypes.windll.kernel32
PROCESS_VM_READ = 0x10

# Vehicle struct offsets (all verified)
OFF_RACEPOS       = 0x4060
OFF_CURRENTLAP    = 0x403C
OFF_FINISHSTATE   = 0x4064
OFF_REASONCODE    = 0x4068
OFF_PITSTATE      = 0x4040
OFF_RETIREDFLAG   = 0x4410
OFF_ACTIVEFLAG    = 0x45F4
OFF_FUELLEVEL     = 0x3984
OFF_SPEEDCAP      = 0x453C
OFF_ACTUALSPEED   = 0x4440   # m/s, verified: 75 m/s ≈ 271 km/h
OFF_TRACKDIST     = 0x4048   # track distance in meters (NOT 0-1)
OFF_CARCONDITIONS = 0x3D6C   # 0=ok, 1=minor, 2+=heavy damage
OFF_DRIVERSTATUS  = 0x4510   # AI state: 0=race, 4=retire, 7=slow
OFF_PHYSX         = 0x001C
OFF_PHYSY         = 0x0020
OFF_PHYSZ         = 0x0024

# Globals
VEHICLE_COUNT_ADDR  = 0x005C329C
VEHICLE_ARRAY_ADDR  = 0x007C54F0
SESSION_TYPE_ADDR   = 0x007C56A4
RACE_PHASE_ADDR     = 0x007C56A8
PAUSE_FLAG_ADDR     = 0x007C56AC

# Constants
SPEED_300KMH = 83.33   # m/s
SPEED_30KMH  = 8.33    # m/s
SECTOR_COUNT = 3

REASON_NAMES = {
    0: 'none', 1: 'engine', 2: 'suspension', 3: 'unk3',
    4: 'brake', 5: 'gearbox', 6: 'puncture', 7: 'unk7',
    8: 'electronics', 9: 'fuel'
}


def find_pid():
    import subprocess
    r = subprocess.run(
        ['tasklist', '/FI', 'IMAGENAME eq F1 Challenge Online.exe', '/FO', 'CSV', '/NH'],
        capture_output=True, text=True
    )
    for line in r.stdout.strip().split('\n'):
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip('"'))
    return None


def read_int(h, addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, addr, buf, 4, None)
    return struct.unpack('<i', buf.raw)[0]

def read_float(h, addr):
    buf = ctypes.create_string_buffer(4)
    k32.ReadProcessMemory(h, addr, buf, 4, None)
    return struct.unpack('<f', buf.raw)[0]

def read_byte(h, addr):
    buf = ctypes.create_string_buffer(1)
    k32.ReadProcessMemory(h, addr, buf, 1, None)
    return struct.unpack('<B', buf.raw)[0]


class CarState:
    __slots__ = ['race_pos', 'lap', 'finish', 'reason', 'pit', 'retired',
                 'active', 'speed', 'track_dist', 'conditions', 'driver_status',
                 'x', 'y', 'z']

    def read(self, h, veh):
        self.race_pos      = read_int(h, veh + OFF_RACEPOS)
        self.lap           = read_int(h, veh + OFF_CURRENTLAP)
        self.finish        = read_int(h, veh + OFF_FINISHSTATE)
        self.reason        = read_int(h, veh + OFF_REASONCODE)
        self.pit           = read_int(h, veh + OFF_PITSTATE)
        self.retired       = read_byte(h, veh + OFF_RETIREDFLAG)
        self.active        = read_byte(h, veh + OFF_ACTIVEFLAG)
        self.speed         = read_float(h, veh + OFF_ACTUALSPEED)
        self.track_dist    = read_float(h, veh + OFF_TRACKDIST)
        self.conditions    = read_int(h, veh + OFF_CARCONDITIONS)
        self.driver_status = read_int(h, veh + OFF_DRIVERSTATUS)
        self.x             = read_float(h, veh + OFF_PHYSX)
        self.y             = read_float(h, veh + OFF_PHYSY)
        self.z             = read_float(h, veh + OFF_PHYSZ)

    def is_on_track(self):
        return self.finish == 0 and self.retired == 0

    def is_in_pit(self):
        return self.pit != 0


def track_dist_to_sector(dist, track_length):
    if track_length <= 0:
        return 0
    frac = (dist % track_length) / track_length
    return int(frac * SECTOR_COUNT)


def main():
    hz = 10
    for i, arg in enumerate(sys.argv[1:], 1):
        if arg == '--hz' and i < len(sys.argv) - 1:
            hz = int(sys.argv[i + 1])

    pid = find_pid()
    if not pid:
        print("F1 Challenge not running")
        return
    h = k32.OpenProcess(PROCESS_VM_READ, False, pid)
    if not h:
        print("Cannot open process")
        return

    count = read_int(h, VEHICLE_COUNT_ADDR)
    veh_addrs = []
    for i in range(count):
        veh_addrs.append(read_int(h, VEHICLE_ARRAY_ADDR + i * 4))

    print(f"Attached PID {pid}, {count} vehicles, polling {hz}Hz")
    print("=" * 80)
    print("TRIGGERS MONITORED:")
    print("  T1: 5+ retirements in 60s in same sector")
    print("  T2: 5+ retirements in 3 minutes")
    print("  T3: Crash > 300 km/h with heavy damage (conditions >= 2)")
    print("  T4: 25%+ grid <= 30 km/h for 30s continuous")
    print("=" * 80)

    # State
    prev = [CarState() for _ in range(count)]
    for i in range(count):
        prev[i].read(h, veh_addrs[i])

    # Retirement event log: list of (timestamp, car_index, sector, speed_at_retire)
    retire_events = deque()

    # T3: track max speed per car, reset on damage transition
    max_speed_since_ok = [0.0] * count
    prev_conditions = [0] * count

    # T4: continuous slow-grid timer
    slow_grid_start = None  # timestamp when 25%+ grid went slow
    slow_grid_threshold = count * 0.25

    # Track length estimation (max track_dist seen)
    est_track_length = 5800.0  # default ~Monza

    events = []
    triggers = []
    start = time.time()
    interval = 1.0 / hz
    tick = 0

    try:
        while True:
            now = time.time()
            session = read_int(h, SESSION_TYPE_ADDR)
            phase = read_int(h, RACE_PHASE_ADDR)
            paused = read_byte(h, PAUSE_FLAG_ADDR)

            if session != 7 or paused:
                time.sleep(interval)
                tick += 1
                continue

            cur = [CarState() for _ in range(count)]
            for i in range(count):
                cur[i].read(h, veh_addrs[i])

            ts = f"{now - start:8.2f}s"

            # Update track length estimate
            for i in range(count):
                if cur[i].track_dist > est_track_length:
                    est_track_length = cur[i].track_dist * 1.05

            # Per-car checks
            on_track_count = 0
            slow_count = 0
            active_racing = 0

            for i in range(count):
                c = cur[i]
                p = prev[i]

                # Track max speed while undamaged
                if c.conditions == 0:
                    if c.speed > max_speed_since_ok[i]:
                        max_speed_since_ok[i] = c.speed

                # --- RETIREMENT DETECTION ---
                if c.finish == 2 and p.finish != 2:
                    reason = REASON_NAMES.get(c.reason, f'unk({c.reason})')
                    sector = track_dist_to_sector(c.track_dist, est_track_length)
                    speed_kmh = p.speed * 3.6
                    max_speed_kmh = max_speed_since_ok[i] * 3.6

                    ev = (f"[{ts}] RETIRE Car {i:2d} P{c.race_pos:2d} "
                          f"reason={reason} sector={sector} "
                          f"speed={speed_kmh:.0f}km/h maxSpeed={max_speed_kmh:.0f}km/h "
                          f"conditions={c.conditions} lap={c.lap}")
                    print(ev)
                    events.append(ev)

                    retire_events.append({
                        'time': now, 'car': i, 'sector': sector,
                        'speed': p.speed, 'max_speed': max_speed_since_ok[i],
                        'conditions': c.conditions, 'reason': c.reason,
                        'in_pit': c.is_in_pit(),
                        'track_dist': c.track_dist,
                    })
                    max_speed_since_ok[i] = 0.0

                # --- DAMAGE TRANSITION ---
                if c.conditions != prev_conditions[i]:
                    speed_kmh = c.speed * 3.6
                    ev = (f"[{ts}] DAMAGE Car {i:2d} P{c.race_pos:2d} "
                          f"conditions {prev_conditions[i]}->{c.conditions} "
                          f"speed={speed_kmh:.0f}km/h")
                    print(ev)
                    events.append(ev)

                    # T3: heavy crash detection
                    if c.conditions >= 2 and max_speed_since_ok[i] >= SPEED_300KMH:
                        crash_kmh = max_speed_since_ok[i] * 3.6
                        trig = (f"[{ts}] *** T3 TRIGGER: Car {i:2d} HEAVY CRASH "
                                f"at {crash_kmh:.0f}km/h conditions={c.conditions} ***")
                        print(trig)
                        triggers.append(trig)
                        events.append(trig)

                prev_conditions[i] = c.conditions

                # Count active on-track cars and slow cars
                if c.is_on_track() and not c.is_in_pit():
                    active_racing += 1
                    if c.speed <= SPEED_30KMH and c.lap > 0:
                        slow_count += 1

            # --- T1: 5+ retirements in 60s in same sector (exclude pit) ---
            cutoff_60s = now - 60.0
            recent_60s = [e for e in retire_events if e['time'] >= cutoff_60s and not e['in_pit']]
            for s in range(SECTOR_COUNT):
                sector_retires = [e for e in recent_60s if e['sector'] == s]
                if len(sector_retires) >= 5:
                    trig = (f"[{ts}] *** T1 TRIGGER: {len(sector_retires)} retirements "
                            f"in sector {s} within 60s ***")
                    print(trig)
                    triggers.append(trig)
                    events.append(trig)

            # --- T2: 5+ retirements in 3 minutes ---
            cutoff_3m = now - 180.0
            recent_3m = [e for e in retire_events if e['time'] >= cutoff_3m]
            if len(recent_3m) >= 5:
                if tick % (hz * 5) == 0:  # only log every 5s to avoid spam
                    trig = (f"[{ts}] *** T2 TRIGGER: {len(recent_3m)} retirements "
                            f"in last 3 minutes ***")
                    print(trig)
                    triggers.append(trig)
                    events.append(trig)

            # --- T4: 25%+ grid <= 30 km/h for 30s ---
            if active_racing > 0 and phase >= 5:  # only during active racing
                if slow_count >= active_racing * 0.25:
                    if slow_grid_start is None:
                        slow_grid_start = now
                        ev = (f"[{ts}] T4 WATCH: {slow_count}/{active_racing} cars "
                              f"<= 30km/h (timer started)")
                        print(ev)
                        events.append(ev)
                    elif now - slow_grid_start >= 30.0:
                        trig = (f"[{ts}] *** T4 TRIGGER: {slow_count}/{active_racing} cars "
                                f"<= 30km/h for 30+ seconds ***")
                        print(trig)
                        triggers.append(trig)
                        events.append(trig)
                        slow_grid_start = now  # reset to avoid re-trigger every tick
                else:
                    if slow_grid_start is not None:
                        duration = now - slow_grid_start
                        if duration > 1.0:
                            ev = (f"[{ts}] T4 CLEARED: slow grid lasted {duration:.1f}s "
                                  f"(need 30s)")
                            print(ev)
                            events.append(ev)
                    slow_grid_start = None

            # Prune old events
            while retire_events and retire_events[0]['time'] < now - 300:
                retire_events.popleft()

            # Periodic status
            if tick % (hz * 15) == 0 and tick > 0:
                active = sum(1 for c in cur if c.is_on_track())
                dnf = sum(1 for c in cur if c.finish == 2)
                max_lap = max(c.lap for c in cur)
                max_spd = max(c.speed * 3.6 for c in cur)
                recent_ret = len([e for e in retire_events if e['time'] >= now - 180])
                damaged = sum(1 for c in cur if c.conditions > 0 and c.is_on_track())
                print(f"[{ts}] STATUS: phase={phase} active={active} dnf={dnf} "
                      f"damaged_on_track={damaged} maxLap={max_lap} "
                      f"topSpeed={max_spd:.0f}km/h retires_3m={recent_ret} "
                      f"triggers={len(triggers)} trackLen~{est_track_length:.0f}m")

            prev = cur
            tick += 1
            elapsed = time.time() - now
            sleep_time = interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        pass

    k32.CloseHandle(h)

    print(f"\n{'=' * 80}")
    print(f"Trace complete. {time.time() - start:.1f}s, {len(events)} events, "
          f"{len(triggers)} triggers fired")

    if triggers:
        print(f"\nTRIGGERS FIRED:")
        for t in triggers:
            print(f"  {t}")

    outfile = "trace_redflag_output.txt"
    with open(outfile, 'w') as f:
        f.write(f"Red Flag Trace - {datetime.now().isoformat()}\n")
        f.write(f"Duration: {time.time() - start:.1f}s\n")
        f.write(f"Events: {len(events)}, Triggers: {len(triggers)}\n\n")
        for ev in events:
            f.write(ev + '\n')
        if triggers:
            f.write(f"\n--- TRIGGERS ---\n")
            for t in triggers:
                f.write(t + '\n')
    print(f"Saved to {outfile}")


if __name__ == '__main__':
    main()
