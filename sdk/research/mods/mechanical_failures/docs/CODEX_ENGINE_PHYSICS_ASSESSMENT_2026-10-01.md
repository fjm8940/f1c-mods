# Engine simulation physics assessment - 2026-10-01

## Judgment

The engine model is a physics-informed gameplay failure model.
Several relationships have the right form, but their coefficients and
couplings are largely calibrated to desired gameplay and the native life clock.
It is not a validated engineering predictor of temperature, pressure, wear,
failure probability or time to failure. No defensible accuracy percentage exists.

Broad failure coverage is already substantial. Improving the physical meaning,
calibration and driving response of existing mechanisms is more valuable than
adding many new failure labels.

## Scope and evidence

Owner request: assess simulation quality, closeness to physical laws, accuracy,
and worthwhile missing mechanisms. This is assessment, not implementation.
Product: mechanical_failures development candidate; investigation of physical
fidelity alongside pending verification and human validation. Portfolio stage
remains investigate; no governance status transition is warranted.

Reviewed model SHA-256:
`c7f4ee7bcf4155570ca723bbb2b236456a8b215eca98972e3320b98f3ab89242`.
It is unchanged from the preceding independent review. Source references
below are to this header unless explicitly named engine_failure.c.
The DLL integration has concurrent hardening work; this assessment does not
accept or reject a newly built DLL on the strength of the old tests.

FACT means source or directly calculated evidence. DERIVED means a consequence
of that evidence. INFERENCE denotes an unmeasured engineering/gameplay judgment.
Recommendations are proposals, not claims about an implemented feature.
No game launch, live capture, build or game memory write was performed here.

Cold start: workspace/governance/component entry points consulted; SDK startup
freshness not stale; prior findings queried for pickup/physics/lean/pneumatic;
EngineStressSimulation inspected through the SDK front door. Native code
references remain static evidence, not empirical engine validation.
The initial workspace validator passed, 0 errors and 0 warnings.

## What 'accurate' would mean

Four different questions must not be conflated:

| Property | Present assessment |
|---|---|
| Correct implementation of the chosen equations | Substantially tested, with defects documented in the preceding review |
| Correct direction of response to RPM, temperature, load and fluid loss | Often plausible; important exceptions and crude thresholds remain |
| Correct absolute temperatures, pressures, rates and damage thresholds | Not established by measured engine data |
| Correct real-world failure frequency and time | Not established; native life budget and chosen random distributions dominate |

The earlier 67,306 model checks and 145-file sweep establish behavior against
synthetic scenarios and game configuration inputs. They are not comparisons
with dyno, bearing, coolant, pneumatic-system or historical reliability data.
An equation can be numerically correct yet represent the wrong physical system.

Architecture matters: `clock_drive = drain_ema / life_at_onset` (1675) makes
many leak/wear rates depend on remaining game-life budget. A real leak of fixed
geometry under fixed pressure does not know the engine's game-life timer.
This is intentional scheduling of a failure narrative, not first-principles
prediction. Early/impact/over-rev exceptions do not change that distinction.

## Physical fidelity by mechanism

