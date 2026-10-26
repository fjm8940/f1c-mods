"""Reviewed Safety Car ABI/semantics and fail-closed SDK test-target selection."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('safety_sdk_front_door',ROOT/'tools/f1c_mod.py')
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
catalogue=mod.load_catalogue()
restart=catalogue['functions']['DynMan_Restart']
assert restart['trust']=='CONFIRMED' and restart['calling_convention']=='cdecl' and not restart['parameters']
for name in ('PassingNorm','TrackAggressionEffective'):
    assert catalogue['vehicle_fields'][name]['semantic']=='latched_control'
assert catalogue['vehicle_fields']['lapNumber'].get('semantic','state') not in ('command_input','latched_control')

project=ROOT/'mods/sdk_sandbox/interface_test'
manifest={'schema_version':1,'id':'interface_test','production':False,'language':'c',
          'target_sha256':mod.TARGET_SHA256,
          'hooks':[{'function':'DynMan_Restart','handler':'restart_handler',
                    'stolen_length':5,'expected_bytes':'68 6F 02 00 00'}],
          'writes':[{'object':'vehicle','field':name} for name in ('PassingNorm','TrackAggressionEffective')]}
mod.validate_manifest(project,manifest,exe=Path('absent-test-target.exe'),check_conflicts=False)
bad=json.loads(json.dumps(manifest))
bad['writes'].append({'object':'vehicle','field':'lapNumber'})
try:
    mod.validate_manifest(project,bad,exe=Path('absent-test-target.exe'),check_conflicts=False)
except mod.ModError as error:
    assert 'lapNumber' in str(error)
else:
    raise AssertionError('scoring output write was accepted')

# No child tests run here; verify the exact environment handed to the runner.
calls=[]
mod.run=lambda command,**kw: calls.append((command,kw))
fixture=Path(__file__).resolve()
expected=hashlib.sha256(fixture.read_bytes()).hexdigest().upper()
mod.TARGET_SHA256=expected  # approved identity in this isolated input-selection unit
args=argparse.Namespace(exe=str(fixture),suite='all')
mod.cmd_sdk_test(args)
assert len(calls)==1 and calls[-1][1]['env']['F1C_SDK_TEST_EXE']==str(fixture)
for invalid in (str(fixture.with_name('not-present-target.exe')),str(ROOT/'AGENTS.md')):
    try:
        mod.cmd_sdk_test(argparse.Namespace(exe=invalid,suite='all'))
    except mod.ModError:
        pass
    else:
        raise AssertionError('invalid test target accepted')
assert len(calls)==1
os.environ['F1C_SDK_TEST_EXE']='unauthorized inherited target'
mod.cmd_sdk_test(argparse.Namespace(exe=None,suite='qualifying-order'))
assert 'F1C_SDK_TEST_EXE' not in calls[-1][1]['env']
print('Safety Car reviewed ABI, input semantics, scoring-output refusal and explicit target selection: PASS')
