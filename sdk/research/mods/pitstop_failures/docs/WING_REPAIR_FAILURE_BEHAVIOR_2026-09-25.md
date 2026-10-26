# Wing-repair and pit-failure behavior — 2026-09-25

## Question

Determine whether the current Pit Stop Failures live implementation adds a
failure during player or AI wing/body-repair stops, and whether it changes the
game's long rear-wing repair delay.

## Static-code findings

Facts from the current development live adapter (`live/live_client.cpp` and
`live/pit_live_timing.h`):

- At service start, the adapter passes `wing_adjustment_active || repair_active`
  as `other` to `pit_live_jobs`.
- `other` makes the stop a valid, real service and contributes its native
  baseline to timing. It does **not** create a tyre-work job.
- The failure policy enables slow-wheel, wheel-gun and cross-thread incidents
  only when a tyre job exists; fuel incidents require refuelling. A valid stop
  with a possible release enables the engine-stall parent.
- Therefore a wing/body-repair-only stop remains eligible for the normal
  7.50% incident gate, but its only natural failure category is an engine
  stall. If tyres and/or fuel are also scheduled, their corresponding failure
  categories are eligible as usual.
- The adapter never changes wing/body-repair duration. It holds the native
  `stop_end_time` only for a committed mod failure, so a selected stall can add
  its own delay on top of the game-calculated repair baseline.
- The implementation loops over both player and AI vehicle slots using this
  same service construction. Thus an AI front-wing repair is treated the same
  way: no invented wheel-gun/slow-wheel fault solely because of the wing
  repair; an engine-stall incident remains possible, and ordinary tyre/fuel
  incidents are possible only if those jobs are also present.

## Native-game baseline (separate from the mod)

The preserved technical reference records the game formula as a maximum of
fuel, tyres, wing and body-repair jobs. It records an 8-second normal wing
job and a 39-second rear-wing job when the rear wing is broken. That native
39-second cost is the likely source of the player's long rear-wing repair
stop; this mod does not shorten, replace or otherwise correct it.

## Evidence and limitations

- Source evidence: `live/live_client.cpp` service construction around lines
  828-832; `live/pit_live_timing.h`; `src/pit_failure_policy_v3.c`
  `pf3_parent_mask`; embedded default v3 policy (750 bp incident gate).
- Native timing evidence: `docs/DOCS.md`, sections 3 and 5.
- This is static-source analysis. The 1.0.3 release record states that player
  and AI failure outcomes in a full race have not been human-validated. A
  guarded player rear-wing and AI front-wing gameplay test remains the
  appropriate next verification if observed behavior differs.

## Task boundary

Read-only investigation apart from this durable record and its README index.
No DLL, configuration, game installation, production artifact, or product
status was changed. The workspace structure validator had its pre-existing
11 errors and 0 warnings at orientation; no structural change was made.
