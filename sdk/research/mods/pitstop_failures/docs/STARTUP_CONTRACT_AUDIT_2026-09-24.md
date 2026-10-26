# Startup contract audit and correction — 2026-09-24

Owner asks whether the installed mod verifies specific Lightloader loading plus required game address patterns, and whether publication is safe. Product: Pit Stop Failures 1.0.2 rc.2, verification stage, publication held. No code or game-install changes in this audit.

## Verified evidence

The installed DLL in the owner-selected 99-02 test game hashes to 23F03E33F6700C4F20B468BBC1D5198075780E853E09E1100FBFA1F3CF96B8AA, matching the recorded candidate. Preserved source/live_client.cpp calls pit_compat::module() at startup. source/pit_compat.h checks actual load base, x86 PE32 headers, required sections and permissions, and 14 required code ranges read from the mapped game. Installer invokes the same compiled policy on EXE files. Whole-file game hashes, filename, timestamp and checksum are not gates. This is a bounded structural/code check, not proof that every dynamic pointer or runtime state is valid.

The actual live_client.cpp DllMain directly creates the worker thread. That worker checks game layout, then discovers broker ABI15 and capabilities; it does not perform a Lightloader host handshake or load-origin proof. The candidate source tree contains no Lightloader host verification. A broker dependency alone cannot establish Lightloader-specific loading.

## Correction

Earlier conversation and the 1.0.1 rc.4 report claimed that existing Lightloader host proof remained. That claim was incorrect for this source/build. This explicit correction supersedes it. Successful installation into four directories establishes installation correctness only. It does not establish either host-only enforcement or live gameplay acceptance.

## Decision and next gate

Not ready for publication under the owner's two-part contract. Add and test the supported Lightloader host verification path, retaining required-code/layout checks. Test genuine loader acceptance, missing/wrong host refusal, incompatible code/layout refusal, then exact rebuilt DLL startup and gameplay/co-mod behavior. Record the same general contract in development/AGENTS.md for future first-party mods; other mods are not audited or rewritten by this task. Public release remains unchanged.

Documentation restoration: before-states are under workspace_organization_audit/PIT_HOST_AUDIT_BEFORE_2026-09-24. No protected artifact was changed. This report is indexed from product README/STATUS. No new gameplay evidence or stage promotion is inferred.

Wasted-effort check: none observed for repeated large reads, broad builds/tests, avoidable failed commands, or polling. This targeted source audit identified an earlier unsupported assertion that installation testing could not establish.
