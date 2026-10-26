# NN — <Topic>: <one-line research question>

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** YYYY-MM-DD · **Author:** <name> · **Status:** STATIC ONLY

## Abstract (TLDR)

<Plain language. No offsets, no function names, no jargon. What happens in practice,
why, and how. A player or rcd editor should finish this knowing exactly what changes
in-game and what knob to turn. 3–10 bullet points or short paragraphs.>

## 1. Introduction

<Research question, scope (in/out), motivation: what gameplay or modding problem this
answers. One paragraph of background a community reader needs.>

## 2. Method: static analysis

<Which parts of the game binary were examined and why. Describe the loader/consumer
functions in prose — the reader cannot see the source. Note where the raw data lives
(rcd key, config file, savegame). State tool chain in one line.>

## 3. Method: runtime verification

<Live experiment design: game state, what was read/written in memory, procedure,
observations, before/after values. If not yet done, write PENDING and describe the
planned experiment in 2–3 lines.>

## 4. Results

<The proven mechanism, step by step. Tables for values/offsets. Pseudocode for
formulas. Every claim carries a tier: FACT / CONFIRMED / DERIVED / GUESS.>

## 5. Discussion

<Caveats, ambiguities (e.g. unknown value scales), alternative interpretations,
threats to validity, open items.>

## 6. Conclusion

<Short, direct answer to the research question. What a modder/player should do
differently based on this.>

## Appendix A — Corrections to prior notes/tools

<Anything discovered to be wrong in earlier docs, SDK entries, or tool labels while
doing this research. Location, old claim, corrected claim.>

## References

- Game binary + version analyzed
- Sibling papers in this folder (by filename)
- External/community sources (with tier)

---
*Changelog: YYYY-MM-DD — initial (static only).*
