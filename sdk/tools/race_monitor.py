"""
race_monitor.py — Live F1 Challenge vehicle struct monitor
Usage: python tools/race_monitor.py [--interval 5] [--drivers 22] [--docs docs/10-vehicle-physics.md]

Attaches to running F1 Challenge Online.exe, dumps all driver structs every N seconds,
and writes a single annotated CSV with change tracking. Run during a race.

Output: monitor_output/
  snapshot_TIMESTAMP.csv   — full annotated dump of all drivers at one point in time
  changes.csv              — fields that changed between snapshots (dynamic fields)
  summary.txt              — human-readable summary after race

Install dependency first: pip install pymem
"""

import struct
import re
import math
import os
import sys
import time
import argparse
import csv
from datetime import datetime

try:
    import pymem
except ImportError:
    print("Missing dependency. Run: pip install pymem")
    sys.exit(1)

# ── config ─────────────────────────────────────────────────────────────────────

STRUCT_SIZE   = 0x48C0
DRIVER0_BASE  = 0x005F0DEC
DRIVER_STRIDE = 0x48C0
OUTPUT_DIR    = "monitor_output"

# ── helpers ────────────────────────────────────────────────────────────────────

def is_sane_float(f):
    if math.isnan(f) or math.isinf(f):
        return False
    return -1e6 < f < 1e6

def is_printable_ascii(b4):
    return all(0x20 <= c < 0x7F for c in b4)

def guess_type(b4, u32, f32, i32):
    if is_printable_ascii(b4):
        return "str4", ''.join(chr(c) for c in b4)
    if is_sane_float(f32) and f32 != 0.0:
        return "float", f32
    if 0x00400000 < u32 < 0x01000000:
        return "ptr", hex(u32)
    if i32 != 0:
        return "int", i32
    return "zero", 0

def parse_slots(data):
    slots = {}
    i = 0
    while i + 4 <= STRUCT_SIZE:
        b4  = data[i:i+4]
        u32 = struct.unpack_from('<I', data, i)[0]
        f32 = struct.unpack_from('<f', data, i)[0]
        i32 = struct.unpack_from('<i', data, i)[0]
        kind, val = guess_type(b4, u32, f32, i32)
        if kind != 'zero':
            slots[i] = (kind, val)
        i += 4
    return slots

NAME_INDEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "name_index.json")


def load_name_index_fields():
    """IDA corpus label source: name_index.json vehicle_fields (offset->name+trust)."""
    known = {}
    if not os.path.exists(NAME_INDEX):
        return known
    import json
    with open(NAME_INDEX, encoding='utf-8') as f:
        vf = json.load(f).get("vehicle_fields", {})
    for off_hex, meta in vf.items():
        try:
            off = int(off_hex, 16)
        except (ValueError, TypeError):
            continue
        if off < STRUCT_SIZE:
            known[off] = f"{meta.get('name','?')} [{meta.get('trust','?')}]"
    return known


