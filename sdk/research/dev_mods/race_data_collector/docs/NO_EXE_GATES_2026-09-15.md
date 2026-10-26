# RDC0.2.5: remove collector executable gates

Owner explicitly rejects remaining checksum/layout checks after0.2.4. That authorizes this exception to the former collector fixed-EXE policy. Remove all collector SHA/header/checksum acceptance tests; file SHA remains export metadata only. Broker ABI/capability discovery and registration precede sampling; no broker provider or other mods changed.

Regression: actual DLL in arbitrary test host plus matching-header host with deliberately wrong PE checksum must both reach telemetry-broker wait without executable refusal. No game executable is edited. Existing data tests retained. Reading correctly from an actually different memory layout is not established by removing these checks; customer native capture still requires verification.

Complete beforestates under workspace_organization_audit/RDC025_REMOVE_EXE_GATES_BEFORE_2026-09-15_211310. Retain prior builds and logs as evidence; restoration by preserving intervening state then restoring beforestates. No public promotion. Full build/installation evidence to follow.

## Completed

Full build and tests PASS: [build](../trials/build_20260915_211310_262/), [exact installed hash](../trials/build_20260915_211310_262/installation/INSTALLATION.json), [restoration](../trials/build_20260915_211310_262/installation/RESTORATION.json). Actual DLL accepts arbitrary host and deliberately wrong PE checksum through to broker wait; binary contains no unsupported-executable refusal string. Full data/report tests pass. Installed with game closed; INI, SC/RF and game EXE unchanged. Friend DLL in send-to-friend. No test logs or activation files bundled. Historical logs retained, not rewritten. Customer race capture pending; no public promotion. Workspace validator same10 pre-existing errors/0warnings.
