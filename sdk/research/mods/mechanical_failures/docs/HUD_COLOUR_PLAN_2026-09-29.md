# HUD colour plan - Mechanical Failures (2026-09-29)

Status: **IMPLEMENTED 2026-09-29 (owner "go", recommendations taken: instructions white, PIT AND RETIRE red); pending live test.** Owner asked for a
draft after seeing an accidentally yellow `VALVE AIR LOW`.

## 1. Owner intent

- Colour ALL mod HUD messages on purpose, but only where colour carries
  meaning.
- Our colours must never leak onto vanilla or other-mod messages.
- Vanilla / other-mod messages must not change our colours.

## 2. Evidence (fact, decompile + existing findings)

Game message strip: `inner = *(u32*)0x7CD204`, 8 slots, stride 92 (0x5C).
Per-slot fields, offsets from the slot base (`sub_45B9C0`, `sub_45B430`):

| Offset | Field | Game default (`sub_45B430` reset) |
|---|---|---|
| +0x3C | text, 80 bytes | "" |
| +0x8C | colour, ARGB dword | -1 (white) |
| +0x90 | countdown timer, float | -1.0 |
| +0x94 | level / priority | 3 |

- Vanilla adds go through `sub_45BD10 -> sub_45B9C0`: reset the slot, then
  write text, colour (`IDToColorMaskMapper`: level 1 = red 0xFFFF0000,
  2 = yellow 0xFFFFFF00, else white), timer 5.0, level. **So vanilla messages
  always set their own colour - they cannot inherit ours.**
- Duplicate text is not re-added (`sub_45B480` only refreshes the timer).
- Slot choice (`sub_45B500`): first slot with timer <= 0 from a rotating
  index; if none, a level test decides eviction.
- Our code (`hud_pump` in engine/electronics, `hud_refresh` in gearbox)
  writes **only text + timer**. Colour and level stay from the slot's last
  user. That is the "leak": our text shows in whatever colour the previous
  vanilla message used. Cause of the yellow `VALVE AIR LOW`.
- Existing finding (safety car menu, 2026-08-01): the strip draws only its
  last four slots, slot order = draw order.
- The box seen around the message in the screenshot is NOT explained. It may
  come from the inherited level or from another field - to verify live.

## 3. Colour assignment (proposal)

Game palette only, so it looks native. White = no colour needed.

**RED - car is done / stop now**
ENGINE BLOWN, ENGINE FAILURE, ENGINE STOPPED, STOP THE CAR, PIT AND RETIRE,
GEARBOX FAILURE, NO DRIVE, ENGINE STALLED, ELECTRICAL FAILURE

**YELLOW - dash warning / diagnosis (something is wrong)**
VALVE AIR LOW, VALVE FLOAT, OIL PRESSURE, OIL TEMP, WATER TEMP, FUEL PRESSURE,
FUEL LEAK, EXHAUST TEMP, RADIATOR BLOCKED, ENGINE PROBLEM, GEARBOX PROBLEM,
ELECTRONICS FAULT, TC FAULT, LOW VOLTAGE, RADIO COMMS LOST, OVER-REV, and the
gearbox lines that name a gear

**WHITE (default) - engineer instruction or all-clear**
PIT FOR AIR, PIT FOR FUEL, PIT - DEBRIS, SAVE FUEL, SHIFT EARLY,
SHIFT EARLIER, REVS DOWN, EASE OFF, COOL THE OIL, COOL THE ENGINE,
WATCH THE REAR, VALVE AIR REFILLED, RADIATOR CLEARED, ENGINE REPAIRED,
ELECTRONICS OK

Open owner choices: instructions white vs. a distinct colour (e.g. light
blue - off-palette, would look non-native); `PIT AND RETIRE` red vs yellow.

## 4. Isolation design

1. **One shared helper `dll/mf_hud.h`** (header-only, like `mf_variance.h`):
   text -> colour table; `mf_hud_write(slot, text)` writes text, timer,
   colour. **Level is never changed** (stays at the game default 3), so our
   messages keep vanilla priority and never block vanilla eviction.
