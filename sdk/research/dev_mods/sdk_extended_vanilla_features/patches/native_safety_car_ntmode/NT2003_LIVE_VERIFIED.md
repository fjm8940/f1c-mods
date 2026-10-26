# NT2003 pace-car mechanism — LIVE-VERIFIED (2026-08-04, modified retail)

Verified empirically against a running NT2003 (modified retail, PID captured
via Frida). The modified build's addresses differ from the retail decompiled
set, but every mechanism matches:

## The live facts

| fact | retail decompile | modified build (live) |
|---|---|---|
| pace-car array count | dword_5BD504 | **0x5B9504** |
| pace-car array base | off_5BD508 | **0x5B9508** |
| pace-car stride | 16672 (0x4120) | **0x4548** |
| pointer table | dword_611388 | **0x6B5BF0** |
| mode field | this[2674] = 0x29C8 | **veh+0x29C8** (byte-identical) |
| per-entry writes | +16644=43, +16668=1 | **+0x452C=0x2B(43), +0x4544=1** |
| mode writer (garage/pit assign) | sub_4E7090 | **0x4E55C0** |
| deploy/recall | sub_4EA350 | **0x4E88E0** |
| gates reading mode | 11 functions | **47 refs** to 0x29C8, all 0x4Exxxx range |
| racing field stride | 4168 | **0x4548** (bound 0x6B3FD0) |

## Verified semantics

**The mode write (0x4E55C0):**
```asm
mov eax, [esi + 0x29C8]     ; old mode
mov [esi + 0x29C8], ecx     ; new mode (1/2/3 = assigned)
```

**Deploy (0x4E88E0, a2=1):** `push 1; push 1; call 0x4E55C0` — old 0 → new
nonzero: state calls (sub_41FEB0-equivalent writes 14 into a state field),
NO teleport.

**Recall (0x4E88E0, a2=0):** `push 1; push 0; call 0x4E55C0` — old nonzero →
new 0: capture (0x4E5B40) + teleport to stall (0x4E57B0) + report to the
caution world (0x6B9888, retail's dbl_617020).

**Live state observed:** 1 pace car, mode currently 0 (recalled/garaged),
id 43, flag 1, at stall position.

## What this means for the F1C port

1. The mode field offset 0x29C8 is stable across builds — F1C keeping it
   dead at the same offset is the compiled-out-pass fingerprint, confirmed.
2. NT's exclusion = one field, ~47 gate reads, no separate hide step.
3. F1C's surviving gate (AIMainUpdate on 0x32C8) is the only one of those
   ~47 that survived; the fork re-adds the placement/sort/row gates.
4. The separate array is what makes NT's count displays correct (22, not
   23). F1C has no second array — the fork's position exile (24) + count
   display gate (sub_49C6D0) are the F1C equivalents.

## Cracked-build caveat

This game's .exe (1,978,368 bytes, sections .text/.rdata/.data/LBMPEG_D/
.data1/.rsrc) is NOT the same binary as the `decompiled/` set (which used
stride 16672 / 4168). All live offsets above are for the modified build only.

---

# ADDENDUM — NT2004 live-verified (2026-08-04, after lunch)

NT2004 (modified retail, SafeDisc-stripped via secdrv + ISO serial) verified
with Frida while the user drove a real caution:

## NT2004 facts (live)

