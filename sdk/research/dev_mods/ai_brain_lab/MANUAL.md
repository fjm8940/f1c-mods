# Driver AI Toolkit v1.0.4 - manual

Last updated 2026-10-08
Author Matias

For F1 Challenge (99-02 and the Online version). **Offline single-player only.**

The Driver AI Toolkit shows what every AI driver is thinking, live, and lets you change how they drive.
It has two halves in one window:

- **Left: the viewer** - read-only. What the selected AI car is doing right now and why.
- **Right: the drawer** - the controls. Every change is undone with **RELEASE ALL (Esc)**.

The divider between the two halves can be dragged: make the drawer wider or narrower; the toolkit remembers the
width. The whole window follows the Windows display scale (125 %, 150 %, 200 %), so nothing is cut off on
high-resolution screens.

Nothing you do changes any game file. All changes live in memory while the game runs and disappear when
you RELEASE ALL, untick them, or close the game.

---

## 1. Install and start

1. Update Lightloader to **2.2.0** or newer.
2. Lightloader > **Tools** > Driver AI Toolkit > **Install** (a setup window may open; it can install Python
   for you after asking).
3. Press **Open**. Windows asks for administrator rights: the game normally runs as administrator and the
   toolkit needs the same rights to read and steer it.
4. Start the game and a session. The viewer appears by itself when a race (or practice/qualifying) runs.

Manual install: extract the ZIP into `<game folder>\tools\` and double-click
`tools\driver_ai_toolkit\DriverAIToolkit.bat`. The first start installs, later starts just open the toolkit.

What is installed: the toolkit folder (`<game>\tools\driver_ai_toolkit`) and one game plug-in
(`<game>\plugins\lightloader\ai_brain_lab.dll`, shown on Lightloader's Mods page as "Driver AI Toolkit
(game plug-in)"; turn it off there to disable the toolkit's control of the game).

---

## 2. The three safety switches - read this first

| Control | What it does |
|---|---|
| **ARM** (switch at the top of the drawer) | Master switch. **Nothing you tick acts until ARM is on.** Untick it to freeze all changes at once (the AI gets its own values back). |
| **RELEASE ALL (Esc)** | Unticks everything for every car and every rule, turns ARM off and hands every value back to the AI. Works from the toolkit window (Esc) and in-game (default key End, see Drive tab). |
| **Check tab** | Lists every change in force right now, per driver and per rule, each with an **x** to undo just that one, and a green / red dot: green = the plug-in applied it and, where the game keeps one value, the value read back from that driver's car is the value sent. |

The plug-in refuses to act when the session is not offline single-player, when a car is not an AI car, and
when a value is outside its allowed range. Refusals are shown as status text on the row ("blocked", "out of
bounds").

---

## 3. The drawer (right side)

**Who do changes go to?** - three big buttons choose the *container*:

- **ONE DRIVER** - only the driver chosen in the **Driver** list.
- **SELECTED DRIVERS** - press **Choose drivers...** and tick drivers.
- **WHOLE GRID** - every AI car in the session.

The coloured banner under the buttons names every driver who receives changes (cyan = one driver, purple =
selected drivers, orange = whole grid), and the thin frame around the tabs has the same colour, so you
always see where a change goes. With SELECTED DRIVERS or WHOLE GRID the list is called **Show values of**: it only
picks whose own values the sliders show.

**Your value or their own value?** With several drivers in the container, a slider you **move** sends your value
to all of them (its number turns yellow). A slider you do **not** move sends every driver **his own** value, so
ticking a lever for the whole grid never copies one driver's value onto everyone. The **AI** button puts a slider
back to "own values".

**Proof that it arrived.** Next to every lever: **applied n/N** (green = every driver in the container, yellow =
only some). Hover it for the per-driver list: plug-in status and, where the game keeps one value, the value read
back from that car's memory. The **Check** tab shows the same for everything that is on.

Drive mode always controls only the selected driver. PARK/STOP for several cars asks first.

**Explanations: Simple / Advanced.** Point at any setting: its explanation appears in the **What does this do?**
box at the bottom. The Simple / Advanced switch in that box chooses plain words or the technical text (game
field, formula, evidence).

Under the Driver list: a small status line (plug-in state, hooks, offline check) and the car's planner state
(green edge = levers acting, yellow edge = nothing ticked yet, **red** = this car is not running the AI planner,
so levers cannot act).

**How every control row works**

```
(on/off)  Label                       value   [AI 0.988]
          ----------- slider -----------      status
