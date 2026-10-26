# Read-only AI interaction accessor

Public header f1c_ai_interaction.h. Exact original x86 vehicle layout only,
target SHA256 1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003.
Caller must enforce target identity, game thread, field stability and live identity.
The accessor copies only three fields from a supplied minimum0x4600-byte object,
rejects nonfinite floats and does not dereference the copied target pointer.
No native function call, write capability, steering API or ABI change.

Static evidence: investigations/ai_interaction_role/README.md, latest corrections;
raw Overtaking_0042AF70 writes target+0x45FC and lateral+0x4420;
OpponentProximityScanTeamRecognition also owns these fields. AIDriverStructInit
resets Passing+0x44CC, AIRacingLine consumes it in lookahead speed calculation.
Rechecked through canonical inspect during Safety Car research review.
Interaction target is not an overtaking or completed-pass indicator. Race
lap-ahead branch writes it on the yielding car. Drift is not writer attribution.

Safety consumer v0.10.3 samples bounded diagnostics only; does not reapply Passing
or alter control cadence. Broker ABI14 lacks a25ms AI-step subscription. A future
callback needs exact ABI/insertion-phase review, bounded workload, lifecycle and
replay guards, independent tests and native validation. Existing post is queued
work and must not be presented as equivalent to a per-AI-step callback.

Verification via canonical sdk-test includes immutable input, pointer opacity,
null/short buffers, nonfinite and output-unchanged-on-failure tests. Native traces
pending. Product evidence: development/mods/full_safety_car/AI_DIAGNOSTICS_2026-09-08.md.
