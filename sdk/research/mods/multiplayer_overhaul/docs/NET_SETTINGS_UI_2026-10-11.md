# CO-OP [Net] settings UI (2026-10-11, evening)

Handoff `HANDOFF_CLAUDE_2026-10-11b.md` work queue item 1. Not released, not installed.

## 1. Lightloader settings page (DONE in dev source)

Fact: the handoff hint "find how SlowLaps/BlueFlags are surfaced" was wrong; no CO-OP key had a
friendly definition. `coop_mode.ini` was shown raw because `Schema.For` returned nothing, which makes
`ManagerUi.cs` list every INI key (ManagerUi.cs ~2250).

Change (`development/lightloader/v2.0.0-hub-dev.1/manager_cs/`):

- `ManagerSchema.cs`: five `coop_mode.ini` rows (Mode help text: "Friends use the host's choice.").
  - Main page: `[Mod] Enabled`, `[CoOp] HostAddress` (text), `[Net] Mode` (choice Modern/Vanilla).
  - Advanced: `[Net] SendHz` (10..100 step 5, same clamp as coop_mode.cpp ~4921), `[CoOp] ManifestPort`.
  - Side effect (intended, matches other mods): other CO-OP keys now appear only with "Show advanced settings".
  - Rows only appear when the key exists in the player's INI. 0.9.4 INIs have no `[Net]` section, so the
    0.9.5 package INI must ship `[Net]` (dev `dll/coop_mode.ini` already does).
- `SelfTest.cs`: 3 checks (Mode choice on main page, SendHz 10..100 advanced, HostAddress + Enabled main).

Before-states: `ManagerSchema.cs.BEFORE_20261011_coopnet`, `SelfTest.cs.BEFORE_20261011_coopnet`.

Verification (tool evidence):

- Build: `build.ps1 -OutputDirectory %TEMP%\ll_coopnet_build -LoaderPayload release_2.3.3_coop_2026-10-11/build/loader/dinput8.dll`
  (loader B336C26C). Final manager EXE SHA-256 00348F46B42F7445E5D07D0929B4BF077BEEDE221E9C97FD727C960AF483A661.
- `--self-test`: exit 0, 176 PASS, 0 FAIL.
- A first build with the default `payload/dinput8.dll` failed only "embedded loader is the broker-built-in build"
  (expected: old dev payload). Stray output folder `%LOCALAPPDATA%\Templl_coopnet_build` (path typo, outside workspace, disposable).

Pending: human look at the settings page (labels, layout); Lightloader version bump (2.3.4) belongs to release prep.

## 2. Owner request: settings in the in-game CO-OP create menu

Owner (chat, 2026-10-11): "make it so that these settings can be done through the in game co-op creator menu".

Evidence (prior art, no new RE yet):

- Fact: today the only in-game CO-OP UI is the extra "CO-OP" item on the main-menu mode spinner
  (vtable 0x5A2400 hooks, coop_mode.cpp ~238). No CO-OP settings page exists.
- Fact (finding `osc-menu-native-ai-gizmos-20261009`): menus are text in `Options/F12K2.osc`; MP host pages
  are MULTIPLAYER_CREATEGAME, HOST_CIRCUIT_SELECT, MPRULESSETTINGS. Menu packs replace this file
  (open item O3 in INVESTIGATION_2026-10-09.md).
- Fact (finding `ui-gizmo-factory-id-name-table`): gizmos come from factory IDs; new gizmo types need the name
  table extended (avoided by design).
- Inference: two routes.
  - A. Hook an existing spinner gizmo instance on an MP host page (same vtable technique as the mode spinner)
    to show "NET: MODERN/VANILLA" and "UPDATES: 60". No .osc edit, works with every menu pack. Needs RE of
    which spinner/vtable is on MULTIPLAYER_CREATEGAME and that its vtable is not shared with other pages.
  - B. Patch F12K2.osc with a new CO-OP page; must be done per menu pack; touches a game file.
- Open: client side. Mode must match on both PCs; a host-only menu needs the host to send Mode/SendHz to
  clients (manifest channel, port 34500) or clients keep their own INI.

Owner decision (chat, 2026-10-11): route A (hook existing spinner), host sends Mode/SendHz to clients.

## 3. In-game menu: static RE (2026-10-11)

Exe `F1 Challenge Online.exe` (1A70E5ED), read as a file with capstone; scripts in `%TEMP%\coopnet_re` (disposable). No game run.

| Fact | Evidence |
|---|---|
| `MULTIPLAYER_CREATEGAME` has no spinner (name, password, buttons). Host rules page `MPRULESSETTINGS` has `APP_SPINBOX_MULTPLAY_CONNECTIONTYPE`; the same gizmo is also at F12K2.osc lines 20145 and 20462. | F12K2.osc (host install, read only) |
| Name `APP_SpinBox_MultPlay_ConnectionType` = factory ID 405 (name table 0x8D80E0 index 404). | name table read |
| App factory 0x47FE97: `jmp [index*4 + 0x8D89C0]`; index 404 -> 0x483FE3 -> ctor 0x474B00 (alloc 0x338). Mode spinner check: index 265 -> 0x482A54 -> ctor 0x472D00 (matches known vtable 0x5A2400). | disassembly |
| Ctor 0x474B00: base int-spinner 0x466D60, vtable 0x5A3F00 (only reference = this ctor), value ptr this+0x31C = 0x7D0E50, max +0x324 = 4, min +0x328 = 0, step +0x32C/+0x330 = 1, 5 labels via AddItem 0x486F30 ("Modem < 56K" ... LAN). | disassembly |
| Vtable 0x5A3F00 has 48 slots; slot 46 0x466DA0 change (dir 1 = +step, else -step, clamp, no wrap, ends with slot 47); slot 47 0x4671F0 label (clamp, GetItem, copy to stack, 0x48B810(this, text)); slot 43 0x467030 enable state; slot 40 0x4670E0 per-ID notifier. | disassembly |
| 0x464127 uses the connection type as a jump-table index, so a value 5 is unsafe; the mod never writes 0x7D0E50. | disassembly |

