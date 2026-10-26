"""Controller for frida_move_car.js v4 — polling-based position manipulation."""
import frida
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))

PID = int(sys.argv[1]) if len(sys.argv) > 1 else None

if not PID:
    for p in frida.enumerate_processes():
        if "F1 Challenge" in p.name:
            PID = p.pid
            break
    if not PID:
        print("Game not running"); sys.exit(1)

print(f"Attaching to PID {PID}...")
session = frida.attach(PID)

with open(os.path.join(HERE, "frida_move_car.js")) as f:
    src = f.read()

script = session.create_script(src)
script.on("message", lambda m, d: print(f"  {m.get('payload', m)}"))
script.load()
time.sleep(1)

api = script.exports_sync
pos = api.get_pos()
if not pos:
    print("No player vehicle. Start a race first.")
    session.detach()
    sys.exit(0)

print(f"\nPlayer at X={pos['x']:.2f} Y={pos['y']:.2f} Z={pos['z']:.2f}")

print("\n--- Test: nudge X+20 for 3 seconds ---")
api.nudge(20, 0, 0)
time.sleep(3)
pos2 = api.get_pos()
print(f"During hold: X={pos2['x']:.2f} Y={pos2['y']:.2f} Z={pos2['z']:.2f}")

print("\n--- Releasing ---")
api.disable()
time.sleep(1)
pos3 = api.get_pos()
print(f"After release: X={pos3['x']:.2f} Y={pos3['y']:.2f} Z={pos3['z']:.2f}")

print(f"\nDelta during hold: X={pos2['x']-pos['x']:.2f} (expected ~20)")
print("\nCheck game window — did car visually move?")
print("Ctrl+C to exit, or keep running for position monitor...")

try:
    while True:
        time.sleep(5)
except KeyboardInterrupt:
    api.disable()
    session.detach()
    print("Detached.")
