# Team Principal Workstation — Order Palette (design draft)

Status: **design only.** No technical implementation implied or promised here. This
document defines *what a team principal can plausibly order*, in the language a real
pit wall would use, for the 1999–2002 era that F1 Challenge simulates. Feasibility
against the game's memory model is a separate, later question (see the last section
for a deliberately non-committal note on that).

## Design intent

The Team Principal view is **not** a debug panel with buttons. It is a roleplay
workstation: you are the person on the pit wall who authorizes a call, and the race
engineer relays it. Three consequences for the design:

1. **You issue intent, not physics.** You order "let him past" — you do not set a
   speed multiplier. The mechanism is hidden; the fiction is the interface.
2. **Every order produces a radio message**, logged with lap number and driver, in
   period-appropriate phrasing. The log *is* the feature — half the pleasure of a pit
   wall is the transcript afterwards.
3. **Orders can be refused.** A driver on a charge who is told to hold station may
   acknowledge and comply, may argue, or may ignore it. This is the single most
   characterful thing the palette can do — Multi-21 is remembered precisely *because*
   it was disobeyed.

## Era constraints (1999–2002)

These shape which orders are period-correct. Getting this wrong makes the whole thing
feel like a modern-F1 mod wearing a 2001 skin.

| In period | Not in period |
|---|---|
| **Refuelling** — fuel load is the primary strategy lever | DRS |
| Tyre war (Bridgestone / Michelin), grooved dry tyres | ERS / battery deployment |
| Traction control (re-legalised mid-2001), launch control | Complex engine modes / "Strat 2" |
| Spare car (T-car) available | Parc fermé lockdown |
| 2-stop and 3-stop as genuine live choices | Mandatory compound rule |
| Engine "turn it down" for reliability — engines were fragile | Track-limits automated detection |

The big one: **because refuelling exists, fuel load and stop count are the strategy**,
not tyre-compound choice. An undercut in 2001 means a short fill and a banzai out-lap.

---

## A. Team orders (inter-driver)

The politically loaded category. These are the orders that make headlines.

| Order | Radio message | Meaning |
|---|---|---|
| **Hold Position** | "Hold position. Confirm, hold position." | Freeze the order between our two cars. The classic. |
| **Swap Positions** | "We need you to swap. [Driver] is on a different strategy." | Faster car through, with a reason given. |
| **Let Him Past** | "[Driver] is faster than you." | The Hockenheim 2010 formulation — deniable, unmistakable. |
| **Give The Position Back** | "You need to give that place back. Now, please." | After an off-track gain or an unfair move. |
| **Free To Race** | "You're free to race. Repeat, free to race." | Rescinds any hold. Both cars released. |
| **Do Not Fight Each Other** | "Race everyone else. Not each other." | Race the field, but no wheel-to-wheel between team-mates. |
| **Priority Car** | "[Driver] is our priority for the championship." | Declares one car senior — affects who gets the pit window. |

Design note: **Hold Position and Swap should carry a compliance risk.** A driver who
is ahead on merit, mid-charge, and in title contention is the one who refuses. That
refusal should appear in the radio log verbatim and cost you something — a sulking
driver who obeys next lap but loses two seconds doing it.

---

## B. Pace and conservation

| Order | Radio message | Meaning |
|---|---|---|
| **Push Now** | "This is the lap. Everything you've got." | Maximum attack, damn the consequences. |
| **Target Delta** | "Target plus three tenths." / "plus five." / "plus one second." | Lap-time target for controlled management. The workhorse order. |
| **Fuel Save** | "Start saving now. Lift and coast into the braking zones." | Trade pace for fuel. |
| **Tyre Management** | "Look after the rears. We need this set to the end." | Nurse the set to extend the stint. |
| **Turn The Engine Down** | "Turn it down. We need this engine to see the flag." | Reliability preservation. Very period. |
| **Bring It Home** | "Just bring it home. No heroics." | Maximum conservation, points secured, no risks. |
| **Build A Gap** | "We need four seconds in three laps. Starting now." | Undercut prep / pit-window creation. Named, quantified, time-boxed. |
| **Break The Tow** | "Get out of his dirty air, give yourself a second." | Drop back to cool the car and reset. |

