**2026-10-17 candidate 1.1.5-rc.1 (, installed in Documents\F1 Challenge 99-02, not live-tested):** owner rule: ESC must never pause multiplayer/CO-OP. Shipped 1.1.4 already arms no-pause online, but its `MenuWithoutPause` INI key was read before the config path was set (toggle ignored), and a 1 -> 2 arm switch with the menu open kept the freeze. Fixed in the client; setting text clarified. DLL `94CFEB1D…`. [Record](releases/v1.1.5-rc.1/RELEASE_CANDIDATE.md).

**2026-10-12 r9 test loader (, not live-tested):** multiplayer menu = CONTINUE / RETIRE TO PITS (no EXIT); RETIRE = native ESC (RACE RESULTS, then garage, LEAVE CIRCUIT separate); garage route kept only at RacePhase < 5. Loader `9773CCC8…` installed on this PC. [Record](verification/retire-standings-order-2026-10-12/REPORT.md).

**2026-10-11 SHIPPED in Lightloader 2.3.4 (broker 95618C30, loader F70AFCB1):** multiplayer EXIT/RETIRE -> garage. Not live-tested. [Release](../../lightloader/v2.0.0-hub-dev.1/release_2.3.4_2026-10-11/README.md).

**2026-10-11 multiplayer EXIT/RETIRE fix (broker, test build, NOT live-tested):** online EXIT RACE re-gridded the player and RETIRE was refused (RacePhase stays 4). Broker now takes the garage retire route for both in multiplayer races. Test loader `8259A279…`. [Record](verification/mp-exit-retire-2026-10-11/REPORT.md).

**2026-10-11 candidate 1.1.2-rc.1 ():** multiplayer/CO-OP menu without pause (broker arm value 2). Installed on owner PC with a test loader; not live-tested. [Record](../coop_mode/docs/COOP_MODS_SYNC_2026-10-11.md).

**2026-10-07 open user report (Caesarion97):** retire still jumps to qualifying, then HUD crash. Player's ESC log shows only old 1.0.5-style client flips (pre-release ticks); newest runs have no ESC retire lines, so broker log + versions needed. Root cause unknown. [Intake](verification/user-report-caesarion-2026-10-07/REPORT.md).

**2026-10-07 current public version: v1.0.7 with Lightloader 2.2.3.** Retire to Pits in races works again via the broker's game-thread route (garage, race continues, DNF at the back; refused before green). [Release manifest](../../lightloader/v2.0.0-hub-dev.1/release_2.2.3_2026-10-07/MANIFEST.md). Earlier entries are historical.

**2026-10-07 next route (design, not built):** game-thread scoped SessionType flip around the single sub_4A0250 call at 0x4A0CFE, so the session synchronizer never sees it. [Route design](verification/garage-retire-route-2026-10-07/REPORT.md).

**2026-10-07 current public version: v1.0.6.** Retire to Pits disabled in races (1.0.5 sent races back to Q2/Warm-up). DLL `5C342946…`, ZIP `996AD5F1…`. Not played in-game yet. [Publication](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_6.md), [review](verification/retire-session-jump-2026-10-07/REPORT.md). Earlier entries are historical.

**2026-09-29 v1.1.0-rc.1 (Lightloader 2.0 compliant) installed in the Documents V10 test game:** protected client `ea757a57…` (1.0.3 source + embedded metadata) with broker r2 `ec7ead43…` (race vs session menus, EXIT RACE confirmation, HUD ownership; owner reports the menu works). Owner validation and publication pending. [Candidate record](releases/v1.1.0-rc.1/RELEASE_CANDIDATE.md). Earlier entries are historical.

