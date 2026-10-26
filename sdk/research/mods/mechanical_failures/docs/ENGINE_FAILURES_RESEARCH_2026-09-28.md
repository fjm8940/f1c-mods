# Engine failures - inventory, realism, and a new layer on top of vanilla (2026-09-28)

**Owner questions:** how diverse / realistic / implemented are the existing
engine failures; same end result or variance; how does OIL LEAK differ from
WEAR OUT or OVERHEATING? Research real F1 engine trouble and how well we can
recreate it. **Constraint:** do not recycle vanilla engine overheating
(radiator/airflow) or the vanilla engine lifetime - keep them, build NEW
failures on top.

**Status:** research + proposal. No code changed. Tiers: FACT = code/live
proven, REAL = public F1 record (sources at the end), INFERENCE = design
reasoning.

## 1. What exists today

### 1a. Vanilla (the game, player car) - keep as is

| Mechanism | How it works (FACT) | Driver agency |
|---|---|---|
| Thermal model | `EngineStressSimulation_0043ef20`: heat from combustion + engine speed; cooling from `RadiatorSetting` x airflow; oil temp at `this+7500` (dash "ot"). Oil temp above `LifetimeOilTemp` accelerates life burn (~doubling per range unit). Live-proven: doughnuts killed an engine at oil 130.7 C | Yes - radiator setting, revs, airflow |
| Engine lifetime | life `0x7C91DC` = Normal(LifetimeAvg, LifetimeVar); drains with rpm curve + oil-temp curve + stress; at 0: ReasonCode 1, one smoke bank, staged death (below -15.4 stress ramps to 0.6) | Yes - revs and temps |
| Fuel starvation | fuel `0x7C75C4` near empty -> stochastic torque multiplier -> dead | Yes - fuel strategy |
| Collision seizure | `damage.ini [PHYSICAL] Engine=` impulse | Crash avoidance |

### 1b. Ours (mechanical_failures engine family)

| Type | Implementation (FACT) | End result | Visual | Power loss feel |
|---|---|---|---|---|
| OVERHEATING (auto 30%) | mod timeline: stress floor 0->0.6 from 40 s, bank1 steam from 90 s, kill at 150 s; radio eid 77 | engine off, RC 1 | white steam | stress lever |
| WEAR OUT (auto 30%) | writes vanilla life = -16 -> vanilla stress ramp; we kill after 5 s at cap | engine off, RC 1 | vanilla bank roll | stress lever |
| BLOW UP (auto 10%) | stress cap + engine off now + sound + fireball | engine off, RC 1 | fireball, smoke | none (instant) |
| ENGINE STOP (auto 10%) | ignition byte 0 | engine off, RC 1 | none | none (instant) |
| OIL LEAK (auto 20%) | mod pressure 100->0 in 60 s, blue smoke, eid 130 at 25%, stress floor below 50%, kill at 0 | engine off, RC 1 | blue smoke | stress lever |

### 1c. Verdict

- **Diversity: low.** Every type ends identically (engine off, ReasonCode 1,
  retire line). The three slow types (OVERHEATING, WEAR, OIL) differ only in
  timing and smoke colour - all lose power through the **same lever**
  (`ENG_STRESS`, the rev-phased torque flutter), so they drive the same.
- **Agency: none.** Once armed, nothing the driver does changes the outcome.
  Real drivers routinely nursed failing engines home (short-shifting, lifting,
  lower revs) - the single biggest realism gap.
- **Recycling (owner constraint):** WEAR OUT *is* the vanilla lifetime death
  (it writes the vanilla life field). OVERHEATING duplicates the vanilla
  thermal failure's story (hot engine -> power -> death) with a forged
  timeline. Both should be replaced.
- **Realism by type:** BLOW UP good (the commonest real V10 end). OIL LEAK
  reasonable but on rails. ENGINE STOP is really an electrical cut, not an
  engine failure. OVERHEATING/WEAR recycle vanilla.

## 2. Real F1 V10 engine trouble, 1999-2002 (REAL)

- Engines were the **top retirement cause** (1990-2013: 31% of mechanical
  breakdowns; 2001: 32 engine DNFs). V10s reached 17,500-19,000 rpm, piston
  acceleration ~8,900 g - the margins were tiny.
- **Pneumatic valve springs:** valves closed by compressed air (ring main +
  on-board gas bottle + regulators). Leak -> pressure drops -> valves float at
  high rpm -> valve hits piston -> engine destroyed. Teams **topped up the air
  bottle at pit stops**. Example: R. Schumacher, Monza 2002 - pneumatic
  pressure loss, smoke, out within a lap.
