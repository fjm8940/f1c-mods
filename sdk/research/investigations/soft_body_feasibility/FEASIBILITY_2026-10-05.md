# Soft-body physics feasibility (2026-10-05)

Stage: investigate. No code changed. Owner question: can soft-body physics be added to F1C?

## Verified evidence
- FACT: Engine already ships sphere-based vertex deformation, tuned by `SeasonData/damage.ini` `[PHYSICAL]` (RadiusAdd/RadiusMult/RadiusMax/AeroDiv/VertMult) and `[VERTEX]` (DefaultLimit, RestrictionSphere, ForceFieldSphere). Parser: `functions/VehicleDamageModel_004f24f0.c`.
- DERIVED (findings.jsonl, 2026-07-29): contact records at veh+0x40C, stride 0x20 ({type, severity, 1.0, flag, x, y, z, 0}), ~13 slots seen.
- FACT: Renderer is Direct3D 8 (`functions/Direct3DCreate8_0059b250.c`).
- Wheels and wings are separate rigid bodies (Wheel*/Part* MassInertia + CollParams), not deformable.

## Assessment (inference)
1. Data-only tuning (zero risk): softer/larger deformation via damage.ini. Already possible.
2. Visual soft body (feasible, medium): DLL hooks the vertex-deform/render path, runs own mass-spring or position-based solver on car mesh verts, seeded from veh+0x40C impacts. Cosmetic only. OPEN: locate function consuming veh+0x40C and car vertex buffer (D3D8 Lock path).
3. Physical soft body (not practical): replacing rigid-body chassis/tyre dynamics in a closed 2002 binary means rewriting integrator, collision response, AI and multiplayer sync. Cost very high, FPS and determinism risk.
4. Tyre carcass flex: would need hooking the tyre force model; deferred, no prior art.

## Next step if approved
Find the vertex-deformation applier (xrefs to veh+0x40C writes/reads), then prototype a spike DLL in `development\mods` that adds damped spring-back/jiggle to deformed verts.