| Mechanism | Credible physical basis | Current limitation | Assessment |
|---|---|---|---|
| Thermal response (1545-1573) | First-order response can represent lumped thermal inertia | Fixed offsets/time constants and load/RPM targets; component nodes do not exchange a conserved heat budget | Plausible trends, unvalidated temperatures |
| Pneumatic valve return (1685-1714) | Gas pressure proportional to mass and absolute temperature for fixed volume; available force vs inertial demand gives a square-root speed relationship | Normalized reservoir/pressure, bay-temperature proxy, no cam/valve geometry, regulator or measured leakage | Reasonable simplified mechanism; ceiling/damage time unvalidated |
| Oil viscosity and film (618, 1725-1762) | Hot oil thins; viscosity, speed and load matter to hydrodynamic lubrication | Fictional effective film temperature and pressure-based film scalar; no clearance, journal geometry, supply flow or aeration state | Weakest physical interpretation among the core mechanisms |
| Oil/coolant leakage (1751, 1830) | Square-root pressure dependence resembles an orifice approximation | Areas are normalized severity variables, not measured holes; coolant/oil flow also scaled by life budget | Useful gameplay flow, not predicted litres/s |
| Coolant pressure/boiling (626, 1815-1858) | Saturation pressure rises with temperature; pressure changes boiling behavior | Saturation is not the whole circuit pressure; no explicit headspace, cap vent mass, latent heat or changing heat capacity | Partial physics; important missing coupling |
| Rod fatigue (1631-1640) | Inertial loading grows with RPM squared; accumulating cycle damage is sensible | Relative stress and assumed endurance/exponent/cycle limits; no engine-specific stress history/S-N data | Fatigue-shaped gameplay proxy |
| Exhaust cracking (1914-1947) | Paris-style crack growth and thermal cycling have genuine physical analogues | Temperature swing substitutes for stress intensity without geometry/material validation; rear damage uses a generic heat integral | Qualitative progression; absolute cracking/rear-break timing unvalidated |
| Fuel pressure/lean running (1964-1999) | Demand exceeding supply can cause power loss and misfire | No air/fuel ratio, rail pressure/injector regulation or mixture-dependent combustion; monotonic heat penalty | Useful supply-shortfall effect; not combustion simulation |
| Over-rev death (1615-1627) | Excess valve/rod inertial demand can cause catastrophic damage | Torque-table zero is used as a mechanical valve/piston limit | Convenient game threshold with no demonstrated mechanical equivalence |
| Quality, grade, hazards and fairness | Statistical/gameplay controls | Triangular quality, bathtub hazard, stock-lifetime-derived grade and fairness floor lack engine population calibration | Explicit design choices, not physical laws |

### Thermal model

