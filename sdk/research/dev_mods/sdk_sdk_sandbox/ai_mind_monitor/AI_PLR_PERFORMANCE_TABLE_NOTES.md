# PLR theoretical-performance-table inputs

Status: evidence note for the two player-profile settings. This does not
modify the active game installation or claim a fully recovered table-builder
algorithm.

## Verified profile values

The inspected clean-install profile `Save\\Matias\\Matias.PLR` contains:

```text
AI Max Load="20000.00000"   // Maximum total load to set up theoretical performance tables
AI Min Radius="17.30000"    // Minimum radius turn to set up theoretical performance tables
```

The executable's options initializer registers the same settings with default
values of `40000.0` and `20.0`, respectively. This establishes that they are
normal persistent AI-configuration inputs, not undocumented mod keys.

## Supported meaning

The comments and option registration agree: these values bound the numerical
domain used to construct the AI's theoretical vehicle-performance tables.
Those tables are upstream of racing-line target speed and therefore affect
where the AI expects to brake and return to throttle. They do **not** directly
mean “AI aggression,” “overtake chance,” or a slipstream setting.

- **AI Max Load** is the upper load boundary supplied to the table setup.
- **AI Min Radius** is the lower turn-radius boundary supplied to the table
  setup.

Extremely low values such as `0.10000` collapse both boundaries far outside
their normal 20,000--40,000 / about-20 operating range. The plausible result
is malformed or severely extrapolated performance data, which can make the
AI appear more committed to throttle, alter early-lap pace, and change traffic
behavior indirectly. That is a pathological calibration effect, not evidence
of a cleaner overtaking algorithm.

## Current confidence and next proof

Confidence is **confirmed for configuration identity and table-setup role**,
but **not yet sufficient for the exact table dimensions, interpolation, or
the direction of every resulting pace change**. The remaining work is to
recover the direct consumer/table-builder chain and compare fresh-session
tables for one normal and one altered value in a disposable development
configuration. No active game profile should be changed for this research.

## Static-corpus boundary (2026-08-18)

The canonical corpus currently resolves each token only to
`PlayerOptionsInitializer_004aa270`, where it is registered as a generic
floating-point option descriptor. The corpus's evidence query finds six
shipped `.PLR` occurrences for each key, but reports no extracted parser for
either key. Therefore neither a direct consumer nor a table-builder address
is claimed from this result; recovering that chain remains a required proof.

The registration context is now bounded further: during
`PlayerOptionsInitializer`, `FlagSetterWithGlobalUpdate_004A1DA0` places its
`a1` object pointer in `0x007CE2E8` while setting an object byte at `+0x10`;
`ShortComputation_004A1B70` uses the nonzero global only as a temporary gate
to append a descriptor to a linked list. The initializer clears the same
global through `FlagSetterWithGlobalUpdate(..., 0)` before it returns.
Therefore `0x007CE2E8` is a construction-time registration gate, **not** a
stable runtime options-object anchor. The recovered decompilation still does
not expose the two option-value storage offsets or the AI table-builder
reader.