2. **Re-assert colour on every refresh tick** of a slot that holds our
   text, so another writer cannot recolour our message.
3. **Hand back clean slots.** Each family remembers which slot it owns. When
   its message is no longer refreshed and the slot timer reaches <= 0, and the
   slot text is still ours, write the game defaults back (colour -1,
   level 3). Never touch a slot whose text is not ours.
4. Vanilla needs nothing: its add path resets colour itself (section 2).
5. Other mods that write text + timer only (red flag `gr29`, safety car menu,
   pit stop mods) then find white defaults, not our colours. Their own
   inheritance from vanilla colours is theirs - out of scope, noted.
6. Cross-thread: engine, electronics and gearbox write the strip from their
   own worker threads. Keep "claim = write text first, then re-check text
   before colour" so two families cannot colour each other's slot.

## 5. Implementation steps (when approved)

1. Preserve before-state (new dated folder under `workspace_organization_audit\`).
2. Add `mf_hud.h`; switch engine `hud_pump`, electronics `hud_pump`,
   gearbox `hud_refresh` to it.
3. Build (-Werror), unit tests, panel smoke test.
4. Install, live test (section 6).

## 6. Verification (live, owner)

- Each colour group shows its colour (one example per group).
- After our message fades: a vanilla message (e.g. pit / penalty / blue
  flag) and PAUSED show in their normal colours.
- Another mod's message (if loaded) shows in its usual colour.
- Our message stays its colour while a vanilla message appears.
- Check whether the box still appears - if yes, investigate other slot
  fields.

## 7. Risks / abort

- Colour field offset wrong -> wrong colour or no effect; abort and restore.
- Strip draws only 4 slots: more simultaneous messages than that can still
  hide some (existing behaviour, unchanged).
- Restoration: before-state folder + rebuild.

## 8. Implementation record (2026-09-29)

- New `dll/mf_hud.h`: colour table (section 3), `mfh_keep` (refresh or
  claim, colour re-asserted every tick, level never written, claim re-checks
  text before colouring), `mfh_release` (faded slot still holding our text
  -> colour back to white 0xFFFFFFFF).
- Engine + electronics `hud_pump` and gearbox `hud_refresh` now use it;
  gearbox also releases every worker tick. No direct slot writes remain.
- Before-state: `workspace_organization_audit\HUD_COLOUR_BEFORE_2026-09-29\`.
- Verification: build -Werror OK, unit tests OK. Installed SHA256 prefix
  `b68eb3a1`. Pending: owner live test (section 6). Box in the owner's
  screenshot: probably the screenshot tool's frame - owner to confirm.

## 9. Colour table trimmed (owner 2026-09-29: "all a colour show")

Rule now: colour only when the driver must act NOW.
- RED: STOP THE CAR, PIT AND RETIRE
- YELLOW: VALVE FLOAT, OIL PRESSURE, WATER TEMP
- Everything else white (section 3 lists superseded).
Old table preserved: `...HUD_COLOUR_BEFORE_2026-09-29\mf_hud_v1.h`.
Built ff9ce1b9; not installed (game running). Pending live look.
Installed `16550fee` (trimmed colour table).

## 10. Action-based table (owner 2026-09-29, third pass - supersedes 9)

Owner: colours must be useful, grab attention, not a colour show; STOP THE
CAR red is useless once the engine is gone. Rule: colour the INSTRUCTION
only where the driver's reaction changes the outcome (code-checked: oil,
valve, cooling, fuel pressure, fuel leak progress scale with load_factor();
pit refills air / clears debris / refuels).
- RED: SHIFT EARLIER (valve float - seconds from a blow-up)
- YELLOW: REVS DOWN, PIT FOR AIR, SHIFT EARLY, COOL THE OIL, COOL THE
  ENGINE, PIT - DEBRIS, EASE OFF, SAVE FUEL, PIT FOR FUEL
- WHITE: all diagnoses/warnings, dead-car lines, gearbox, electronics,
  exhaust (no quick reaction changes them).
Previous table: `...HUD_COLOUR_BEFORE_2026-09-29\mf_hud_v2.h`. Build `d14616bc`.
