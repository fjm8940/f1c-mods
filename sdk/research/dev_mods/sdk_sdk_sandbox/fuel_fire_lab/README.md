# fuel_fire_lab — pit-stop fuel fire (Verstappen Hockenheim 1994)

Sandbox lab (production=false). Synthesises a **fuel fire** at the player's
car during a pit stop: big orange flames from the engine cover, fuel filler
and cockpit, a black smoke column, exhaust flames even at standstill — the
look of Jos Verstappen's Benetton going up in the 1994 Hockenheim pits.

Nothing in vanilla does this: F1C has no fuel-fire failure. The mod drives
two stock mechanisms instead:

1. **FLAMES reaction descriptor** (id 4) from the per-track SpecialFX table
   (`0x8ACEA0`, stride 108, `MaterialShaderReactionEffectParser_004ca480`),
   emitted directly through `ParticleTrailEmitter` (0x4CBC50) from five
   car-body anchors — the same proven path as `bluesmoke.c`.
2. **Player engine FLAMES bank** `0x7C921C` (ENG_BANK3, positive value
   renders even parked — `engine_failure.c` isolation-proven 2026-07-30).

## GUI — fire texture lab

`gui/fire_lab_gui.py` — a Tkinter window that drives the DLL over the named
shared-memory block `F1C_FUEL_FIRE_CTL` (same mechanism as
`safety_car_gui.py` / `pit_failure_gui.py`). Shows the **live descriptor the
game is actually rendering** and lets every knob be dragged mid-fire:

- **Flame**: scale, growth, rate, bursts/tick, black smoke, bank3 flames,
  particle pool (`max`), colour (preset combo + ARGB hex)
- **Pit failure**: chance %, burn seconds, kill engine, DNF after burn
- **Buttons**: FIRE ON/OFF, DNF, DUMP, NEXT COLOR
- **Live readout**: game phase/session, pit state, descriptor
  scale/growth/colour/max read back from the game's memory, last event

```
python gui/fire_lab_gui.py
```

Run it next to the running game. The window can be closed any time; the DLL
keeps its last values and the F-key hotkeys keep working.

### Integration with the pit-stop failures package (pit_failure_gui.py)

**The two are separate DLLs, deliberately.** `pit_failure_gui.py` drives
`pit_commentary.dll` inside `mods/extended_vanilla_features/patches/
pitstop_modding/` — a preserved-history tree that the corpus rules freeze
(no modifications). `fuel_fire_lab` is the sandbox promotion of the same
idea, so the fire lives here, not there.

They coordinate at two points, so running both gives one coherent stop:

1. **Shared pause.** The "PAUSE pit rolls" checkbox here sets/clears the
   SAME named event `F1C_EVF_PAUSE_pit_failure_commentary` that
   pit_failure_gui's PAUSE toggle uses. Pausing either GUI pauses both mods'
   auto-rolls.
2. **Same trigger edge.** Both roll on `pittingStatus 3` + `serviceReady`
   (veh+0x3450/0x345C), so in one pit stop pit_commentary may inject a
   wheel-gun/hose line while fuel_fire_lab lights the car. The fire is this
   mod's own failure type — pit_commentary knows nothing about it, and its
   DNF is optional.

## Experimenting with the flame look

Two ways, both live (no rebuild, no track reload):

- **`fuel_fire_lab.ini`** — re-read every 100 ms. `scale_mult`, `growth_mult`,
  `rate`, `bursts`, `blacksmoke`, `color` (ARGB hex), `max`. Save the file,
  next frame applies. Works mid-fire: the live descriptor is rewritten per
  tick.
- **Hotkeys** (F1-F8): F1 toggle fire, F2 cycle colour presets (orange →
  yellow → white → red → vanilla), F3/F4 scale ±0.5, F5/F6 bursts ±2,
  F7 DNF now, F8 dump state.

The vanilla `terrain.ini` flames line (`Max=256 Scale=(0.40,0.40,0.40)
Growth=(1.5,1.5,1.5)`) is never edited — original descriptor values are
saved at first fire per track load and restored when the fire stops.

## Pit failure

On the `pittingStatus 3` (box) + `serviceReady` edge, roll `[Fire]
chance_pct` (default 25%). On a hit: `[Fire] burn_seconds` of fire, engine
killed (`0x7C9081`), then optionally DNF via the proven player retirement
path (`FinishState=2`, `ReasonCode=9` fuel, `PLAYER_OUT=1`) — same recipe as
`pit_commentary.c` MOD_REFERENCE §16.

## Known limits

- The descriptor is a per-track allocation; the pointer is re-resolved every
  tick (it can move on session change).
- Fire is player-only (slot 0). AI fuel fires are future work.
- `Max=` raise allocates more particles; keep within reason (2048 default).
- F-keys: verify in-game they don't collide with your binds.

## Build / test

```
python tools/f1c_mod.py build fuel_fire_lab
python tools/f1c_mod.py stage fuel_fire_lab <game-root>
python tools/f1c_mod.py unload F1C_FUEL_FIRE_LAB_UNLOAD   # self-eject
python gui/fire_lab_gui.py                                 # control window
```

The launcher's dev-dlls.json entry also carries `"sharedMemory":
"F1C_FUEL_FIRE_CTL"` so the SharedMemoryBinder binds it like the other
GUI-driven mods.

Live testing follows the guarded full-weekend path (f1cm_guard.asi), never
QuickRace. Log: `fuel_fire_lab_log.txt` beside the DLL.
