# Agent-driven F1 Challenge championship universe

Status: completed redesign/specification, application implementation pending. Owner's latest instruction supersedes the manual copy/paste default in NARRATIVE_WORKFLOW_2026-09-15.md. The Wikipedia visual specification remains applicable. Documentation is in the canonical collector component during consumer definition; new consumer source belongs under development/season_narrative when implemented, outside every installed game/season folder. That consumer directory has not been created by this design task.

## Purpose and acceptance

Create a coherent fictional F1 Challenge 99-02 championship universe for a selected season. Every driver and team accumulates history. Results are preserved, development is contextual and gradual, and unusual events remain unusual. A Wikipedia-style article is one output of that continuing universe.

Normal interaction: select universe/season once; after racing, select the detected completed capture and choose Process Race; review article and compact universe changes; choose Finalize. No prompt assembly, copy/paste, JSON editing or repeated season explanation. Application automatically creates a bounded agent job, provides tools/files, validates the result and renders the preview. User can ask for a prose revision in natural language without recalculating the championship.

Never silently select a different season from a track name or export timestamp. Capture identity, calendar round, qualifying/race linkage, roster and active profile must agree. Show only unresolved identity questions. Support deliberate alternate universes for the same year. A late-season starting point requires an explicit baseline or owner-authorized fresh history; never fabricate earlier rounds. Red-flag segments and restarted races need explicit logical race grouping; partial captures remain marked partial.

## Evidence for replacing the existing workflow

Read-only inspected installed Season Narrative gui.py, core/stats/writer.py and narrative_config.json on 2026-09-15. Current app splits report/evaluation/ranking/state/summary inputs. The detailed evaluation is player-centric; registry notes and grid-wide power rankings offer partial continuity.

writer.py _derive_player_status (around 604) averages up to three scores and escalates WARNING -> LAST CHANCE -> DROPPED on successive below-4 updates. Prompt around 1613 directs naming a replacement. Teammate points gaps of 8 and 15 trigger pressure/risk wording around 1645. Config sets outside-points scoring to 3-5 and a fixed P6 recovery bonus. These are verified current rules, not realism requirements. They can conflate car performance, luck and driver ability and are unsuitable as the new career decision engine. Earlier inspection also found pasted player points accepted and non-identity-based score deduplication; both must be removed from the new commit path.

## Agent workflow

1. Application reads immutable raw capture and computes a race dossier: classification, configured scoring, qualifying when available, useful race events, neutralization periods, valid teammate comparisons and data-quality limits.
2. Application reads committed universe revision and prepares context: every driver/team profile, recent assessed weekends, unresolved stories, relevant older events, current contracts when known, season regulations and calendar. Do not discard a long-running story because it is outside a rolling prompt window.
3. Agent analyzes the race and each entity using read-only dossier/context access. It can retrieve older evidence by ID rather than receiving every lap of every race. It returns structured proposed assessments and story events.
4. Application checks coverage and deterministic policy limits. Agent may correct a rejected draft using explicit validation errors, bounded to two repair attempts. On continued failure, preserve draft and show actionable error without changing canon.
5. Agent writes encyclopedic article prose against the validated proposed state. It may suggest fictional reactions where enabled; generated dialogue and beliefs retain fictional provenance.
6. Application builds tables/infobox/links/notes and verifies the complete proposal. One review screen shows the rendered article and meaningful changes grouped as drivers, teams and season. Unchanged entries are collapsible, not missing.
7. Finalize commits one revision linking the exact dossier, article, evaluations and state changes. Failures/cancellation leave the previous committed universe usable. The next round reads that committed revision.

These are sequential responsibilities, not a requirement to run one agent per driver or a costly multi-agent committee. One orchestrated agent with application validation is the initial architecture. The model never supplies final standings, changes raw results, or writes directly to authoritative state.

## Realism policy

The following are proposed simulation defaults to test with the owner, not empirically calibrated historical F1 rules.

### Separate performance, form, ability, reputation and employment

