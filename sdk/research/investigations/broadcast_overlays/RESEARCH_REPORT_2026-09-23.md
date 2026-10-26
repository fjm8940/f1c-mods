# Broadcast Overlays, the bottom classification, and a Safety Car lower third — research report

Date: 2026-09-23. Author: autonomous SDK research session.

## Answers at a glance (session summary, recorded 2026-09-23)

- **Key 3 / action code 4** flips `dword_7CF638` (`sub_4D8450:56-98`). OFF clears the
  active display (`sub_4D8760(-1,0)`) and blocks **all** display transitions because
  `Return0Stub_9280` (`0x4D9280`, `xor al,al; ret 4`) always returns 0. The
  "Broadcast Overlays" graphic option (default On) has a control constructor binding
  `&dword_7CF638`; same-state is probable (`DERIVED`), exact menu row→control binding
  unresolved. Retire the old label `AuthorizationBypassFlag`.
- **Bottom classification** = display 19 → `sub_4DA680` → `sub_4DA9E0` per row: six
  rows, left column x=50/590, right column x=640/1230, y=818/858/898; positions from
  `vehicle+0x4060`, abbreviated names from `+18168`, values are **cumulative
  race-time gaps to the leader** (per-lap crossing-time array written by
  `LapCompletion:171`, read by `sub_4BC270`), leader row shows lap count. Shown at
  leader laps 2, 7, 12, … and at finish. `hud broadcast.tga` is an orphan.
- **Message layers**: eight-slot native timed text (`TextLineWrappingAndRendering` →
  `UtilityFunction_b9c0`, cleared by `ShortComputation_bdf0`) is **not** broadcast-gated,
  so Safety Car text stays visible when overlays are off; flags
  (`HUDFlagRendererMain`) and HUD quads (`sub_4D8CF0`, `HUD 512 64.TGA`) are separate
  layers.
- **Recommendation**: Route C (separate window overlay) now for the reference-style
  lower third, independent setting defaulting to follow key 3, `NO OVERTAKING` kept in
  the native slots; Route B (broker game-thread quad/texture draw) specified as a
  missing capability; Route A rejected. Full event contract, layout, lifetime, tests,
  abort/rollback in section 7.
- **No game, DLL, mod source, or executable was changed; no runtime integration was
  performed.** Validator unchanged at 8 pre-existing errors, 0 warnings.
Task type: investigation and implementation specification. **No gameplay code, DLL, game
file, mod source, or executable was changed. No runtime integration was performed.**

Scope authority: [RESEARCH_HANDOFF_2026-09-23.md](RESEARCH_HANDOFF_2026-09-23.md).
Reference image: `reference_safety_car_lower_third.png`, SHA-256
`44821B4C7D7470BE1BA4DE8F99ACCBC05FA02057F84F1F03334B89DBECC8FC45` (verified unchanged).
Workspace validator baseline at orientation and after documentation:
**8 pre-existing errors, 0 warnings** (unchanged).

Evidence tiers used below:

- `RAW_HEXRAYS` — raw Hex-Rays function body, file/line cited.
- `RAW_DISASM` — read-only bounded disassembly of the canonical executable
  (`exe-disassemble` or a byte-level read of the same file), address cited.
- `RAW_SCAN` — read-only byte scan of the canonical executable for pointers/immediates.
- `DERIVED` — a conclusion assembled from two or more raw facts; marked as such.
- `OWNER_OBSERVED` — owner report; not independently captured.
- `UNRESOLVED` — explicit missing evidence.

---

## 1. What the game means by "Broadcast Overlays"

### 1.1 Runtime toggle (action code 4)

`sub_4D8450` (`functions/sub_4D8450_004d8450.c`) is the per-frame HUD display
manager, called from `ShortComputation_f070` (`functions/ShortComputation_f070_0049f070.c:12`,
`sub_4D8450(&dword_8AD298)`), which is reached from `GamePostMainLoop`.

