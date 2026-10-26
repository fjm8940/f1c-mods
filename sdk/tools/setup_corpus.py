"""Link a clone of the corpus repo into sdk/ so the SDK tools find it.

Usage: python sdk/tools/setup_corpus.py PATH_TO_CORPUS_CLONE

Creates a directory junction (Windows) or symlink for each corpus folder and
file inside sdk/. Nothing is copied or deleted; existing entries are left alone.
"""
import os, subprocess, sys
from pathlib import Path

SDK = Path(__file__).resolve().parents[1]


def link(src: Path, dst: Path) -> str:
    if dst.exists() or dst.is_symlink():
        return "exists"
    if os.name == "nt" and src.is_dir():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(src)], check=True, capture_output=True)
    elif os.name == "nt":
        os.link(src, dst)  # hard link; files must be on the same volume
    else:
        dst.symlink_to(src, target_is_directory=src.is_dir())
    return "linked"


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    corpus = Path(sys.argv[1]).resolve()
    if not (corpus / "functions").is_dir():
        print(f"not a corpus clone (no functions/): {corpus}")
        return 1
    for entry in sorted(corpus.iterdir()):
        if entry.name.startswith(".") or entry.name in {"README.md", "LICENSE"}:
            continue
        print(f"{link(entry, SDK / entry.name):7} {entry.name}")
    (SDK / "mods" / "sdk_sandbox").mkdir(parents=True, exist_ok=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
