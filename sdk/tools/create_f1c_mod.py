#!/usr/bin/env python3
"""Instantiate an isolated native mod from the canonical SDK template."""
from pathlib import Path
import argparse
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "tools" / "sdk" / "templates" / "native_mod"

parser = argparse.ArgumentParser()
parser.add_argument("name", help="C identifier-style mod id, e.g. weather_probe")
parser.add_argument("--title", help="human-readable name")
parser.add_argument("--output", type=Path,
                    help="new directory; default mods/sdk_sandbox/<name>")
parser.add_argument("--language", choices=("c", "cpp"), default="c")
args = parser.parse_args()

if not re.fullmatch(r"[a-z][a-z0-9_]{1,47}", args.name):
    parser.error("name must match [a-z][a-z0-9_]{1,47}")
output = (args.output or ROOT / "mods" / "sdk_sandbox" / args.name).resolve()
if output.exists():
    print(f"REFUSED: destination already exists: {output}", file=sys.stderr)
    sys.exit(2)
output.mkdir(parents=True)

title = args.title or args.name.replace("_", " ").title()
try:
    output_display = output.relative_to(ROOT).as_posix()
except ValueError:
    output_display = str(output)
tokens = {
    "{{MOD_ID}}": args.name,
    "{{MOD_ID_UPPER}}": args.name.upper(),
    "{{MOD_NAME}}": title,
    "{{OUTPUT_PATH}}": output_display,
    "{{SOURCE_EXT}}": args.language,
    "{{COMPILER}}": (r"C:\msys64\mingw32\bin\g++.exe" if args.language == "cpp"
                       else r"C:\msys64\mingw32\bin\gcc.exe"),
    "{{LANG_FLAGS}}": ("-std=c++17" if args.language == "cpp" else "-std=c11"),
}
outputs = {
    "mod.c.in": f"{args.name}.{args.language}",
    "build.bat.in": "build.bat",
    "mod.json.in": "mod.json",
    "mod.ini.in": f"{args.name}.ini",
    "gitignore.in": ".gitignore",
    "README.md.in": "README.md",
}
for source_name, destination_name in outputs.items():
    text = (TEMPLATE / source_name).read_text(encoding="utf-8")
    for token, value in tokens.items():
        text = text.replace(token, value)
    (output / destination_name).write_text(text, encoding="utf-8", newline="\n")
print(f"Created isolated mod: {output}")
print(f"Next: python tools/f1c_mod.py build {output}")
