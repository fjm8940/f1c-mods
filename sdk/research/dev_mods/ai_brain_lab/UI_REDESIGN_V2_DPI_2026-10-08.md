# Driver AI Toolkit 1.0.5 candidate: display-scaling fix + visual redesign (2026-10-08, Claude)

 until the owner's live check. Stage: implement -> verify (offline) done; human validation
(look and feel, live race) pending. Not installed, not deployed, catalog unchanged.

## Request

1. Player report (Tester H, Discord, 2026-10-08, screenshot): the right-hand control panel cut off its text
   ("Che Qui Everyt" tabs, "Pace mu", "Corner :") with no way to widen it.
2. Owner: "fix all scaling issues" and redesign the GUI so it looks professional / premium, less busy, without
   hiding important information.

## Cause of the cut-off text (fact, from code)

`app/main.py` declares the process DPI-aware (`SetProcessDpiAwareness(1)`), so Tk scales every font to the
Windows display setting (125 / 150 / 200 %). But every width, height, wrap length and canvas coordinate was a
fixed pixel number: the drawer was a fixed `DRAWER_W = 420` px column (`pack_propagate(False)`), glance cards a
fixed 104 px, tree columns, tooltips, canvases and `wraplength=1300 / DRAWER_W - n` all unscaled. Fonts grew,
boxes did not. That matches the screenshot (inference: Tester H's display is scaled; not confirmed with him).

## What changed

| File | Change |
|---|---|
| `f1c_ida_decompiled/mods/sdk_sandbox/ai_mind_monitor/ui_theme.py` (new, shipped in `viewer/`) | Display scale (`px()`), one palette, named fonts (Segoe UI / Semibold, Cascadia Mono or Consolas), ttk styles (flat trees, thin scroll bars, hidden notebook tabs), flat widgets: Button, Toggle (switch / check), Radio, Segmented, Scale, TabBar, Callout; `fit_window`; `grab` (PrintWindow capture for layout checks). `F1C_UI_SCALE=1.5` forces a scale for testing. |
| `.../ai_mind_monitor/ai_brain_gui.py` (viewer) | All sizes through `px()`; fixed-layout canvases scaled with `canvas.scale`; quiet top bar (driver drop-down, ‹ ›, ↻, Follow / Freeze / Record / ?); glance cards: white value + state-colour edge bar (red states also red text); signals strip: quiet = faint text, active = filled pill (fixed positions kept); underline tab bar with "Show all tabs" at its right; section captions; `--shots` uses PrintWindow and forces the window visible. |
| `tools/ai_brain_lab_gui.py` (drawer) | Default width 460 design px x scale, min 380; every wrapped label follows the live drawer width (`_fit_wraps`); ARM is a switch; RELEASE ALL a red button; switches for levers, check boxes for sub-options; flat sliders; sections with chevron + yellow "N ON" badge; banner = callout (coloured edge; red = solid alarm); SIMPLE / ADVANCED moved into the "What does this do?" box; thin container-colour frame + underline tabs. |
| `tools/abl_scope.py`, `tools/abl_everything.py` | Segmented ONE DRIVER / SELECTED / WHOLE GRID, callout banner, flat search / buttons / drop-downs. |
| `app/main.py` | Theme init, window fits the screen (centred; maximised on small screens), drawer in a `PanedWindow`: drag the divider; width saved as `drawer_w` in `ai_brain_lab_settings.json`. |
| `tools/build_toolkit.py` | Ships `viewer/ui_theme.py`. |
| `tools/gen_manual.py` -> `MANUAL.md` | Divider / display scaling, ARM switch, explanation switch location, on/off switches, N ON badge, signals strip, top bar. |

Nothing was removed: every card, signal, tab, lever, status line, read-out and proof label is still shown. Two
items moved: SIMPLE / ADVANCED (top bar -> info box) and Refresh (text button -> ↻ with tooltip). The status line
(DLL / hooks / offline) is now small dim text under the driver row.

Behaviour contract kept on purpose: the new slider fires `command` on user moves and on `.set()`, never on
variable writes - measured on tk.Scale (Python 3.12 / Tk 8.6): `var.set` -> no call, `scale.set` -> call.

## Verification (offline; evidence tier = measured on this PC, no game interaction)

| Check | Result |
|---|---|
| `tools/test_abl_ui.py` (own test shared memory `Local\F1C_ABL_UI_TEST`) | 25 / 25 PASS, 0 failures |
| `tools/test_abl_shm.py` | 0 failures |
| Viewer `--selftest 8` on replay `watch_20261002T203532Z/.../battle_Christian_Klien_1007` | OK, 29 tab switches |
| Viewer `--shots` at 100 % and forced 150 %, all 14 tabs | rendered; text inside cards / tabs at both scales |
| Full window (replay viewer + panel on test shared memory, fake drivers) at 100 % / 150 %, all 6 drawer tabs | no clipped labels; wrapping follows width |
| `app/main.py` no-game state at 150 % | renders; drawer 690 px (= 460 x 1.5) |
| Package `releases/driver_ai_toolkit/1.0.5/driver_ai_toolkit-1.0.5.zip` | SHA-256 `AEDEF5FE7E7DF057541C3AC54090F7129642CFA3B6A81CCED071E57EF97C2878`; app starts from the package folder (`python -I`), `ui_theme` loaded from `viewer/` |
| Game plug-in in the package | `ai_brain_lab.dll` SHA-256 `6c60b666...` = identical to 1.0.4 (no DLL change) |

Layout screenshots (disposable, per retention policy): `%TEMP%\toolkit_ui_shots\`, `%TEMP%\toolkit_ui_check\`.
Note: one render ran while the owner's game was running; the app opened the live plug-in's shared memory read-only
(ARM off, no writes, no clicks).

Bugs found and fixed during verification: named fonts deleted by garbage collection (keep references);
`px(1)` rounds 1.5 to 2 inside canvases (use the float scale); a menubutton with an image measures `width` in
pixels; 64-bit window handles truncated by ctypes without `argtypes`; processes started from the agent shell open
minimised (PrintWindow returned 0x0).

## Not verified / pending (human validation)

- Real 125 / 150 / 200 % Windows displays (tested with a forced Tk scale on a 100 % display, which scales fonts
  and px() the same way, but Windows per-monitor DPI changes while running - moving the window to another
  monitor - are not handled: `SetProcessDpiAwareness(1)` is system-DPI).
- Look and feel ("premium, not busy") - owner judgement.
- Live race: lever switching, sliders by mouse, drag of the divider and saved width, keys dialog, pick dialog.
- Windows 10 font fallback (Segoe UI Semibold exists on Windows 10; Cascadia Mono may not -> Consolas).
- Ask Tester H whether the 1.0.5 drawer fits on his display.

## Restoration

Before-state copies: `*.BEFORE_20261008_v2ui` next to each changed file (viewer, main.py, ai_brain_lab_gui.py,
abl_scope.py, abl_everything.py, build_toolkit.py, gen_manual.py, MANUAL.md, README.md). `ui_theme.py` is new.
Release 1.0.4 package untouched.