- Weekend performance is an assessment of what the driver did with the available opportunity. Result is an input, not the grade.
- Form considers multiple comparable weekends; unavailable evidence is null, never a bad score. Mechanical DNFs can leave qualifying/race pace assessable while making the final position unsuitable for judging execution.
- Underlying ability changes slowly. Avoid numerical ability updates in the initial release; retain evidence-backed development hypotheses until multiple comparable observations support them.
- Reputation and confidence may react to events, but a press story does not establish changed ability or an employment decision.
- Employment considers established team expectations, contract situation, accumulated evidence, alternatives, experience and season timing. Unknown contract terms remain unknown.

### Evaluate every driver fairly

Mandatory dimensions: qualifying relative to usable teammate evidence, comparable race pace, execution/consistency, incident responsibility when established, strategy exposure, reliability exposure and result relative to contextual expectations. Each dimension includes assessment, source IDs, confidence and missing evidence. Do not invent precise fuel/tyre-adjusted pace where the capture cannot support it. SC/RF/pit/out/in/damaged laps and accelerated finalization observations cannot be treated as ordinary comparable racing laps.

A midfield P8 may be an excellent race; a leading-car P5 may disappoint. If both teammates struggle, investigate car/team limitations before inferring two simultaneous driver declines. DNS, non-qualification, contact counters or mechanical failure alone do not establish driver fault. The player follows identical rules. Driver coverage includes absent contracted drivers with no new driving evaluation, replacements and reserves only when they exist in established roster/canon; don't invent race participation.

### Patience and career decisions

One poor weekend produces a weekend assessment. It cannot independently trigger dismissal or a formal performance ultimatum. Initial default is a four-comparable-weekend observation period before performance-only formal review, with rookie/team-change acclimatization recorded; this is a configurable conservative guard, not an automatic review after four races. Incomplete or mechanically compromised weekends cannot simply count as failures.

Concern requires sustained, attributable underperformance relative to credible expectations. Review records the reasons, management response, expected improvement and next review point. Recovery can stabilize or de-escalate concern. Repeating an old rumor does not increase its evidence. Review never automatically turns into dismissal on the next bad race. Replacement proposals require contextual justification, known/explicitly authored contract implications and a plausible available candidate. Major roster decisions are separately visible at race finalization. Injury or funding exceptions require established fictional canon/owner input; the agent must not invent them to bypass patience rules. End-of-season renewal decisions and mid-season replacements are different events.

### Team development

Every team retains car competitiveness by circuit/conditions where evidenced, reliability, operational execution, driver pairing, expectations and established development plans. Separate a technical plan from delivery and delivery from proven benefit. Plans have a target round, status and dependencies; changes cannot become immediate universal pace boosts. A good circuit or favorable strategy is not automatically an upgrade. Do not infer sponsor budgets or engineering breakthroughs from finishing position. Team progress may stall, fail or show no measurable change.

For the initial release development is narrative/assessment state. No automatic RCD, physics, reliability or AI edits. If gameplay effects are later enabled, planned, owner-applied and observed effects must remain distinct so fiction cannot claim a game change that was never applied.

### Era and alternate-history coherence

Each season profile has its own scoring/regulations, entries, calendar and start-date knowledge. Load verified/owner-provided 1999-2002 context as a starting point, without injecting real-world events that had not happened by the simulated date. Once the universe diverges, its committed history is authoritative. An actual future transfer is not inevitable. Relationships, injuries, contracts, retirements and technical projects persist until an explicit dated event changes them. Event types distinguish observed race fact, approved fiction, character belief/rumor and unresolved hypothesis. No quota of scandals, sackings, surprise transfers or upgrades per race.

## Persistent state and transaction design

Use SQLite for indexed identities, season results, driver/team histories, story timelines, jobs and committed revisions. Preserve collector JSON as immutable input; use versioned JSON for agent exchange and portable universe export. HTML is an output view. Do not convert the DLL's collector format merely to reduce prompt size; prepare compact dossiers in the consumer.

Stable universeId, seasonId, raceId, sessionId, driverId and teamId; names are labels and aliases, not primary identities. Logical race identity is distinct from the byte hash so a reformatted JSON cannot score twice. Changes to past results create a new revision and invalidate dependent standings/drafts; do not overwrite prior evidence. Replays/test captures do not silently become a championship round.

