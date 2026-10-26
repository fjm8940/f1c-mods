# Investigation: native MP "Edit starting grid" with AI drivers (2026-10-10)

Stage: investigate. Owner request: the host's native Edit Starting Grid should list and move CO-OP AI drivers too.

## Static facts (decompile corpus + installed exe 1A70E5ED)
- UI pages: MPHOSTGRID / MPCLIENTGRID opened by sub_4A0430 (game mode 5). Widget class names in table 0x8D80E0 (index: 30 RaceGrid, 31 HostRaceGrid, 32 ClientRaceGrid, 47/48 `APP_Selbox_Multiplayer_QualOrder_LeftPane/RightPane`). Table referenced from code at 0x47FE63 (function not in the decompile corpus; factory not yet read).
- The MP grid order is a NETWORK-NODE list in the netcomm object: count byte +16345, NODEID array +16346; per-slot node map +15896[104] (slot -> NODEID).
- Host builds it in sub_462A10: iterates network player records (count +15892, 140-byte records from +916, slot byte +1040, NODEID +15476[i]), sorts by each slot's qualifying place (veh+0x4058 array dword_5F0730), writes the list.
- Host sends it with the race-start message (sub_45F2B0, session 7). Every PC applies it in sub_462E20 (MultiplayerQualificationGridAssignment, hooked by coop_mode): NODEID -> slot via node map -> place.
- CONSEQUENCE: bots have no network node, so they are never in the list; the editor can only reorder humans. The CO-OP mod then places bots with its own GRID line (host manifest) / qualifying order (`race grid: source=qualifying` 10:34).

## Design options
1. Native editor with AI (owner wish): give each bot a fake NODEID (same on host and clients, e.g. 0x7F000000 + slot), write it in the node map on every PC, and append bots to the +16346 list after sub_462A10. Needs: the QualOrder pane code (which structure it edits and how it draws names), and proof the race-start message tolerates fake IDs on clients. Risk: medium-high (network message + UI).
2. Fallback: keep the native editor for humans; host orders bots in the CO-OP GRID line (already synced) with a host-only editor page.
Next step: read the 0x47FE63 factory + QualOrder pane classes (IDA), then one read-only live trace of +16345/+16346 while the host edits the grid.