- `sub_4D8450:26` calls `HUDFlagRendererMain` first.
- `sub_4D8450:56-98` scans the input action array `word_7CB21C[0 .. dword_7CB218-1]`:
  - `*v7 == 4` → `dword_7CF638 = (dword_7CF638 == 0)` (`:65-66`).
    Turning **off** calls `sub_4D8760(-1, 0)` (`:69`) and queues the localized
    `"Broadcast Displays Off"` string (`0x5D4AA8`, `strings_index.json` entry for
    `004d8450`). Turning **on** queues `"Broadcast Displays On"` (`0x5D4AC0`).
  - `*v7 == 3` → `dword_7CF788 = (dword_7CF788 == 0)` (`:107-108`) with
    `"HUD Off"` / `"HUD On"` (`0x5D4A98` / `0x5D4AA0`); the off branch also calls
    `IndexValidationSetter(-1)` (`:126`).
- The physical **key 3 → action code 4** binding is owner-observed; the action map
  contents are runtime input state, not a static key table in the corpus
  (`word_7CB21C` is written by the input system). Static code proves the
  action-code branch, not the physical key (`OWNER_OBSERVED` + `RAW_HEXRAYS`).

### 1.2 What the flag actually gates

`sub_4D8760` (`functions/sub_4D8760_004d8760.c`) is the guarded display-state
transition used by the 25-slot HUD display system (`&dword_8AD298`):

- `:11` `v10 = this[a2 + 4] != 0` — target display object must exist.
- `:12-13` authorization: `if (dword_7CF638 || Return0Stub_9280(a2)) v11 = 1;`
- `Return0Stub_9280` (`0x004D9280`) disassembles to `xor al,al; ret 4`
  (`RAW_DISASM`) — **it always returns 0**. Therefore, when Broadcast Overlays
  are OFF, the authorization term is false for every target, and every display
  transition is rejected.
- `:17-19` ordering guard via `IDToValueMapper` (`0x004D91E0`, `RAW_DISASM`:
  switch over ids 0..0x18 returning order values).
- `:22-30` on success: `this[55] = 0` or `0.5f`, `*this = a2`, then
  `VTableDispatchWithFieldClear` (`0x004D7E30`, `RAW_HEXRAYS`: clears `this[3]`,
  calls vtable slot +16 = activate).

Turning Broadcast Overlays **off** therefore does two things: it clears the
currently active display index (`sub_4D8760(-1,0)` → `*this = -1`) and it
**blocks all subsequent display transitions**. This is a hard gate, not a
cosmetic label.

### 1.3 The "Broadcast Overlays" graphic option

`PlayerOptionsInitializer` (`0x004AA270`) registers the option row at
`functions/PlayerOptionsInitializer_004aa270.c:643-649`:

- section string `aGraphicOptions`, name string `aBroadcastOverl`
  (`0x5D12C8`, `"Broadcast Overlay"` per `strings_index.json`; menu label
  "Broadcast Overlays"),
- value block `+4512=1, +4516=1, +4520=0, +4524=1` — boolean row, default On,
  min 0, max 1 (same shape as the `Allow HUD in cockpit` row at `:1036-1041`).

Separately, a UI control constructor `ObjectConstructor_3cc0` (`0x00473CC0`,
`functions/ObjectConstructor_3cc0_00473cc0.c:4-12`) installs vtable
`off_5A3000`, stores **`&dword_7CF638` at `this+0x31C`** (`this[199]`), presets
`381`, and registers the `"On"` text handle (`aOn`, `0x5C32C8`). Its sibling
`ShortComputation_3330` (`0x00473330`, grep `functions/`: `this[199] = &dword_7CF788`)
binds the **HUD** toggle flag in the same way. A byte scan of the canonical
executable (`RAW_SCAN`) finds the tiny per-option factory `sub_483C90` (which
calls `0x473CC0`) referenced from a control-factory table at `0x5EDFB4`;
`sub_482A90` (calls `0x473330`, the HUD-flag control) is in the same table
family.