For a physically interpretable lumped node, use an energy balance such as
`C dT/dt = heat_in - heat_out`, with thermal capacity C in J/K and heat flows
in watts. A first-order lag follows under restricted assumptions, so using
lags is not itself an error. The current component targets have not been shown
to arise from such balances. There are no established error bars for the
3/5/8-second time constants or universal material-limit labels.
[MIT engine heat-transfer lectures](https://ocw.mit.edu/courses/2-61-internal-combustion-engines-spring-2017/resources/mit2_61s17_lec18/).

F1C already supplies the main oil/water thermal simulation; retain its useful
baseline rather than duplicate the entire engine. Additional damage nodes
should couple consistently to it. Engine-off behavior drives every added node
toward ambient, omitting heat transfer from hotter retained parts. A limited
heat-soak extension would make slow laps, pit stops and hot restarts meaningful.

### The lubrication calibration is not a physical temperature

FACT: MF_FILM_TEMP_GAIN = 300/35 (95). With oil at 380 K and a displayed
bearing at 415 K, viscosity is evaluated at 680 K, not 415 K.
Direct evaluation of the current Vogel expression gives:

| Temperature used | Relative viscosity |
|---|---|
| 380 K, oil reference | 1.0000 |
| 415 K, displayed bearing example | 0.33781 |
| 680 K, effective calibration temperature | 0.015504 |

The effective calculation is about 22 times thinner than the same expression
at the displayed bearing temperature. The source explicitly says this preserves
the previous 300 K-rise calibration. It must not be presented as measured
oil-film temperature. Replacing the gain alone would substantially change
failures; recalibrate the film/wear model as one change.

Supply pressure is not identical to hydrodynamic load-carrying film pressure.
A compact geometry-calibrated viscosity/speed/load proxy, with a separate oil
supply/aeration factor, would have a clearer meaning. Full Reynolds-equation
or bearing finite-element simulation is unnecessary for this gameplay goal.
High-speed oil aeration is a measured engine phenomenon, supporting a separate
supply-quality mechanism rather than treating all starvation as hot viscosity.
[SAE oil-aeration research, abstract](https://saemobilus.sae.org/papers/oil-aeration-high-speed-combustion-engines-940792).

FACT: the inputs are in g, and the DLL divides native acceleration by 9.81.
The pickup threshold is `12 * oil_fraction` (1728). It is therefore 12 g at
full modeled oil and 3 g at one-quarter oil. This is not proof of an accidental
unit bug: the intended threshold has not been established. It is a very crude
instantaneous switch, with no direction, duration, tank geometry or recovery.
Do not change 12 to 12/9.81 merely because it looks suspicious.

### Laws used as approximations

The valve pressure/mass/temperature form is consistent with an ideal-gas
fixed-volume approximation. A real dynamic return system additionally needs
its operating geometry and force demand. The model's square-root ceiling is
a useful scaling, not a measured RPM limit.
[NASA ideal-gas relation](https://www.grc.nasa.gov/www/k-12/BGP/eqstat.html).

Antoine is an empirical water saturation-pressure relation with temperature-
dependent fit ranges, not a general circuit-pressure equation. The NIST tables
explicitly give fit intervals. The model uses Celsius/mmHg coefficients and
converts to bar; NIST's displayed coefficients use kelvin/bar, so coefficients
must not be swapped without unit conversion. Circuit pressure also depends on
expansion volume, pumping, leakage and the cap. Boiling requires an energy/mass
balance to describe evaporation, not just a pressure-dependent cooling scale.
[NIST water data](https://webbook.nist.gov/cgi/cbook.cgi?ID=C7732185&Mask=4).

Paris crack growth uses stress-intensity range and fitted material data.
The current normalized temperature swing times sqrt(crack) is a proxy for
that range. Connecting it to temperature requires geometry/thermal constraint,
and material/cycle calibration. Likewise, fatigue accumulation needs a stress
history and fatigue curve before its predicted lifetime is meaningful.
[NASA fatigue-model research](https://ntrs.nasa.gov/citations/20200003113).

Fuel supply shortfall is not a measured equivalence ratio. Combustion depends
on mixture, pressure, temperature and timing; severe deficiency can produce
partial burn/misfire rather than an ever-increasing heat penalty. Do not make
the current 'leaner = hotter' term a universal physical claim or add knock
based only on water temperature.
[MIT combustion and knock lectures](https://ocw.mit.edu/courses/2-61-internal-combustion-engines-spring-2017/35c1c5bbf304c0f41e1de0cec0b0ee85_MIT2_61S17_lec10.pdf).

## What should be improved or added

Priorities below are engineering/gameplay judgment (INFERENCE), not validated
new feature specifications. Each must have an observable effect or alter fault
selection, as the owner's simplicity rule requires.

| Priority | Work | Observable benefit | Boundary |
|---|---|---|---|
| First | Replace fictitious film temperature; recalibrate existing lubrication | Oil cooling, RPM and load affect pressure/wear consistently | Preserve native baseline; do not merely delete a multiplier |
| First | Replace torque-zero mechanical death threshold with a calibrated safe-speed/return-force criterion | Over-rev damage follows actual engine capabilities | Requires per-engine data or openly labeled conservative fallback |
| Next | Transient oil supply/aeration and fuel collector depletion/recovery | Corner/braking power or pressure dips that recover when supply returns | Respect dry-sump/collector design; g alone does not prove starvation |
| Next | Coolant inventory, pressure loss, boiling and thermal-capacity coupling | Leaks/low-speed operation cause understandable heat/steam progression | Fit native heat response first; cap/vent and latent heat can remain lumped |
| Next | Limited heat soak with reversible derating separated from permanent wear | Backing off can recover a hot engine; structural damage remains | Reuse existing temperatures and define recovery time; avoid automatic healing |
| Conditional | Retained engine mileage across stints/weekend | Practice abuse matters later; replacement restores a new engine | Needs explicit engine identity and owner replacement rules; never confuse session with engine |
| Later | Compression/cylinder contribution and richer rough-running output | Distinct ring/valve/ignition faults feel different | A single effective cylinder fraction may suffice; no need for ten full cylinders |

Transient oil/fuel supply should initially improve the *existing* fault paths,
not create frequent failures in healthy cars without engine-specific evidence.
Positive-pressure liquid leaks should generally continue when fault growth
stabilizes: freezing a hole's enlargement is different from sealing the hole.
Check the current grow multiplier on fluid loss against that distinction.

The merged electronics source already has misfire, ECU cutout and alternator/
battery symptoms. Coordinate with those before adding another generic ignition
failure. A full electrical circuit, crank-angle combustion, turbulent CFD,
3D slosh, full crack finite elements or universal turbo/ERS simulation is not
justified by the target's current observable requirements.

Detonation/pre-ignition are physically worthwhile phenomena but low priority
until mixture/ignition/fuel and control behavior have credible inputs. Adding
a random 'knock' roll would increase labels without increasing fidelity.
Exhaust heat causing rear-suspension failure should depend on vulnerable
layout/material, or remain an openly documented stylized cascade.

## Calibration and acceptance before more complexity

1. Separate measured physical parameters from gameplay knobs in the specification.
   Mark quantities normalized to the life budget as dimensionless proxies.
2. Validate native healthy-car oil/water/fuel/RPM response first. Use those
   traces to prevent duplicated heat, power or fuel effects in the add-on.
3. Obtain suitable measured curves where possible: viscosity vs temperature,
   pressure vs RPM/oil condition, coolant transient response, valve pressure vs
   safe speed, power loss vs fault severity. Record measurement uncertainty.
4. Fit on one set of curves and check different operating points/cars. Holding
   out evidence matters more than a large number of self-generated assertions.
5. Add monotonic/sensitivity and balance checks: leak flow responds to pressure,
   fluid cannot be created, thermal exchange does not create energy, lowering
   load recovers reversible heat but not structural health, and changing only a
   gameplay life budget does not silently change a claimed physical hole size.
6. Report errors separately for temperature, pressure, flow, power and timing.
   Until such data exists, label absolute accuracy unknown and validate gameplay
   usefulness with the owner. Do not invent a realism score or percentage.

FACT arithmetic: a 3% RPM reduction gives a 5.91% decrease in an RPM-squared
inertial term (`0.97^2 = 0.9409`). The additional save growth x0.6 is a separate
gameplay benefit; it does not follow directly from that inertial law. Very
nonlinear damage may amplify small RPM changes, but the exact factor needs
its own calibration. Preserve that distinction in descriptions.

## Documentation, limitations and restoration

This assessment is indexed from the preceding independent review, which is
already linked by the current handoff/review prompt. New local evidence is
also appended to the SDK findings journal with source and evidence tiers.
The previous review's complete before-state is preserved under the named
workspace audit batch before adding its follow-up link.

No implementation, binary, game file or governance product state was changed.
Pending validation: measured physical calibration, native integration and human
gameplay usefulness. Literature sources establish mechanisms, not this model's
accuracy for any particular historical F1 engine. SAE conclusions here use the
accessible abstract, not an unread paywalled full paper. A journal-bearing PDF
fetch timed out; its contents were not used as verified evidence.

Wasted-effort check: initial combined orientation reads were too broad and
truncated; targeted source sections followed. No broad builds/tests or idle
polling. One validator invocation used the SDK working directory and failed
to find the root-relative script; rerunning at the root passed. Some external
page fetches failed; no repeated retry loop or inference from missing content.

Documentation gate: canonical dated record, discoverable follow-up index,
explicit FACT/DERIVED/INFERENCE, stable model hash, limitations and pending
validation named. UTF-8 without BOM and edited lines must be checked at close.
