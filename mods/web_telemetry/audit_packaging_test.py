"""Fail-closed packaging guard regressions; only creates disposable TEMP fixtures."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
staged = Path(sys.argv[1]).resolve()
fixture = Path(tempfile.mkdtemp(prefix='f1c-package-guard-')).resolve()

def gate(path, preflight=True):
    args = [sys.executable, str(root/'verify_payload.py'), str(path)]
    if preflight:
        args.append('--preflight')
    return subprocess.run(args, capture_output=True, text=True).returncode

assert gate(root) != 0, 'workspace output must be refused'
assert gate(Path(os.environ['TEMP'])) != 0, 'TEMP root must be refused'
safe = fixture/'safe'
safe.mkdir()
assert gate(safe) == 0, 'dedicated TEMP build directory must be accepted'
target = fixture/'target'
target.mkdir()
sentinel = target/'sentinel.txt'
sentinel.write_text('unchanged', encoding='utf-8')
redirect = fixture/'redirect'
redirect.mkdir()
subprocess.run(['cmd', '/c', 'mklink', '/J', str(redirect/'payload'), str(target)],
               check=True, capture_output=True)
assert gate(redirect) != 0, 'payload junction must be refused before compilation/copy'
assert sentinel.read_text(encoding='utf-8') == 'unchanged'
assert sorted(p.name for p in target.iterdir()) == ['sentinel.txt']
for name in ('web_telemetry.dll','workbench_ui.js'):
    shutil.copyfile(staged/name, safe/name)
shutil.copytree(staged/'payload', safe/'payload')
assert gate(safe, False) == 0
(safe/'payload'/'unexpected-directory').mkdir()
assert gate(safe, False) != 0, 'extra directory must fail the exact install set'
print('PASS packaging audit: workspace/TEMP-root/junction/extra-directory refusals; safe TEMP accepted', flush=True)
