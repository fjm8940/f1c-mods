# CO-OP mode — investigation, 2026-10-09 (Claude)

Static only: exe read as a file (capstone) + decompile + `Options/F12K2.osc`.
Exe: canonical `F1 Challenge Online.exe` (hash family 1A70E5ED). No game run, no memory write.

## 1. Game-mode enum (FACT, exe strings + spinner init 0x472D20)

`dword_7D0410` = front-end game mode = index into the main-menu mode spinner:

| value | label | spinner |
|---|---|---|
| 0 | CURRENT | hidden (forced to 3) |
| 1 | TEST DAY | shown |
| 2 | QUICKRACE | hidden (forced to 3) |
| 3 | RACE | shown |
| 4 | CHAMPIONSHIP | shown |
| 5 | MULTIPLAYER | shown |
| 6 | DRIVING SCHOOL | hidden (forced to 3) |
| 7 | REPLAY PLAYBACK | hidden |

`dword_7D0418` / `dword_7D041C` = runtime min/max clamp (no static writer found).
Many game paths test `== 5` (AIFormationLap, AIOvertakeDefendStateChange, GameMode,
ScreenMenuNavigationController...), so CO-OP must keep the mode at **5**.

## 2. Mode spinner class (FACT, disassembly)

Gizmo `APP_SpinBox_GameOpt_GameModeSelect` (on page "MAIN MENU" in F12K2.osc,
line ~6417) is a class with its own vtable at **0x005A2400**. Its methods are
vtable slots, all mode-spinner-only:

| slot | address | role |
|---|---|---|
| 28 | 0x473010 | clamp mode to [7D0418, 7D041C] |
| 29 | 0x473040 | `this+0x320 = mode` (store selection) |
| 40 | 0x472D20 | rebuild: clears item list, adds 8 labels via `AddItem 0x486F30` (thiscall(this, char* label, -1), ret 8); forces 0/2/6 to 3; shows label `GetItem 0x487010(mode)` (thiscall(this, idx), ret 4, returns char*) via `SetText 0x566650` on `this+0xB0`; `this+0x320 = mode` |
| 43 | 0x472FB0 | per-frame sync: forces 0/2/6 to 3; if `this+0x31C != mode` stores it and calls slot 40 (vtable+0xA0) |
| 46 | 0x472E30 | change: thiscall(this, dir), ret 4. dir 0 = next, dir 1 = previous |

Cycle (jump tables 0x472F9C / 0x472F88):
next: TEST DAY(1) -> RACE(3) -> CHAMPIONSHIP(4) -> MULTIPLAYER(5) -> TEST DAY.
previous: reverse.

The OK button `APP_Button_MainPageNav` acts on the mode global, not on the
spinner index (MP path: GameModeSelectorEntry 0x463BC0 -> "Multiplayer Create").

**Implication:** a CO-OP entry can be added by replacing 3 pointer slots in one
unique vtable (40, 43, 46). Mode stays 5; a mod-owned `coop_selected` flag and
spinner index 8 ("CO-OP") distinguish it. Plain MULTIPLAYER stays vanilla.

## 3. Menu data (FACT, F12K2.osc)

- Menus are text (`Options/F12K2.osc`, 25k lines, ISO-8859, CRLF): pages, gizmos,
  `Do_Page` navigation. Menu packs (`install_RH04_menus.bat` etc.) swap this file.
- Native AI gizmos exist and are page-independent:
  `APP_SPINBOX_GAMEOPT_OPPONENTS` (race page SELECTCIRCUITPAGE),
  `APP_SLIDER_GAMEOPT_AIDRIVERSTRENGTH`, `APP_SLIDER_GAMEOPT_AIDRIVERAGGRESSION`
  (ADVANCED DIFFICULTY). Placing them on a CO-OP page needs no new gizmo code.
- MP host pages: MULTIPLAYER_CREATEGAME, HOST_CIRCUIT_SELECT, MPRULESSETTINGS,
  MP RULES SETTINGS 2 (aids: `APP_UNIQUE_SCROLLBOX_MP_DIFFICULTY`), MPHOSTGRID, MPLOBBY.
- Gizmo names live in a name-pointer table in `.text1` (~0x8D81E8..0x8D8504);
  new gizmo TYPES would need that table extended — avoided by design.

## 4. Open (Claude-owned, not delegated)

- O1: whether OPPONENTS / AI strength / aggression values are honoured for
  host-injected bots in MP (live check).
- O2: CO-OP AI page placement: page-navigation redirect when `coop_selected`
  (hook on page dispatch `LookupDispatch`) vs a button on an MP host page.
- O3: .osc patch for each menu pack (F1C99-02ORG, RH04, RH05, YOURF1C) and
  whether any pack loads the menu from a .MAS.
- O4: client side: joiners must also pick CO-OP so their mod arms (or the host
  flag must reach them); MP-mode joiners with the mod installed stay inert.
- O5: `this+0x320`/`0x31C` and MainMenuTransition restore — confirm CO-OP
  selection survives returning to the main menu (else default to MULTIPLAYER).