**Verdict:** the option row and the runtime toggle operate on the same global
state with high probability (`DERIVED` from the direct pointer binding plus the
paired HUD control), but the exact options-screen row → factory → control
instantiation was not statically closed. Unresolved alternative: the option
could persist separately and be applied to `dword_7CF638` at session setup; no
such writer was found, and `dword_7CF638` has exactly three direct references in
the corpus (`sub_4D8450` write, `sub_4D8760` read, `ObjectConstructor_3cc0`
pointer bind). The old SDK label `AuthorizationBypassFlag` is misleading and
should be retired in favor of `BroadcastDisplaysEnabled`.

### 1.4 Toggle scope (static)

- The bottom classification (display 19, section 2) is inside the gated display
  system → hidden by key 3 and blocked from returning while off.
- Other display-system panels (grid, countdown, venue, pit, timer, vehicle
  panel, leader panel) are equally gated.
- The cockpit HUD path (`sub_49D070` `SteeringWheelAnimation` →
  `sub_49D5D0` → session-7 `sub_49D700`) is gated by `dword_7CE048` (HUD mode)
  and cockpit flags, **not** by `dword_7CF638`
  (`functions/SteeringWheelAnimation_0049d070.c:18-65`,
  `functions/sub_49D5D0_0049d5d0.c:27-43`). Static inference: the top-left
  current/gap panel remains when Broadcast Overlays are off.
- Native timed messages (section 3.1) are not gated by `dword_7CF638`; the
  toggle messages themselves are rendered through that path.
- Player/spectator, pause, and replay behavior of the toggle were **not
  captured**; `sub_4D8450` runs from the post-main loop and its input scan runs
  regardless of the message-expiry gate (`:56` onward), but pause semantics need
  a guarded capture (`UNRESOLVED`).

---

## 2. The bottom classification: producer, data, draw

### 2.1 Display-system model

- `sub_4D88D0` (`functions/sub_4D88D0_004d88d0.c:10-19`) creates 25 display
  slots: `this[4+i] = Direct3DObjectFactory(i)` for `i = 0..24`, allowed by
  `IDTypePermissionMatrix(i, dword_7D0410)`.
- `Direct3DObjectFactory` (`functions/Direct3DObjectFactory_004daea0.c`)
  constructs only ids **0, 1, 2, 6, 7, 8, 13, 17, 19** with fixed vtables.
- Vtable dump (`RAW_DISASM`, `0x5ACF70..0x5AD068`) maps slot → render method
  (vtable +4):
  - 0 → `0x4D9420` StartingGridUIDraw (`"GRID"` / `"STARTING"`)
  - 1 → `0x4D98A0` TimerCountdown
  - 2 → `0x4D99E0` HUDVenue
  - 6 → `0x4D9C10` (not decompiled)
  - 7 → `0x4D9DD0` TimerDisplayRender
  - 8 → `0x4D9EE0` (not decompiled)
  - 13 → `0x4D9F80` → `sub_4DA0D0` two-sided vehicle panel
  - 17 → `0x4DA400` leader lap-time panel (calls `0x4D98A0`)
  - **19 → `0x4DA680` bottom classification** (not in corpus; disassembled)

### 2.2 Renderer `sub_4DA680` (RAW_DISASM at `0x4DA680`)

- Calls the state machine `sub_4DA840` (`functions/sub_4DA840_004da840.c`);
  nonzero return aborts the frame.
- Decrements three timers (`+0x14`, `+0x18`, `+0x1C`).
- State 1 (`+0x10`): draws one yellow string at `(100, 818)` via
  `UITextRenderWithScale` (string `0x5D4B64`).
- State 2: `esi = sub_4DA9C0(this)` (start row), `ebp = sub_4DA990(this)`
  (row count); loop `edi = 0..5` while `esi < ebp`:
  - `sub_4D1B50(&dword_8ACEE0, esi)` maps standings index → vehicle index;
    `> 0x68` aborts.
  - vehicle base `&unk_5EC6D8 + 0x48C0 * index`.
  - `sub_4DA9E0(this, edi, vehicle)` draws row `edi`.
