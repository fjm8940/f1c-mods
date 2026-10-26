# RDC0.2.4 compatible-executable startup

Owner requests correction of unsupported executable identity refusal. Pipeline implement/verify in canonical race_data_collector. Root cause in source: exact SHA256 whitelist rejected any byte difference before broker discovery. Remove SHA whitelist; retain SDK PE-layout validation and broker ABI/capability negotiation because collector also uses reviewed fixed-address extra reads. Hash remains capture metadata. Actual changed-address engines are not claimed compatible. Screenshot does not identify the rejected EXE; exact customer executable was not provided.

Tests: unsupported host must fail layout, while a separate isolated host with compatible mapped headers and a different on-disk hash must reach broker discovery. Test changes only its own transient mapped PE header, not any game file; restores header after stop. Existing collector/storage/narrative tests retained. No SDK boundary weakened.

Preservation: complete source/tests/build/docs beforestates under workspace_organization_audit/RDC024_COMPATIBILITY_BEFORE_2026-09-15_210931. New build outputs preserved. Restore by preserving newer states then copying mapped beforestates; no deletion. Installation and exact hashes to follow only if verified. Native customer gameplay remains pending.

## Completed verification and installation

0.2.4 full build passed: [build](../trials/build_20260915_210931_543/), [installation/hash](../trials/build_20260915_210931_543/installation/INSTALLATION.json), [restoration](../trials/build_20260915_210931_543/installation/RESTORATION.json). Actual DLL loaded in two isolated processes; altered compatible mapped header reaches broker despite different file SHA, unmodified incompatible host refuses. No game EXE changed. Full core/storage/consumer/report tests pass. Same10 pre-existing workspace errors/0warnings.

Game closed at copy; installed exact hash AB6564B4C55BD3E8B60FB183E1E0641C37F2AC6DF6906E73A4993F0221100761. Existing INI and SC/RF unchanged. Native friend EXE/capture not supplied and remains pending. Local installed log actually showed successful0.2.2 exports and local EXE matched the old SHA; screenshot cannot be attributed to this local process. Private DLL to send is in send-to-friend; test-host refusal logs are deliberately not bundled. No production promotion.
