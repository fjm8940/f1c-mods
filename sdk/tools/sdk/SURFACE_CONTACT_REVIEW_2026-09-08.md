# Read-only wheel-contact material adapter — 2026-09-08

f1c_surface_contact.h mirrors the reviewed Red Flag v76 drivable_surface_contacts
consumer: vehicle+0x3674, four wheel-hit pointers stride0xF8; hit+0x54 material;
material+0x30/34/38 tag tuples road(6,21,27), wet road(7,22,27), rumble(11,21,27).
Source authority: development/mods/red_flag/trials/2026-09-08/manual-countdown-v76/
redflag.input.hardened.c around462-469 and7622-7641. No new native call/hook or game write.
Exact canonical executable only; caller passes bounded-copy callback on stable game thread.
Helper reports known contacts and drivable contacts separately. Null/unreadable chains and
address overflow rejected. Mixed road/runoff wheels preserve positive road evidence.
This evidence tier is reuse of existing reviewed native-layout consumer plus synthetic
memory validation, not fresh runtime proof. Materials do not establish passage width,
racing-line overlap, contact age, detached debris or a route usable by AI.

Canonical sdk-test integrates test_surface_contact.py via run_all.bat. Positive material
classes, mixed surfaces, invalid pointers/null/overflow tests pass. Consumed by Full Safety
Car v0.10.14 to corroborate slow incident candidates; game-side validation pending.
Existing broker ABI and binary unchanged. README/checkpoint indexed. Before-state map:
workspace_organization_audit/SC_COMPLEMENT_BEFORE_2026-09-08_164513/restoration.json.