- **Oil pressure / oil system:** dry sump, ~11 scavenge pumps. Low oil pressure
  was often **managed**: M. Schumacher, Brazil 2000, slowed for the last laps
  nursing oil pressure and still won; Hakkinen retired from oil pressure loss
  in the same race.
- **Exhaust failure:** Schumacher, Monaco 2000 - cracked exhaust overheated and
  broke the left-rear suspension -> retirement. Failures had **knock-on**
  effects on other systems.
- **Catastrophic mechanical:** conrod / piston / crank - sudden, smoke, often
  oil fire (= our BLOW UP).
- Electronics/traction-control faults were separate (our ELECTRONICS family).

## 3. What we can do on top of vanilla - levers

| Lever | Status | Use |
|---|---|---|
| Life `0x7C91DC` (double) | FACT writable | extra wear on top of the vanilla drain (never replace it) |
| Stress `0x7C91D4` | FACT | power flutter (use sparingly - it is what every type uses now) |
| Ignition `0x7C9081` | FACT | cuts / stalls / rev-ceiling misfire |
| RPM `0x7C90C4`, rev limit `this+7388` | FACT read | rpm-dependent rules, driver agency |
| Oil temp `this+7500` (`0x7C9174`, double, Kelvin) | **FACT read + write (live 2026-09-28, §6)** | add heat into the vanilla thermal model -> vanilla life burn does the damage |
| Pit state `veh+0x3450` (3 = in box) | FACT | pit repairs / air top-up |
| Suspension block `F1C_SUSFAIL_CTL` | FACT (merged DLL) | knock-on failures (exhaust -> rear suspension) |
| Emitters + particles, HUD strip, eid 77/130/119, sound | FACT | presentation |

Radio vocabulary is thin for engines: only eid 77 (overheating), 130
(marshal "mechanical problem"), 119 (retire). New failures need HUD dash text.

## 4. Proposal - new engine failures (replace OVERHEATING + WEAR OUT)

Design rule: **cause is ours, consequences run through the vanilla model**
(life drain, thermal model, ignition), and each has its **own end-state and
driver agency**.

| # | Failure | Real basis | Mechanism (on top of vanilla) | Driver agency | End result | Confidence |
|---|---|---|---|---|---|---|
| N1 | **Valve air leak** (pneumatic) | Monza 2002 | air pressure bleeds (mod value). Below ~70%: a **rev ceiling** drops with pressure - above it, ignition flickers (valve float). Revving past the ceiling for > 2 s -> valve-piston contact -> BLOW UP | stay under the ceiling; **pit stop tops up the air** (pressure restored) | finish slower / pit fix / blow-up if abused | high (levers proven) |
| N2 | **Oil pressure loss** (scavenge/pump) | Brazil 2000 | pressure falls to a low plateau; extra life drain proportional to (rpm/limit)^4 **added** to vanilla drain | short-shift, lower revs -> can finish | survive slow / engine death if pushed | high |
| N3 | **Oil leak v2** | common | as today, but oil *temperature* climbs by adding heat into the vanilla thermal model instead of a forged stress ramp; blue smoke; vanilla life burn kills it | lift and cool (radiator/airflow still matter) | death timing depends on driving | medium (oil-temp write untested) |
| N4 | **Exhaust crack** | Monaco 2000 | small power loss + flame flickers; heat builds at one rear corner -> after N minutes arms rear suspension FAILURE (cross-family) | none (knock-on) | engine keeps running, suspension breaks | high |
| N5 | **Piston ring / blow-by** | common | slow rising blue-grey smoke + gentle power loss; after a random time -> BLOW UP | lower revs delay it | delayed blow-up | high |
| N6 | **Overrev damage** (missed downshift) | common | not random: rpm > limit x1.04 (money shift) takes a chunk off vanilla life, big overrev -> valve damage (N1 at 50%) | don't overrev | driver-caused | high |
| keep | BLOW UP | conrod/crank | as now | - | instant | validated live |
| move | ENGINE STOP | electrical | fold into ELECTRONICS (ECU cut-out already exists) | - | - | - |

Resulting end-state variety: finish slowed (N1/N2 managed), pit-fixable (N1),
delayed death (N3/N5), knock-on to suspension (N4), instant (BLOW UP),
self-inflicted (N6).

## 5. Unknowns to settle before building

1. Oil-temp write (`this+7500`) - does added heat persist through the thermal
   integration? One live poke test. N3 depends on it; N1/N2/N4/N5/N6 do not.
2. Rev-ceiling feel (N1): ignition flicker above a rpm threshold must feel
   like valve float, not a limiter - owner judgement.
