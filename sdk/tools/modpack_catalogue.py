"""
modpack_catalogue.py — build `mods/_modpack/`, the source-of-record for every
shipped mod, so the pack can be rebuilt from a copy-paste.

WHY THIS EXISTS
    `mods/` holds 60+ .dll builds and 40+ .c revisions with no marker saying
    which one is current. `red_flag/inject.bat` points at `gr36.dll` while the
    folder contains sources up to `gr45.c` and a newer `redflag.c`; the header
    comment inside `redflag.c` still says "gr44.c"; `safety_car/sc2.c` still says
    "sc1.c". A community release cannot be cut from that.

    This tool freezes one CURRENT source per mod into `mods/_modpack/<mod>/`,
    beside a generated `MOD.md` that lists every function, every game address the
    mod touches, its hotkeys and its exact build line — all extracted from the
    source, never authored here. Rebuild = open source.c, paste, run the build
    line printed in MOD.md.

    Nothing is deleted. `mods/<mod>/` keeps its whole revision history; this is a
    curated view over it.

CHOOSING THE CURRENT SOURCE
    Resolved by the CURRENT table below, one entry per mod, each carrying the
    evidence for the choice. Three signals, in priority order:
      1. an explicit statement in the mod's own inject.bat or README
      2. the newest .c that has a same-named .dll built after it
      3. newest .c
    Where the signals CONFLICT the entry says so and the conflict is reprinted
    into MOD.md — an unresolved "which build ships" is a release blocker, not a
    detail to paper over.

USAGE
    python tools/modpack_catalogue.py build     # write mods/_modpack/
    python tools/modpack_catalogue.py check     # report drift, write nothing
"""

import os
import re
import shutil
import sys
import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MODS = os.path.join(ROOT, "mods")
OUT = os.path.join(MODS, "_modpack")


# mod key -> (subdir, source file, shipped dll, status, evidence for "current")
#
# `status` is copied from the mod's own README/_README row — it is the project's
# existing verdict, not a fresh judgement. A mod that is not in-game proven must
# say so here, because the pack must never ship an untested DLL silently.
CURRENT = {
    "commentary_retire": dict(
        dir="extended_vanilla_features/dll", src="retire_commentary.c",
        dll="retire_commentary_v5.dll",
        status="IN-GAME CONFIRMED (2026-07-28) — PWS_RETIRE02 heard by ear",
        evidence="dll/README.md names v5 as the working build; retire_commentary.c "
                 "is the only source. inject.bat points at retire_commentary.dll "
                 "(unversioned) — CONFLICT: confirm which of the two .dll files v5 is.",
    ),
    "grid_reorder": dict(
        dir="grid_reorder", src="gr28.c", dll="gr28.dll",
        status="IN-GAME CONFIRMED (2026-07-22)",
        evidence="gr28 is the confirmed build and mods/_README.md already calls it "
                 "the current one. gr29 is NOT used by the pack from 2026-08-01: it "
                 "is gr28 plus the red-flag HUD sprite, which repaints the "
                 "blue-flag sprite by writing 0x4D8338 in the HUD flag renderer. "
                 "Pulling red_flag from the pack while shipping gr29 would have "
                 "left that write in place. gr28.c contains neither the sprite "
                 "patch nor veh+0x4108. CONFLICT: inject.bat still points at "
                 "gr36.dll, which has no source in this folder.",
    ),
    "safety_car": dict(
        dir="safety_car", src="sc2.c", dll="sc2.dll",
        status="BUILT, NOT RACE-TESTED (2026-07-25)",
        evidence="inject.bat states outright: 'Current build = sc2.dll. sc1.dll is "
                 "the superseded first build'. Header inside sc2.c still says sc1.c.",
    ),
    "no_tab_pause": dict(
        dir="no_tab_pause", src="no_tab_pause.c", dll="no_tab_pause.dll",
        status="single build, no revision history",
        evidence="only source in the folder.",
    ),
    "vkeys": dict(
        dir="vkeys", src="vkeys_kb.c", dll="vkeys_kb.dll",
        status="working build; vkeys.c/vkeys_dbg.c are the disproven DirectInput "
               "attempts kept for the record",
        evidence="the source header states vkeys_kb is 'the working version' and "
                 "explains why the other two are dead ends.",
    ),
    "engine_blowup": dict(
        dir="engine_blowup", src="engine_blowup.c", dll=None,
        status="FACTS ONLY — not poke-proven in a race, needs SessionType==7",
        evidence="only source; no .dll has ever been built.",
    ),
    "ai_pit_retire": dict(
        dir="ai_pit_retire", src="ai_pit_retire.c", dll="ai_pit_retire_v3.dll",
        status="PARTIAL — forced write set does NOT produce a real retirement "
               "(car stuck at pit box, not garage). Superseded in practice by the "
               "arm-the-trigger method in red_flag/safety_car.",
        evidence="only source; v3 is the newest .dll. Kept for the documented "
                 "negative result, not as a shippable feature.",
    ),
}

