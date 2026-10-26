# Public plain-language function guide: solution assessment

Date: 2026-10-01 (local time). Product: f1c-sdk; implement/candidate, next SDK gate verify. Task stage: investigate/specify a public guide. Status: recommendation only; no public repository or Pages settings changed.

## Objective and boundary

Matias wants everyone to understand the original game without publishing custom mod implementations, private integration tooling or commercial protection. Investigate how to add a trustworthy plain-language map to the public pseudocode corpus. The public scope is knowledge of the original game, including useful reviewed native behavior; the fact that a native mechanism helps modders is not itself a reason to withhold it. A broad dump of private notes is outside scope.

Documentation destination is this assessment, indexed from docs/README.md and findings.jsonl. The SDK stage, checkpoint, canonical paths and interfaces do not change. Implementation and further publication are a follow-up, not claimed complete here.

## Inventory: facts about files, not certifications of behavior

| Store | Current measurement | Suitability |
|---|---|---|
| Published FUNCTION_INDEX.csv | Addresses, descriptive labels, paths, hashes; no explanations | Good public identity spine; join by address |
| skeletons_filled/*.md | 4,504 files/addresses, all with nonempty summary lines | Useful draft pool; older derived explanations and full private research context require review |
| skeletons/*.md | 4,509 files, 4,504 exported addresses; 4,290 summary lines match placeholder markers | Not a publication source for descriptions; includes unfinished facts/prose templates |
| priority_index.json | 2,672 entries, 2,507 nonempty summaries | Secondary draft source; overlaps heavily with filled notes and includes private mod-relevance ranking |

The filled/priority summary address overlap is 2,131. Their union covers 4,880 exported addresses; 2,252 of the 7,132 exported functions have no summary in either store. These counts are exact for current files, not evidence that descriptions are correct. Existing summary lengths in filled notes range from 48 to 726 characters. The six decompile failures should also appear in the catalogue, for 7,138 existing catalogue entries total, while remaining visibly unavailable.

Two summary lines matched a coarse private-context term scan: one says "crack-spread" about a rendering effect and another "SDK version 220" about Direct3D8. Both are false positives for custom-mod disclosure. No conclusion about privacy of the entire full-note corpus follows from a summary-only scan.

## Reproduced reasons not to bulk-publish existing prose

- At 0x0042A110, both the filled note and priority summary say "forces AI driver to exit pits". The newer typed function is RequestPitEntry and calls PitEnterCalledState and SetCallToBox. The old descriptive entry is therefore in conflict with the current reviewed naming/type work, not a ready-made authoritative explanation.
- At 0x004A5FA0, the old filled/priority prose describes proximity collision detection/response. The typed export names the routine PitBoxAlignmentReady. The existing tooling implementation log specifically records correction of that old DERIVED name to the reviewed name. Resolve old meaning claims before publication.
- At 0x004BEB10, filled prose describes AI overtaking; the newer typed filename is SessionSimulationStep. This is another review flag, not proof that all current-name semantics have independently passed gameplay validation.
- At 0x00499EB0, the short C body writes max(a2, 0) and max(a3, 0), then returns original a3. The filled summary includes "taking the absolute/non-negative value", which conflates clamping with absolute value. For -5, the shown code writes 0, not 5. Even simple explanations need precise language. This observation compares prose with exported pseudocode, not a fresh assembly certification.

Of the 17 filenames changed between exports, 14 addresses have filled legacy notes and 15 have priority summaries. Review all 17 before reusing their descriptions; also inspect their callers/callees where purpose depends on a renamed routine. A label DERIVED does not excuse publishing a known conflicting explanation as the current explanation.

## Recommended architecture

Maintain one curated, address-based public data catalogue, then derive its browsing formats. Key identity is corpus/target snapshot plus address, not filename or guessed name. There is one entry per catalogued address, with raw and typed representations linked on that entry.

1. **FUNCTION_GUIDE.json**: the canonical approved public catalogue. It contains public-safe summaries, evidence references, aliases and explicit review status. It contains no raw private note bodies. Unknowns remain records, not missing rows.
2. **FUNCTION_GUIDE.csv**: a compact derived/downloadable map with address, current label, summary, review status, subject area and raw/typed links. Useful without any website.
3. **guide/**: a small static HTML/CSS/JavaScript reader generated from the same approved data, hosted on GitHub Pages when separately implemented/published. Search by words, address and old/new name; filter by subject and evidence status; open code directly. No backend, login, AI service, external analytics or database is needed.
4. **docs/FUNCTION_GUIDE.md**: short repository-native introduction, status totals and links to the reader, data download and selected reviewed explanations. Add deeper Markdown pages only for functions/systems that need them; do not require 7,138 near-identical handwritten pages or one giant Markdown table.

GitHub documents Pages as static hosting for repository HTML, CSS and JavaScript, available for public repositories on GitHub Free. This supports the proposed simple reader without additional infrastructure: https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages (retrieved 2026-10-01). No hosting setting or workflow was enabled during this assessment.

A simple Markdown/CSV map is the cheapest first release; the searchable reader is the best public reading experience at this scale. A wiki would introduce a second content store and manual synchronization. Bulk-uploaded skeletons preserve too much private context and stale prose. A large graph can help specialists later, but does not substitute for short readable explanations.

## Data and evidence contract

Separate four things explicitly: the observed/exported operation, its interpreted game purpose, the confidence of the descriptive name, and publication approval. A single percentage or blanket "FACT" badge cannot express those differences.

Suggested fields:

```json
{
  "corpus_id": "f1c-public-snapshot-2026-10-01",
  "address": "0x00499EB0",
  "display_name": "Clamp two fields to nonnegative values",
  "aliases": ["AbsValueFieldSetter", "sub_499EB0"],
  "summary": "Stores two numbers in an object, replacing negative inputs with zero. Returns the original second input.",
  "summary_status": "static_reviewed",
  "purpose": null,
  "purpose_status": "unknown",
  "name_status": "descriptive",
  "subject": "unclassified",
  "subject_status": "unclassified",
  "export_status": "available",
  "publication_review": "pending",
  "evidence": [
    {
      "kind": "exported_pseudocode",
      "path": "pseudocode/raw/AbsValueFieldSetter_00499eb0.c",
      "start_line": 5,
      "end_line": 8,
      "claim": "Clamps both stored values at zero and returns unmodified a3."
    }
  ],
  "limits": ["Object purpose is not established here.", "No fresh assembly or gameplay check."],
  "reviewed_on": "2026-10-01"
}
```

This is a local illustrative entry based on the read C body, not a claim that it is already approved/published. Final records should reference the source commit and exact file hash from the published index. Lines are representation-specific. Function labels are not promoted to original developer symbols.

Recommended summary statuses: unknown, inferred, static_reviewed, corroborated, runtime_observed, disputed. Evidence can be mixed, so status belongs to the specific summary/claim. Static-reviewed means compared with specified exported code; it does not silently mean checked against assembly. Corroborated names its independent evidence. Runtime-observed names the tested behavior and context; it does not verify every claim in the entry. Retain existing workspace trust tiers internally rather than automatically translating every legacy FACT label into a public badge.

For an unknown function, show "Purpose not yet established". A neutral code-operation description may be supplied when supported, but do not turn the guessed filename into a confident gameplay explanation. Missing exports use export_status unavailable and identify their recorded failure. Disputed candidates are retained privately for repair, with a public short uncertainty note only if publication review approves it.

Suggested subject areas: AI, vehicle physics, pit stops, race/session control, rendering, audio, input, networking, files/configuration, utility/runtime, unclassified. Subject assignment is itself interpreted and should carry provenance; an incorrect old name must not automatically determine the category.

## Private-to-public processing boundary

The private exporter/review tools and input notes stay local. The public reader consumes only the approved public catalogue. Its generic presentation code can be published because it implements the public guide, not mod functionality.

Extract candidate address/title/summary fields from the two prose stores; drop mod scores, offsets commentary, persistence/poke recipes, runtime captures, SDK invocation advice and private-path/source links by default. Allow factual original-game offsets/behavior when specifically reviewed and useful; private integration recipes are not needed to explain a function. Do not publish findings.jsonl wholesale. Facts can be distilled into self-contained public evidence without exposing the associated custom implementation.

Privacy and factuality are separate gates. Simple keyword filtering caught false positives and cannot determine either gate. Reject unsupported details, uncertain ownership, conflicting summaries and private implementation context until reviewed. Render text safely using textContent/escaped markup; do not execute HTML from candidate notes.

## Implementation sequence and acceptance gates

1. Add one address entry for every existing catalogued function; join against published index/coverage. Seed exported-file links and explicitly distinguish the six missing exports.
2. Keep 4,880 existing summary candidates private while resolving conflicts. Prioritize all 17 renamed functions, their dependent meanings and simple repeated helper patterns. Do not repeat the entire old expensive batch indiscriminately.
3. Promote only reviewed original-game descriptions to the public catalogue. Fill unsupported areas with explicit unknown status. Review operational meaning first, then game-purpose claims. New descriptions for the 2,252 gaps are separate research work, not automatic output from names.
4. Derive CSV and a small Markdown entry point. Build the static reader from that same data with search, filters, status counts and code links. Add caller/callee navigation later from a sanitized address-only graph; graph edges do not certify purpose.
5. Validate unique addresses, complete catalogue coverage, export links and hashes, evidence line ranges, summary/status consistency, missing-export status, approved disclosure fields and source commit. Confirm all browser output matches canonical data and search returns known examples. Meaningful semantic spot checks include pit entry/exit direction and nonnegative clamping.
6. Review reader usability and approved catalogue, then publish only those public files, update checksums and verify the remotely retrieved snapshot. Private tools/notes/history stay private. Publishing authority for this next change is not assumed from this investigative request.

Abort publication on a conflict presented as resolved, unexplained missing address, stale evidence link/hash, private-context inclusion, fabricated purpose, mismatched canonical/rendered data or source mutation. Preserve candidates/failed work. Do not rewrite raw/typed C or immutable skeletons to make descriptions fit.

## Verification and record closeout

Performed: mandatory cold start and zero-error/zero-warning workspace validator; fresh SDK start; route/prior-art queries; whole-store summary/address census and overlap; targeted old/new prose/code comparison; front-door inspect 0x0042A110; tooling-log confirmation of corrected naming; public README and official GitHub Pages documentation reads. No native function calls, game writes, builds, full SDK tests, publication or Pages changes.

Evidence paths: skeletons_filled/, skeletons/, priority_index.json, corresponding raw/typed functions, tools/sdk/LLM_TOOLING_IMPLEMENTATION_LOG.md typed-export entry, public FUNCTION_INDEX.csv/README/COVERAGE, earlier corpus publication operations record. Inventory is FACT about local data; recommendation and status schema are design judgments. Semantics remain bounded by the inspected evidence.

Changed files: this new assessment, a docs-index link and new findings entries. Complete docs-index/findings before-state is under workspace_organization_audit/FUNCTION_GUIDE_ASSESSMENT_BEFORE_2026-10-01/. Preserve concurrent findings during restoration. No product stage or active SDK objective changed. Pending: implementation authorization, disclosure/semantic review of candidate summaries, website usability checks and remote publication verification if the guide is built.

Wasted-effort check: repeated large reading limited to mandatory orientation; no broad builds/tests; one inventory output failed on Windows console encoding after printing measurements (targeted follow-up used Python UTF-8), and one rg command used unsupported Windows wildcard path arguments (corrected to rg -g filters); two bounded inventory completion checks, no material idle time. Documentation/UTF-8 and workspace gates run before final response.