- `sub_4DA9C0` (`functions/DivisionBasedAlignment_004da9c0.c`) =
  `6 * ((count - 1) / 6)` → **last page of the classification**.
- `sub_4DA990` (`functions/sub_4DA990_004da990.c`) counts vehicles whose
  `+0x403C` lap is `>= this[9]` (the leader's lap, set in `sub_4DA840` from
  `dword_5F0714[4656 * leaderIndex]` = `vehicle + 0x403C`).

### 2.3 Row renderer `sub_4DA9E0` (RAW_HEXRAYS)

`functions/sub_4DA9E0_004da9e0.c`:

- `SizeOffsetTableByMode` (`functions/SizeOffsetTableByMode_004dac30.c`):
  - rows 0/1/2 → left column, x = 50 (position) / 590 (value), y = 818/858/898
  - rows 3/4/5 → right column, x = 640 / 1230, y = 818/858/898
  → a two-sided list of up to six rows, three per side.
- Position glyph: `StringFormatWithBufferOverflowCheck(SrcStr, "%c", pos-1 + byte_5D4AE8)`
  where `byte_5D4AE8` is the position glyph base; rendered white with text flag
  `3` (the boxed/background style at `:65`).
- Name: `vehicle + 18168` (abbreviated driver name) at x+40, white (`:66-70`).
- Value (`:29-63`):
  - `position == 1` → `"%d %s"` with `this[9]` and `"LAPS"` (`:43`) → the leader
    row shows the lap count.
  - otherwise `v8 = sub_4BC270(vehicle + 16436, this[9])` (per-lap array at
    `+0x8C`, `functions/sub_4BC270_004bc270.c`) minus
    `v12 = sub_4BC270(referenceVehicle + 16436, this[9])` where the reference is
    standings index 0 = leader (`functions/ArithmeticComparison_ad50_004dad50.c`
    → `sub_4D1B50(&dword_8ACEE0, 0)`).
  - only positive differences render: `"%d:%06.3f"` or `"%.3f"`, yellow
    `0xFFFF00` (`:73`).
- The per-lap array is written by `LapCompletion` (`0x4BC4E0`,
  `functions/LapCompletion_004bc4e0.c:170-171`):
  `this[35][lap-1] = a3` where `a3` is the crossing race time. So the array
  holds **cumulative race time at each lap completion**, not lap durations.

**Answer to the handoff question:** the displayed numbers are **cumulative
race-time gaps to the leader** (difference of two cars' crossing times for the
leader's current lap), not individual lap times and not last-crossing
intervals. The leader row shows the lap count instead of a zero gap. Lapped cars
whose lap is below `this[9]` are excluded by the count; a missing array entry
returns `-1.0` (`sub_4BC270`) and the row value stays blank.

### 2.4 Activation and lifetime

- `sub_4A0B20` (`functions/sub_4A0B20_004a0b20.c:5-9`), called from
  `LapCompletion:136` for each lap completion:
  - `position == 1 && lap >= 2 && (lap + 3) % 5 == 0` → `sub_4D8760(19, 0)`
    (leader laps 2, 7, 12, …).
  - `vehicle identity == player identity && lap % 5 == 1` → `sub_4D8760(13, 0)`
    (vehicle panel).
- `RaceFinishHandler` (`functions/RaceFinishHandler_004a0ba0.c:17-18`):
  `position == 1` at finish → `sub_4D8760(19, 0)`.
- `sub_4DA840` runs the display's own lifecycle: state 0 → state 2 (or state 1
  with a 2.0 s header timer), then row display, then possibly hands off to
  display 17 and finally returns 1 → the manager clears the index
  (`sub_4D8450:31-43`, `VTableDispatchChain_7e00` returns the render result).
