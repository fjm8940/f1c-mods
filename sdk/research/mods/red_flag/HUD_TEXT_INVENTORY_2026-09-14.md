# RF v82 HUD textbox inventory — 2026-09-14

Read-only source audit for owner-requested messaging standardization. Product remains candidate/verify; this task is investigation. No source, DLL, INI, game state or message wording changed. These are current candidate scripts, not a claim that every message was observed in the installed build.

Source: [redflag.input.hardened.c](trials/2026-09-14/configurable-removal-v82/redflag.input.hardened.c). SHA256: 5554DC550A0AFC0F462D68F29DD800E267D9E056FB6E353EEC6247597EF8ED55.

18 unique source textbox templates. Duplicate call sites grouped. `%d`/`%u` are numbers, `%%` displays percent, `%s` is configured key label. Native textbox writer caps each line at31 characters; rows marked truncated show the actual prefix. Voice-event labels, log/flush reasons, separate monitor-window/control-panel text, native game-generated text and empty clear operations are excluded. This inventory covers all HUD writers/call sites in the current script, including legacy/diagnostic messages.

| Source template | Source lines | At31 characters |
|---|---|---|
| `RACE WILL BE STOPPED` | 6884 | Fits; numeric/key expansion remains bounded by writer |
| `CONTINUE?` | 6885 | Fits; numeric/key expansion remains bounded by writer |
| `PRESS T TO CONFIRM` | 6886 | Fits; numeric/key expansion remains bounded by writer |
| `RED FLAG WARNING` | 6903 | Fits; numeric/key expansion remains bounded by writer |
| `MULTI-CAR CRASH` | 6890 | Fits; numeric/key expansion remains bounded by writer |
| `BLOCKED RACING LINE` | 6892 | Fits; numeric/key expansion remains bounded by writer |
| `WEATHER` | 6893 | Fits; numeric/key expansion remains bounded by writer |
| `OVERTURNED CAR` | 6894 | Fits; numeric/key expansion remains bounded by writer |
| `SEVERE INCIDENT` | 6895 | Fits; numeric/key expansion remains bounded by writer |
| `SECTOR %d` | 6898 | Fits; numeric/key expansion remains bounded by writer |
| `SECTOR UNKNOWN` | 6900 | Fits; numeric/key expansion remains bounded by writer |
| `RED FLAG IN %u SECONDS` | 9687 | Fits; numeric/key expansion remains bounded by writer |
| `PRESS T TO CANCEL` | 9690 | Fits; numeric/key expansion remains bounded by writer |
| `RED FLAG: WAITING FOR SC - T TO CANCEL` | 9716 | `RED FLAG: WAITING FOR SC - T TO` (truncated) |
| `RACE RESTART IN %d` | 6709 | Fits; numeric/key expansion remains bounded by writer |
| `CLEARING THE TRACK` | 6756 | Fits; numeric/key expansion remains bounded by writer |
| `RED FLAG` | 6680 | Fits; numeric/key expansion remains bounded by writer |
| `PREPARING RESTART` | 6739 | Fits; numeric/key expansion remains bounded by writer |

`RED FLAG` and `PREPARING RESTART` are legacy writer strings: hud_refresh_red_flag and hud_preparing_restart have no caller in this source; hud_write_red_flag is only called by the unused refresh helper. They are retained here for completeness, not claimed active. Confirmation uses three boxes; automatic warnings use warning/reason/sector boxes. The waiting-for-SC prompt is truncated and loses its cancel instruction.

Verification: searched HUD writer definitions, all callers, direct native textbox writes, format buffers, activation reason assignments and key formatter; SC headers have no additional HUD calls. No binary rebuild needed for this inventory. Native visual/timing validation and owner-selected standardized replacements are pending. Audit identified overlong strings and inconsistent SC/Safety Car/restart wording; no fixes made in this task. Workspace validator retained10 pre-existing errors/0 warnings. A Windows glob search initially failed and was repeated with rg -g; no files changed by that failed search.

Documentation gate: record exists and is linked from component README. Before-state of modified README and restoration mapping: %USERPROFILE%\Desktop\F1\workspace_organization_audit\HUD_TEXT_BEFORE_20260914_143755. No lifecycle stage change.