def load_known_offsets(docs_path):
    # Primary label source in the IDA corpus is name_index.json; docs are an
    # optional legacy override layered on top.
    known = load_name_index_fields()
    if not docs_path or not os.path.exists(docs_path):
        return known
    pattern = re.compile(r'\+0x([0-9a-fA-F]+)\s*\|?\s*([^\|\n]+)')
    with open(docs_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                offset = int(m.group(1), 16)
                label  = m.group(2).strip().rstrip('|').strip()
                if offset < STRUCT_SIZE:
                    known[offset] = label
    return known

# ── main monitor loop ──────────────────────────────────────────────────────────

def monitor(interval, num_drivers, docs_path):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    known = load_known_offsets(docs_path)
    print(f"Loaded {len(known)} documented offsets.")

    # attach
    print("Attaching to F1 Challenge Online.exe ...")
    try:
        pm = pymem.Pymem("F1 Challenge Online.exe")
    except Exception as e:
        print(f"Failed to attach: {e}")
        print("Make sure the game is running.")
        sys.exit(1)
    print(f"Attached. PID={pm.process_id}")

    # track changes across snapshots
    # structure: {driver_idx: {offset: last_value}}
    prev_snapshots = {}
    # track which offsets ever changed value (dynamic)
    dynamic_offsets = set()  # (driver_idx, offset)
    # track all unique values seen per offset across all drivers/snapshots
    all_values = {}  # (driver_idx, offset) -> set of values

    changes_path = os.path.join(OUTPUT_DIR, "changes.csv")
    changes_file = open(changes_path, 'w', newline='', encoding='utf-8')
    changes_writer = csv.writer(changes_file)
    changes_writer.writerow([
        "timestamp", "driver", "offset_hex", "offset_dec",
        "type", "old_value", "new_value", "ghidra_label"
    ])

    snapshot_count = 0
    print(f"Monitoring {num_drivers} drivers every {interval}s. Press Ctrl+C to stop.\n")

    try:
        while True:
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            snapshot_path = os.path.join(OUTPUT_DIR, f"snapshot_{ts}.csv")

            with open(snapshot_path, 'w', newline='', encoding='utf-8') as sf:
                writer = csv.writer(sf)
                writer.writerow([
                    "driver", "offset_hex", "offset_dec",
                    "type", "value", "ghidra_label", "is_dynamic"
                ])

                active_drivers = 0
                for d in range(num_drivers):
                    addr = DRIVER0_BASE + d * DRIVER_STRIDE
                    try:
                        data = pm.read_bytes(addr, STRUCT_SIZE)
                    except Exception:
                        continue

                    slots = parse_slots(data)
                    if not slots:
                        continue
                    active_drivers += 1

                    prev = prev_snapshots.get(d, {})

                    for offset, (kind, val) in slots.items():
                        key = (d, offset)

                        # track value history
                        if key not in all_values:
                            all_values[key] = set()
                        all_values[key].add(str(val))

                        # detect change
                        if offset in prev and str(prev[offset][1]) != str(val):
                            dynamic_offsets.add(key)
                            changes_writer.writerow([
                                ts, d, hex(offset), offset,
                                kind, prev[offset][1], val,
                                known.get(offset, '')
                            ])
                            changes_file.flush()

                        label   = known.get(offset, '')
                        is_dyn  = 'Y' if key in dynamic_offsets else ''
                        writer.writerow([d, hex(offset), offset, kind, val, label, is_dyn])

                    prev_snapshots[d] = slots

            snapshot_count += 1
            print(f"[{ts}] Snapshot {snapshot_count} — {active_drivers} active drivers, "
                  f"{len(dynamic_offsets)} dynamic fields found so far")

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\nStopped by user.")

    finally:
        changes_file.close()
        write_summary(known, dynamic_offsets, all_values, num_drivers, snapshot_count)

def write_summary(known, dynamic_offsets, all_values, num_drivers, snapshot_count):
    path = os.path.join(OUTPUT_DIR, "summary.txt")
    with open(path, 'w', encoding='utf-8') as f:
        def w(line=''):
            f.write(line + '\n')

        w("=" * 80)
        w("RACE MONITOR SUMMARY")
        w(f"Snapshots taken : {snapshot_count}")
        w(f"Drivers tracked : {num_drivers}")
        w("=" * 80)
        w()

        # group dynamic offsets by offset (across all drivers)
        dyn_by_offset = {}
        for (d, off) in dynamic_offsets:
            if off not in dyn_by_offset:
                dyn_by_offset[off] = []
            dyn_by_offset[off].append(d)

        w(f"── DYNAMIC OFFSETS ({len(dyn_by_offset)} unique) — changed at least once during race ──")
        w(f"{'Offset':<10} {'Drivers':<12} {'Ghidra Label':<40} {'Sample values'}")
        w("-" * 80)
        for off in sorted(dyn_by_offset.keys()):
            drivers = dyn_by_offset[off]
            label   = known.get(off, '')
            samples = set()
            for d in drivers:
                samples.update(list(all_values.get((d, off), set()))[:3])
            sample_str = ', '.join(list(samples)[:4])
            doc_mark = '*' if label else ' '
            w(f"{doc_mark}{hex(off):<9} {str(drivers):<12} {label:<40} {sample_str}")

        w()
        undoc_dynamic = {off for off in dyn_by_offset if not known.get(off)}
        w(f"── UNDOCUMENTED DYNAMIC FIELDS ({len(undoc_dynamic)}) — highest research value ──")
        w(f"{'Offset':<10} {'Drivers':<12} {'Sample values'}")
        w("-" * 60)
        for off in sorted(undoc_dynamic):
            drivers = dyn_by_offset[off]
            samples = set()
            for d in drivers:
                samples.update(list(all_values.get((d, off), set()))[:3])
            w(f"{hex(off):<10} {str(drivers):<12} {', '.join(list(samples)[:4])}")

        w()
        w(f"── STATIC OFFSETS (active but never changed) ──────────────────────────────────")
        w("These are config/setup data loaded at race start, not runtime state.")
        static_count = 0
        for (d, off), vals in all_values.items():
            if (d, off) not in dynamic_offsets and d == 0:
                static_count += 1
        w(f"  Driver 0 static active fields: {static_count}")
        w()
        w(f"Output files in: {OUTPUT_DIR}/")
        w(f"  changes.csv      — every field change with timestamp")
        w(f"  snapshot_*.csv   — full struct state per snapshot")
        w(f"  summary.txt      — this file")

    print(f"\nSummary written: {path}")
    print(f"Key output: {os.path.join(OUTPUT_DIR, 'changes.csv')}")

# ── entry point ────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='F1 Challenge live struct monitor')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='Seconds between snapshots (default: 5)')
    parser.add_argument('--drivers', type=int, default=22,
                        help='Number of driver slots to scan (default: 22)')
    parser.add_argument('--docs', type=str, default='docs/10-vehicle-physics.md',
                        help='Path to Ghidra docs for cross-reference')
    args = parser.parse_args()

    monitor(args.interval, args.drivers, args.docs)
