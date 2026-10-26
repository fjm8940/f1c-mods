# Gilles Villeneuve 2002 Sheets import repair - 2026-10-11

Product: Race Data Collector, verify/candidate; task: investigate, repair export copy, verify. No product-stage change.

Objective: resolve the reported continuation rejection without changing recorded race results. Documentation destination: this component docs record, indexed in README.

Verified: Core.gs rejects any previousCaptureId or continuationReason. Editor --rf-inspect found two linked segments, no missing links/cycles: predecessor 20260927_141747-5064-1 has zero result rows, zero observed race time, native_frame_invalid, session_restarted; final segment 20260927_141943-5064-5 has 24 rows, winner 34/34 laps, 14 finished and 10 retired. Both segments report redFlagObservedInSegment false; continuationReason is unclassified_session_restart. Inference: this is an empty prior capture linked to the completed race, not evidence of an actual red flag. Actual gameplay cause remains unknown.

Repair: existing editor engine, combined mode, restart_new_race profile; explicitly ignore empty predecessor, preserve original grid/history/retirement lists/timing, mirror classification to driverStandings. Decisions: GILLES_IMPORT_FINAL_DECISIONS_2026-10-11.json. Metadata retains source provenance and original continuation, clears import-blocking continuation fields, creates distinct capture ID. Tool automatically emits owner_reviewed_red_flag_repair and clears requiresClassificationReview; these labels are engine conventions, not a claim of human validation. No retirement reversals were made (Coulthard remains recorded retired).

Final upload file: `%USERPROFILE%\Desktop\Gilles_Villeneuve_2002_READY_TO_IMPORT\SINGLE_Gilles Villeneuve, 2002_20260927_141943-5064-5_20260927_141943.json`.
Original remains at `%USERPROFILE%\Desktop\SINGLE_Gilles Villeneuve, 2002_20260927_141943-5064-5_20260927_141943.json`.
Original SHA256: `0d70d212e32ac19469d4d33350c3ff1230b06f1111bda0f68d2c22dd179b059d`.
Final SHA256: `e45218ca5809f560baad7f6fa29f2d9bc61a658dd79009f617c7f7bf523ebafa`.
Manifest accompanies the final file; upload the race JSON only.

Verification: source hash equals manifest before-hash; full gameResults parsed equality passes; every other top-level section except metadata/driverStandings unchanged. Core.gs normalize under Node passes with 24 drivers. Top three: TesterA, Burti, Michael Schumacher, all 34 laps; native recorded times preserved. No Sheets upload performed; owner import and presentation review pending.

Failed approach preserved: Desktop/Gilles_Villeneuve_2002_import_fixed is NOT the upload copy. restart_order_plain unexpectedly rewrote all driver times to the common restart clock despite times=false. Rejected after comparison; superseded by the READY_TO_IMPORT output with full results equality. Initial tool orchestration syntax error made no changes. This engine behavior merits a separately scoped repair/test; no importer or engine source changed here.

Restoration: original untouched, use it to reproduce; README before-state in workspace_organization_audit/GILLES_IMPORT_BEFORE_2026-10-11/README.md. New decisions/report and external output/manifest only. No game files, production artifacts or credentials touched. SDK retrieval not applicable: file-format consumer logic only, no native behavior or offsets investigated.

Workspace validator before and after: two pre-existing unexpected top-level directories (.claude, plans). No structural correction attempted. Documentation gate: record exists, indexed via README, fact/inference separated, pending owner validation named. Wasted-effort check: initial broad search/read produced excess output; subsequent reads narrowed. One rejected repair caught by result equality review; no broad tests needed.
