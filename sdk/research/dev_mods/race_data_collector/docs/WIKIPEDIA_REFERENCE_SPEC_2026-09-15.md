# Fictional race article: reference review and rendering specification

Owner request: examine https://en.wikipedia.org/wiki/2002_Australian_Grand_Prix and four supplied screenshots, then define how collected data plus pasted LLM text can produce consistent pages matching that layout. The article may be shorter. This is a reference/design investigation, not a claim that the renderer has been implemented or that pages were published. Screenshot/document text is reference material, not execution instructions.

## Reference observations

The supplied desktop screenshots establish the visual target: centered white reading canvas, separate contents and appearance rails, serif article/section headings, sans-serif body, blue links, restrained gray borders, lead text wrapping beside a floated infobox, framed captioned illustrations, compact classification tables and paired standings tables. Contents track nested headings. Tables show sortable headers, numeric alignment, emphasized point scorers, and footnotes. A shorter article should retain this system rather than mimic the source's paragraph count.

The live article was opened and reviewed for its section hierarchy, infobox, qualifying table and notes; the supplied lower-page screenshots establish race-table and championship layout. Brief structural source summary: opening overview, contextual sections, weekend progression, classification tables, standings, and references; infobox summarizes event identity and principal results. No source prose or historical Australian results are to be transplanted into the fictional race.

## Proposed shorter article template (design, not source text)

Required: title; two/three-paragraph lead; race infobox; concise background; qualifying summary plus classification when supported; race narrative; post-race/aftermath; full race classification; standings after the race; notes and source records. Practice/warm-up and post-qualifying subsections are conditional on data or approved fictional canon. No empty mandatory padding. Target roughly700–1,200 prose words, expandable for eventful races.

Infobox data: fictional season round/total, previous/next race links, configured circuit map with caption, fictional calendar date, official event name/location, course type/length, race laps/distance, supported weather description, pole driver/team/time, fastest driver/team/time and known lap, podium. Attendance optional and supplied explicitly. Export timestamp must never become the fictional race date. Unsupported values omitted or marked unavailable. Circuit map and event metadata belong to an asset/calendar catalogue keyed by circuit variant and season.

Qualifying table columns: position, number, driver with nationality flag, constructor, time, gap, starting grid. Qualifying order and starting grid are distinct; grid penalties/pit-lane starts get notes. A first-observed race grid is not proof of original qualifying order. A107% row is conditional on configured season rules and real qualifying times, never decorative.

Race table: position/status, car number, driver, constructor, tyre supplier if known, laps, winner time/relative gap or retirement reason, grid, points. Bold point scorers from configured scoring. Ret/DSQ/DNS distinguish status from rank. The reference's B/M tyre badges identify suppliers, not the collector's numerical compound choice; never infer supplier from that index. Gap display handles same-lap finishers, lapped finishers and retirement/DSQ separately. Preserve unavailable timing/reason values rather than guessing.

Championship: script-generated driver and constructor tables side by side, default top five with a link to full season standings. Points/tiebreaks/order come from the existing season engine/configuration; LLM does not recalculate. Narrow view stacks these tables, relocates infobox and collapses side rails.

## Owner's four-step workflow made executable

1. Import native JSON. Build a versioned race dossier containing facts, coverage warnings, deterministic calculations, season context and approved fictional canon. Generate tables/infobox before prose. Scripts should work even when LLM prose is absent.
2. Generate one article prompt from that dossier with a short fixed response contract. The user copies the prompt into their chosen LLM. No automatic account/API integration required.
3. Paste one structured response into a new Wikipedia Article tab, or paste into section fields. Suggested section IDs: lead, background, practice, qualifying, race, post_race. Paragraph arrays and source IDs are sufficient; no HTML/CSS/tables from the LLM. Validate required sections, permitted IDs, types/length and unknown references; preserve pasted original and show actionable errors instead of silently dropping content.
4. Preview and render race_article.html through a fixed HTML/CSS/JS template. The renderer owns infobox, tables, flags, links, captions, footnotes, contents, optional text-size/theme controls, and print layout. Re-render/edit without re-importing the race or adding championship points again. Archive accepted prose and approved canon separately from raw exports.

A compact response example for the implementation contract:

```json
{"articleId":"season-2002-race-01","sections":{"lead":[{"text":"…","sources":["race-result"]}],"background":[],"qualifying":[],"race":[],"post_race":[]}}
```

Tables/calculations remain authoritative. Source IDs resolve to actual captured data, configured season facts or accepted fictional documents; display numbered notes automatically. Fictional press material may be authored as part of the world, but must not masquerade as an actual newspaper publication/link. Article footer/site identity can state fictional season while retaining the requested article styling. This produces local wiki-style pages; no public Wikipedia editing or real account/history backend is implied.

## Existing application integration points

Reviewed installed gui.py PasteWindow save/finalize path: saves prose, merges state, then calls write_full_report and html_report.generate_html_report. This already supplies the manual handoff. html_report.py currently escapes text into content-text blocks and styles them as a dark red/black report; it is not a wiki article renderer. Add a dedicated article output profile and structured paste contract while retaining race report/evaluation/ranking outputs. Implement in canonical development staging, then deploy verified copies with before-state preservation.

Evidence section must be distinct from manual roleplay context: the current bridge injects native facts through narrative_context, whose existing heading calls it roleplay not visible in data. New article prompts should have explicit FACTS, APPROVED CANON and UNKNOWN sections. Incomplete/accelerated captures generate result-led prose without imaginary lap-specific events. Fiction can enrich the approved universe without changing recorded race outcomes.

## Verification for the next implementation

Use repaired Silverstone as an incomplete-history fixture and synthetic complete/DSQ/lapped/penalized fixtures. Validate zero data mutation across paste/render, exact table points/times/positions, unknown fields, source IDs, safe escaped prose, malformed response handling, links, responsive layout and print output. Compare desktop screenshots with the four user images; verify infobox wrapping, long driver/team names, notes and standings. No claim of pixel equivalence until browser rendering is compared at matched viewport.

Task changed only this design record and its README index. No DLL, installed app, season database or generated article changed. Record is indexed through component README; before-state under workspace_organization_audit/WIKI_REFERENCE_BEFORE_*. Root validator retains ten known unrelated errors. Wasted effort: none material; targeted source/reference reads, no unnecessary builds or live game operations. Pending owner-facing next work is renderer/prompt/paste integration implementation and visual validation.