Minimum records: season profile and rules; raw source manifest/hash; qualifying/race classifications; driver assessments; team assessments; relationships; contracts when known; story events with effective round and knowledge scope; source evidence; proposed changes; commit manifest; rendered artifact hashes. Each active driver/team must receive an assessment or explicit unchanged/insufficient-evidence outcome per race.

Agent response includes jobId, raceId, baseRevision, sourceDigest, driverAssessments, teamAssessments, storyProposals and articleSections. Validate IDs, source links, state transitions, schedule consistency, contradictory relationships, all-entity coverage and forbidden computed-field edits. Prose factuality also needs semantic review; a valid JSON schema alone is not proof.

Only the application performs commit. Use a transaction for state plus prewritten immutable article/dossier artifacts; commit references them only after successful validation. Unreferenced failed artifacts remain preserved. A repeated Finalize returns the same committed revision; a stale draft is rejected. Interrupted jobs can resume from their saved stage without repeating a commit. Public article contains only appropriate public fictional knowledge; private team assessments remain in the management view.

## Integration decision

Preferred initial adapter: local Codex runner controlled by the consumer, behind a provider-neutral job interface. codex.exe was found at %USERPROFILE%/AppData/Local/Programs/OpenAI/Codex/bin/codex.exe; authentication, installed-version feature support and a real generation job were NOT tested. No account secrets were read and no model job was dispatched.

Official documentation checked 2026-09-15:
- https://learn.chatgpt.com/docs/non-interactive-mode documents codex exec, streamed JSON events, schema-constrained final output, output-to-file and reuse of saved CLI authentication.
- https://learn.chatgpt.com/docs/codex-sdk documents programmatic local agent integration and points custom interactive clients to app-server.

Use an initial read-only dossier workspace for agent access and capture its structured response through the application. Never hand an agent the whole game install as its working directory. No permanent background monitoring is needed; discover new files when the app opens/refreshes. Show progress, cancel and recoverable authentication/usage errors. Enforce configurable job budgets and bounded retry count; no recursive agent spawning. A failed provider run cannot finalize a race. Use app-server/SDK if interactive orchestration later needs it; no dependency installation required for this design phase.

## Acceptance scenarios before release

1. Three poor or unlucky races cannot automatically dismiss the player or an AI driver.
2. Two mechanical DNFs plus strong qualifying preserve uncertainty and distinguish reliability from driver execution.
3. A backmarker exceeds plausible expectations without points; assessment recognizes it without inflating standings.
4. Repeated attributable underperformance can lead to contextual review, improvement targets and eventual justified action; protective guards cannot make careers consequence-free.
5. A rookie's acclimatization and an established driver's history affect expectations without predetermined outcomes.
6. Both teammates decline at one circuit: report alternatives/uncertainty rather than assert simultaneous loss of talent.
7. Technical plan persists for several rounds; no instantaneous improvement or forgotten cancellation.
8. A fresh agent session reconstructs all unresolved stories and relevant older evidence from saved state.
9. Missing driver/team assessment, unknown ID, future-event leakage, false facts or a rumor promoted to confirmed transfer are rejected or flagged for correction.
10. Import/Finalize twice, format-only JSON changes, interruption, stale draft and article rewrite cannot duplicate points, assessments or story events.
11. Two same-year universes and their contracts/results remain isolated; selected round mismatch is visible.
12. Actual Silverstone partial history produces restrained narrative, with no fabricated unseen overtakes, crash responsibility or lap-by-lap strategy.
13. Article visual comparison against owner's screenshots and a two-race end-to-end usability review with Matias pass before claiming the app is ready.

## Change and documentation gate

This turn adds the design and README index only. Existing 2001 app, DLL, season records and gameplay files are unchanged. README before-state is preserved under workspace_organization_audit/AGENTIC_UNIVERSE_BEFORE_<timestamp>. No source move or top-level directory creation. Collector stage unchanged. Workspace validator retains ten previously recorded unrelated errors and zero warnings. Implementation/runtime integration and realism/visual tests above are pending, not simulated passes.

Wasted effort: bounded cold-start rereads; no builds or tests beyond document/structure checks; broad first official search returned model pages, corrected with direct documentation URLs; no polling or waits.
