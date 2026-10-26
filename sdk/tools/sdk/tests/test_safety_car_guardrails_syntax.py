"""Compile the complete source-only v0.10.16 candidate; no DLL is emitted."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[3]
C=ROOT.parent/'development/mods/full_safety_car/trials/v0.10.16_guardrails'
p=subprocess.run([r'C:\msys64\mingw32\bin\gcc.exe','-fsyntax-only','-std=c11','-Wall','-Wextra','-Wno-unused-function','-I'+str(ROOT/'tools/sdk'),str(C/'safety_car_guardrails.c'),str(C/'target_check.c')],capture_output=True,text=True)
assert not p.returncode,p.stdout+p.stderr
print('Safety Car v0.10.16 complete candidate syntax: PASS (no DLL emitted)')
