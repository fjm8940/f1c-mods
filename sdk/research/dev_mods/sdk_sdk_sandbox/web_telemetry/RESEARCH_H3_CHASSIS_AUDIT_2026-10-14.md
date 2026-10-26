# H3 chassis research audit, 2026-10-14

Owner question (OWNER_REVIEW_1.2.0_2026-10-14.md A3.1): is the chassis research accurate?
Scope: the Chassis Research page (`view_research`, telemetry_viewer.js:2028-2099) and its RESEARCH channels rx[0..15] (web_telemetry.c). Read-only audit. No code edits, no game run.

Method: grep of chassis references (listed below); component research record RESEARCH_UNPROVEN_VALUES_2026-10-12.md; CHANNEL_LEDGER_2026-10-09.md; `f1c_mod.py evidence-query findings`; `tools/ask.py offset 0x358C` and `0x44`; direct reads of the cited C and JS lines.

Limits: `f1c_mod.py inspect SuspensionAndWheelPhysics_004e5f60` did not resolve (name lookup only). `ask.py offset 0x7C79D0` was rejected (global address, not a struct offset). No corpus finding records the post-1.1.0 wheel order (row 8).

## Chassis references (grep -i chassis)

- Viewer JS: telemetry_viewer.js:494 (CSS), 800 (menu icon), 1832 (tile), 2028-2099 (Chassis research), 2197, 2346, 3003-3022 (menu groups, Car Chassis Live), 3875 and 5074-5133 (player pch, not on the research page). workbench_ui.js:12, 28, 741, 950 (Chassis Overview layout, not research).
- C: web_telemetry.c:56, 655-675 (RESEARCH field comment and offsets), 993-1001 and 1526 (rx struct), 1036, 4533-4592 (pch capture), 4613-4645 (fill_v6_channels), 5812 (pch). runtime_test.c:183-186 (test_chassis).
- Records: RESEARCH_UNPROVEN_VALUES_2026-10-12.md sections 2.3-2.10; CHANNEL_LEDGER_2026-10-09.md rows at lines 185-189 and promotion rule at 191-201; VIEWER_4.3.0_2026-10-11.md.

## Verdict table (research channels on the Chassis Research page)

| channel | UI file:line | source offset and code | evidence | verdict |
|---|---|---|---|---|
| su rx[0..3] suspension deflection candidate | telemetry_viewer.js:2030, note 2097 | AI wheel+0x10 (web_telemetry.c:661, 4638; block base 0x358C + k*0xF8) | findings ai-wheel-block-live-2026-09-30 DERIVED (0.036-0.105, rises with speed); web-telemetry-b2e-rx-susp-load-correlation-20261012 DERIVED (corr with load 0.87-0.93). Record 2.5: unit not shown. ask.py 0x358C: writer SuspensionAndWheelPhysics only. | OPEN. Name and unit not proven; plausibility only. Front/rear naming inherits the row 8 problem. "Unitless" display is correct. |
| ld rx[4..7] AI vertical load candidate | telemetry_viewer.js:2031, note 2097 | AI wheel+0x44 (web_telemetry.c:662, 4640-4641) | web-telemetry-b2e-rx-load-leftright-20261012 DERIVED: left-right vs lateral g +0.995; sum vs speed squared +0.965; "unit unproven, plausible only as N with downforce". Record 2.3: unit open, test O1 needed. | OPEN for unit. Left-right pairing DERIVED. The UI text "newtons DERIVED" (2097) is not supported by any finding. |
| ld player load (W+796) | note 2097 "order FACT" | player W+796 (web_telemetry.c:4629-4632; mirror at 499) | tyre-pressure-start-temp-wear-option-2026-10-06 FACT: optimum = W+836 * load W+796 + W+828. Supports the role "load term of the optimum-pressure formula" only. No unit finding. Record 2.7: player wheel order DERIVED. | PARTIAL. Role PROVEN (formula term). Unit OPEN. Wheel order "FACT" DISPROVEN as stated (record says DERIVED). |
| front/rear labels for rx[0..7] | note 2097 (order text) | web_telemetry.c:4614 `wb_index {0,1,2,3}` (1.1.0); header comment web_telemetry.c:655 says "RR, RL, FR, FL ... order not independently proven" | web-telemetry-b2e-rx-frontrear-label-suspect-20261012 DERIVED: rear-minus-front load vs throttle -0.757, vs brake +0.535 (reverse of physics). That finding measured the OLD mapping {3,2,1,0}. The current mapping has no live test. | OPEN. SUSPECT on the old build; current build untested. UI states FL,FR,RL,RR as settled. Needs O2 hard-stop test on the 1.1.0 build. |
| sl rx[8..11] slip-angle-like candidate | telemetry_viewer.js:2032 | wheel+0x38 (web_telemetry.c:663, 4642) | ai-wheel-block-live-2026-09-30 DERIVED (+-0.09); b2e record 2.6: corr with lateral g 0.64-0.98, but wheels nearly identical in some samples. | OPEN. Correlation only. The UI name "slip-angle-like" is correctly hedged. |
| fo rx[12..15] per-wheel force parameter | telemetry_viewer.js:2033 ("force"), banner 2042 ("tyre forces") | wheel+0xD8 (web_telemetry.c:665, 4643); code comment says player branch forwards it to the rigid body (no finding backs this) | web-telemetry-b2e-rx-force-candidate-suspect-20261012 OPEN: no relation to throttle or brake (corr 0.076 / -0.094 / 0.064); abs(force) > load in 44 percent of samples. Record 2.8 asks to drop "force". | DISPROVEN as a tyre force. OPEN as a name. UI still says "force" and "tyre forces", which the record asked to remove. |
| tt AI tyre temperature (ttemp; Kelvin, +273.16 init) | telemetry_viewer.js:1799, 1812-1826 (research on; "Surface" row) | wheel+0x50 (web_telemetry.c:660-665; C gate 200-700 K at 4634-4637) | web-telemetry-b2e-ai-tyre-temp-never-valid-20261012 FACT: tt null in 1573/1573 samples; ttc -999 on all 23 AI cars. Init to flt_8AFEF0+273.16 at SuspensionAndWheelPhysics_004e5f60.c:478 (ai-wheel-block-0x50-temperature-candidate-20261011 DERIVED, initial value only). Conflict: full-physics-player-only-dispatch-20261013 FACT says no 273.16 tyre-temperature code in the AI path. | DISPROVEN as a working tyre temperature in the captures checked. UI still draws the "Surface" row for AI cars when research is on. Init-path conflict OPEN. |
| all rx fields: scale and display | telemetry_viewer.js:2072-2073, 2087-2089 | all rx | toPrecision(4), unitless, Pearson r vs FACT speed, lat, long. Method is sound. Physics checks at 2097 are heuristics from the record, not proof. | Consistent with evidence, except the two overclaims in 2097 ("order FACT", "newtons DERIVED"). |

