# Pit failure probability review — 2026-09-21

Scope: brief code review, cheap-fix candidates, real-F1 mechanism references and
proposed randomization. No source, configuration, binaries or runtime changed.
Product remains development prototype, implement stage; runtime adaptation pending.

## Verified source findings

src/pit_commentary.c: load_cfg (354 onward), pick_player (543 onward), pick_ai
(586 onward), stop-entry roll (1136 onward), recovery (1462 onward).
Player single per-mille cumulative draw: slow corner 4%, wheel gun 2.5%, fuel 2%,
stall 1.5%, cross-thread 1%, terminal jam 0.4%; total 11.4% per detected box entry.
Corner weights: FL/FR/RL/RR 28/28/22/22. Slow FL alone therefore 1.12% per stop.
Fuel SLOW/NOFUEL/CUT/SPILL are equally weighted (each 0.5% per player stop).
AI uses separate hard-coded sequential rolls: 3% stall, then 6% fuel, four 4%
wheel rolls, then 8% generic; effective total 1-.97*.94*.96^4*.92 = 28.752054%.
These are selector probabilities, not validated real-game occurrence rates.
Player two-stop chance = 21.5004%. Direct terminal jam over 40 stops = 14.81296%,
excluding additional cross-thread escalation. Escalation checks recur after two
failed recovery attempts; jam_terminal is not the full retirement probability.

Cheap fixes to implement next: unify player/AI configuration and selector;
validate nonnegative bounded weights, total probability, finite ordered duration
triplets and recovery values; support finer resolution than current 0.1% steps;
separate seeded failure RNG from cosmetic/audio random draws and log seed/outcomes;
replace equal fuel subtype weights with explicit configurable weights.
Recovery percent zero can cause endless retries; validate and design a bounded
recovery outcome rather than an unbounded retry loop.
Eligibility is missing from picker inputs: roll once per actual eligible service,
only tyre faults for tyre work and fuel faults when refuelling is enabled/requested.
Do not reroll an ineligible fault until something happens: retain clean probability.
Session lifecycle/global fuel-rate ownership are larger runtime work, not cheap fixes.

## Proposed calibration (design, NOT measured historical rates)

Per eligible serviced stop, common player/AI model:
slow corner 0.60%, recoverable wheel gun 0.20%, recoverable fuel rig 0.30%,
stall/restart delay 0.08%, prolonged recoverable cross-thread 0.04%, terminal
wheel/hub damage 0.01%. Total 1.23%; clean 98.77%.
The terminal budget must include escalation; do not add another unbudgeted DNF
roll. Select cause and severity once, then let retries realize the selected outcome.
For an illustrative 22-car, two-stop race (44 independent eligible stops):
P(any mod incident) = 1-.9877^44 = 41.99%; player with two stops = 2.4449%.
No compulsory race incident or hard one-per-race cap. Clustering remains possible.
Use >=10000 buckets for 0.01% resolution. Start equal corner weights unless data
justify bias. Front-left is a location, not a separate root failure mechanism.
Suggested fuel conditional weights: slow flow 70%, temporary no-flow 20%, cut 9%,
spill 1%; provisional only, with spill disabled until its implementation is reviewed.
If disabled, leave its probability clean rather than redistribute silently.

Model actual added delay against concurrent service completion: a wheel job that
still finishes before normal refuelling should not automatically add its whole
fault duration to total stop time. Avoid double-counting the game's native variation.
Validate offline using fixed seeds and large simulated stop counts, including
total and per-cause frequency, eligibility, severity/DNF totals and retry limits;
then guarded-weekend behavior and owner calibration. Those tests are not run here.

## Real-world context and evidence limits

Refuelling-era fuel delivery faults belong in a 1999-2002 model; modern examples
help explain wheel mechanisms but cannot supply that era's incident frequencies.
Mechanical difficulty does not imply retirement: a cross-threaded nut delayed
Bottas 52.4 seconds in Bahrain 2024 and was recovered. A damaged nut prevented
wheel removal and retired him at Monaco 2021. Teams/equipment can show repeated
problems, so later team modifiers may be appropriate after baseline validation.
No comprehensive 1999-2002 per-stop fault dataset was established in this review.

Sources:
- https://www.formula1.com/en/latest/article/we-need-to-make-sure-it-doesnt-happen-again-bottas-reflects-on-52-second-pit.4hMNECVKLfPDeLZ0QhdhAE
- https://www.formula1.com/en/latest/article/mercedes-reveal-bottas-wheel-is-still-stuck-on-his-car-and-can-only-be.4MzMmDArL1l4WsMqZm4A7X
- https://www.formula1.com/en/latest/article/its-a-concern-bottas-and-zhou-hope-kick-sauber-can-fix-critical-issue-after.52xXSXQ0tdS391IChLye5O
- https://www.formula1.com/en/latest/article/deciding-the-grid-a-history-of-f1-qualifying-formats.1oh1hemlnZ4x9rt2nff8vn

## Verification, restoration and documentation gate

Source inspection and analytic probability calculations performed; no gameplay
or statistical runtime validation claimed. Eight baseline workspace-validator
errors remain unchanged. README indexes this report. Only new report and README
link written. README before-state: workspace_organization_audit/
PITSTOP_PROBABILITY_REVIEW_BEFORE_2026-09-21/README.md; restore to component README
after preserving then-current state. Retain all artifacts. Governance status unchanged.
Wasted effort: combined output truncation required narrower source reads; no broad
builds/tests, failed commands or polling waits. Pending: implement reviewed fixes,
offline statistical verification, SDK/runtime adaptation and human validation.