- The panel is therefore **periodic/event-driven**, not permanently visible;
  it appears around leader crossings and at the finish. `OWNER_OBSERVED`
  ("drivers populate the lower list on start/finish crossing") is consistent.

### 2.5 Negative findings

- `hud broadcast.tga` has **no loader reference** in the corpus or string
  index; it remains an orphan/uncertain asset (section 5).
- `ScoringClassificationDisplay` (`0x41E320`) is commentary/championship
  scoring, not this graphic (confirmed by its callee set: championship points,
  constructor standings).
- The top-left current/gap panel (`sub_49D700`) is **not** the lower list; it
  draws a different, cockpit-side block at `x = dword_5CCBC0 / dword_7CE238`.
- The prior README statement "`sub_49D070` is the on-screen HUD path" is
  corrected: `sub_49D070` is `SteeringWheelAnimation`, the cockpit HUD
  renderer; the top-left panel is reached through `sub_49D5D0` → `sub_49D700`.

---

## 3. Other visible message layers

### 3.1 Native timed text (the layer the Safety Car mod uses)

- `TextLineWrappingAndRendering` (`0x45BD10`,
  `functions/TextLineWrappingAndRendering_0045bd10.c`): drops the line when the
  requested priority `a3` exceeds `dword_7D02E4`; wraps at 79 characters;
  submits each line to `UtilityFunction_b9c0(renderContext, line, color, 5.0f,
  priority)`. Render context is `&dword_7CD204`.
- `ShortComputation_bdf0` (`0x45BDF0`, `functions/ShortComputation_bdf0_0045bdf0.c`)
  resets `this[199]` and calls `PerIndexStructFieldResetter` for indices 0..7 —
  **eight message slots**, matching the Safety Car HUD audit's "eight shared
  native message slots".
- `ShortComputation_bde0` (`0x45BDE0`) submits a raw string.
- Producers: penalties (`WarningHandler`, `StopGoPenaltyHandler`,
  `DisqualificationHandler`), pit timer, multiplayer notifications, session
  sync, and `sub_4D8450` itself (the toggle messages).
- **Not gated by `dword_7CF638`** (no reference in the message path); the
  Safety Car mod's `NO OVERTAKING` / race-control lines therefore remain visible
  when Broadcast Overlays are off (static inference; capture pending).
- Safety Car mod usage: `hud_show` writes its message and a 4000 ms sim-time
  deadline; `hud_notice` is once-per-cycle; the audit records up to three owned
  lines refreshed with the native 1.5 s timer and a 31-character bound
  (`development/mods/full_safety_car/trials/v0.10.69_session_clear_call_trace/baseline-source.c:1596-1630`,
  `HUD_AUDIT_2026-09-23.md`). These are broker-mediated game-thread writes, not
  the display-system panels.

### 3.2 Flag glyphs

`HUDFlagRendererMain` (`0x4D82D0`, `functions/HUDFlagRendererMain_004d82d0.c`)
collects flag bytes (yellow 62, blue 65, black 63, pit 102, checkered 66, plus
`0x7CFEA0 == 2` extras) and renders via `sub_553070`; gated by `dword_7CFEA0`,
`ConditionalFlagReturn(&dword_5EA43C)`, `dword_8AB6E4 == 2`, and `this[2] != -1`.
Called first in `sub_4D8450`; independent of the broadcast flag.

### 3.3 HUD quads / textures

- `RaceInitWithConstants` (`0x4D8220`) → `sub_4D8CF0(1..5)`
  (`functions/sub_4D8CF0_004d8cf0.c`): screen quads; case 3 builds a path
  `"%s%s%s"` ending in `"HUD 512 64.TGA"` (`0x5D4AD8`) and loads it with
  `D3DTextureCreation`; the quad is `1280 x 160` at `(0,0)`.
- `sub_4D8AB0` loads additional HUD textures through `sub_53CFE0`; its path
  fragments in the string index are non-textual/garbled and remain unresolved.
