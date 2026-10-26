# Pit Stop Failures current state

2026-10-09: public **1.1.1**, shipped through Lightloader and the stable link, hashes externally verified. Production: production/mods/pitstop_failures/v1.1.1. [Canonical release evidence](../../../operations/release_management/pitstop_failures/RELEASE_1_1_1.md).

Source: release_candidates/pitstop_failures-v1.1.1-rc.1; final package under its release/ folder. Defaults unchanged; all parent/subtype weights, fuel-fire chance and 0-100% gate editable. Lightloader 2.2.9 exposes main chance 0-100 and all subtypes under Show advanced settings, supplies missing defaults, and persists on Save or upgrade. No manual INI copy needed. For fuel spills only, set spill/short fill to 100 and other three fuel subtype weights to 0. [Manager fix evidence](../../lightloader/v2.0.0-hub-dev.1/release_2.2.8_2026-10-09/README.md). Three wheel-gun WAVs included.

Automated gates passed; exact-build gameplay/audio/presentation remains pending. Owner explicitly requested shipment; no human pass is inferred. Earlier STATUS/README news is historical.

2026-10-09 cleanup: Lightloader 2.2.9 removed unimplemented fire and redundant override controls. One main chance control; old numeric overrides migrate to it on Save/upgrade. All 14 subtype weights grouped under parents with 32px indentation. [Cleanup evidence](../../lightloader/v2.0.0-hub-dev.1/release_2.2.9_2026-10-09/README.md). Human real-game/presentation acceptance pending.
