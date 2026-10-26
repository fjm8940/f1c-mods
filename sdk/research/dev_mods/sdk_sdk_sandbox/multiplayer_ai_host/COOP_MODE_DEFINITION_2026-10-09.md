# CO-OP mode — definition draft, 2026-10-09

Stage: define. No implementation. Owner decisions recorded 2026-10-09.

## Owner decisions

- CO-OP is a new native main-menu option next to RACE, TEST DAY,
  CHAMPIONSHIP, MULTIPLAYER (not a Lightloader button).
- Purpose: friends vs a full AI grid. Each human picks any free car.
- Scope v1: single race weekend. CO-OP championship deferred (v2).
- Disconnect: car disappears (vanilla multiplayer behaviour).
- Fairness: host sets which driver aids are allowed, reusing the native
  multiplayer ALLOWED/DISALLOWED aid settings.
- Underneath: native multiplayer session (mode 5) + host AI injection
  (see COOP_OFFLINE_RACE_FEASIBILITY_2026-10-09.md).

## Proposed flow (draft, pending owner review)

CO-OP -> HOST CO-OP | JOIN CO-OP

Host:
1. Session: season, track, server name, password, max humans, LAN/Internet.
2. Weekend: Practice / Qualifying / Warmup / Race toggles, race length,
   weather / time of day where native supports it.
3. Rules: damage, failures, fuel and tyre usage, flags, pit rules,
   driver aids ALLOWED/DISALLOWED (native MP page).
4. AI field (new page): Fill grid (default) / AI count / pick opponents;
   AI strength and aggression; cars taken by humans removed automatically.
5. Lobby (native MP): humans join, pick free cars, ready; host starts.

Join: LAN browse or direct IP; refuse with a clear message on season or
content mismatch; car list excludes taken and reserved cars.

Principle: reuse native multiplayer screens; CO-OP adds only the AI field
page, defaults and the menu entry.

## Open / to verify

- Which settings native MP host rules already expose (AI strength in MP?).
- Whether clients can see/choose cars while bots are already reserved.
- Late join before session start vs mid-weekend.
- How a new main-menu entry is built (menu resources/dispatch unrecovered).