- Per-frame draw: `VTableDispatchChain_7e00` (`0x4D7E00`,
  `functions/VTableDispatchChain_7e00_004d7e00.c`) calls vtable +24 (check),
  then `sub_4D7FE0` → `sub_4D8A30(*(this+4))` renders the panel quad, then
  returns vtable +4 (the panel render method; 1 = finished).
- `VTableDispatchWithFieldClear` (`0x4D7E30`) activates a display (vtable +16).

### 3.4 Layer map and data-to-draw diagram

| Layer | Owner | Producer / data | Lifetime | Visibility gate |
|---|---|---|---|---|
| Native timed text (8 slots) | OnScreen render context `dword_7CD204` | `TextLineWrappingAndRendering` → `UtilityFunction_b9c0` | per-call deadline (mod uses 4 s / 1.5 s refresh) | priority `dword_7D02E4`; not broadcast-gated |
| Flag glyphs | `HUDFlagRendererMain` | flag checks → `sub_553070` | per frame | `dword_7CFEA0`, flags |
| HUD quads | `sub_4D8CF0` / `sub_4D8A30` | textures (`HUD 512 64.TGA`) | session | display/manager state |
| Display panels (0,1,2,6,7,8,13,17,19) | manager `dword_8AD298` | `Direct3DObjectFactory` objects; render via vtable +4 | display state machine | `sub_4D8760` gate incl. `dword_7CF638` |
| Bottom classification (19) | same | `sub_4DA680` → `sub_4DA9E0` per row; vehicle array + standings array `dword_8ACEE0` | leader-crossing events / finish | same gate |
| Cockpit HUD (top-left panel) | `dword_7CE058` | `sub_49D070` → `sub_49D5D0` → `sub_49D700` | per frame | `dword_7CE048`, cockpit flags |

```
race event (lap completion / finish, sub_4A0B20 / RaceFinishHandler)
   -> sub_4D8760(19,0)  [authorized only if dword_7CF638]
      -> manager dword_8AD298: active index = 19
         -> VTableDispatchChain_7e00 -> vtable+4 = sub_4DA680
            -> sub_4DA840 state machine (timers)
            -> sub_4DA9E0(row, vehicle) x <=6
               -> SizeOffsetTableByMode -> UITextRenderWithScale
               -> sub_4BC270(vehicle+0x40C0, leaderLap) - leader value -> gap text
data writers: LapCompletion:171 -> vehicle +0x40C0[lap-1] = crossing time
              AIDriverSorting -> vehicle +0x4060 position
              sub_4D1B50(&dword_8ACEE0, i) -> standings order
```

---

## 4. Toggle matrix (static; captures pending)

| Control | Effect | Evidence |
|---|---|---|
| key 3 (action 4) | toggles `dword_7CF638`; off clears active display + blocks all transitions; shows "Broadcast Displays Off/On" | `RAW_HEXRAYS` sub_4D8450:56-98; sub_4D8760:12-13; `RAW_DISASM` 0x4D9280 |
| "Broadcast Overlays" option | boolean row default On, section Graphic Options; control constructor binds `&dword_7CF638` | `RAW_HEXRAYS` PlayerOptionsInitializer:643-649, ObjectConstructor_3cc0:4-12; `RAW_SCAN` factory table 0x5EDFB4; `DERIVED` |
| HUD option / action 3 | `dword_7CF788`; off clears HUD index and shows "HUD Off" | `RAW_HEXRAYS` sub_4D8450:107-141; grep `this[199] = &dword_7CF788` |
| bottom classification | hidden when broadcast off; reappears only after on; periodic (leader laps 2,7,12,… + finish) | `RAW_HEXRAYS`/`RAW_DISASM` sections 1.2, 2.4 |
| top-left panel | not broadcast-gated | `RAW_HEXRAYS` sub_49D5D0 / SteeringWheelAnimation |
| Safety Car native text | not broadcast-gated | `RAW_HEXRAYS` message path; no flag reference |
| player/spectator, pause, replay | not captured; `sub_4D8450` input scan is frame-driven | `UNRESOLVED` — needs guarded capture |

