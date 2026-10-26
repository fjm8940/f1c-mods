# Reused pit HUD — 2026-09-21

f1c_pit_hud.h ports the original pit_commentary.c hud_write layout:
outer pointer 007CD204, eight slots, stride 5C, text offset 3C, timer offset 90.
Source: mods/extended_vanilla_features/patches/pitstop_modding/dll/pit_commentary.c.
This is inherited source/live-observation evidence, not a new complete native
HUD ABI proof. Unlike the old helper, it checks committed writable memory,
finite expired timers and explicitly terminates text at 31 bytes. It preserves
occupied slots and leaves native four-second expiry in charge. No slot restore.
Only the broker game thread invokes the wrapper. Return means slot written,
not proof that a frame displayed it. Product tests cover bounds, all eight
slots, occupied-slot preservation and original wheel feedback labels.

Native HUD appearance, compatibility and session transitions require gameplay
validation. This does not implement the previously proposed acknowledged HUD
ownership/presentation API and cannot authorize fire ignition.

Product reuse record:
../../../development/mods/pitstop_modding/verification/reuse-2026-09-21/REPORT.md.