Design note: **Target Delta is the most-used real order and should be the most
prominent control** — a small stepper (+0.2 / +0.5 / +1.0 / off) rather than a
one-shot button.

---

## C. Strategy and pit calls

| Order | Radio message | Meaning |
|---|---|---|
| **Box This Lap** | "Box, box, box." | Immediate. |
| **Box Next Lap** | "Box next time round. Confirm." | Gives the driver a lap's notice and the crew time. |
| **Stay Out** | "Stay out, stay out. We're extending." | Overrides a planned stop. |
| **Extend Stint** | "Give us three more laps." | Quantified extension. |
| **Cover [rival]** | "[Rival] has stopped. We're covering — box this lap." | Reactive stop to protect track position. |
| **Undercut [rival]** | "Short fill, and we need a banzai out-lap." | Pit first, use fresh rubber and light fuel. |
| **Overcut [rival]** | "Stay out, push now — we're going long on him." | Pit later, bank clean-air laps. |
| **Switch To 2-Stop / 3-Stop** | "We're going to Plan B — three stops." | Wholesale strategy change. |
| **Fuel Load** | "Short fill." / "Fill her up — we're going long." | Light = fast but early stop. Heavy = track position. **The core 2001 lever.** |
| **Tyre Selection** | "Prime." / "Option." / "Inters ready." | Compound for the upcoming stop. |
| **Abort The Stop** | "Stay out! Abort, abort!" | Crew not ready / rival came out ahead. |

Design note: **Plan A / B / C should be pre-declarable before the race.** Half the
fiction of a pit wall is that "Plan C" already means something specific to both of
you, so the radio call can be two words at the decisive moment.

---

## D. Racecraft

| Order | Radio message | Meaning |
|---|---|---|
| **Attack** | "He's struggling with the rears. Have a go." | Authorize the move. |
| **Defend** | "Defend the inside into turn one." | Hold position, defensive line. |
| **Don't Take Risks** | "Settle. The points matter more than the place." | Stop fighting, bank the result. |
| **Let Him Go** | "Don't fight him, he's on a different strategy." | Concede a place to a car you can't beat now. |
| **Blue Flags** | "Blue flags — leader coming, don't hold him up." | Lapped-car courtesy. |
| **Watch [driver]** | "Careful with [driver] — he's been erratic today." | Situational awareness. |

---

## E. Car management

| Order | Radio message | Meaning |
|---|---|---|
| **Brake Balance** | "Brake balance one click forward." | On-the-fly setup tweak. |
| **Cool The Car** | "We need temperatures down. Lift on the straights." | Overheating management. |
| **Damage Report** | "Talk to me about the car. What have we got?" | Request driver assessment. |
| **Check For Damage** | "Come past the pit wall slowly so we can look at you." | Visual inspection on the next lap. |
| **Traction Control Setting** | "TC to position two." | Period-correct after mid-2001. |

---

## F. Incident and conduct

| Order | Radio message | Meaning |
|---|---|---|
| **Retire The Car** | "Box and switch off. That's the end of our race." | Precautionary or terminal retirement. |
| **Precautionary Retirement** | "Bring it in — we're not risking the engine for eleventh." | Save the unit for the next round. |
| **Serve Penalty** | "We're taking the penalty this lap." | Acknowledge and serve. |
| **Report To Stewards** | "We're protesting that. Stay out of it." | Escalate rather than retaliate. |
| **Stay Calm** | "I know. We'll deal with it after the race. Drive." | Defuse a furious driver. |

---

## G. Weather

| Order | Radio message | Meaning |
|---|---|---|
| **Inters Ready** | "Intermediates are ready if you want them." | Pre-warn, driver's call. |
| **Wets Now** | "Box for wets. It's much worse in sector two." | Committed change. |
| **Stay On Slicks** | "Stay out — the shower's passing in two laps." | Gamble on a short rain window. |
| **Driver's Call** | "It's your call. You're the one out there." | Devolve the decision. Very characterful. |