---

## 5. Asset inventory

| Asset | Path | Size / hash | Loader evidence | Status |
|---|---|---|---|---|
| Reference frame | `investigations/broadcast_overlays/reference_safety_car_lower_third.png` | SHA-256 `44821B4C…8FC45` | none (internal research only) | confirmed, not a shipped asset |
| `hud 512 64.tga` | `F1 Challenge V10\SeasonData\Vehicles\hud 512 64.tga` | 524,332 B, SHA-256 `29D16B16…BBB99` | `sub_4D8CF0` case 3 builds `"HUD 512 64.TGA"` and loads it | loaded as a HUD quad texture (type 3), **not proven to be the classification** |
| `hud broadcast.tga` | `F1 Challenge V10\SeasonData\Vehicles\hud broadcast.tga` | 32,812 B, SHA-256 `467EADAB…3BCA2` | none found | **orphan/uncertain** — do not assume it backs the bottom list |
| `FTIM24HUD.BMP` | same folder | 1,116,162 B | not traced | out of scope |
| `font.mas` | same folder (observed next to HUD strings) | — | text system | font asset, not traced |

---

## 6. Routes A/B/C

**A — extend a reviewed native broadcast element.** Rejected for now. The
classification panel is a display-system object with internal data and a
periodic lifecycle; there is no reviewed insertion/ownership API, and the only
ways in are hooking `sub_4D8760`/`sub_4DA680` or duplicating display state.
That is a raw-address, game-thread change with no reviewed contract; it also
risks the broadcast gate semantics proven above.

**B — SDK/broker-owned in-game textured/text overlay.** Long-term correct
product route, but the current broker exposes native timed text only. Missing
capabilities (must be specified, not invented): game-thread quad/texture draw
with D3D device-loss recreation, overlay priority contract vs displays/messages,
and ownership/unload lifecycle. The Safety Car mod must not be the owner of a
new render layer.

**C — separate window/external overlay.** Recommended now. Zero game/mod
changes, reversible, testable. Address: transparency (`WS_EX_LAYERED` /
`WS_EX_TRANSPARENT`), input focus (click-through), fullscreen mode (require
borderless/windowed; abort on exclusive), capture/stream behavior, DPI/scaling,
pause/replay (state-driven hide), resource recreation (own window owns its
resources), ownership (new tool, not the SC mod), performance (negligible), and
rollback (close the overlay).

**Recommendation:** implement the reference-style lower third as **Route C**
now; specify **Route B** as the future in-game capability; keep the Safety Car
HUD correctness fixes in the native message slots (Route C must not gate them).

---

## 7. Implementation specification — Safety Car lower third (conceptual; not implemented)

### 7.1 Event/state contract (authoritative SC state, not button presses)

| SC state (from the audited inventory) | Lower third |
|---|---|
| deploy request / confirmation shown | **not shown** (native text only) |
| deployment accepted; SC leaving pits | show `SAFETY CAR` + subtitle `DEPLOYING` |
| SC on track / controls field | show `SAFETY CAR` + subtitle `ON TRACK`; persistent `NO OVERTAKING` stays in native slot |
| SC in this lap / return preparing | show `SAFETY CAR` + subtitle `IN THIS LAP` / `RETURNING` |
| SC in pits, GREEN FLAG | show `GREEN FLAG` for max 5 s, then hide |
| SC fault / lost / reset failed | show `SAFETY CAR` + fault subtitle; hide only when state clears or session exits |
| Red Flag handoff | hide SC band; Red Flag presentation owns the screen |
| session exit / race results | hide |
| rapid deploy/recall | 1.5 s debounce; ignore stale queued state |

### 7.2 Toggle and coexistence

- Independent setting `lower_third.enabled`; default `follow_broadcast = 1` so
  key 3 / the Broadcast Overlays option hides the band as the owner expects.