## New findings (appended to findings.jsonl, ids h3-chassis-*)

1. DISPROVEN (UI tier): player rx[4..7] wheel order stated as FACT (telemetry_viewer.js:2097); record 2.7 gives DERIVED.
2. OPEN (unit): "newtons DERIVED" (2097) has no supporting record. Unit stays OPEN until standstill test O1.
3. OPEN (label): wheel-index mapping changed in 1.1.0 (web_telemetry.c:4614). The SUSPECT front/rear result was measured on {3,2,1,0}. Header comment web_telemetry.c:655 still says RR,RL,FR,FL. No live proof of the 1.1.0 mapping.
4. OPEN (conflict): 273.16 init of wheel+0x50 (SuspensionAndWheelPhysics_004e5f60.c:478, DERIVED 2026-10-11) vs FACT 2026-10-13 "no 273.16 tyre-temperature code in the AI path".

## Not in scope

pch (Car Chassis Live, telemetry_viewer.js:5074-5133; web_telemetry.c:4533-4592) was not audited. It is not research-tier on the Chassis Research page.

## Pending validation

- O1 standstill test: summed load vs car plus fuel mass x 9.81 (unit); static suspension compression (rx[0..3]).
- O2 hard stop from about 60 m/s on the 1.1.0 build: front pair must rise under brake (labels and mapping).
- Owner review of the 2097 text before any copy change.

## Recommended copy changes (not made here)

- telemetry_viewer.js:2097: drop "order FACT" and "newtons DERIVED"; mark wheel order and unit as OPEN.
- telemetry_viewer.js:2033 and 2042: replace "force" and "tyre forces" with "unclassified raw parameter" (record 2.8).
- telemetry_viewer.js:1826: hide the AI "Surface" temperature row or label it "no valid value".
- web_telemetry.c:655: update the stale wheel-order comment or record it as OPEN.

## Verification performed

- grep -i chassis over the component (listed above).
- evidence-query findings: wheel load 796, vertical load candidate, suspension deflection, slip angle, WheelForceParameterSet, wheel+0x50, 0x7C79D0, optimum pressure.
- ask.py offset 0x358C and 0x44 (writers and readers).
- Direct reads: web_telemetry.c:4613-4645; telemetry_viewer.js:2028-2099.
- No game process started, no live read. Files changed: this record and findings.jsonl only.