```

- **On/off switch** - turns the change on for the drivers in the container (needs ARM). On = on for at least one
  of them; the applied n/N read-out says for how many. Small square boxes are sub-options of a switch.
- **Slider / value** - the value you want.
- **[AI ...]** - the AI's own value right now. While the box is unticked the slider follows the AI, so ticking
  changes nothing until you move the slider. Click it to copy the AI value back. "AI no cap" = the game's
  "unlimited" value. Rows without an AI value have **reset** (back to neutral) instead.
- **Status / applied n/N** - what the plug-in did: idle, APPLIED, ARM is off, blocked, out of bounds, not in DLL
  (your plug-in is older than the toolkit; reinstall). With several drivers: applied n/N.
- **Hover** any label for its full explanation. Section titles show a yellow **N ON** badge when something in
  that group is switched on; click a section title to fold it; hover a Rules section title for how that whole system works.

Tabs: **Check · Quick · Everything · Rules · Presets · Drive** (each explained below).

---

## 4. The viewer (left side)

Top row, always visible:

| Box | Shows |
|---|---|
| MODE | The AI state machine: 0 racing, 1 recovery stop, 2 one-tick reset, 3 route check (pit spot), 4 pit wait, 5 garage wait, 6 reversing, 7 stopping. |
| DOING | THROTTLE / PART THROTTLE / BRAKING / COAST, with throttle %, brake %, gear and speed. |
| WHY | The reason, e.g. "flat out: every corner in look-ahead reachable", "corner ahead needs 117 km/h, now 288", "29 % (hold speed / off line / traction)". |
| NEXT CORNER | The corner that forces braking (speed it needs, distance) or "free". |
| TRAFFIC | Nearest car and what this car is doing about it: **ATTACKING** (record mode 1/2), **SIDE-STEP** (7/8), "closing on" (16), or hold (5). |
| LINE | Where its line comes from (corridor, request, forced) and the offset in metres. |

Signals (the row under the boxes; faint = quiet, filled colour = active; the colour bar on each box's left edge
is its state colour): NOT ADVANCING (game paused / menu), NON-RACE, START (race-start branch),
MODE (not racing), RETIRED, STUCK (wrong-way counter running), YIELD (has a traffic target), CLAMP (speed
limiter active), PIT CALL, OFF CORRIDOR (outside racing corridor + 1.5 m: 50 % throttle rule), STEER MAX,
LINE OVERRIDE.

Tabs (Basic view: Overview, Speed, Line & steering, Traffic, All cars; **Show all tabs** adds the rest):

| Tab | What you learn |
|---|---|
| Overview | Car-up map (walls, planned line, steering target, nearby cars), speed plan (white = now, blue = projected braking curve, red ticks = corner targets) and pedals/steering. |
| Speed | Why this speed: corner-target formula, limiting corner, pace multiplier walk, braking look-ahead steps. |
| Line & steering | Which line wins (corridor -> working -> request -> forced -> published), the lateral strip and the steering law. |
| Traffic | The 5 nearest cars with distance and record mode, interaction target, teammate link, throttle cap. |
| Modes & safety | Mode machine, wrong-way/stuck counter and the 4-second progress check. |
| Pit & strategy | Pit plan (stops, next stop lap, tyres), pit command flags, race position, best lap, engine life. |
| Driver profile | The driver file (.rcd) values and how the game transformed them. |
| Tick pipeline | One 25 ms AI tick in order and the outputs of each stage. |
| All fields | Every known AI field with value, tier, persistence and meaning; double-click to pin to Watch. |
| Watch | Pinned fields as live graphs. |
| Timeline | Event log: mode changes, braking zones, traffic record changes, gear. |
| Waypoints | The 20 track waypoints the AI looks at ("stop" = its braking projection reaches 0 there). |
| All cars | Every AI car at a glance; "interest" ranks cars for Follow (STOPPED, YIELD, RETIRED, ... score higher). |
| Record & SDK | Record a session (`%TEMP%\ai_brain_live\`) and replay it later. |

Top bar: ‹ › change car (or the drop-down; ↻ re-reads the list), **Follow** jumps to the most interesting car, **Freeze** stops updating, **Record**.

---

## 5. Vocabulary the AI uses

- **Pace multiplier** - scales every corner target; the AI walks it itself every 4 s between its floor
  (baseline) and floor + push, depending on traffic.
- **Corner target** - the speed the AI wants at each waypoint: tyre-grip speed x CORNERINGMULT +
  CORNERINGADD, x wet factor, x pace.
- **Braking look-ahead** - 20 waypoints ahead; the AI brakes when its projected speed curve cannot reach a
  corner target. "Braking confidence" (PASSING) decides how quickly it thinks it can stop.
- **Racing corridor** - the line width stored per waypoint in the track file. Outside it + 1.5 m the AI
  thinks it is off track (pure geometry, it does not see grass).
- **Record mode** (one per nearby car) - 16 approach, 5 hold, 1/2 attempt (pull out -3/+3 m), 7/8 alongside.
  A follower pulls out when its built-up **attack patience** beats a pass-cost threshold (see Rules >
  Following & overtake decision).
- **Interaction target** - the car this car is yielding to (blue flag, side-step request or team order).
  While set, throttle is capped at difficulty x 0.75 - and the game never restores the cap afterwards
  (fixed by Rules > "Never cap throttle at 75 %").
- **Team orders** - the car with the higher race number of a team (higher garage bay) yields to its
  teammate after lap 3 when the teammate is alongside/attacking. Quick > Cooperation "never moves aside"
  blocks it for that car.
- **Defending** - the AI has no defending mode; the car ahead keeps its line.

---
## 6. Quick tab - everyday levers (per car, follow the container)

Ticking a row overrides the AI's own value for the drivers in the container while ARM is on.

### Racing line

| Control | Range | What it does in the game |
|---|---|---|
| Move line | on/off | Moves the car's line sideways through the AI's own steering. **forced** always wins; **request** lets the AI override it in battles. Offsets beyond the racing corridor + 1.5 m count as off track (50 % throttle) unless Rules > Off-track rejoin is ticked. Aborts by itself in park/recovery, when retired, in the pit lane, without a waypoint or on a lateral spike. |
| Offset (m) | -6 .. +6 | Target offset; - left, + right. |
| Ramp (m/s) | 0.1 .. 3 | How fast the offset moves. |
| Limit offset (m) | 0.5 .. 6 | Hard cap on the offset. |

### Speed

| Control | Range | AI value shown | What it does in the game |
|---|---|---|---|
| Pace multiplier | 0.5 .. 1.5 | yes | Overall speed of the driver. 1.00 = how fast the game made him. Lower = slower all lap, higher = faster everywhere. Small steps matter: 0.97 is already clearly slower.<br>*Technical:* Scales every corner target. The AI moves its own value every 4 s by 0.01 (traffic). |
| Corner speed x (CORNERINGMULT) | 0.85 .. 1.2 | yes | How fast he takes corners, as a multiplier. 1.00 = normal. Above about 1.10 he goes faster than his tyres can hold and starts sliding off.<br>*Technical:* Multiplies the tyre-grip corner speed. Above 1.10 = beyond tyre grip (live: 1.50 sent Coulthard off). |
| Corner speed + (CORNERINGADD) | -20 .. 20 | yes | Extra corner speed added on top (in metres per second). 0 = normal. Positive = quicker through every corner, negative = slower in every corner. A few m/s is a big change.<br>*Technical:* Adds to the corner speed (m/s). |
| Wet factor | 0.5 .. 1.2 | yes | How much he slows down in the rain. 1.00 = dry pace. Lower = more careful in the wet.<br>*Technical:* 1.0 = dry. Rain lowers it. |
| Max speed (m/s) | 5 .. 120 | yes | Top-speed limit on the straights (m/s; multiply by 3.6 for km/h). Use it to stop one car from being too fast on straights.<br>*Technical:* Top-speed cap. |
| Speed governor (m/s, lowers only) | 5 .. 120 | neutral 120 | A speed limiter like in the pit lane: he can never go faster than this. It can only slow the car down.<br>*Technical:* Like a pit limiter: can only slow the car, never lifts the real pit limiter. |

### Braking

| Control | Range | AI value shown | What it does in the game |
|---|---|---|---|
| Braking confidence (higher = later) | 0.5 .. 1.5 | yes | How late he brakes. Higher = brakes later (braver, faster, more mistakes). Lower = brakes earlier (safer, slower).<br>*Technical:* PASSING value used in the braking look-ahead: higher -> the AI believes it stops quicker -> brakes later. |
| Brake build-up speed | 0.05 .. 1.5 | yes | How quickly he presses the brake pedal fully. Higher = sharper stops, lower = softer, longer braking.<br>*Technical:* How fast brake pressure ramps toward its target (RACEABILITY). |
| Brake multiplier | 0 .. 2 | neutral 1 | Multiplies his brake pedal. 1.00 = normal. Below 1 = weaker braking (he overshoots corners). Above 1 = stronger.<br>*Technical:* Applied after the AI decides. |
| Throttle multiplier | 0 .. 1 | neutral 1 | Multiplies his throttle pedal. 1.00 = normal, 0.50 = never more than half throttle.<br>*Technical:* Applied after the AI decides. |

### Traffic

| Control | Range | AI value shown | What it does in the game |
|---|---|---|---|
| Aggression (TRACKAGGRESSION) | 0.01 .. 2 | yes | How hard he races other cars: higher = follows closer, attacks more, fights side by side. Lower = keeps his distance and gives way.<br>*Technical:* Feeds the lateral room / traffic thresholds in AISteering. |

### Driver traits

| Control | Range | AI value shown | What it does in the game |
|---|---|---|---|
| Steering lock (rad, lower = sharper) | 0.15 .. 1.2 | yes | How sharp his steering is. Lower = quicker, twitchier steering. Higher = lazy, smooth steering that can run wide.<br>*Technical:* steer = heading error / this. Live 0.4014 (23 deg). Lower = twitchier, higher = lazier. |
| Stuck patience (watchdog limit) | 5 .. 500 | yes | How long he keeps trying when he is stuck or facing the wrong way before he stops to sort himself out. Higher = more patient.<br>*Technical:* Wrong-way counter limit before the recovery stop (mode 1). Higher = keeps trying longer. |
| Pace floor (baseline) | 0.5 .. 1.2 | yes | His normal pace when driving alone in clean air. 1.00 = his full rating.<br>*Technical:* The pace multiplier drifts back to this when no traffic is processed. |
| Pace push (ceiling = floor + push) | 0 .. 0.3 | yes | How much extra pace he can find when chasing another car. 0 = never pushes harder.<br>*Technical:* How far above the floor traffic can push its pace. |
| Traffic pace gamble | 0 .. 1.5 | yes | How often he decides to push harder when stronger cars are near. Higher = pushes more often.<br>*Technical:* Random pace gate vs stronger cars nearby: higher = more often pushes up. |
| Crash tendency (.rcd CRASH) | 0 .. 100 | yes | Chance that he has a random incident in the race. 0 = never. It is a chance, so watch several laps.<br>*Technical:* Random incident gate in races (AICollision). Effect is statistical. |

### Overtaking and cooperation

| Control | What it does in the game |
|---|---|
| Overtake commitment | **AI decides** (game); **Commit: never chicken out** - an attack, once started, is not given up; **Chicken out at once** - every attack is abandoned immediately; **Never attempt, never ask** - never pulls out to pass and never asks others to move. |
| Cooperation override: never moves aside | Ignores blue flags, side-step requests **and team orders** (no yielding to a teammate). |
| Cooperation override: never asks others to move aside | Does not ask the car ahead to side-step. |
| No traffic lift | Clears this car's traffic target and keeps its throttle cap normal: no 75 % lift near other cars. |

### Car state

| Button | What it does |
|---|---|
| PARK (mode 4) | The car stops driving and waits (garage-wait state). Stays until Release state. |
| STOP (mode 7) | Brakes to a stop, then the game returns it to racing by itself. |
| Release state | Gives the mode back to the AI. |
| Release (apply-to cars) | Hands every lever of the drivers in the container back to the AI. |

---

## 7. Everything tab - every other per-car value

Built from the plug-in's catalog. **Find** filters by name or help, **only ticked** shows what is on. The dot shows how sure the meaning is (green proven/confirmed, yellow from the code, orange guess). Rows marked "via Quick" are the same control as on the Quick tab. Risky rows ask before switching on.

### Pace and skill

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Wet-weather skill | 0 .. 0.3 | held | confirmed | How much rain slows this driver. Higher = slower in the wet.<br>*Technical:* How much rain slows this driver: the wet factor is 1 - wetness x this. Higher = MORE slowdown (.rcd RAINABILITY / 100, scaled by difficulty). |
| Qualifying skill | 0 .. 1 | held | read from the game code | His qualifying talent. Only matters for sessions the game simulates, not a race you watch.<br>*Technical:* Driver's .rcd QUALIFYINGABILITY turned into pace for simulated sessions. No live AI-tick reader was found, so a watched race should not change. |
| Driver style variant | 0 = variant A, 1 = variant B, 2 = variant C | held | read from the game code | A small personal style choice the game rolls for each driver. Only changes his line a little near the start.<br>*Technical:* The driver's random style variant 0/1/2 (.rcd pick). AISteering turns it into a small steering-line bias (+1 / 0 / -1 m), applied only near the race start. |
| Pace floor (baseline) | 0.5 .. 1.2 | via Quick | confirmed | His normal pace in clean air. 1.00 = full rating.<br>*Technical:* The pace the AI drifts back to in clean air; the live pace value walks to it at +-0.01 every 4 s. 1.0 = its full rating. |
| Pace push above floor | 0 .. 0.3 | via Quick | read from the game code | How much faster he can go when chasing. 0 = never pushes.<br>*Technical:* How far above the floor the AI may push when it is chasing: target = floor + this. 0 = never pushes. |
| Traffic pace gamble | 0 .. 1.5 | via Quick | read from the game code | How often he pushes when stronger cars are near.<br>*Technical:* Probability gate for stepping the pace up near stronger cars: higher = pushes more often. |
| Crash tendency | 0 .. 100 | via Quick | confirmed | Chance of a random incident in the race. 0 = never.<br>*Technical:* The .rcd CRASH percentage feeding the AI's race incident gate. Statistical: raise it and watch several laps. 0 = never. **Asks first.** |
| Max speed cap (m/s) | 5 .. 120 m/s | via Quick | confirmed | Top-speed limit (m/s; x 3.6 = km/h).<br>*Technical:* Per-car top-speed cap in m/s. On track your value is the cap; the game may lower it in the pit lane. |

### Corners

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Corner speed target x | 0.8 .. 1.2 x | every tick | read from the game code | Multiplies every corner speed. 1.00 = normal; above 1.10 he slides off.<br>*Technical:* Multiplies every corner speed target each tick. 1.0 = the game; above ~1.1 is beyond tyre grip (live: 1.5 sent a car off). **Asks first.** |
| Grip multiplier (CORNERINGMULT) | 0.85 .. 1.2 | via Quick | confirmed | Corner speed multiplier. 1.00 = normal; above 1.10 he slides off.<br>*Technical:* Multiplies the tyre-force term of every corner speed target. The game bakes ~0.93-0.99; above ~1.1 is beyond grip. **Asks first.** |
| Grip offset (CORNERINGADD) | -10 .. 10 m/s | via Quick | confirmed | Corner speed added on top (m/s). Plus = quicker corners, minus = slower.<br>*Technical:* Adds m/s to every corner speed target. The game bakes ~1.4-3.5; negative slows every corner. **Asks first.** |
| Braking confidence (PASSING) | 0.5 .. 1.5 | via Quick | confirmed | How late he brakes. Higher = later and braver.<br>*Technical:* Braking look-ahead loss per step: higher = the AI believes it stops quicker, so it brakes later. 0.9..1.0 = game values. |
| Brake build-up (RACEABILITY) | 0.05 .. 1.5 | via Quick | read from the game code | How quickly he presses the brake fully.<br>*Technical:* How fast brake pressure ramps to its target; the AI uses 0.2 x this as its step. |

### Start

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Launch strength | 0.5 .. 1.5 | held | read from the game code | How hard he launches at the race start. Below 1 = softer start, 1.5 = full power. Set it before the lights go out.<br>*Technical:* Throttle ramp at the race start (.rcd STARTSDRY/STARTSWET blend). Below 1 caps the start throttle; 1.5 = no cap. Only read during the race start: set it before the lights. |

### Pit strategy

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Pit this lap (organic stop) | button | held | proven live | Calls him into the pits this lap for a normal stop. He drives in, gets serviced and rejoins.<br>*Technical:* Makes the car come in for a normal stop: it drives the pit lane, gets serviced and rejoins by itself. Nothing is forced. **Asks first.** |
| Planned pit stops | 0 .. 4 stops | held | read from the game code | How many pit stops he plans for the race.<br>*Technical:* How many stops the car plans for the race. |
| Next stop lap | 1 .. 200 lap | held | proven live | On which lap he makes his next stop.<br>*Technical:* Lap of the next planned stop. The car checks it every few seconds and pits when its lap reaches it. |
| Tyres for the next stop | 1 = default, 2 = hard, 3 = soft, 4 = intermediate, 5 = wet, 6 = monsoon | held | read from the game code | Which tyres he fits at the next stop.<br>*Technical:* Compound the car will fit at its next stop (ids from the game's tyre-name table; not yet tested live). |

### Reliability

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Engine life left | 0 .. 50000 s | held | read from the game code | How long his engine lasts (seconds of driving). 0 = the engine fails now.<br>*Technical:* Countdown to an engine failure, in seconds of running. Live healthy range 20000-41000 s; 0 = the engine goes now. **Asks first.** |
| Scheduled stall time | 0 .. 20000 s | held | read from the game code | When (session time) his car stalls and stops. 0 = now.<br>*Technical:* Session time (s) when this car stalls and parks (mode 4/5). 0 = now; 'AI none' (FLT_MAX) = never. The game disarms it after it fires. **Asks first.** |
| Brakes life left (s) | 0 .. 50000 s | held | confirmed | How long his brakes last. 0 = brake failure now.<br>*Technical:* Countdown to a brakes failure in seconds of running. 0 = failure now. **Asks first.** |
| Gearbox life left (s) | 0 .. 50000 s | held | confirmed | How long his gearbox lasts. 0 = gearbox failure now.<br>*Technical:* Countdown to a gearbox failure in seconds of running. 0 = failure now. **Asks first.** |
| Suspension life left (s) | 0 .. 50000 s | held | confirmed | How long his suspension lasts. 0 = suspension failure now.<br>*Technical:* Countdown to a suspension failure in seconds of running. 0 = failure now. **Asks first.** |

### Gearbox

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Highest gear allowed | 1 .. 7 | every tick | read from the game code | Highest gear he may use. Lower = slower on the straights.<br>*Technical:* The auto-shift never goes above this gear. Lower = slower on straights. |

### Traffic

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Aggression (TRACKAGGRESSION) | 0.01 .. 2 | via Quick | confirmed | How hard he races other cars. Higher = closer, braver; lower = gives room.<br>*Technical:* Traffic thresholds: follow gap, side-by-side window, overtake pressure. 0.69 = the game's AI-realism default. |

### Steering

| Control | Range / choices | Applied | Sure? | What it does in the game |
|---|---|---|---|---|
| Steering lock (rad) | 0.15 .. 1.2 rad | via Quick | confirmed | Steering sharpness. Lower = twitchier, higher = lazier.<br>*Technical:* Heading error / this = steer command. Lower = sharper/twitchier, higher = lazier. Live ~0.4014 rad (23 deg). |
| Stuck patience (watchdog) | 5 .. 500 | via Quick | read from the game code | How long he keeps trying when stuck before stopping to recover.<br>*Technical:* Wrong-way/stuck counter limit before the car stops itself to recover. Game default 50; higher = keeps trying longer. |
| Park / stop | 0 = race, 4 = park, 7 = stop | via Quick | confirmed | Park = car stops and waits. Stop = brakes to a halt, then races on. Race = normal.<br>*Technical:* 4 = park (garage wait; the car stops driving). 7 = brake to a stop, then back to race. 0 = normal. **Asks first.** |

---

## 8. Rules tab - the AI's driving rules (all AI cars)

Rules are constants inside the game's AI code. Ticking one redirects that piece of code to your value for the drivers in the container at that moment (WHOLE GRID = every AI car; needs plug-in v4, older plug-ins: every AI car). The Check tab lists who each rule acts on. Untick or "Reset ALL rules" gives the game value back. One-click buttons: **Traction control OFF**, **Steer lift OFF**, **Full-throttle rejoin**, **Tailgate before passing**.

**Never cap throttle at 75 % after yielding (all cars)** - fixes a game bug: after any yield or side-step the game leaves a car capped at 75 % throttle for the rest of the session. Ticked, every AI car keeps its normal cap; cars still yield and move aside. During an active yield the cut still applies unless "Traffic throttle cap" is also set to 1.0.

### Yielding

*How it works:* Blue flags: how close the lapping PLAYER must be before a lapped AI car makes room.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Blue-flag yield radius (m squared; 625 = 25 m) | 625 | 25 .. 3600 | read from the game code | How close YOU must get before a lapped AI car makes room for you. Bigger = it moves over earlier.<br>*Technical:* A lapped AI car starts making room for the lapping PLAYER inside this squared distance (race only). 100 = 10 m, 2500 = 50 m. |

### Overtaking & side-step

*How it works:* How far a car moves sideways when it is asked to step aside (and when it attacks: +/-3 m).

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Side-step offset, mode 2 (m) | 3 | 0 .. 8 | read from the game code | How far a car moves to the side when it is asked to let someone past (one side).<br>*Technical:* How far a car asked to side-step (Overtaking mode 2) moves its line. Lapped cars and qualifying traffic use it. |
| Side-step offset, modes 1/7/8 (m) | -3 | -8 .. 0 | read from the game code | Same, for the other side.<br>*Technical:* Same as above for the other side (modes 1, 7, 8). |

### Traffic awareness

*How it works:* The throttle cut while a car is yielding / side-stepping (game 0.75 x difficulty). The cap is never restored by the game afterwards: use "Never cap throttle at 75 %" too.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Traffic throttle cap (x difficulty) | 0.75 | 0.2 .. 1 | read from the game code | How much a car lifts off the throttle while it lets someone past. 1.00 = it does not lift at all.<br>*Technical:* While a car has an interaction target (yielding / side-stepping) its throttle is capped at difficulty x this. 1.0 = no lift. |

### Off-track rejoin

*How it works:* Outside the racing corridor + 1.5 m (pure geometry, not grass detection) the AI holds 50 % throttle (40 % at or below 90 km/h) until it is back.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Off-track throttle above 90 km/h | 0.5 | 0.2 .. 1 | confirmed | Throttle a car uses to get back on track after running wide at speed. 1.00 = full throttle (can spin on the grass).<br>*Technical:* When an AI car is off the racing corridor (+1.5 m) and not braking, the game holds its throttle at this value until it is back on track. 1.0 = full throttle rejoin (risk: spins on grass). |
| Off-track throttle at 90 km/h or less | 0.4 | 0.1 .. 1 | read from the game code | Same, when it is slow (90 km/h or less).<br>*Technical:* Same rule for slow cars (at or below 25 m/s = 90 km/h). |

### Traction control

*How it works:* Two wheel-grip readings (1.0 = gripping) are blended (70 % worse wheel), smoothed, and below 0.9 the throttle is pulled toward a safe value, harder as grip falls (full cut at 0.45). Mostly 1st-2nd gear wheelspin.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| TC kicks in below grip | 0.9 | 0 .. 1 | confirmed | Traction control. 0 = OFF (more wheelspin out of slow corners). Higher = it steps in earlier.<br>*Technical:* Traction control acts when the smoothed wheel grip (1.0 = full grip) drops below this. 0 = traction control OFF. Higher = earlier, more cautious. |
| TC cut steepness | 1.11111 | 0.5 .. 3 | confirmed | How hard traction control cuts the power once it acts. Lower = gentler.<br>*Technical:* How hard the cut grows as grip falls: depth = 2 x (1 - this x grip), clamped 0..1. Game 1.111 = no cut at 0.9, full cut at 0.45. Lower = gentler. |
| TC only above speed (m/s) | 4 | 0 .. 40 | confirmed | Traction control only works above this speed. Raise it to allow wheelspin when pulling away.<br>*Technical:* Traction control is skipped below this speed (m/s; 4 = 14 km/h) unless the car is stuck. Raise it to allow wheelspin when launching. |
| TC weight of the worse wheel | 0.7 | 0 .. 1 | confirmed | Technical traction-control detail. Leave it unless you know why (worse + better = 1).<br>*Technical:* The two grip readings are blended; this is the share of the WORSE one. With the next slider they should add to 1.0. Higher = reacts to the slipping wheel. |
| TC weight of the better wheel | 0.3 | 0 .. 1 | confirmed | Technical traction-control detail. Leave it unless you know why (worse + better = 1).<br>*Technical:* Share of the BETTER grip reading in the blend (game 0.3). Keep worse + better = 1.0. |
| TC brake-on-slip gain | 4 | 0 .. 10 | read from the game code | Technical traction-control detail. Probably has no visible effect.<br>*Technical:* Brake added = this x throttle removed by the cut (output 0x4028). Never seen firing in 90 559 Monza samples: probably no visible effect. |

### Steer lift

*How it works:* Throttle x (1 - 0.7 x steering) while steering more than 10 % above 72 km/h.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Steer lift from steering above | 0.1 | 0 .. 1 | confirmed | Cars lift off the throttle while steering hard. This sets how much steering it takes. 1.00 = never lifts.<br>*Technical:* Throttle is lifted while steering more than this (0..1). 1.0 = steer lift OFF. |
| Steer lift only above speed (m/s) | 20 | 0 .. 100 | confirmed | The steering lift only happens above this speed.<br>*Technical:* Steer lift only acts above this speed (m/s; 20 = 72 km/h). |
| Steer lift amount | 0.7 | 0 .. 1 | confirmed | How much throttle they lift while steering. 0 = no lift (full throttle through corners).<br>*Technical:* throttle x (1 - this x steering). Game 0.7: half-lock steering = 35 % less throttle. 0 = steer lift OFF. |

### Following & overtake decision

*How it works:* How a follower decides to pull out: within (aggression+1) x 20 m of the car ahead an attack PATIENCE builds up each tick (cap 450) and drains outside; the car switches lane (+/-3 m) when its pass-cost score drops below 500 - patience. Closer than 4 m directly behind it backs off 5 m/s and holds. Alongside = gap under 5.5 m. "Tailgate before passing" sets 1.5 m / 1 m/s / threshold 900.

| Rule | Game value | Range | Sure? | What it does in the game |
|---|---|---|---|---|
| Too close behind (m) | 4 | 0.5 .. 15 | read from the game code | How close a car follows the car in front before it backs off (metres). Lower = sits right on the gearbox.<br>*Technical:* Closer than this directly behind (overlap under 1.25 m) the AI backs off and treats the car as "hold". Lower = sits closer to the gearbox before deciding (more crash risk). |
| Back-off when too close (m/s) | 5 | 0 .. 20 | read from the game code | How much it slows down when it is too close. 0 = never backs off.<br>*Technical:* When too close, the AI aims this much below its own speed. 0 = no back-off (it keeps pushing). |
| Attack interest range (m per aggression+1) | 20 | 5 .. 100 | read from the game code | How close it must be to the car ahead before it starts thinking about a pass.<br>*Technical:* Within (aggression+1) x this metres of a car ahead, attack patience builds up; outside it decays. Game: 40 m at aggression 1. |
| Patience decay outside range (per tick) | 5 | 0 .. 50 | read from the game code | How quickly it forgets about attacking when the car ahead gets away.<br>*Technical:* How fast the built-up attack patience drains when the car ahead is out of range (40 ticks/s). |
| Attack threshold (higher = stays in line longer) | 500 | 50 .. 2000 | read from the game code | How long it waits in line before pulling out to pass. Higher = waits longer.<br>*Technical:* The AI switches lane (+/-3 m) when its pass-cost score is below this minus the built-up patience. HIGHER = it follows in line longer before pulling out; lower = pulls out early. |
| Patience cap | 450 | 0 .. 2000 | read from the game code | Technical overtaking detail. Leave it unless you know why.<br>*Technical:* Maximum built-up attack patience (game caps at 450, so the threshold never drops below 50). Both the compare and the clamp store use this. |
| Alongside below gap (m) | 5.5 | 0 .. 20 | read from the game code | From which gap two cars count as side by side.<br>*Technical:* Below this gap (and laterally apart) the pair counts as alongside (modes 7/8); above it the follower is in approach (16). |
| Patience kept when dropping to approach | 0.85 | 0 .. 1 | read from the game code | How much attacking mood a car keeps when it drops back. 1.00 = keeps it all.<br>*Technical:* When the follower falls back to approach, its built-up patience is multiplied by this. 1.0 = keeps it all; 0 = starts over. |

---

## 9. Presets tab

- **Ready-made presets** load in one click (**LOAD**, which also turns ARM on). Each card says plainly what it
  changes and **who**: an orange **WHOLE GRID** chip = always every AI car (e.g. "No 75 percent throttle cap",
  "Traction control OFF", "Tailgate before passing", "Racier grid"); a cyan chip = whoever is in the container
  when you load it (e.g. "Brave overtaker", "Late braker").
- **Load at every session start** - pick one preset; it loads by itself whenever a session starts (and turns
  ARM on). **Turn off** stops that. Example: make "No 75 percent throttle cap" load for every race.
- **Save what is active now as my preset** - saves the levers that are on for the driver shown in the list, the
  ticked rules and the 75 % fix, as `presets\<name>.json` in the toolkit folder. Choose whether it later applies
  to whoever is selected or always to the whole grid. Presets survive toolkit updates; presets saved with older
  versions still load.
- **Instant tricks** - timed one-offs (a lift, running wide) for the container; they switch back by themselves.

## 10. Drive tab - drive an AI car yourself

**DRIVE MODE** hands chosen controls of the selected car to you (never more than one car):

- **I control throttle / brake / steering / gears** - tick what you take over; the AI keeps the rest.
- Sliders, or **keyboard control** (default keys: throttle I, brake K, steer J / L, gears U / O,
  RELEASE ALL End; **Change keys...** to rebind; keys work while the game has focus).
- **invert steering** if left/right feels wrong.

## 11. Recipes

| Goal | Do this |
|---|---|
| A driver who overtakes hard | Quick: Overtake commitment "Commit", Aggression 1.6-2.0, Braking confidence 1.10-1.15, Pace +0.02-0.04, Cooperation "never moves aside" (also cancels team orders). Rules: "Never cap throttle at 75 %". |
| Followers that tuck in before passing | Rules > **Tailgate before passing** (1.5 m, back-off 1 m/s, threshold 900). |
| Ragged, wheelspinning exits | Rules: TC kicks in below grip 0, Steer lift amount 0. |
| Bogged-down, cautious exits | Rules: TC kicks in below grip 1.0, TC cut steepness 2.0-2.5, Steer lift amount 0.9-1.0. |
| Full-throttle rejoin after running wide | Rules > **Full-throttle rejoin** (off-track throttle 1.0). |
| Make a car pit now | Everything > Pit strategy > **Pit this lap** (an organic stop: it drives in, is serviced, rejoins). |
| An engine failure | Everything > Reliability > Engine life left 0 (asks first). |
| Stop/park a car | Quick > Car state PARK / STOP; Release state to give it back. |

## 12. Troubleshooting

| You see | Do this |
|---|---|
| "Game not found" | Start F1 Challenge. Run the toolkit with the same rights as the game (it asks for administrator). |
| "Game found, no race running" | Start a session; the viewer appears by itself. |
| "Plugin not loaded" | Start the game once after installing; check Lightloader > Mods shows "Driver AI Toolkit (game plug-in)" turned on. |
| Status "not in DLL" | Your game plug-in is older than the toolkit: Lightloader > Tools > Update (or run the .bat again with the game closed). |
| Status "ARM is off" | Tick ARM. |
| Status "blocked" | The car is not an AI car, or the session is not offline single-player. |
| Banner "Game paused" | Levers resume when you unpause. |
| A car does nothing | Retired, parked or in the garage cars do not run the AI planner (red banner). |

## 13. Limits and honesty

- Offline single-player only. The plug-in refuses to act in multiplayer sessions; that refusal has not yet
  been tested on a real LAN.
- Every value's meaning is marked with how sure we are: proven live, confirmed, read from the game code, or a
  guess. Big values can make cars crash, spin, stall or stop - that is part of experimenting.
- Rules (section 8) are code constants. A rule ticked while the container is ONE DRIVER or SELECTED DRIVERS acts
  on those drivers only (plug-in v4); with WHOLE GRID, or an older plug-in, on every AI car.