- `NO OVERTAKING` and all race-control text remain in the native message slots
  regardless of the band.
- Band must not cover the native message area (upper-left) or the classification
  rows; at 1280x1024 the classification occupies y ≈ 818-898, so anchor the band
  below it (y ≈ 916-1004) or use a narrow strip that leaves rows readable.
- Priority: band above classification, below modal confirmation prompts.

### 7.3 Drawing / layout parameters (conceptual mock, not a game screenshot)

```
+--------------------------------------------------------------------------------+
|  (native messages, top-left)                                                   |
|                                                                                |
|                                            [classification rows y 818-898]     |
| +----------------------------------------------------------------------------+ |
| | [flag+car 96x64]   SAFETY CAR                                subtitle     | |  <- band y 916-1004
| +----------------------------------------------------------------------------+ |
+--------------------------------------------------------------------------------+
   band: width 92% centered, height 96 px at 1024p, scale = screenH/1024 (clamp 0.75-1.5)
   fill: near-black 78% alpha; left motif 96x64 (yellow flag + car silhouette)
   text: white caps, heavy weight, >= 28 px at 1024p; subtitle 60% size, yellow accent
```

### 7.4 Lifetime, cancellation, accessibility

- Max lifetimes: announcement 6 s; lifecycle subtitles refresh while state
  persists; green 5 s; fault until cleared.
- Cancel on state end, session exit, replay, or user disable; no stale frame
  after pause.
- Readability: min supported 1280x720; high-contrast option (solid background);
  no color-only meaning; text scales with resolution.

### 7.5 Cleanup, tests, abort, rollback

- Cleanup: overlay process owns its window/resources; stop with the game.
- Tests (guarded full weekend only, never QuickRace): exact game/SDK/SC hashes;
  key 3 on/off; option on/off; SC active/inactive; leader crossing; pause/replay;
  spectator camera; race exit and second race load; capture the classification
  visibility and band behavior; assert SC native text remains when broadcast off.
- Abort: exclusive fullscreen or unsupported compositor → do not start; stop on
  state desync.
- Rollback: close the overlay; no game files touched. If Route B is later built:
  broker capability + ABI review, game-thread draw, device-loss recreation,
  unload and session-restart tests.

---

## 8. Remaining blockers and next test

1. Guarded capture matrix (key 3, option, pause/replay, spectator) — not
   performed; the toggle-scope rows in section 4 marked `UNRESOLVED` need it.
2. Options-screen row → control factory mapping (`0x5EDFB4` table consumer) —
   not closed; the option/flag same-state claim is `DERIVED`, not `CONFIRMED`.
3. `sub_4D9C10` / `sub_4D9EE0` (display 6/8 renderers) and `0x5D4B64` header
   string remain unidentified.
4. `hud broadcast.tga` remains an orphan; do not ship or cite it as the
   classification asset.
5. Exact `dword_8ACEE0` semantics (standings order array) is inferred from its
   use in `sub_4D1B50`; a runtime read would confirm.

**Next test (when authorized):** one guarded full weekend, capture key 3
on/off and the graphics option on/off with timestamps, one leader crossing, one
SC deployment, pause and replay; record process identity and hashes; verify the
classification hides when broadcast is off and SC native text stays visible.

**Changed files (this task, documentation only):**

- new: `f1c_ida_decompiled/investigations/broadcast_overlays/RESEARCH_REPORT_2026-09-23.md`
- edited: `f1c_ida_decompiled/investigations/broadcast_overlays/README.md` (link)
- edited: `development/mods/full_safety_car/CURRENT_STATUS.md` (status line)
- before-states: `workspace_organization_audit/BROADCAST_OVERLAY_RESEARCH_BEFORE_20260923/research_report_2026-09-23/`

**Explicit statement:** gameplay/runtime integration was **not** performed; no
game installation, executable, DLL, mod source, or production artifact was
changed. Static evidence only, plus the owner's observations marked as such.
