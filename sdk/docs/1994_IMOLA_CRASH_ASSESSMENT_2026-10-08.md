# Senna / Imola 1994 crash assessment — 2026-10-08

Product: installed F1 Challenge V10 season content; supporting SDK is in active development. Task stage: investigate; diagnosis OPEN. No lifecycle change.
Target: `%USERPROFILE%\Documents\F1 Challenge V10`.
Objective: explain the repeatable 1994 crash without modifying the game. Acceptance: attribute a fresh failing run to an evidenced cause. No repair, deployment, game launch, or installation change was performed.

## Owner evidence
Owner reports a crash when the circuit is being loaded, clarified as Senna 1994 Imola, crashing the moment the circuit is selected. Whether this happens on the first circuit selection after a fresh process start remains unconfirmed.

## Verified file and log evidence
- 481 comparable 1994 MAS archives and BMP/JPG assets match SHA-256 against the owner clean install at `%USERPROFILE%\Documents\VM\F1 Challenge V10_clean`. Equality is not proof that the source pack is correct.
- All 69 comparable active files in `SeasonData/Circuits/San_Marino/1994_Imola` and 12 comparable files in `SeasonData/Vehicles/Williams/1994_FW16` match the clean install byte for byte. Logs, backups and extra oph INI were excluded.
- The 1994 VEH HDVehicle/Graphics/HeadPhysics basenames and sampled HDV tyre/suspension/gearing/damage/engine references resolve in the vehicle file inventory. This is a static basename check, not proof of runtime search-path resolution.
- The shared 1994 Goodyear TBC differs from clean only by WetWeather=1 added to Hard and Soft compounds. The active 2002 Bridgestone file also labels every compound wet. This difference is not established as causal.
- Interlagos SCN has an extra LOGOMAP2.MAS reference; that archive exists. This is outside the reported Imola scenario.
- LOG/trace.txt is empty (timestamp 2026-10-08). LightLoader.log reports 2.2.5 with ten loaded plug-ins and no load failures in the 12:28 startup. Successful DLL loading does not rule out subsequent plug-in faults.
- Latest available Application Error 1000 F1C event: 2026-10-07 host-local time, EXE offset 0x10AF7, access violation. Its matching `AppData/Local/CrashDumps/F1 Challenge Online.exe.7728.dmp` exception is 0xC0000005 at 0x410AF7. Read-only string search found 30 occurrences of 2002_Hockenheim and zero of 1994_Imola/1994_Williams02. This dump does not substantiate an Imola diagnosis.
- SDK inspect of function start 0x410AF0 shows unchecked swap-with-last removal from a pointer array. Another older event at 0x54C044 overlaps a known Safety Car re-entry issue, documented in development/mods/full_safety_car/CURRENT_STATE.md. Neither establishes this owner's Imola cause.
- Installed Safety Car DLL SHA-256: 5577b2d99efc9162ec60e377c0ce9313005e5a8a85ef264a49b24d5bfc760cdd; enabled. No version inference made from stale ownership markers.

## Pending investigation
Fresh-process Senna/Imola reproduction with a timestamp and new trace/crash dump is required. Determine whether merely highlighting the circuit crashes or entering the 3D loading screen does. Then perform controlled plug-in isolation if the capture supports it. Current evidence does not distinguish a shared source-pack defect, menu/profile fault, renderer fault or runtime plug-in fault. No specific repair is justified yet.

## Preservation, validation and limitations
No game files or existing documents edited; restoration unnecessary. This new report and an appended findings.jsonl entry are the only intended changes. SDK start freshness=false (not stale); prior art queried for 1994, season crash, 410AF0 and WetWeather. No SDK implementation changed; build/test gate not applicable. Workspace validator before and after reports existing unregistered top-level `.claude` and `plans`; these were left untouched. Product status unchanged. Documentation indexed through the SDK findings front door.

Wasted-effort check: no broad builds/tests or polling. Initial crash-interior inspect calls failed because inspect requires the function start; corrected using search. One guessed Lightloader status path was absent. Broad text search produced excess unrelated narrative/log results; subsequent investigation was bounded. No repeated large document reading beyond cold-start requirements.

## Follow-up: cause identified from fresh owner reproduction

The owner reproduced the failure on 2026-10-08. LOG/trace.txt (7677 bytes, last write 12:52:37 host-local time) ends with:

`vidman.cpp 1114: CUBE error loading scene file SEASONDATA\CIRCUITS\San_Marino\1994_Imola\1994_Imola.SCN: Could not locate mesh file XSECTOR1A.MTS`

Normal Game::Exit follows. No new Application Error 1000 event or CrashDumps file was found. This is an explicit scene-load failure, not an unexplained access violation.

FACT: SCN line 1084 references xsector1A.MTS and line 1090 references xsector2A.MTS. The shared San_Marino.mas directory contains XSECTOR1.MTS and XSECTOR2.MTS; the referenced A-suffixed names do not occur in either San_Marino.mas or 1994_Imola.mas, and no loose XSECTOR1A.MTS was found anywhere under Circuits. Other Imola seasons reference the unsuffixed names. The clean copy shares this SCN, so matching clean files did not establish a functional track. Correction to earlier uncertainty: the immediate cause is now CONFIRMED as an unresolved timing mesh reference in the 1994 Imola scene, independently of Senna's car. It does not prove that every 1994 circuit fails.

Candidate repair: provide correct 1994 timing trigger meshes or correct the two SCN references to verified compatible geometry. Simply using later Imola timing meshes requires checking their location against the 1994 track. No repair applied; native loading and sector timing validation remain pending. Game and production files unchanged. This follow-up is indexed by a new CONFIRMED findings entry. Wasted-effort check: no builds/tests, retries or polling; searches of local archives were bounded to relevant track files.
