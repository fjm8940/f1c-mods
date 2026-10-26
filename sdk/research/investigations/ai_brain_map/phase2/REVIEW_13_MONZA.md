# Claude review 13 — Monza live validation (2026-10-02)

Verdict: **PASS. Braking model closed.**

- Corrected braking step `v -= seg_len/v * 0x44CC * TireForce(v, wp+116, wp+120, wp+112) * 0.915` replayed with
  tireforce.py + cars_static.jsonl: meas/pred median 1.000 (n=1936), 85.4 % within +-20 % (mid steps 93.4 %, last steps
  67.3 % = small-loss ratio noise). Promoted to OBSERVED (finding ai-brain-braking-step-live-confirmed-2026-10-02).
- Steering law third track: 99.4-99.9 %. Opponent record pairing by id: 63191/63191. Pit limiter per track
  (Imola 80, Monaco 60, Monza 100 km/h).
- Three tracks now cover: braking decision + braking curve, steering law, traffic records, pit sequence, watchdog /
  recovery / reverse, retire. Never seen: modes 2, 3, 5.

Phase 2 read-only work is complete. Next phase (owner): first WRITE stage - separate handoff from Claude.