# Mods deliberately NOT in the pack, and why. Recorded so the omission is a
# decision on the record rather than an oversight.
EXCLUDED = {
    "red_flag": "OWNER'S DECISION 2026-08-01 — pulled from the pack. The red flag "
                "is not a finished feature and it is invasive when it is simply "
                "present: loading the DLL repaints the blue-flag sprite red by "
                "writing 0x4D8338 inside the HUD flag renderer, binds Numpad 1/2/3 "
                "for the whole session, and can re-grid the field. It must run "
                "only when it is deliberately injected, never as part of the "
                "default pack. Source and builds stay in mods/red_flag/ and are "
                "injected by hand: mods\\red-flag-dll\\inject.exe mods\\red_flag\\redflag.dll",
    "f1cui_web_remote": "developer tool (web telemetry bridge), not a gameplay mod; "
                        "3157-line source with bundled JNA binaries",
    "red-flag-dll": "injector only, shipped separately as the pack's loader",
    "ai_wing_retire": "investigation notes, no mod",
    "sdk": "headers, consumed by the mods rather than shipped as one",
}


HEADER_RE = re.compile(r"/\*(.*?)\*/", re.S)
# `void foo(int a) {`  /  `static DWORD WINAPI thread_main(LPVOID p) {`
FUNC_RE = re.compile(
    r"^((?:static\s+|extern\s+)?[A-Za-z_][A-Za-z0-9_ \*]*?)\s*"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\(([^;{]*)\)\s*\{\s*$")
# `#define OFF_SPEED 0x4440   /* FACT, m/s */`
DEFINE_RE = re.compile(
    r"^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*?)\s*(?:/\*(.*?)\*/|//(.*))?\s*$")
HEX_RE = re.compile(r"0x[0-9A-Fa-f]{3,8}")
VK_RE = re.compile(r"\bVK_[A-Z0-9_]+\b")
BUILD_RE = re.compile(r"^\s*\**\s*(.*gcc\.exe.*)$", re.M)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def header_block(text):
    """The source's own leading /* ... */ — the author's words, kept verbatim."""
    m = HEADER_RE.search(text)
    if not m or m.start() > 40:
        return ""
    body = m.group(1)
    lines = []
    for line in body.splitlines():
        line = re.sub(r"^\s*\*ance?", "", line)
        line = re.sub(r"^\s*\*+ ?", "", line)
        lines.append(line.rstrip())
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return "\n".join(lines)


def functions(text):
    """(line_no, return_type, name, args, preceding comment) for each definition."""
    out = []
    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = FUNC_RE.match(line)
        if not m:
            continue
        ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
        if name in ("if", "for", "while", "switch", "else", "do", "return"):
            continue
        # Walk back over a contiguous comment block directly above.
        doc = []
        j = i - 1
        while j >= 0:
            prev = lines[j].strip()
            if prev.startswith("//"):
                doc.insert(0, prev[2:].strip())
            elif prev.startswith("*") or prev.startswith("/*"):
                doc.insert(0, prev.lstrip("/* ").rstrip("*/").strip())
            elif prev.endswith("*/"):
                doc.insert(0, prev.rstrip("*/").lstrip("/* ").strip())
            else:
                break
            j -= 1
        out.append((i + 1, ret, name, args, " ".join(d for d in doc if d)))
    return out


def addresses(text):
    """#defines whose value contains a hex literal — the game memory the mod touches."""
    out = []
    for line in text.splitlines():
        m = DEFINE_RE.match(line.strip())
        if not m:
            continue
        name, value = m.group(1), m.group(2).strip()
        if not HEX_RE.search(value):
            continue
        comment = (m.group(3) or m.group(4) or "").strip()
        out.append((name, value, comment))
    return out


def hotkeys(text):
    keys = sorted(set(VK_RE.findall(text)))
    return [k for k in keys if k not in ("VK_LBUTTON", "VK_RBUTTON")]


def build_line(text):
    m = BUILD_RE.search(text)
    return m.group(1).strip() if m else ""


def render_mod_md(key, info, src_path, text):
    hdr = header_block(text)
    funcs = functions(text)
    addrs = addresses(text)
    keys = hotkeys(text)
    build = build_line(text)
    rel = os.path.relpath(src_path, ROOT).replace("\\", "/")

    L = []
    L.append("# %s\n" % key)
    L.append("<!-- GENERATED by tools/modpack_catalogue.py — DO NOT EDIT.")
    L.append("     Edit the source, then rerun: python tools/modpack_catalogue.py build -->\n")
    L.append("| | |")
    L.append("|---|---|")
    L.append("| Source of record | `source.c` (copy of `%s`) |" % rel)
    L.append("| Shipped binary | %s |" % ("`%s`" % info["dll"] if info["dll"] else "*none built*"))
    L.append("| Status | %s |" % info["status"])
    L.append("| Why this is the current source | %s |" % info["evidence"])
    L.append("")

    if build:
        L.append("## Build\n")
        L.append("```bash")
        L.append(build)
        L.append("```\n")
    else:
        L.append("## Build\n")
        L.append("> **No build line in the source header.** Add one — the pack cannot")
        L.append("> be rebuilt from a copy-paste without it.\n")

    if keys:
        L.append("## Hotkeys\n")
        L.append(", ".join("`%s`" % k for k in keys) + "\n")

    L.append("## What it does — the source's own header, verbatim\n")
    L.append("```")
    L.append(hdr if hdr else "(no header comment)")
    L.append("```\n")

    L.append("## Functions (%d)\n" % len(funcs))
    L.append("| Line | Function | Returns | What |")
    L.append("|-----:|----------|---------|------|")
    for line_no, ret, name, args, doc in funcs:
        sig = "%s(%s)" % (name, args.replace("|", "\\|"))
        L.append("| %d | `%s` | `%s` | %s |" % (
            line_no, sig, ret, (doc or "—").replace("|", "\\|")))
    L.append("")

    L.append("## Game memory it touches (%d)\n" % len(addrs))
    if addrs:
        L.append("Extracted from the `#define`s. Trust tags are the author's own —")
        L.append("verify against `name_index.json` before depending on one.\n")
        L.append("| Symbol | Value | Note |")
        L.append("|--------|-------|------|")
        for name, value, comment in addrs:
            L.append("| `%s` | `%s` | %s |" % (
                name, value.replace("|", "\\|"), (comment or "—").replace("|", "\\|")))
    else:
        L.append("No hex `#define`s — this mod does not address game memory directly")
        L.append("(it may still do so via `f1c_sdk.h` or inline literals).")
    L.append("")
    return "\n".join(L) + "\n"


def render_index(built, problems):
    L = []
    L.append("# _modpack — source of record for the mod pack\n")
    L.append("<!-- GENERATED by tools/modpack_catalogue.py — DO NOT EDIT. -->\n")
    L.append("Generated %s\n" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
    L.append("One folder per mod. Each holds `source.c` (the current source, copied")
    L.append("verbatim) and `MOD.md` (every function, every game address, the hotkeys")
    L.append("and the exact build line, all extracted from that source). Rebuilding a")
    L.append("mod is: open `source.c`, paste, run the build line in `MOD.md`.\n")
    L.append("`mods/<name>/` keeps the full revision history. This is a curated view;")
    L.append("nothing here is authoritative over the source it was copied from.\n")

    L.append("## Mods\n")
    L.append("| Mod | Source | Binary | Functions | Status |")
    L.append("|-----|--------|--------|----------:|--------|")
    for key, info, nfuncs in built:
        L.append("| [`%s`](%s/MOD.md) | `%s` | %s | %d | %s |" % (
            key, key, info["src"],
            "`%s`" % info["dll"] if info["dll"] else "—",
            nfuncs, info["status"]))
    L.append("")

    L.append("## Not in the pack\n")
    L.append("| Folder | Why |")
    L.append("|--------|-----|")
    for key, why in sorted(EXCLUDED.items()):
        L.append("| `%s` | %s |" % (key, why))
    L.append("")

    if problems:
        L.append("## Release blockers (%d)\n" % len(problems))
        L.append("Unresolved before this can ship to the community.\n")
        for p in problems:
            L.append("- %s" % p)
        L.append("")
    return "\n".join(L) + "\n"


def build(write=True):
    built, problems = [], []
    for key, info in sorted(CURRENT.items()):
        src_path = os.path.join(MODS, info["dir"], info["src"])
        if not os.path.exists(src_path):
            problems.append("`%s`: source `%s` not found" % (key, info["src"]))
            continue
        text = read(src_path)

        if info["dll"]:
            dll_path = os.path.join(MODS, info["dir"], info["dll"])
            if not os.path.exists(dll_path):
                problems.append("`%s`: shipped binary `%s` does not exist" % (key, info["dll"]))
            elif os.path.getmtime(dll_path) < os.path.getmtime(src_path):
                problems.append(
                    "`%s`: **binary is older than its source** — `%s` was edited after "
                    "`%s` was built. Rebuild before shipping." % (key, info["src"], info["dll"]))
        else:
            problems.append("`%s`: no binary has ever been built from this source" % key)

        if "CONFLICT" in info["evidence"]:
            problems.append("`%s`: %s" % (key, info["evidence"].split("CONFLICT:", 1)[1].strip()))
        if not build_line(text):
            problems.append("`%s`: source header carries no build command" % key)

        funcs = functions(text)
        if write:
            dest = os.path.join(OUT, key)
            os.makedirs(dest, exist_ok=True)
            shutil.copyfile(src_path, os.path.join(dest, "source.c"))
            with open(os.path.join(dest, "MOD.md"), "w", encoding="utf-8") as f:
                f.write(render_mod_md(key, info, src_path, text))
        built.append((key, info, len(funcs)))

    if write:
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, "INDEX.md"), "w", encoding="utf-8") as f:
            f.write(render_index(built, problems))
    return built, problems


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    built, problems = build(write=(cmd == "build"))
    for key, info, nfuncs in built:
        print("%-20s %-24s %3d functions" % (key, info["src"], nfuncs))
    print("\n%d mods, %d release blockers" % (len(built), len(problems)))
    for p in problems:
        print("  ! " + re.sub(r"\*\*|`", "", p))
    if cmd == "build":
        print("\nwrote %s" % OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