| fact | value |
|---|---|
| field array | base 0x62BF68, stride **0x62D8**, 48 slots (bound 0x7588B0) |
| active pointer table | **0x75AAD0** (slot 0..47 = field, like F1C's 0x7C54F0) |
| pace car object | 0x7547E8, **active-table slot 48 — ABOVE the 48-car field** (docs' "slot 48" arrangement, confirmed) |
| pace car array base | [0x5FD348] → 0x7547E8, count [0x5FD344] = 1 |
| mode field | **veh+0x4074** (differs from NT2003's 0x29C8 — struct grew) |
| mode writer (garage/pit assign) | **0x51BC20** (same pattern: read old, write new, a3-gated transitions) |
| pace-build pass | 0x4E2A43 (loop 0x4E29D0) — mode 1/2/3 at build, same as NT2003 |
| roster | base [0x7D6A5C], count [0x7D6A58]=68, stride **0x4D0 (1232)**, **PACECAR = entry 0** (`SEASON04\VEHICLES\PACECAR\PACE...`) |
| caution state | [0x75CBB4], session [0x75CBB0] |
| deploy/recall caller | caution entry 0x4961F0 → 0x51BC20 |
| live motion fields | 0x449C/0x4550/0x45BC/0x47AC/0x5D2C changing (car on track) |

## THE DEPLOY HAPPENED — mode semantics corrected

The user drove a real caution; the pace car came out and led the pack
("IT IS IN THE FIELD LEADING THE PACK RIGHT NOW"). Live read at that moment:

- **mode 0x4074 = 0** — and the object was MOVING (5 changing fields).
- caution state cycled 6 (active) → 2 (clearing).

**So mode 0 = DEPLOYED / ON TRACK**, and build-time mode 1/2/3 = the
assigned-but-not-deployed state. This corrects the earlier NT2003 reading
where the parked car at mode 0 was interpreted as "recalled" — with the
NT2004 evidence, mode 0 means the car is active in the sim; the pace car
spends its build time at 1/2/3 and goes to 0 when it takes the track.

**For the F1C port this is the key line:** NT's pace car deploys by moving
from the special mode (1/2/3) to mode 0 (normal car behaviour), NOT by
becoming special. F1C's 0x32C8 role gate (0=player/1=AI/-1=out) is the
surviving descendant — and F1C's parked SC at 0x32C8=-1 matches NT's
"mode != 0 = not driving".

## SECOND DEPLOY — RECALL NEVER FIRES (the important negative result)

Second live caution, full trace armed on the mode writer (0x51BC20) and the
caution entry (0x4961F0):

- **Deploy captured live**: crash → `caution [0x75CBB4] = 6` → pace car
  accelerates from the stall onto the track (0x449C 143→144.5+ climbing,
  0x45BC/0x47AC lap distance 6910→8979→8705+, speed 213→621). **NO mode
  writer call fired.** Deploy = caution state alone making the AI drive the
  slot-48 car. Mode stayed 0 throughout.
- **Recall NEVER fired**: caution stayed 6 indefinitely; the pace car kept
  circling (speed 621.7, dist climbing) with 23/48 cars bunched behind; the
  user closed the race. No a2=0 recall call, no mode flip, no stall return.

**Conclusion: NT2004's pace car recall does not happen automatically in the
modified build.** The caution machine holds state 6 and the pace car circles
until the race ends. The NT2003 decompiled recall path (mode writer a3=1:
capture + teleport to stall) exists in code but was never observed firing in
either game.

**Port implication:** the F1C fork's Numpad1/Numpad2 deploy/recall is a
COMPLETE lifecycle that NT2004 itself lacks in practice. Nothing is lost by
implementing recall ourselves — NT2004 cannot do it without manual state
writes either.

## THIRD SESSION (retail exe) — THE DESPAWN, CAPTURED

Ran the full caution to completion on `NASCAR_Thunder_2004.exe`
(layout-identical to the modified retail; same strings/globals/offsets).

**The complete cycle, live-observed:**

1. **Deploy**: caution → 6, pace car (slot 48) drives out, leads.
2. **Caution run**: field governed under him — pace car 254, field mean 243,
   min 80.6 (incident car); cars in lane, bunched. (An earlier read of
   "field at 219-220, no governing" was mid-bunching — the governed state
   DOES form.)
3. **Restart**: "preparing to re-start" → both pace car and leader
   accelerate together (632→635→731), leader ~200-300 m behind the pace car.
4. **THE DESPAWN**: the pace car crosses the S/F line → caution 6→5 →
   **he is removed from the sim, instantly and totally**: speed and lap
   distance FREEZE at their despawn-instant values (731.6 / 9174) and the
   model disappears. No teleport to stall, no mode flip (stayed 0), no
   stall return. The car is simply dropped from the sim loop.

**Corrected conclusion:** NT2004's recall = **instant sim removal at the
restart line crossing**. The frozen-data signature (values stop ticking) is
the fingerprint. This is cleaner than the NT2003 decompiled teleport-back —
NT2004 just stops simulating the car. The field goes green immediately
(leader 739.9 = full race speed).

**CAVEAT (immediately after capture): the user noted the game was PAUSED
during these frozen reads.** The frozen signature may be pause-induced, not
despawn-induced. The despawn itself is still real (user-observed: the model
disappeared at the line crossing), but the "frozen values" reading must be
re-verified with the game UNPAUSED before treating it as the despawn
fingerprint. TODO: re-run, unpaused, confirm the pace car's fields stay
frozen (or get zeroed / removed from the table) after the line crossing.

**CAVEAT RESOLVED (unpause check):** after unpausing, the field raced at
full speed while the pace car's fields stayed locked at the despawn instant
(speed 731.6, dist 9174, mode 0, still in table slot 48, zero movement over
multiple 300 ms samples). **The despawn = permanent sim removal confirmed.**
He is not teleported, not mode-flipped, not returned to the stall — the game
stops simulating him at the restart line crossing, and his data is a frozen
snapshot from that instant.

**Port implication (final):** NT2004's despawn = stop simulating the car
(≈ F1C role -1 + rebuild, which drops him from the active array; AIMainUpdate
self-gates on role -1, freezing the same fields). The F1C fork's Numpad2
recall (role -1 + rebuild) is the exact functional analog — NT2004 doesn't
teleport, it un-sims. The fork's parked state (role -1) matches NT2004's
despawned state semantically. The only F1C difference: the scene-graph model
stays visible (cosmetic; the fork's parked SC sits in its pit box, which is
the desired look anyway).

## NT2004 vs NT2003 vs F1C offsets

| concept | NT2003 (modified) | NT2004 | F1C |
|---|---|---|---|
| mode field | veh+0x29C8 | veh+0x4074 (modified) / **veh+0x40B0 (retail, this[4140])** | dead at 0x29C8 |
| pace car location | separate array (0x5B9508, stride 0x4548) | **field slot 48 (0x75AAD0[48])** | counted slot 22 |
| field stride | 0x4548 | 0x62D8 | 0x48C0 |
| active ptr table | 0x6B5BF0 | 0x75AAD0 | 0x7C54F0 |
| mode 0 meaning | on track (corrected) | on track (observed) | role 1 = driven |

NT2004 = the closest structural ancestor of F1C: pace car at a slot ABOVE
the field in the SAME pointer table, never grid-placed, mode-gated out of
scoring/placement while at 1/2/3.

## CORRECTION 2026-08-04 (late): the NT2004 decompiled set HAS the pace car

The `Nascar Thunder 2004/decompiled/` set (8,610 files, nascar04) DOES
contain the full pace-car system — an earlier check missed it because the
addresses differ from the modified build and the string search used "space"
false positives. Found:

- **sub_4E41F0** = NT2004 pace-build pass (sibling of NT2003 sub_4B93A0):
  loop over `dword_600404` (pace count), mode write `sub_51E540(3|2|1, 0)`,
  roster scan `_strcmpi(aPaceCar, roster + 246)` (stride 1232, name at +246),
  `sub_4EF870(i + 10)` ID assign.
- **sub_51E540** = the mode writer (retail; cracked = 0x51BC20). Mode field
  = `this[4140]` = **veh+0x40B0** (retail). Same semantics as NT2003:
  - recall (a3 && session==5 && new mode 0, old nonzero): capture
    (sub_51EB00) + teleport to stall (sub_51E740) + report to the caution
    world (sub_44A530, dbl_6719F8) + `dbl_671BA4 = this[4692]`
  - deploy (old 0, new nonzero): sub_42C170(veh, 18) + sub_42C1A0(veh) +
    `this[4708] = dbl_6739EC`
- **aPacecarVeh** (sub_4E8C10) = the .veh path builder.
- **aPacecarrelease** (sub_402B90) = an AIW/setup config token (release
  distance, clamped 0..1 / 0..0.87), NOT the recall function.

So BOTH games have readable pace-car source in the decompiled sets.
NT2003's mode field (0x29C8) matches F1C's dead field exactly; NT2004's
(0x40B0/0x4074) does not. NT2003 remains the primary reference for the
mode-field port; NT2004 is now confirmed as a fully readable SECOND
source for the caution/deploy semantics.
