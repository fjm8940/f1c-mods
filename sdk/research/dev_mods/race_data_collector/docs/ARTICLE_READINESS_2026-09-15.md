# Fictional encyclopedic article readiness and 0.2.2 installation

Owner clarified the goal: gathered game data supports fictional Wikipedia-style articles and predetermined script generation through the installed Season Narrative. This task installs the already-verified fixes and assesses that workflow; it does not rewrite its editorial configuration.

## Installation

Game closure verified. Exact0.2.2 DLL SHA256 `edfc4d7bbc0b3f4a037fbda80a5f0fe4de2eecb35a74792ea050d03c829e1e5e` and bridge installed. [Manifest and restoration before-states](../trials/build_20260914_224502_310/installation_20260915_082314.json). [Installed normal-GUI report test](../trials/build_20260914_224502_310/installed-control-20260915.log) passes in isolated output; no game launched or installed season state changed. Native validation remains pending. Earlier awaiting-exit notes are historical.

## Current data depth

42 top-level JSON sections retain legacy shape, but some are explicitly unavailable. Strongest factual base: track/session/distance, current field and grid, observed classification/laps/best times, raw retirement codes, sampled positions/lap crossings/pits/fuel/weather, SC/RF observations and capture provenance. Native final classification can survive an early finish/exit even when the later racing was not observed. It does not reveal unobserved battles, lap-by-lap strategy or causality.

Weak or absent: complete qualifying session progression/sectors, reliable overtake versus pit/reclassification distinction, paired collisions/blame, authoritative penalty causes, verified live tyre/fuel semantics, full RF cross-session reconstruction, directly sourced championship identity/round, and mod telemetry publisher identity for older mappings. Compound indices may not yield meaningful named tyres without definitions. Paddock politics, interviews and fictional career consequences are authored narrative inputs, not memory data.

## Actual consumer reviewed

Installed path: Documents/F1 Challenge V10/Custom/2001/Season Narrative. stats_writer.py write_all builds race stats, driver-evaluation prompt, race-report prompt and power-ranking prompt, saves season_state, and uses SINGLE championship ordering/continuity. docs/SINGLE_CHAMPIONSHIP_REFERENCE.md confirms the manual championship registry. Existing narrative_config.json defines team lineups, test/replacement drivers, era context, scoring/pressure, word counts, press publications, paddock events and report/debrief structures.

Actual race-report configuration is a1,800–2,200-word journalistic feature with atmosphere,5–7 key moments,3–5 fictional driver quotes, title fight, team assessments and paddock notebook. It is not an encyclopedic article template. Templates can request more chronological specificity than a short/incomplete native capture supports. Current native evidence is injected through narrative_context, whose report heading describes that block as manual roleplay not visible in game data; this conflates source categories even though the bridge inserts its own explicit evidence labels. Follow-up should give machine evidence a separate prompt section.

## Recommended bounded next development

1. A canonical race dossier with separate observed facts, deterministic calculations, approved fictional canon, and unknown/missing facts. Persist stable season driver/team/race IDs; raw runtime IDs alone are not cross-season identity.
2. Deterministic tables and calculations: official/observed classification distinction, qualifying/grid, points/standings, gaps, lead changes, pit strategy summaries and SC/RF timeline with provenance. Do not equate every position swap with an on-track overtake.
3. A separate Wikipedia-style output profile: lead, background, qualifying, race, classification and championship aftermath. Use script-built tables and let the LLM write prose supported by the dossier. Keep dramatic newspaper output as another profile.
4. Explicit fiction policy: approved quotes, paddock stories and consequences may be invented within the alternate universe, but cannot change recorded outcomes or silently fill missing gameplay. Mark generated stories as proposed until accepted into persistent canon. Do not fabricate real-world source citations.
5. Output validation: names/teams, positions, times, points and chronology match the dossier; numerical tables come from code. Evidence references connect claims back to captures/events/configured canon. Incomplete capture produces a shorter result-led article rather than invented lap-by-lap detail.

Assessment: suitable foundation for result-led fictional race articles and season storytelling; insufficient for fully faithful, detailed race reconstructions from the current short/early-exit captures. More raw samples are not the sole goal: reliable event interpretation, deterministic enrichment, canonical continuity and source-aware prompts matter more than JSON size. No readiness percentage inferred from section count.

Documentation is indexed through README/status. Before-states preserved under workspace_organization_audit/RACE_DATA_ARTICLE_BEFORE_20260915_082340; validator retains ten known unrelated errors. Wasted effort: none material; focused consumer reads, necessary installation/hash checks and installed integration test; no repeated build or live launch.
