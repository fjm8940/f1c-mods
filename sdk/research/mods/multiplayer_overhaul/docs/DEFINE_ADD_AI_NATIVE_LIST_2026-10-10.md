# DEFINE: CO-OP "Add AI" native menu list (2026-10-10)

Stage: define -> investigate. Owner: Matias. Not implemented.

## Objective (owner words)
In-game menus show a list of drivers that can be added as AI. Completely native,
vanilla game UI (no overlay, no HUD-strip rendering, no external GUI).

## Assumed acceptance (to confirm with owner)
- Host-only; reachable from the CO-OP menu flow before the session starts.
- Lists season drivers not currently on the grid; selecting one adds that AI.
- Field change syncs to clients through the existing manifest.
- Works with the full-grid default (2026-10-10 fix): list is mainly useful after
  bots were released or for non-season drivers.

## Non-goals
- Mid-race adding (session type 7 guard stays).
- Overlay/broker HUD menus (esc_pause_menu style is NOT native screen UI).

## Investigation evidence so far (2026-10-10)
- Fact: CO-OP already extends one native widget: mode spinner vtable 0x5A2400,
  slots 40/43/46 replaced to add a ninth item (`coop_mode.cpp`).
- Fact: no SDK finding covers native list widgets (evidence-query: menu/list/spinner/widget).
- Fact (decompile names, unreviewed): native list code exists, e.g. `UIListIndexedAccessor`
  (used by `ModeSpecificScreenSetup_00471440`), `ScrollbarAreaRendering_004868c0`,
  `MenuItemLayoutCalculator_0047b850`, `OnScreen_*_0049ef40..0049f120`,
  `ScreenMenuNavigationController_00470040`, `AIOpponentSelection_004bf790`.
- Fact: menu layouts ship in `Options\*MENU.MAS` (RH04/RH05/YOURF1C installers) -
  screen layout is data-driven, so a native list probably needs both a layout entry
  and code hooks. Unverified.
- Fact: findings `DISPROVEN-menu-selection-by-memory-write` - UI selection cannot be
  driven by writing globals; must go through widget code.

## Open questions (investigate next)
1. Which vanilla screen has a scrolling driver/name list we can reuse (MP lobby player
   list, driver select, load profile)? Its widget class/vtable and populate function.
2. Can a list widget be added to an existing screen, or must an existing control be
   repurposed (lower risk, like the spinner)?
3. Where MAS menu layout defines controls; whether a new control needs a MAS edit
   (shipping MAS changes conflicts with RH04/RH05 menu packs).
4. Game-thread safe point to spawn the chosen AI (reuse prepare-field path).

## Lower-risk fallback (needs owner OK)
Native spinner "ADD AI: <driver>" + native button on the CO-OP session screen,
using the already-proven spinner vtable technique.

## Investigation round 1 (2026-10-10, static, exe 1A70E5ED read as file)
- FACT (decompile 0x47F440 `SeasonDatabaseObjectFactory`): gizmos are created by numeric
  factory ID (1..508). ID = index+1 in the gizmo name-pointer table at 0x8D80E0..0x8D879C
  (431 names, e.g. 25 `APP_Unique_ScrollBox_MP_DriverSelect`, 35 `APP_Unique_Selbox_Multiplayer_GameList`,
  36 `..._ChatOutput_Host`, 39 `APP_Unique_Selbox_ReplayFileList`, 50 `APP_Selbox_MiniGrid`).
  Index/ID alignment checked on 35/36 (factory case 35/36 = listbox vtable 0x5A7008); others DERIVED.
- FACT: two list families. ScrollBox (IDs 12-27) = icon grids (team/driver select with pictures).
  SelBox = text listbox, base ctor `sub_4893C0`: row chain this[36] (row+140 next, columns at
  row+160/168/176/184), row count this[37], scroll top this[83], visible rows this[89],
  selection this[90]; `UIListSelectionHandler 0x489AB0(this, 1-based row, scroll)`.
- FACT (vanilla populate pattern, replay list 0x47BF30 and MP game list 0x475150):
  `StateCleanupNotification(this)` clears, `UIMenuItemCreator(text, c2, c3, c4, 0)` adds one row,
  `UIListSearchAndSelect(name, 0)` reselects; `UIScreenTextAccessor(this)` reads the selected text.
- FACT (F12K2.osc): host in-session page `MPHOSTGRID` (line ~19337) has `APP_SELBOX_MINIGRID`,
  replay viewer and session controls; client car pick pages use `APP_UNIQUE_SCROLLBOX_MP_TEAMSELECT`
  / `..._MP_DRIVERSELECT`.

## Proposed design (DERIVED, pending owner OK on look)
- `.osc`: MPHOSTGRID gets a native GENERIC_BUTTON_NORMAL "ADD AI" -> new page "COOP ADD AI"
  (same background/attribute sets) holding a vanilla SelBox (text rows: driver | team | #),
  an ADD button and BACK. Button/page shown only in CO-OP (hook) or harmless in plain MP.
- DLL: when page "COOP ADD AI" opens, hook the SelBox's populate to list season drivers not on
  the grid; ADD reads selection, queues spawn on the game thread through the existing prepare
  path, rebuilds the list. Host only; blocked in race session (type 7).
- Risk: .osc edit must be applied per menu pack (F1C99-02ORG, RH04, RH05, YOURF1C) - open O3.
- Next RE: pick the SelBox class to reuse (needs a populate entry we can hook, unused on MP
  pages), confirm vtable slots for populate/selection, and the button action dispatch for a
  custom page (Do_Page works natively; ADD needs an action we can intercept).

## Owner addition 2026-10-10 (later): Edit Starting Grid must include AI
Native MP "Edit starting grid" works for human players; owner wants the host to move AI drivers there too.
Status: not started. Next: investigate how the native grid editor builds its list (likely humans-only filter on role/slot, similar to the slot/role blind spots in IMPLEMENTATION_LOG_2026-10-10.md) and whether the CO-OP race grid hook (RaceGridRewrite=0, native MP grid) keeps its order.
