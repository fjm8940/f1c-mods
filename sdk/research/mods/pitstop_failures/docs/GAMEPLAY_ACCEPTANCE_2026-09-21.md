# Owner's end-to-end test target — 2026-09-21

This records the owner's explicit gameplay expectation following the test-GUI request. It is the acceptance target for the complete package, not a claim of implemented or verified behavior. Product: pitstop-modding, development prototype.

## Player

1. Select the player and arm a 100% failure chance for the next eligible pit service. Here “catastrophic” means a guaranteed botched stop with a real fault; it does not mean guaranteed terminal severity. Keep ordinary relative category weights, eligibility, durations, recovery and conditional fire chance.
2. Drive into the pits normally. Preserve the alpha experience: Limiter On, Brake, Go and Limiter Off at their correct gameplay milestones. Do not mechanically sequence these by a fixed timer. Go must wait for actual safe service release; terminal stops must not issue a false Go.
3. At the box, experience the selected issue: useful matching text, correct engineer fault callout, visible/audible behavior and real consequences. A log entry or delayed sound alone is not a working failure.
4. Recoverable issues complete and permit departure. A selected supported terminal issue follows the actual terminal path. Fire, when selected, is linked to a fuel problem and follows the reviewed transaction/presentation/cleanup requirements.

## AI

1. Select any active AI driver, arm its next eligible stop and request call-to-box for that same full identity. Leave all other drivers' probabilities and service behavior unchanged.
2. The owner spectates that car. The selected problem visibly affects its service duration/behavior. AI narration must identify the correct subject; a player first-person clip is not a substitute.
3. Recoverable fault: the car remains in service for the correct computed delay, then rejoins normally.
4. Terminal outcome: observe genuine retirement, hear an appropriate driver-specific retirement announcement, and observe the car transferred to its pit/garage through a reviewed game-owned path. Announce retirement only after authoritative confirmation, not merely after a request. If no such path exists yet, this acceptance case is blocked rather than passed by a semantic event or a raw teleport.

## GUI and probability boundary

The existing frontend's combined button requests an arm plus pit-in for the selected player/AI. Player movement into the lane/box remains manual. The command is one-shot, not a global 100% configuration; normal selection and downstream execution must be shared with ordinary gameplay. Current GUI connection contract: TEST_GUI_CONTRACT.md. The frontend exists; its backend and complete runtime mod do not.

A 100% fault chance alone does not guarantee every category or a fire will appear in a short test. Use deterministic offline cases to cover them; any later per-category probability selector must be explicit and still use the ordinary execution path. Do not covertly force retirement or fire under the existing generic button.

## Evidence to collect per test

Record exact runtime/config hashes, session/vehicle/service identity, normal baseline, chosen plan, override consumption, actual release/retirement state, and a capture covering sound/text/vehicle behavior. Test normal and forced modes, player and AI, next stop, cancellation/reset, simultaneous cars and no unintended effect on the other car. Owner listening/viewing is required for audio subject, cue timing and presentation acceptance.

Source evidence today: src/pit_commentary.c documents player pit-state-edge cues eid 51 Limiter On, eid 45 Go, eid 43 Limiter Off. These are inherited alpha source claims, not live playback verification today. Brake clip/trigger and driver-specific retirement clip/subject mapping have not been established by this check; investigate the prior alpha assets/catalogue before assigning identifiers. The previous terminal-semantic-only scope does not satisfy the owner's now-explicit AI retirement/garage requirement. Treat it as a separate required provider/capability work item, not permission to reuse legacy direct writes.

## Status and follow-up

Full-mod acceptance remains pending runtime integration, native mechanism verification and guarded gameplay. Revision-3 specification review: ../verification/integration-review-r3-2026-09-21/REVIEW.md. Feed this document into DS's integration acceptance matrix, including the alpha cue restoration and terminal lifecycle work. No game access, code change or live write was performed to create this record.
