# Red Flag — current state

**As of 2026-09-30.** This page is the short "what is true now" view. `README.md`
keeps the full dated history unchanged. Update this page whenever the public,
installed or candidate state changes.

Labels: **Verified** = checked on disk on 2026-09-30. **Recorded** = stated in a
project record, not re-checked. **Inference** = a conclusion that no record
states.

## Identities

| Channel | Version | DLL SHA-256 | Label | Source |
|---|---|---|---|---|
| Lightloader 2 public catalog | 1.1.5 | `43050C2DFD607EC4…` | Verified: DLL inside `production/lightloader/2.0.0/red_flag-v1.1.5-lightloader.zip` (ZIP `ED861C15…`); listed in `production/lightloader/v2.0.1/catalog.json` | Owner-run deploy, 2026-09-29 (`operations/release_management/lightloader/RELEASE_2_0_0_2026-09-29.md`) |
| Installed in `F1 Challenge V10` | 1.1.5 | `43050C2DFD607EC4…` | Verified (hash of `plugins/lightloader/redflag.dll`) | — |
| Legacy website channel `/redflag` | 1.1.3 | `FCC9243F…` | Recorded: the LL2 release record says "/redflag unchanged" | `operations/release_management/red_flag/RELEASE_1_1_3.md` |
| `production/mods/red_flag/` newest folder | v1.1.3 | — | Verified (folder listing) | — |

**Why 1.1.5:** Red Flag 1.1.4 and older find the broker by file name and stay
inert under Lightloader 2, whose broker is built into `dinput8.dll`. 1.1.5 finds
it by export scan. There are no gameplay changes
(`releases/v1.1.5-rc.2/RELEASE_CANDIDATE.md`).

**Label drift:** the candidate record still says "rc.2"
because it predates the owner's publication decision. The release record is
authoritative. The candidate record is kept unchanged as history.

## Candidate (not shipped)

**2026-10-08 — Red Flag 1.1.6-rc.1 ():** the re-synced shipped
1.1.5 base plus the owner's U2 consent-gated per-session usage summary
(`src/rf_usage.h` on the shared `f1c_usage.h`), the shared crash VEH installed
after successful init and removed on unload, and the broker ABI15 session-end
flush. No gameplay change. `red_flag.meta.json` version `1.1.6`, `lightloader
>=2.2.3`. Built DLL `6FB254E4765FCD509696EDFE1B5422DBCF40A9680CB1E0C56ADF367AF703B32C`
(the exact candidate; owner fix 2026-10-08: `rf_session_end` sets a request flag
only and the worker writes the summary; rebuilt against the ABI 2
`f1c_race_control.h` for the combined session; in-game test deferred to the
combined test session). Evidence:
`docs/RF_1_1_6_2026-10-08.md`, `releases/v1.1.6-rc.1/RELEASE_CANDIDATE.md`.

## Validation

- Offline: metadata, build reproducibility and packaging checks passed
  (candidate record).
- **In game, 1.1.5 itself: no recorded result.** The owner's publication
  decision on 2026-09-29 explicitly shipped without the remaining pre-announce
  checklist. The in-game PASS recorded that day is for Safety Car 0.10.66 and
  the loader, not Red Flag.
- The 1.1.3 gameplay acceptance (owner-tested `FCC9243F`, paired with SC
  `2F4ECAD0`) carries over **by inference only**: the 1.1.5 diff is limited to
  broker discovery.

## Open items

1. An in-game smoke test of 1.1.5 under Lightloader 2: the red flag triggers,
   the restart completes, and the log shows the broker found by export.
2. Decide whether the legacy `/redflag` channel should keep serving 1.1.3,
   which needs the separate broker file.
3. Earlier pending gameplay items (replay roster guard, late-race restart
   strategy, penalty/fuel preservation) remain open. See the checkpoint entries
   `rf_*`. Run `python tools/f1c_mod.py evidence-query findings RF --json` in
   `f1c_ida_decompiled`: it returns both the recorded RF entries and the curated
   Red Flag findings. `"red flag"` alone returns only the curated ones.