---

## Workstation layout (proposed)

```
┌─ TEAM: <name> ────────────────────────────────────────────────┐
│  CAR 1  <driver>   P4   L32/53   Fuel: 18L   2 stops          │
│         Gap ahead +2.7   Gap behind −1.4   Tyres: 14 laps     │
│  [ Hold ] [ Swap ] [ Push ] [ Δ+0.5 ] [ Box ] [ Fuel ▾ ]      │
│                                                                │
│  CAR 2  <driver>   P9   L32/53   Fuel: 31L   1 stop           │
│         Gap ahead +0.9   Gap behind +5.2   Tyres: 6 laps      │
│  [ Hold ] [ Swap ] [ Push ] [ Δ+0.5 ] [ Box ] [ Fuel ▾ ]      │
├─ STRATEGY ─────────────────────────────────────────────────────┤
│  Plan A: 2-stop, L20 / L38     [ACTIVE]                        │
│  Plan B: 3-stop, L15 / L30 / L44                               │
│  Plan C: long first stint, react to weather                    │
├─ TEAM RADIO ───────────────────────────────────────────────────┤
│  L32  → CAR 1  "Hold position. Confirm, hold position."        │
│  L32  ← CAR 1  "...Understood. But I'm faster than him."       │
│  L30  → CAR 2  "Target plus three tenths."                     │
│  L28  ← CAR 2  "Rears are gone. I need to stop."               │
└────────────────────────────────────────────────────────────────┘
```

Principles:
- **Six buttons per car, not thirty.** The common orders are immediate; the rest live
  behind a dropdown. A pit wall under pressure needs the frequent call to be one click.
- **Context-gated.** "Box This Lap" greys out if the car is already in the pit lane.
  "Swap Positions" only appears if the team-mate is actually adjacent on track.
- **The radio log is the centrepiece**, not an afterthought — it is the artefact you
  read back after the race, and it should include the driver's replies.
- **Confirmation only for the irreversible.** Retire the car asks twice. Target delta
  does not.

## Driver compliance model (proposed)

The feature that turns a button panel into a story. Each driver gets a small set of
hidden traits — call them *obedience*, *ego*, *loyalty* — and orders are checked
against the situation:

| Situation | Likely response |
|---|---|
| Hold position, driver is behind and slower | Complies without comment |
| Hold position, driver is faster and title-chasing | Argues, may refuse outright |
| Swap positions, driver is the senior/priority car | Complies, tersely |
| Swap positions, driver is on a charge and just took the place | High refusal chance |
| Bring it home, car is damaged | Complies with relief |
| Push now, tyres are finished | "There's nothing left in these tyres, mate." |

Refusals should be **rare enough to matter and remembered when they happen**. A
compliance rate around 90% for reasonable orders and closer to 60% for a genuinely
unfair one produces the right texture.

---

## On feasibility

Deliberately out of scope for this draft, per the brief. Broadly, the orders above
fall into three buckets, and it is worth being honest that the split is currently
unproven rather than implying otherwise:

- **Likely reachable** — anything expressed as a pit call or a pace target, since the
  game already has pit-request and AI pace machinery that other mods in this corpus
  have driven before.
- **Uncertain** — orders that need the AI to *yield a specific position to a specific
  car*, which is a much more precise behaviour than "go slower".
- **Cosmetic regardless** — the radio log, driver replies, and compliance model need
  no game hooks at all. They are pure presentation, and they carry most of the
  roleplay value.

That last bucket is the important one: **a large fraction of what makes this feel like
a pit wall costs nothing technically.** Even if only the pit and pace orders ever
actually reach the AI, the workstation is still worth building — the transcript, the
plan declarations, and the driver personalities do the heavy lifting.

Note also that the write-side actions this would build on (`/action`: force pit,
force retire, VSC, red flag) are still marked **UNTESTED** in this mod's README from a
prior session. Verifying those live is the sensible prerequisite before committing to
any order that must actually reach the car.
