# Collector and consumer audit — 2026-09-14

## Evidence and scope

Inspected the actual installed F1CUILive.jar (SHA256 a7772c7fb4794f7edc04117cc9e3b36661317d4e132d480ca6b583e8d2601623), its f1p_utils.f1c_raceDataExporter bytecode, reader/timing bytecode, six existing RaceData exports, and the installed Season Narrative Python parser and report writer. The earlier workspace JARs were different versions; they do not establish the installed exporter's behavior.

Primary evidence: [initial inspection](../../../../research/F1CUI_JSON_COLLECTOR_INSPECTION_2026-09-14.md), [bytecode evidence](../../../../research/F1CUI_JSON_EVIDENCE_2026-09-14_211602/), [field inventory](../evidence/legacy-field-inventory.json), [reader](../evidence/java-reader.javap.txt), [timing](../evidence/java-timing.javap.txt), and [SDK offset review](../../../../f1c_ida_decompiled/tools/sdk/RACE_DATA_OBSERVATION_REVIEW.md). Consumer: %USERPROFILE%/Documents/F1 Challenge V10/Custom/2001/Season Narrative. No installed files were changed or imported into its season database.

## Verified defects and risks

1. All six examined JSON files are syntactically invalid: serviceMessages opens an array and returns without closing it when empty. The next positionHistory key occurs inside that unfinished array. Bytecode shows the early-return construction; other section writers have the same pattern. Native structural serialization removes this failure class.
2. The Java exported flag is set before writing succeeds. A failed export can suppress retry. Native publication marks success only after flushed temporary-file publication; failed attempts remain preserved and retryable.
3. Narrative's tolerant JSON reader truncates the damaged tail. Its comment says that tail is unused, but the parser actually consumes positionHistory and newFastestLapAlerts. Thus successful import does not prove the race history survived.
4. Java pit fields and consumer expectations differ. Native output includes consumer-compatible lap, stopTime/duration, tireCompound and fuelAdded while distinguishing whole pit-lane time from service time. Native timing interpretation still needs gameplay validation.
5. Native path wetness is a 0..1 fraction, not 0..100 rain intensity. Narrative also defaults unknown weather to Dry. The bridge reports observed racing-path wetness and leaves rain/forecast unknown.
6. Car-condition bits are not calibrated damage severity; tyre condition/puncture is not validated tread wear. The native collector preserves raw values under explicit names and leaves those inferred legacy values unknown.
7. Hit-counter increments cannot identify a victim, guilt or intent. Narrative's incident deduplication discards entries with no victim; its legacy retirement-resolution expectations also differ in key casing. The bridge includes neutral observed contacts in prompt evidence rather than supplying fabricated blame.
8. The legacy lap-position map can fill skipped historical laps with the current position. The DLL records only observed crossings and flags gaps. Accelerated/native-result simulation can omit lap times; missing values remain unknown.
9. The legacy ratings export includes definitions outside the actual field (for example a 1995 driver definition in the inspected 2002 race). Native matching is confined to current drivers and requires an unambiguous exact name; native scaled tuning is separately named.
10. Java's platform-default FileWriter, manual escaping/assembly and direct publication are vulnerable to encoding, partial-write and collision problems. Native UTF-8 structural JSON, finite-number handling, exclusive creation and flush-before-rename cover these cases. Filesystem failure can still prevent any output; it is logged rather than hidden.

Code-review risks, not demonstrated gameplay failures: export before trailing cars finish, unsynchronized mutable histories, session reset/slot reuse, and interpreting unrelated personal-best sectors as a single lap. Native generation identities, all-observed-car completion checks, single-worker aggregation and raw sector provenance address these without claiming live validation.

## Recommended collection and format

Keep compact JSON as the import contract. Add append-only JSONL for event evidence/recovery, stored outside the import directory's top level. This implementation supplies both. The two-driver test summary is 18,987 bytes compact versus 26,739 pretty printed (29% smaller); gzip would be 3,483 bytes (82% smaller than compact). These are fixture measurements, not a full-race storage/performance benchmark. Compression is appropriate for later archiving, but would require importer support if used directly. A binary format brings little benefit here relative to compatibility and inspectability.

Prioritize provenance, capture/session/driver identities, missing-sample counts, race completion, observed lap crossings and positions, service-versus-lane pit timing, fuel deltas, retirement codes, raw damage changes, neutral contacts and observed weather. They are implemented. The journal flushes about once per second and at clean closure; a crash may lose the last unflushed second. Checkpoints are written every 30 seconds; neither checkpoints nor recovered events are represented as final results.

Remaining integrations should add an authoritative provider for championship identity/round and official final results, verified sector crossings and timing units, validated fuel/tyre semantics, and optional typed events from Safety Car/DRS/VSC/penalty systems. F1CUI radio, radar, deleted-lap policy, driver-of-the-day and BlackMagic configuration are not recreated as native gameplay features merely to populate a collector. All legacy sections remain present, with explicit unavailable coverage. This is a standalone native collector replacement, not full semantic parity with every F1CUI-owned subsystem.

## Limitations and next validation

The DLL supports only the exact reviewed executable hash and an ABI 15 compatible broker. No advancing-game capture, performance budget, race-exit hook or human narrative-quality test has passed yet. Game-root SeasonData/Drivers is the current RCD lookup location; customized SEASONDIR layouts may leave ratings unavailable. Summary classification is observed and explicitly nonofficial. Sampled speed maxima can miss peaks. Historical data is capped; losses are reported. Unknown championship round remains 0/Unknown rather than guessed.

See [status and native matrix](../STATUS.md). Sources and build trials are preserved. No installation, JAR removal, existing JSON repair, consumer mutation or production promotion was performed.