**2026-09-28 current public version: v1.0.3.** [Download](https://mods.palloset.com/esc_pause_menu). Owner reported the ESC sound pause works (broker `BF7E69C2…`); ZIP `B73ECBB5…` ships it with the unchanged protected client `A9FBC2FA…`. Public redirect, hash, catalog and telemetry request passed. Exact shipped pair not separately played. [Publication, posts and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_3.md). Prior entries are historical.

**2026-09-28 sound-pause broker candidate, human validation pending:** ESC froze the sim but did not call the native sound pause that P calls (`sub_4C8D60`/`sub_4C8D70`), so the engine voice kept looping. Broker candidate `BF7E69C2…` calls the same native pair on menu open/close. Build and full SDK gate pass; installed 2026-09-28 in `DocumentsF1 Challenge V10` (previous broker `C340A9E6…` preserved), no gameplay yet. Public v1.0.2 remains current. [Report](verification/sound-pause-2026-09-28/REPORT.md).

**2026-09-23 current public version: v1.0.2.** [Download](https://mods.palloset.com/esc_pause_menu). Matias reports the exact protected client `A9FBC2FA…` retained Disqualified through Retire to Pits and the compatible-variant broker `F79A1563…` opened RACE RESULTS in the isolated 99-02 game. Lightloader and broker logs corroborate the transaction. Public ZIP `2F5858BB…` and server HTTPS bytes match production. Normal-retire DNF, full menu actions and V10 regression were not separately reported for this pair. [Publication and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_2.md). Prior entries are historical.

**2026-09-23 v1.0.2-rc.1 candidate, human validation pending:** The new broker `F79A1563…` permits the garage results hook on reviewed layout-compatible executables, including the observed 99-02 file, while retaining the broker's byte/layout safety checks. The protected client remains `DDF1CD04…`. The pair is staged only in an isolated 99-02 game copy. Focused tests and the full SDK gate pass; no in-game results pass has yet been recorded. Public v1.0.1 remains current. [Repair and pending validation](verification/variant-garage-2026-09-23/REPORT.md).

**2026-09-23 99-02 Race Results finding:** The same published DLLs load in both tested installations. The 99-02 executable is accepted as a compatible variant, but the shared broker refuses the canonical-only Leave Circuit results hook; the native action therefore skips the results screen. The later hook-byte checks have not been evaluated for that variant. [Comparison, evidence, and next validation](verification/variant-results-2026-09-23/REPORT.md).

**2026-09-23 current public version: v1.0.1.** [Download](https://mods.palloset.com/esc_pause_menu). The install instructions now name Space and link the bug-report page; both DLL hashes remain unchanged from v1.0.0. The public redirect, archive hash, selector, and controlled telemetry request passed. [Publication and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_1.md). The v1.0.0 and earlier entries below are historical.

**2026-09-23 free v1.0.0 published:** [Download](https://mods.palloset.com/esc_pause_menu). The frozen ZIP hash is `453ECB3A176F8D0A9907736D3C112C7BBF24384A2BD290443302F82170B0889C`; it contains the client, accepted broker, and install instructions. The public HTTPS download, short link, privacy page, bug-report selector, and controlled telemetry request passed. The DLLs are unsigned. Full gameplay of the protected client hash and a live isolated game run remain unreported. [Publication and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_0.md). Entries below are historical candidate and development states.

**2026-09-23 free release candidate installed for live gate:** rc.2 client `DDF1CD04…` is installed beside the unchanged accepted broker `9D85FD4D…`; the game was closed at staging and both installed hashes read back. Lightloader-only host proof passed offline. Owner gameplay, signing decision, clean loader-only validation, and publication remain pending. [Candidate, evidence, and restoration](releases/v1.0.0-rc.2/RELEASE_CANDIDATE.md).

**2026-09-23 owner accepted; development finished:** Matias reports the P-key fix works and considers the ESC Pause Menu mod finished. Installed broker `9D85FD4D…` and ESC client `84FA9C5C…` identify the accepted development pair. Exact installed broker hash, SDK ownership marker, offline gates, and preservation pass. No public release was performed. [Acceptance and rollback](verification/p-key-2026-09-23/REPORT.md).

**Owner gameplay pass, 2026-09-23:** installed r5 broker `C1FA9203…`; owner reports the requested retirement/garage/Leave Circuit/results flow now works properly. Native completion corroborated in logs, including maximized monitor. This supersedes pending r5 statements below; broader modes and mechanical-failure path remain separate gates. [Pass record](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

**Current r5 trial installed (2026-09-23):** r4 resumed driving because native ESC cleared UI-active. Broker `C1FA9203…` retains the UI-active/frame context and race data through results; ACCEPT exits. Focused/native UI-setter regression and full SDK pass; one-click populated results gameplay pending. [Evidence and rollback](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md). Earlier summaries below are historical.

**Current r4 trial installed (2026-09-23):** r3 opened empty Race Results because exit cleanup preceded the native countdown. Broker `1D22A793…` retains the session on the results page until native ACCEPT exits. Build/focused/full SDK gate pass; populated results and ACCEPT gameplay pending. [Evidence and rollback](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Current r3 trial installed (2026-09-23):** broker `D7E50F67…` maps the on-screen Leave Circuit icon to the native ESC exit after UI processing, covering normal and maximized monitors. Build/full SDK gate pass; gameplay pending. Earlier trial failures below remain historical. [Evidence and restoration](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

**Latest owner test, 2026-09-23: FAILED.** Broker 76B0F99A intercepts normal Leave Circuit clicks without finalizing; MONITOR MAXIMIZED bypasses the new route. Earlier pending/pass statements below are historical. Review of older mods found retained ESC-results fixes but no confirmed complete garage-icon pass. See the latest section of the [investigation record](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

# ESC Pause Menu status

**2026-09-23 current trial:** broker `76B0F99A…` installed after build/full
SDK verification. Actual icon-release hook schedules native race finalization
on the results grid's second update. Original OSC restored; keyboard-X remap
withdrawn. Complete icon → Race Results → ACCEPT gameplay remains pending.
[Current record](verification/leave-icon-2026-09-23/REPORT.md).

The entries below describe the preceding candidate and rejected route.

Development candidate installed for owner gameplay validation on 2026-09-22.

- Source of truth: this directory.
- Pipeline stage: human validation.
- Installed client: `F1 Challenge V10\plugins\lightloader\esc_pause_menu.dll`, SHA-256 `84FA9C5CD30F9B49FFEE02A1F1DE85B8DC4B0C514B159BBA6C057AF926BF508F`.
- Installed broker: `F1 Challenge V10\plugins\lightloader\f1c_game_update_broker.dll`, SHA-256 `14F8DA959F9AA51C15B99C90A673998D81C7DFAA5EF7F5BF54144152D3BCCCDE`.
- Offline verification: project validation/build, pause-menu broker regressions, hook-layout compatibility, and the full canonical SDK gate pass.
- Current repair: REPLAY was removed from the visible menu; ESC holds the audio-suspension gate until the menu closes; RETIRE TO PITS uses the bounded SessionType 7→5 garage handoff and restores 7 after `PLAYER_OUT` consumption. The broker arms X after that handoff, but the actual monitor X/ESC action bypasses its in-race key handler and currently returns to the main menu. The broker no longer emits a no-op event every paused frame.
- Latest live finding: corrected r6 is installed and the owner confirmed the race monitor now shows `DNF` instead of the false `Accident` label. GameState 5 is compatible with this observed monitor. The monitor X was tested and went directly to the main menu, with no broker conversion; X-to-results remains broken. See `verification/live-accident-2026-09-23/REPORT.md`.
- Native reason investigation: ESC does not choose a cause. Damage/failure systems set vehicle ReasonCode before exit; the results UI maps it to labels such as Accident or Suspension. The r6 client preserves that existing code, so a voluntary retirement is generic while a previously recorded fault remains available to the UI. Damage-specific gameplay validation is pending.
- Pending after installation: confirm sounds stop while ESC is open, Retire enters the garage, then X simulates the remaining race and opens RACE RESULTS.
- Separate crash investigation: owner confirms a new-race load crash also occurs without this mod. Windows recorded `0xC0000005` at `SceneGraphChildCount` during a later load; the responsible module/object is unknown. See `verification/results-route-crash-2026-09-23/REPORT.md`. No mod was disabled or blamed.

Evidence and restoration details are in [verification/install-2026-09-22/REPORT.md](verification/install-2026-09-22/REPORT.md).

- 2026-10-12: retire laps/order + live garage timing fixed in broker test r8 (owner live + results PASS; MP untested; not released). Record: verification/retire-standings-order-2026-10-12/REPORT.md; handoff HANDOFF.md.

- ESC Pause Menu 1.1.4 shipped 2026-10-13: first activation after long Windows uptime fixed; usage version corrected. [Exact hashes, verification, restoration and pending gameplay](releases/v1.1.4-rc.1/RELEASE_RECORD.md).
