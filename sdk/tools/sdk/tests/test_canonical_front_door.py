#!/usr/bin/env python3
"""Enforce the single public tool contract without rewriting archives."""
from pathlib import Path
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
CONTRACT = json.loads((ROOT / "tools/sdk/F1C_TOOL_CONTRACT.json").read_text(encoding="utf-8"))


def fail(message):
    raise AssertionError(message)


def main():
    if CONTRACT["front_door"] != "python tools/f1c_mod.py":
        fail("front door changed without migrating the contract")
    forbidden = [
        re.compile(r"python\s+tools[\\/](?!f1c_mod\.py)[\w.-]+\.py", re.I),
        re.compile(r"cmd\s+/c\s+[^\r\n`]*build\.bat", re.I),
        re.compile(r"\binject(?:or)?(?:\.exe|\.bat)", re.I),
        re.compile(r"(?:MODE\s*=\s*|autolaunch\s+)QuickRace", re.I),
    ]
    for relative in CONTRACT["active_guidance"]:
        path = ROOT / relative
        if not path.is_file():
            fail(f"active guidance missing: {relative}")
        content = path.read_text(encoding="utf-8", errors="replace")
        if "tools/f1c_mod.py" not in content.replace("\\", "/"):
            fail(f"active guidance does not name canonical front door: {relative}")
        for line_number, line in enumerate(content.splitlines(), 1):
            for pattern in forbidden:
                if pattern.search(line):
                    lower = line.lower()
                    if (("quickrace" in lower or "inject" in lower) and
                            any(term in lower for term in ("do not", "never", "forbidden"))):
                        continue
                    fail(f"public bypass in {relative}:{line_number}: {line.strip()}")

    output = subprocess.check_output(
        [sys.executable, ROOT / "tools/f1c_mod.py", "sdk-info", "--json"],
        cwd=ROOT, text=True)
    info = json.loads(output)
    if info["front_door"] != CONTRACT["front_door"]:
        fail("sdk-info disagrees with tool contract")

    # Every agent entry point must carry the same freshness and prior-art rule.
    entry_points = ["AGENTS.md", "CLAUDE.md", "GEMINI.md", ".github/copilot-instructions.md",
                    ".cursor/rules/f1c-sdk.mdc", ".cursorrules",
                    "tools/sdk/LLM_OPERATING_CONTRACT.md"]
    required_phrases = ["start --json", "freshness.stale",
                        "evidence-query findings", "findings.jsonl", "RECORDED"]
    for relative in entry_points:
        content = (ROOT / relative).read_text(encoding="utf-8", errors="replace")
        for phrase in required_phrases:
            if phrase not in content:
                fail(f"agent entry point {relative} lacks required rule text: {phrase}")

    briefing = json.loads(subprocess.check_output(
        [sys.executable, ROOT / "tools/f1c_mod.py", "start", "--json"],
        cwd=ROOT, text=True))
    for key in ("freshness", "recent_checkpoint_entries", "evidence_rules"):
        if key not in briefing:
            fail(f"start briefing lacks {key}")
    if "freshness.stale" not in briefing["decision_rule"]:
        fail("start decision rule does not guard stale checkpoint objectives")
    if not any("findings.jsonl" in item for item in briefing["definition_of_done"]):
        fail("definition of done does not require recording findings")
    print("Canonical front-door contract passed.")


if __name__ == "__main__":
    main()