3. Pit top-up hook (N1): trigger on `pittingStatus` 3 edge - proven field.
4. Balance: vanilla lifetime already kills engines in this install's reduced
   2002 lifetimes (ENGINE_NATIVE_PLAYER_FAILURES doc) - our layer adds on top,
   so frequencies need owner tuning.

## Sources

- [2002 Italian Grand Prix - Wikipedia](https://en.wikipedia.org/wiki/2002_Italian_Grand_Prix)
- [Pneumatic Valve Springs in F1 - Formula 1 Dictionary](https://www.formula1-dictionary.net/f1-pneumatic-valve-springs/)
- [The Rise and Fall of F1's 20,000 RPM V10 Engines - Driver61](https://driver61.com/motorsport-engineering/f1-v10-engines-20000-rpm/)
- [BMW E41 / P80 engine - Wikipedia](https://en.wikipedia.org/wiki/BMW_E41_/_P80_engine)
- [areppim: F1 car failures grouped by cause](http://stats.areppim.com/stats/stats_f1carreliabil_hist.htm)
- [2001 F1 season stats - F1 Daily Brief](https://f1dailybrief.com/seasons/2001)
- [Dry sumps in F1 - F1technical.net](https://www.f1technical.net/forum/viewtopic.php?t=28440)
- [Cosworth CA V8 - F1technical.net](https://www.f1technical.net/features/18858)
- [2000 Brazilian Grand Prix - Wikipedia](https://en.wikipedia.org/wiki/2000_Brazilian_Grand_Prix)
- [2000 Monaco Grand Prix - Wikipedia](https://en.wikipedia.org/wiki/2000_Monaco_Grand_Prix)
- [Ferrari V10 engine - Wikipedia](https://en.wikipedia.org/wiki/Ferrari_V10_engine)

## 6. Live test - oil temperature write (2026-09-28, practice)

Unknown #1 resolved. Probe `trials/realism_2026-09-28/oil_temp_probe.py`
(elevated, 10 Hz; raw CSV `oil_temp_probe_result.txt`). Car idling at
1363 rpm (likely stationary), SessionTime advancing.

- Address correction: oil temp is `0x7C7428 + 7500` = **`0x7C9174`** (the
  earlier `0x7C9124` in §3 was an arithmetic slip, now fixed). Code: double,
  Kelvin (`flt_8AFEF0 + 273.16` ambient term), integrated each tick as
  `new = old + (heatIn - heatOut) * dt` (`EngineStressSimulation_0043ef20`
  lines 83-92).
- **FACT:** one write of +15 K (94.99 C -> 109.99 C) was accepted and read
  back; the game did **not** overwrite it - it kept integrating from it and
  cooled the oil naturally: 109.99 -> 108.20 (1 s) -> 104.16 (6 s) ->
  98.30 (15 s). Roughly 1 K/s at idle.
- Engine life drained ~0.14-0.15/s before and after - 110 C is below this
  car's `LifetimeOilTemp` base (~123 C in the doughnut test), so no extra
  burn yet, as the vanilla model predicts.
- **Consequence for N3:** feasible exactly as designed - inject heat
  continuously (K/s scaled by leak severity); vanilla cooling (radiator,
  airflow, revs) fights it; once oil passes the car's lifetime base the
  vanilla life burn accelerates and kills the engine. Driver agency comes for
  free. Nothing forged beyond the heat input.

## 7. N3 OIL LEAK v2 - BUILT (2026-09-28), awaiting install + owner test

Replaces the old forged oil leak (60 s pressure ramp + stress floor).
- Oil quantity drains at a rate rolled at arm: slow 0.25 / medium 0.5 /
  fast 0.85 %/s (~400 / 200 / 120 s to empty).
- Heat into vanilla oil temp `0x7C9174`: `3.0 K/s x loss^1.3 x (0.25 +
  0.75 x rev^2)`, rev = (rpm - idle) / (limit - idle), unit-agnostic.
  Only while the engine runs. Vanilla cooling and vanilla life burn do the
  rest; no stress written.
- Live reads (elevated `rev_probe.py`, 2026-09-28): rev-limit field
  `0x7C9104` double = 1895.4 and rpm field idles at 1362.6 (same unit,
  idle = 0.72 of limit - hence idle-relative revs); this car's
  `LifetimeOilTemp` base `0x7C917C` = 377.86 K = **104.7 C** with idle oil at
  95.8 C - only ~9 K margin, so the heat max was halved from 6 to 3 K/s.
- Oil gone -> seizure (running 0, RC 1, retire line). Blue smoke grows with
  loss; eid 130 at 25% loss; eid 77 once oil passes the limit; HUD OIL TEMP
  (within 5 K of limit) then OIL PRESSURE (< 25% oil).
- Engine ABI 7 -> 8 (216 -> 228 B): `oil_temp_c`, `oil_limit_c` (floats),
  `oil_rate_x100`. Panels + tests updated (OK). Build `895fd086…`, not
  installed (game running). Before-state:
  `workspace_organization_audit/OIL_LEAK_V2_BEFORE_2026-09-28/`.
- Pending owner test: drive with the leak; does lifting/short-shifting keep
  the oil under the limit longer? Does the vanilla death feel right?

## 8. N1/N2/N4/N5/N6 BUILT + INSTALLED (2026-09-28)

Installed `mechanical_failures.dll` SHA256 `81570bb8…` (build = game copy),
includes oil leak v2 (§7) and the wear-out fix. Game was closed.
Before-state: `workspace_organization_audit/ENGINE_N1_N6_BEFORE_2026-09-28/`.
Patch: `trials/realism_2026-09-28/patch_engine_n.py`.

| Type (auto id) | Build | Tunables |
|---|---|---|
| N1 valve air leak (6) | air bleeds 0.25-0.75 %/s; <70% -> rev ceiling 35..100% of idle->limit band; above it ignition alternates each 50 ms tick (valve float); over ceiling+8% for 2.5 s (decaying) -> BLOW UP; pittingStatus 3 -> air 100% (leak remains); HUD VALVE AIR LOW / VALVE AIR REFILLED | rates, 70%, 2.5 s |
| N2 oil pressure (7) | falls 4 %/s to a 20-45% plateau; extra vanilla life drain 160 x rev^4 x (1-p) /s; life < 0 -> wear_watch finishes it; HUD OIL PRESSURE | OIL_P_K 160 |
| N4 exhaust crack (8) | stress floor 0.06, 0.3 s flame spits every 3-8 s, eid 130; after 90-240 s writes `F1C_SUSFAIL_CTL` wheels = one rear corner + cmd_stage2 (PART BROKEN) | timings |
| N5 blow-by (9) | damage += (0.3 + 1.7 rev^3)/s to a 150-300 limit; stress floor 0.15 x progress; blue-grey smoke; >30%: HUD ENGINE PROBLEM + eid 130; limit -> BLOW UP | rates |
| N6 over-rev (always on, `overrev_enable`) | rpm/limit > 1.04 -> life -= 4000 x excess /s, HUD OVER-REV; > 1.10 for 0.2 s -> arms N1 at 50% air | 1.04 / 1.10 |

- Auto mix now: valve 20 / oil pressure 20 / oil leak 20 / blow-by 15 /
  exhaust 10 / blow up 15. Overheating, wear out and engine stop are no
  longer rolled (recycled vanilla / electrical); still manual in the full
  panel. Last-lap guard: blow up -> oil pressure.
- Engine ABI 8 -> 9 (228 -> 276 B). Simple panel engine card rebuilt (six
  failures, live status for each, over-rev switch + rev % + over-rev count);
  full panel gained the four new buttons. Tests OK, both smoke tests PASS.
  Screenshot `trials/realism_2026-09-28/panel_v4.png`.
- **Not live-validated.** INFERENCE risks: N1 50 ms ignition alternation may
  feel like a limiter rather than valve float; N6 depends on the game letting
  rpm exceed the limiter on a downshift (unverified); N4 relies on the
  suspension family processing cmd_stage2 from another thread's write (same
  path the panel uses).

## 9. Coverage check - are these the main ICE failure causes? (2026-09-28)

General internal-combustion failure families vs our build. Source: general
engineering knowledge (INFERENCE tier - no new primary sources this pass).

| Family (typical cause) | Covered by | Gap |
|---|---|---|
| Lubrication (pressure loss, leak, starvation) | N2 oil pressure, N3 oil leak | - |
| Valvetrain (valve float, spring/pneumatic, broken valve) | N1 valve air, N6 over-rev | cam/timing drive failure (instant) - could be a BLOW UP variant |
| Rotating/reciprocating (conrod, piston, crank, bearings) | BLOW UP, N5 blow-by | - |
| Over-stress (over-rev) | N6 | - |
| Exhaust | N4 | - |
| **Cooling (water leak, radiator damaged/blocked by debris, water pump)** | none (vanilla only models radiator SETTING/airflow; our old OVERHEATING was dropped) | **missing - top-tier cause.** Build on top: debris blocks the radiator (cooling cut, can clear/pit) or water leak (heat into vanilla thermal model, like N3) |
| **Fuel system (pump, fuel pressure, injector)** | none (fuel starvation was removed from electronics) | **missing - classic F1 retirement ("fuel pressure").** Engine cuts under load/high g, worsening |
| **Throttle / hydraulics (throttle actuator stuck or dead)** | gearbox stuck gear covers hydraulics for shifting only | **missing** - throttle stuck partly open/closed |
| Ignition / ECU / sensors | ELECTRONICS family | - |
| Intake (airbox damage, debris) | none | minor; skip |
| Detonation/knock | none | rare in F1 (controlled fuel/maps); fold into blow-by |

## 10. N7 cooling failure + N8 fuel pressure - BUILT + INSTALLED (2026-09-28)

Installed `mechanical_failures.dll` SHA256 `51f24138…` (= build; game
closed). Before-state: `workspace_organization_audit/ENGINE_N7_N8_BEFORE_2026-09-28/`.
Patches: `trials/realism_2026-09-28/patch_engine_n78.py`, `patch_panel_n78.py`.

- Static read of the vanilla thermal model (`EngineStressSimulation_0043ef20.c`
  lines 83-131, DERIVED): water temp this+7564 (`0x7C91B4`, K) is cooled by
  `(airflow x this+7580 + this+7572) x (ambient - water)` and heated from oil
  via this+7548; above the oil limit and above 100 C water the game itself
  scales power (this+7540 / this+7588).
- **N7 cooling (id 10):** scales both radiator coefficients (`0x7C91BC`,
  `0x7C91C4`). Debris: 25-45% at once, 12%/10 s chance to blow clear above
  50 m/s, pit box clears. Water leak: 100 -> 10% over 150-300 s, steam under
  load > 110 C, water > 125 C for 10 s -> head gasket (engine off, plume).
  HUD WATER TEMP > 105 C, eid 77 > 110 C. Originals restored on clear /
  repair / session leave / unload. `coeff_held` reports if the game rewrites
  the coefficient (**persistence unproven** - see investigation brief Q4).
- **N8 fuel pressure (id 11):** pressure 100 -> 0 over 120-300 s; ignition
  cut chance per tick = (1 - p) x revs x 0.6, only on positive torque
  (coasting clean), cut length grows as pressure falls; < 8% -> engine off
  with **ReasonCode 9 (fuel)**. HUD FUEL PRESSURE < 70%.
- Auto mix: valve 15 / oil pressure 15 / oil leak 15 / cooling 15 / fuel
  pressure 10 / blow-by 10 / exhaust 8 / blow up 12.
- Engine ABI 9 -> 10 (276 -> 304 B). Simple panel engine card: 3 columns,
  9 failures incl. RADIATOR BLOCKED / WATER LEAK / FUEL PRESSURE, water temp
  in the status line (`trials/realism_2026-09-28/panel_v5.png`). Tests OK.
- Not live-validated.
- Next: `docs/VANILLA_FEATURES_INVESTIGATION_BRIEF_2026-09-28.md` - brief for
  a cheaper LLM (12 questions: sounds, dash lights, native rev limiter,
  radiator persistence, heat power loss, fuel leak, commentary, throttle,
  FFB, pit repairs, retirement text, open scan).

## 11. Corrections from the worker result (2026-09-28)

Source: `docs/VANILLA_FEATURES_INVESTIGATION_RESULT_2026-09-28.md` (worker,
static). Spot-checked here:

- **N7 bug (source-verified, fixed):** `SpringDamperCompute_0043bbd0.c:6`
  rewrites this+7580 every physics call as `setting x this[774] + this[773]`,
  so the installed `51f24138…` build's airflow scaling was overwritten each
  tick (only the base this+7572 held). Fix: scale the load-time sources
  `this[773]` `0x7C8C50` and `this[774]` `0x7C8C58` (+ base as before);
  saved/restored the same way. Built + installed `e300efb9…` (= game copy).
  Before-state: `workspace_organization_audit/COOLING_FIX_BEFORE_2026-09-28/`.
- **Retraction (§10):** "above the oil limit / 100 C water the game itself
  cuts power" is wrong - per the worker, this+7540 feeds the oil-pressure
  gauge, this+7588 has no reader, and neither reaches the torque chain
  (~0.1-0.5% at +10/+20 K). Cooling and oil failures still kill through the
  vanilla LIFE burn (oil past its limit); any felt power loss must be ours.
- **Rev limiter (source-verified):** set only at load, `sub_43C250_0043c250.c:
  488-492` (this+7388 limit; cut band this+7396 = limit - v55, this+7404 =
  limit + v55). Writing 7388 alone is cosmetic. Candidate upgrade for N1:
  lower the band (7396/7404) instead of our ignition flicker - not done.

## 12. Owner live test (2026-09-28): oil leak v2 + radiator blocked

- **Oil leak v2 (fast, 0.85 %/s):** log `oil 94.7 C, limit 104.7 C` ->
  `oil 104.7 C past limit 104.7 C` -> `oil gone at 104.7 C - bearings
  seized`. The oil sat exactly at the car's limit - INFERENCE: the vanilla
  integrator holds oil at/near LifetimeOilTemp (the sin() term in the heat
  input), so our added heat mostly feeds the life burn, not the gauge. Death
  path works; driver-agency feel unconfirmed.
- **Radiator blocked:** `radiator 33% (base 0.003793, air 0.0006011 = setting
  x 0.0001022 + 0.0002945), water 87.7 C`; owner: radiator stayed 32%
  (expected - debris is constant), **water fell to 82 C** - the stock
  radiator has enough margin that 33% still cools.
- Fix (built `1937eaff…`, NOT installed - game running): debris 10-25%;
  heat injected straight into vanilla water temp `0x7C91B4`:
  `1.5 K/s x (1 - eff) x (0.3 + 0.7 rev)` while running (same integrator
  pattern as the live-proven oil write; water write itself not yet
  live-proven). Before-state: `workspace_organization_audit/COOLING_HEAT_BEFORE_2026-09-28/`.

## 13. Owner live test: FUEL PRESSURE had no effect before 0% (2026-09-28)

- Owner: "took the engine out when it went to 0%, no effect before 0% -
  didn't do the less-than-1-litre throttle cutting".
- INFERENCE: the per-tick cut chance was gated by `ENG_TORQUE > 0`
  (0x7C7430); worker result #1 says +8 is the *filtered* torque - the gate
  apparently never passed, so cuts never fired. Not measured live.
- Rework (built `9106cab3…`, not installed - game running): below 85%
  pressure a stress floor `0.55 x lack^0.7` gives the game's own rev-phased
  torque flutter (the same channel as vanilla low-fuel starvation feel);
  full cut-outs by revs only, `(1-p) x rev x 0.25` per 50 ms tick; torque
  gate removed; stress cleared on repair/clear. Before-state:
  `workspace_organization_audit/FUELP_FIX_BEFORE_2026-09-28/`.

## 14. Owner live test: radiator blocked v2 (2026-09-28)

Installed build `8e16d381…` (owner ran install; includes water heat
injection + fuel pressure rework). Live monitor `trials/realism_2026-09-28/
rad_monitor.py` (read-only, 2 Hz) -> `rad_monitor.csv`:
- Debris at 10-11%, radiator setting 1 (owner). Water 27 -> 113 C in 80 s
  and **141 C** at 153 s; oil 64 -> **133 C** (limit 104.7). Engine still
  running - debris had no cooked-engine end (only water leak did).
- this+7580 read 0.0002945 (= unscaled k0) throughout; setting field
  `0x7C9040` read 0 -> with radiator 1 the airflow term is k0 only, and 7580
  was NOT recomputed from our scaled k0 during driving. INFERENCE: the
  worker's "rewritten every physics call" did not hold in this session;
  SpringDamperCompute may run only on setup changes. The heating came from
  the base-coefficient scaling + direct water heat injection.
- rpm field `0x7C90C4` read a constant 441 for 153 s - either the car idled
  or the field is not live engine speed. The "idle" field `0x7C90EC` (1363)
  is what repair lifts rpm to - probably not idle. Rev-based features
  therefore act between ~1363 and the limit (1895). OPEN: confirm rpm
  units/liveness while revving.
- Fix built `7818f255…`, not installed (game running): the 125 C /
  10 s cooked-engine end now applies to both debris and water leak.

## 15. Live snapshot after the overheating death (2026-09-28, read-only `rad_snap.py`)

Owner: radiator setting 1, "the engine blew up due to overheating".
water 152.8 C, oil 161.7 C (limit 104.7), life 0, stress 0, ReasonCode 1,
running byte 1, rpm field 1362.64, pit 0. Radiator: base this+7572
0.003793, airflow this+7580 0.0002945, sources k0 0.0002945 / k1 0.0001022
(all back to original values), setting field `0x7C9040` = 0.

- FACT (read): with radiator setting 1 the setting value is 0, so
  this+7580 = k0 only (0.0002945); at the earlier default the arm log showed
  0.0006011 = 3 x k1 + k0. The radiator setting therefore only moves the
  airflow coefficient 0.00029 -> ~0.0009, scaled by airflow (~0.2 x speed),
  while the base coefficient 0.0038 is always on.
- The death was the **vanilla** life burn (life 0, RC 1): oil 57 K past
  its limit. Build `8e16d381…` had no debris cook-out, so vanilla did it.
- rpm field read 1362.64 with a dead engine (and 441 while running earlier):
  the field is not a plain live rpm. OPEN - identify the true live engine
  speed before trusting rev-based features (valve ceiling, oil pressure,
  blow-by, fuel pressure, over-rev).

## 16. Live rpm field found; all rev-based failures were reading a constant (2026-09-28)

Read-only `trials/realism_2026-09-28/rpm_hunt.py` (4 Hz, 60 s, owner
driving idle -> flat out through gears 1-6): **veh+0x39A0 float** (mirrored
at veh+0xA0 float) = 0 engine off, 372 idle, 735 in 1st at 16 m/s, 1747 at
75 m/s in 5th - same unit as the limit this+7388 (1895). **0x7C90C4 read a
frozen 441.1 the whole run** (FACT) - it is not live engine speed.
Consequence: every rev-dependent feature in builds up to `d681e4c0…`
(valve ceiling, oil pressure drain, oil leak heat, blow-by, fuel pressure
cuts, over-rev, water heat) used a constant. Fixed: `rev_frac()` and the
oil/over-rev ratios read veh+0x39A0 (idle fraction 0.20). Built
`a1c3b968…`, not installed (game running).

## 17. Owner live round 3 + fixes (2026-09-28)

Owner results: valve stutter works but the ignition flicker flashed the dash
LEDs; oil pressure sat at 32%; oil leak works; radiator heated then cleared
(asked for explicit BLOCKED / CLEARED messages, clear on pit service); water
leak not tested; fuel pressure should use the native low-fuel cutting, and
ignition flicker should go everywhere except electronics; blow-by works;
exhaust spat flames but no HUD - wants a message and a sound. Also: failures
did not pause with the game (ESC/P) nor speed up with CTRL+T; nothing must
carry over between stints; progression should follow how hard the engine is
worked.

Built (patch `trials/realism_2026-09-28/patch_round3.py`; before-state
`workspace_organization_audit/ROUND3_BEFORE_2026-09-28/`):
- **Game time:** all engine failure pumps, detection delays and the blow-up
  timeline use the session clock (`dbl_8AFEA0`, ms); electronics symptom
  timers and the gearbox phase/notice timers too. Pause = frozen, CTRL+T =
  faster. HUD fade and radio retry stay on wall clock.
- **Load-driven:** valve bleed, oil drain, exhaust countdown, water-leak
  decline and fuel-pressure decline x `0.1 + 0.9 x revs` (live rpm
  veh+0x39A0; throttle field not proven - probe `thr_probe.py` caught idle
  only). Oil pressure now keeps sinking below its plateau with revs (floor 5%).
- **No ignition flicker outside electronics:** valve float and fuel
  pressure use the game's own rev-phased torque flutter (stress 0.6 over the
  valve ceiling; fuel pressure 0.6 x lack^0.6 from 95%). The native
  "< 1 L" multiplier (`sub_447B10`) is driven by the real fuel level - using
  it would mean faking an empty tank on the gauge, so it was not used.
- **Radiator:** RADIATOR BLOCKED (engineer, 8-15 s after WATER TEMP),
  RADIATOR CLEARED on random clear (6%/10 s above 50 m/s) or pit service.
- **Exhaust crack:** EXHAUST TEMP 20-40 s in (EGT sensor); each flame spit
  plays one of the game's own `Audio\Vehicle\backfire_ext_{2,5,6}.wav` via
  winmm PlaySound (path resolved two folders above the DLL).
- **No carry-over:** engine `stint_pump()`, electronics live gate and gearbox
  `stint_over()` clear every running failure on session left, race
  retirement (FinishState >= 2) or pits in a non-race session (same rule as
  suspension auto-reset). Borrowed values (radiator, flames descriptor,
  banks) restored; the engine state itself is left to the game.
Built OK, tests OK. Not live-validated.

## 18. Radiator pit advice + status (2026-09-28)

- Owner: after RADIATOR BLOCKED, the engineer should call the car in -
  "PIT TO CLEAR DEBRIS" + PWS_PIT01. `full_event_dump.json`: PWS_PIT01 =
  **eid 49** (score 47, sdt 48, cooldown 1.0, unblocked) - "pit this lap".
  Not yet isolation-tested live; the first in-game play is the check.
- Built: 3 s after RADIATOR BLOCKED first shows -> eid 49 once + HUD PIT TO
  CLEAR DEBRIS for 8 s (game time). Debris can still blow clear (RADIATOR
  CLEARED); pit service clears it. Not installed (game running).
- Owner: exhaust crack "worked nicely". EXHAUST TEMP realism: V10s carried
  exhaust gas temperature sensors per bank (used for mapping), so the
  *engineer* spotting it on telemetry is plausible; it is a diagnosis, not a
  driver dash alarm (INFERENCE, general knowledge - no primary source cited).

## 19. Native low-fuel throttle cut for fuel pressure + valve float (2026-09-28)

Owner: fuel pressure lowered top speed (stress flutter); wants ONLY the
game's own "< 1 L" throttle-cut stutter, and likely the same for valve
float. Radiator must be removed by a pit service.

- FACT (source + disassembly of the shipped exe): `sub_447B10` returns 0 if
  ignition off, 1.0 if fuel `this+412` >= 0.9, 0 if <= 0, else
  `TrigTableLookup_00447ab0(this) x 1.5 + fuel + 0.1` capped at 1.0;
  `Gearbox_00443b70` multiplies torque by it every physics tick. Prologue
  `8A 81 59 1C 00 00` = `mov al,[ecx+1C59h]` (6 bytes, position independent).
- Built: detour at 0x447B10 (trampoline, prologue verified before patching,
  restored on unload). While active, the hook returns
  `min(original, same formula with a VIRTUAL fuel)` - real fuel and gauge
  untouched. Fuel pressure: virtual fuel = 0.9 x pressure%; stress removed
  (no top-speed cap). Valve float: virtual fuel 0.15 while above the rev
  ceiling; stress removed. Both cleared on repair/stint end/unload.
- Radiator debris now clears on pittingStatus >= 3 (box or exiting) in any
  session; stint end also shows RADIATOR CLEARED.
- Owner: water leak "works just fine".
- Not live-validated. The hook is new game-thread code: first test should
  watch for any stutter while NO failure is active (must be none).
Before-state: `workspace_organization_audit/ROUND3_BEFORE_2026-09-28/engine_failure.c.before_native_fuelcut`.

## 20. Owner live validation (2026-09-28)

Installed build `49467df9a3de726e…` (owner installed; = dev build). Owner: "everything
appears to work" - clean lap (native fuel-cut detour idle), fuel pressure,
valve air leak, radiator blocked + pit clear. Water leak validated earlier
(§19). Human validation PASS for these engine types. Still untested live:
over-rev damage, oil pressure (new sink), blow-by / oil leak since the
game-time + load changes, no-carry-over on retire/leave.

## 21. Worker #1 levers built (2026-09-29)

Patch `trials/realism_2026-09-28/patch_worker1_levers.py` (+ rev-band
restore on unload). Before-state: `workspace_organization_audit/WORKER1_LEVERS_BEFORE_2026-09-29/`.
- **Fuel leak** (auto id 12, ABI 11 = 316 B, `cmd_fuelleak`/`fuelleak_on`/
  `fuel_now`): extra drain on vanilla fuel `0x7C75C4` 0.08-0.20/s x
  (0.6 + 0.4 load); gauge, low-fuel cut and RC 9 native. FUEL LEAK 20-40 s,
  SAVE FUEL, below 8: eid 49 + PIT FOR FUEL. Auto mix: fuel pressure 6 /
  fuel leak 6 (from fuel pressure 10).
- **Valve air leak -> native rev limiter** (worker Q3): below 70% air the
  limit this+7388 (0x7C9104) and its cut band this+7396/7404 (0x7C910C/14),
  plus this+7316 shift point (0x7C90B4), are scaled to 0.20 + 0.80 x ceiling
  of stock; restored on air >= 70, clear, stint end, unload. Living on the
  new limiter (within 3%) for 2.5 s accumulated -> blow up; the native-cut
  stutter stays as valve-float flavour while on the limiter.
- **Pit repair time** (worker Q10): once the game has set the stop end
  (veh+0x3470 > now), add 3-5 s (valve air top-up) or 4-6 s (radiator
  clear) as one absolute write.
- Throttle failure NOT built: a stuck-open throttle needs a game-thread input
  override (worker Q8); a worker-thread write would flicker against the pedal.
Built OK, tests OK, not installed yet.

## 22. Fuel leak advice from measured burn (2026-09-29)

Owner: SAVE FUEL shown with 61 L on board, 2x fuel consumption - the advice
ignored real need. It was unconditional (any fuel >= 8) and the pit call used
a fixed 8. Now (engine ABI 11 grows to 320 B, `fuel_laps_x10`): burn rate =
EMA of the measured fuel drop (vanilla consumption incl. the game's fuel
multiplier `FuelMult` + the leak; refuels skipped), lap time = measured,
laps the fuel lasts = fuel / burn / lap time. Race: short by <= 15% ->
SAVE FUEL; more -> eid 49 + PIT FOR FUEL; any session < 1.8 laps -> pit
call. No advice before one timed lap. Panel shows "lasts X laps".
Before-state: `workspace_organization_audit/FUEL_ESTIMATE_BEFORE_2026-09-29/`.