Inference (not live-verified): dir 1 is the right arrow. If arrows are inverted, Modern still works but sits "before" LAN instead of after it.

## 4. Implementation: coop_mode 0.9.5-dev4 (dev; installed on host 2026-10-11, hash FCC25E8D verified; dev3 F058FF4F backed up to workspace_organization_audit/WEB_TELEMETRY_430RC2_INSTALL_2026-10-11/coop_095dev3_before_dev4/)

- `coop_mode.cpp`: vtable 0x5A3F00 slots 46/47 hooked (all-or-nothing, COOP-E04 on mismatch, restored on unload).
  In CO-OP only: at LAN a forward step sets Modern and shows "Modern"; a back step from MODERN sets Vanilla and
  shows the native label. Each change writes `[Net] Mode` to coop_mode.ini. Outside CO-OP the spinner is native.
  Display: whenever Modern is on (also the INI default), the spinner shows Modern regardless of the native value.
- Host manifest gets a `NET <Modern|Vanilla> <SendHz>` line before `END`; the client applies it (runtime only, not saved),
  logs `net mode (client): host uses ...`. A 0.9.4 host sends no NET line: client keeps its INI. A 0.9.4 client would
  parse NET as a bot line (mixed versions are unsupported anyway).
- SendHz stays INI / Lightloader advanced only (owner: simple).
- Limitation: a client changing the spinner after the host manifest arrived overrides the host value until the
  host manifest text changes. Not fixed in dev4.
- `ADDRESS_INVENTORY.md` +5 rows; `coop_mode.ini` [Net] comment. Before-states `*.BEFORE_20261011_netmenu`.

Verification: build `%TEMP%\coop_mode_build_dev4\coop_mode.dll` SHA-256
FCC25E8DEEE9B8E3A6386A69208BC88CE312050AACA05F61AFB50D493BD7F074; tests 5/5 PASS (address inventory 65).

Pending live (owner): log `menu: net mode entry installed`; CO-OP > Multiplayer settings > Connection type: step past LAN
shows Modern, back returns to LAN, INI updates; non-CO-OP MULTIPLAYER spinner unchanged; client log shows
`net mode (client): host uses Modern 60 Hz`; host set to Vanilla -> client follows; arrow direction (inference above).

## 5. Live host check (2026-10-11, log times 10:24 UTC)

Fact (host coop_mode_log.txt): `0.9.5-dev4 start`, `menu: net mode entry installed (vtable 005A3F00 slots 46/47)`,
six toggles `net mode: Vanilla/Modern (in-game menu, saved to INI)` 00-00-00-00-00-00, then `manifest server: UDP 34500`.
INI after: `Mode=Modern SendHz=60`. Owner: "Modern is seen now", server started, no crash.
Pending: client following host (laptop needs dev4), race smoothness under dev4, non-CO-OP spinner unchanged.

USB staging 2026-10-11: `D:\coop_mode_0.9.5-dev4\` (coop_mode.dll FCC25E8D, INSTALL_ON_LAPTOP.bat from dev3 with version/backup names changed, SHA256SUMS.txt).

## 6. Two-PC run (host view, 2026-10-11-10:28 UTC)

Fact: laptop joined 10:26:40 (slot 1, took Burti bot car); f1c-memory 10:28:17 slot 1 on track 178 km/h, pit_status 0, sim_time advancing (175.9 -> 201.0 s); host 165-172 fps, worst frame 16 ms; Modern active; FastestLapFix, SlowLaps, PitExitFix logging normally; 0 COOP-E/error lines. Not seen from host: client NET line receipt (laptop log needed).
Owner (laptop view, 2026-10-11): "looks okay, small jitter but its okay". Residual jitter = candidate for N1b smoothing buffer (NETCODE_AUDIT O1, handoff item 5).

## 7. Laptop log (dev4 client, 10:26-10:30 UTC, Debug=1)

Fact: `0.9.5-dev4 start`, `config: Net Mode=Modern SendHz=60`, `menu: net mode entry installed`, host manifest verified (22 bots, files identical), `net mode: Modern active (send 60 Hz ...)`, visibility records age 16 ms (~60 Hz arrival), 142-152 fps worst 16-32 ms, 0 COOP-E/error lines. No `net mode (client): host uses` line: expected, it is logged only when the host value differs from the local one (both Modern 60). Host->client NET override itself therefore not yet exercised; test: host Vanilla, laptop Modern.
Owner: small jitter on AI cars only (laptop view). Input for N1b smoothing investigation.


## Note 2026-10-11 (other session)
Lightloader 2.3.4 shipped (multiplayer ESC fix) WITHOUT these schema/self-test rows; source tree keeps them plus the 2.3.4 pins (version 2.3.4, loader F70AFCB1). Next release with this work = 2.3.5. Record: development/lightloader/v2.0.0-hub-dev.1/release_2.3.4_2026-10-11/README.md.

## 8. Shipped 0.9.5 (2026-10-11)
Fact: deployed catalog-only on top of LL 2.3.4; live catalog 7767022694221712B7F9813C96FB7FCECF7E56FE95AF93E798BA8242C3A4223F, `/coop_mode` serves zip C5EAECE3, DLL FC7D978C. Record: development/lightloader/v2.0.0-hub-dev.1/release_coop_0.9.5_2026-10-11/README.md. Pending: exact 0.9.5 DLL in game, host Vanilla -> client override test, N1b jitter.
