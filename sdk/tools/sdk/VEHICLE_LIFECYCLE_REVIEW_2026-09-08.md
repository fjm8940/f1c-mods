# Read-only vehicle lifecycle interface

Candidate interface f1c_vehicle_lifecycle.h and canonical `python tools/f1c_mod.py vehicle-lifecycle-read --process "F1 Challenge Online.exe"`. Python diagnostic validates supported EXE SHA256 and image base before reading. No native writes.

Reviewed layout: role0x32C8 int, finish0x4064 int, retired0x4410 byte, AI0x45F4 byte; body XYZ0x34E0; team0x48A4 and bay0x48A8 ints. Active participant means role0/1, retired0, finish0. Terminal means retired!=0 or finish2/3. AI0 alone is not terminal.

Garage scene is the direct object0x005EA2C0. Exact instructions at0x004D116E in RenderCallWithGuard load ECX with that immediate before IndexedStructDataRead0x00403FC0. Garage table pointer is object+0x158, width object+0x164; position starts at table+28*(team+bay*width). sub_4C6F10 bounds assignment searches to52 teams and3 bays. AIStateReset0x0042AF40 sets retired1/AI0; AIMainUpdate0x00424E70 supports finish2 as non-active. Existing ai_soft_tame research warns that disabled telemetry speed can remain frozen; ai_organic_pit_trace distinguishes normal AI-disabled pit service.

Bounds: width1..52, team within width, bay0..2, 32-bit pointer overflow rejection, checked reads, finite bounded pose, nonzero garage marker. Terminal plus AI0 plus distance<=5m establishes candidate garage clearance. The radius is observed in two cars on one live track, not universally calibrated.

Live Gené body19.05737,-20.034687,-16.65424 versus garage18.966,-20.273,-14.554 gives2.115691m; Zonta2.047731m. Initial pointer-dereference interpretation failed bounds and was corrected by exact machine code; failed evidence retained. Product evidence and native-validation limits: ../../../development/mods/full_safety_car/LIFECYCLE_FIX_2026-09-08.md relative to repository root (use workspace development tree).

Tests/test_vehicle_lifecycle.py checks actual C reader and Python parity for measured geometry, road wreck, active/pit/player states, malformed bounds, overflow, NaN and unreadable memory. Registered in run_all.bat; final canonical SDK test gate passes. New interface remains experimental; whole-cycle gameplay and cross-track garage geometry pending. No broker ABI change.
