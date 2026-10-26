# Multiplayer co-op AI authenticity audit — 2026-08-23

## Result

The validated one-bot implementation is using F1 Challenge's native AI driver,
vehicle, physics, damage, retirement, and talent-loading paths. It is not
controlling a duplicate game process, copying coordinates, synthesizing player
inputs, or replacing the game's steering/throttle logic.

The host owns each bot as a normal role-1 AI vehicle. Other human instances own
a role-2 network replica of that same roster vehicle and therefore do not run a
second AI brain. The remaining unverified area is scale: more than one bot has
not yet passed the same full-weekend multiplayer validation as Tester A.

## Stock initialization chain

Single-player field construction in `AIDriverCountSetup_004bfd90` uses this core
identity sequence for an AI opponent:

1. `SessionStateTransition(..., 1, 0)` assigns the stock AI role.
2. `NationalityDriverInfoLoader(vehicle, roster_index)` copies the selected
   roster entry's driver/talent identity into the vehicle.
3. `TeamIDSetter(vehicle, slot)` assigns the vehicle slot/team identity.
4. The normal full scene/vehicle setup initializes the AI and physics state.

`multiplayer_ai_host.cpp` follows the same sequence, then explicitly runs the
normal full vehicle setup through `VehgenScnFileLoader(vehicle, 1)` and
`SceneRenderUpdate(vehicle, 1)`. The multiplayer-specific difference is only
authority: the host assigns role 1 and clients assign role 2.

## Proof that the RCD skill file is loaded

The static call chain is:

`selected .veh path`
→ `VehicleLookupByName`
→ roster index
→ `NationalityDriverInfoLoader`
→ `SetDriverName` copies the roster's driver/RCD section identity to vehicle
  offset `+0x4620`
→ `SceneRenderUpdate(..., full_setup=1)`
→ `AIDriverStructInit(..., full_setup=1)`
→ `sub_424080`
→ lookup of the RCD section keyed by the vehicle's `+0x4620` identity.

`sub_424080` reads the native RCD keys, including `StartsDry`, `StartsWet`,
`StartStalls`, `QualifyingAbility`, `TrackAggression`, `RainAbility`,
`RaceAbility`, `Passing`, `CorneringAdd`, `CorneringMult`, `Crash`,
`CompletedLaps%`, and `Script`. It then applies the stock transforms and stores
the resulting per-driver AI state. Examples already mapped in the canonical AI
research are:

- `Passing` → vehicle `+0x44CC`, as `min(1.0, 0.9 + Passing / 1000)`.
- `TrackAggression` → vehicle `+0x44F8`, after the stock global scale/clamp.
- `CorneringAdd` and `CorneringMult` → vehicle `+0x4500` and `+0x4504`.
- `RaceAbility` and `RainAbility` → the stock derived pace/condition scalars.

The GUI does not edit, copy, normalize, or substitute any `.rcd` value. It
shows the real selected RCD source and a few raw values as an audit aid.

## Proof that only the host drives the bot

`AIMainUpdate_00424e70` rejects vehicles whose role at `+0x32C8` is greater
than 1. The host's bot is role 1, so it enters the stock AI update containing
`AIAbility`, opponent/team scans, racing-line calculation, steering, correction,
gear shifting, pit-state handling, start behavior, and scripted failures. The
client replica is role 2, so it returns before those decisions and receives the
host-authoritative network state instead.

This is the key distinction from the earlier divergent experiment, where both
instances independently ran role-1 AI and produced teleporting/conflicting
positions.

## Vehicle identity and duplicate exclusion

Human display/profile names are not used as the collision key. The runtime
resolves every selected `.veh` to its canonical `VehicleRosterIndex` and scans
all active human/vehicle slots before creating a bot. If a human profile named
Nicky Lauda is driving the roster entry for Michael Schumacher's Ferrari, the
Michael Schumacher `.veh` resolves to the already-active roster index and the
bot is skipped.

The configurator additionally lets the owner mark the Ferrari as `HUMAN CAR`,
making `HUMAN CAR` and `AI BOT` mutually exclusive in the saved grid. That is a
useful pre-session guard, but the live roster-index check remains authoritative.

## Roster verification

All 22 entries in `multiplayer_ai_roster_2001.ini` were checked against the
active `F1 Challenge V10` installation:

- 22/22 `.veh` files exist.
- 22/22 `.veh` `Driver=` identities match the GUI entry.
- 22/22 mapped `.rcd` files exist.
- 22/22 `.rcd` files contain the exact matching driver section.
- All 22 vehicle paths are unique.

## Runtime evidence

The current live milestone is two human instances plus one Tester A bot:

- Host: local human slot 0, Tester A role 1.
- Client: local human slot 1, Tester A role 2.
- Only Tester A left the garage under AI control.
- Human cars no longer received AI control or teleported between human and AI
  positions.
- Tester A participated in normal collision/damage handling and retired after the
  collision with the host.

This is strong end-to-end evidence for one bot. It does not yet certify 2–14
bots, session transitions, a complete race weekend, pit strategy, reconnects,
or late joins at scale.

## Required scale-up gates

Keep the validated one-bot configuration as the restoration baseline. Test in
full-weekend multiplayer mode, never Quick Race:

1. Two humans + two bots through practice, qualifying, race start, pit cycle,
   finish/retirement, and next-session transitions.
2. Repeat with four bots.
3. Repeat with eight bots.
4. Continue to the selected season's complete non-human roster. The stock
   multiplayer human-player list is limited to 16, but the vehicle storage and
   multiplayer slot-sync mask contain 104 entries; host-owned bots do not consume
   the human player-list entries.

At every gate, confirm that each bot is role 1 only on the host, role 2 on every
client, occupies a unique roster index, loads its own RCD-derived values, and
does not displace or control a human vehicle.
