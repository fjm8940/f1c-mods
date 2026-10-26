"""Offline synthetic HTTP and dashboard regression checks, no game attachment."""
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time
from urllib.error import HTTPError
from urllib.request import Request, build_opener, ProxyHandler
urlopen = build_opener(ProxyHandler({})).open


def read_first_pass_line(proc):
    """First 'PASS '/FAIL line of a runtime_test run. Diagnostic lines the
    harness prints before it (PERF/PERFG, SIZES, the 4.4.0 PERFALL all-cars
    trace cost) are printed through instead of being mistaken for output."""
    for _ in range(12):
        line = proc.stdout.readline().strip()
        if not line:
            return ''
        if line.startswith(('PERF ', 'PERFG ', 'PERFALL ', 'SIZES ')):
            print(line, flush=True)
            continue
        print(line, flush=True)
        return line
    return ''


def build_synthetic_lap():
    """Mirror of the C harness lap builder: 11 channels, 3 samples, one
    deliberate mph/g/deg conversion per channel family."""
    def enc5(text):
        return bytes((c + 5) & 0xFF for c in text.encode('latin-1'))[::-1]
    b1 = b'DRIVER=Import Test VERSION=2003.14'
    b2 = b'RAIN=0,0'
    names = [b'time', b'distance', b'velocity', b'throttle', b'brake',
             b'engine rpm', b'gear', b'yaw', b'lat accel', b'x', b'z']
    rows = [(0.0, 100.0, 0.0, 0.0, 100.0, 0.0, 0.0, -180.0, 0.0, 100.0, 5.0),
            (0.1, 120.0, 22.371365, 50.0, 0.0, 8000.0, 3.0, 0.0, 0.5, 120.0, 6.0),
            (0.2, 140.0, 44.74273, 100.0, 0.0, 12000.0, 4.0, 90.0, -0.25, 140.0, 7.0)]
    out = b'CHECKSUM=0\r\n' + struct.pack('<I', len(b1)) + enc5(b1.decode('latin-1'))
    out += struct.pack('<II', 2, 0) + struct.pack('<5f', 0.1, 0, 0, 0, 0)
    out += struct.pack('<I', 7) + struct.pack('<ff', 1.0, 0.0)
    out += struct.pack('<5I', 0, 0, 0, 0, 0)
    name = b'lap desc$'
    out += struct.pack('<I', len(name)) + name
    out += struct.pack('<I', len(b2)) + enc5(b2.decode('latin-1'))
    out += struct.pack('<II', 1, len(names))
    for n in names:
        out += struct.pack('<I', len(n)) + n
    for r in rows:
        out += struct.pack('<11f', *r)
    return out


build = Path(sys.argv[1])
# ---- sidecar staging gate (rc.7): source == staged == manifest, byte-exact ----
component = Path(__file__).resolve().parent
manifest = json.loads((component/'VERIFICATION_MANIFEST.json').read_text(encoding='utf-8'))
sidecar_expected = manifest['sidecar']['sha256'].upper()
sidecar_source = (component/'workbench_ui.js').read_bytes()
sidecar_staged = (build/'workbench_ui.js').read_bytes()
assert hashlib.sha256(sidecar_source).hexdigest().upper() == sidecar_expected, 'source sidecar hash != manifest'
assert sidecar_staged == sidecar_source, 'staged sidecar is not byte-identical to the source (line-ending conversion?)'
assert hashlib.sha256(sidecar_staged).hexdigest().upper() == sidecar_expected, 'staged sidecar hash != manifest'
print('PASS sidecar staging byte-exact: source == staged == manifest '+sidecar_expected,flush=True)
# 4.3.0 viewer sidecar: same byte-exact rule, own manifest key.
viewer_expected = manifest['sidecar_viewer']['sha256'].upper()
viewer_source = (component/'telemetry_viewer.js').read_bytes()
viewer_staged = (build/'telemetry_viewer.js').read_bytes()
assert hashlib.sha256(viewer_source).hexdigest().upper() == viewer_expected, 'source viewer hash != manifest'
assert viewer_staged == viewer_source, 'staged viewer is not byte-identical to the source'
print('PASS viewer sidecar staging byte-exact: source == staged == manifest '+viewer_expected,flush=True)
# ---- batch 4 (U-15, AC-15): static delete audit. No hard-delete call anywhere in the storage code; every SHFileOperation
# request carries FOF_ALLOWUNDO. Runs before the server starts (no game, no network).
import re as _re
_storage_src = ''.join((component / n).read_text(encoding='utf-8') for n in ('storage_v2.inc', 'storage_writer.inc', 'storage_jobs.inc', 'storage_api.inc'))
for _bad in ('DeleteFile', 'RemoveDirectory', '_unlink', 'unlink(', 'remove(', 'rmdir(', '_rmdir(', 'SHDeleteFile'):
    assert _bad not in _storage_src, 'hard-delete call in storage code: ' + _bad
_fo = [m.start() for m in _re.finditer(r'FO_DELETE', _storage_src)]
assert _fo and all('FOF_ALLOWUNDO' in _storage_src[i:i + 400] for i in _fo), 'FO_DELETE without FOF_ALLOWUNDO'
_fl = [m.start() for m in _re.finditer(r'fFlags\s*=', _storage_src)]
assert _fl and all('FOF_ALLOWUNDO' in _storage_src[i:i + 120] for i in _fl), 'SHFileOperation flags without FOF_ALLOWUNDO'
print('PASS U-15 static delete audit: no hard-delete call in the storage code; every FO_DELETE and fFlags carries FOF_ALLOWUNDO', flush=True)

# ---- BACKEND 2 fix 4 payload gate: staging lives in the build dir (%TEMP%), never in the workspace ----
payload = build/'payload'
assert not (component/'payload').exists(), 'workspace payload dir must not exist (staging lives in %TEMP%)'
payload_files = sorted(p.name for p in payload.iterdir() if p.is_file())
assert payload_files == sorted(['web_telemetry.dll','workbench_ui.js','telemetry_viewer.js',
                                'web_telemetry.ini','web_telemetry.dll.mod.json']), payload_files
assert (payload/'workbench_ui.js').read_bytes() == sidecar_source
assert (payload/'telemetry_viewer.js').read_bytes() == viewer_source
assert (payload/'web_telemetry.dll').read_bytes() == (build/'web_telemetry.dll').read_bytes()
assert (payload/'web_telemetry.ini').read_bytes() == (component/'web_telemetry.ini').read_bytes()
assert (payload/'web_telemetry.dll.mod.json').read_bytes() == (component/'web_telemetry.dll.mod.json').read_bytes()
manifest_payload = manifest.get('payload')
assert manifest_payload, 'manifest payload section missing'
assert 'f1c-web-telemetry' in manifest_payload['dir'] and '%TEMP%' in manifest_payload['dir'], manifest_payload['dir']
for name in payload_files:
    got = hashlib.sha256((payload/name).read_bytes()).hexdigest().upper()
    assert manifest_payload['files'][name] == got, (name, manifest_payload['files'][name], got)
print('PASS payload gate: staging in %TEMP%, workspace keeps no payload, exactly 5 install files, byte-exact, manifest hashes match',flush=True)
import re
pe=subprocess.run([r'C:\msys64\mingw32\bin\objdump.exe','-p',str(build/'web_telemetry.dll')],
                  check=True,capture_output=True,text=True).stdout
imports=set(re.findall(r'DLL Name:\s*(\S+)',pe))
assert imports=={'KERNEL32.dll','msvcrt.dll','WS2_32.dll'}, imports
assert 'file format pei-i386' in pe
# Refuse to collide with an existing game telemetry listener.
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 18080))

# Enumerate local IPv4 interfaces before the timed synthetic server starts.
addresses_json = subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command',
    "Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } | Select-Object -ExpandProperty IPAddress | ConvertTo-Json -Compress"],
    check=True,capture_output=True,text=True).stdout.strip()
addresses = json.loads(addresses_json) if addresses_json else []
if isinstance(addresses,str): addresses=[addresses]
assert addresses, 'A real non-loopback interface is required for the remote-access regression'
remote_address = next((a for a in addresses if a.startswith('100.')), addresses[0])

# rc.17: the shipped default listens on 127.0.0.1 only; the remote-access
# regression runs with an ini that sets [Mod] Remote=1.
remote_ini = build/'remote_test.ini'
remote_ini.write_text('[Mod]\nRemote=1\nBindAddress=%s\n' % remote_address, encoding='ascii')   # M1: Remote=1 binds the private address only
run = subprocess.Popen([str(build/'runtime_test.exe'), 'serve', str(build/'web_telemetry.dll')], cwd=build,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                       env=dict(os.environ, TELEMETRY_INI=str(remote_ini)))
try:
    first = read_first_pass_line(run)
    assert first.startswith('PASS '), first + run.stdout.read()
    parsed = json.loads((build/'telemetry.json').read_text())
    assert len(parsed['c']) == 104 and parsed['version'] == '1.2.5'
    assert parsed['lhlim'] == 120
    assert 'vf' in parsed['c'][0] and 'tid' in parsed['c'][0] and 'secel' in parsed['c'][0]
    # Fix 1 (api_version 2): rpm/max_rpm are converted display RPM; r/mr stay
    # raw rad/s. A field named RPM must never carry rad/s.
    rpm_factor = 9.549296585513721
    for car in parsed['c']:
        if car['p'] <= 0: continue
        if car['r']:
            assert abs(car['rpm'] - car['r']*rpm_factor) <= 1.0, (car['rpm'], car['r'])
        if car['mr']:
            assert abs(car['max_rpm'] - car['mr']*rpm_factor) <= 1.0
    raw_1000 = [c for c in parsed['c'] if c['r'] == 1000.0]
    assert raw_1000, 'synthetic fixture must include the 1000 rad/s raw rpm'
    assert all(abs(c['rpm'] - 9549.3) < 1.0 for c in raw_1000), 'rpm must be converted, not rad/s (1000)'
    assert not any(c['rpm'] == 1000.0 for c in raw_1000), 'no RPM-labelled field may carry rad/s'
    # Fix 3: the player's steer is vf-unavailable with a reason; AI steer is
    # documented-valid with an empty reason.
    assert parsed['c'][0]['vf'] & 8 == 0, 'player steering must be unavailable'
    assert parsed['c'][0]['steerwhy'].startswith('player steering unavailable'), parsed['c'][0]['steerwhy']
    ai_with_steer = [c for c in parsed['c'] if c['vf'] & 8]
    assert ai_with_steer, 'AI steering expected valid'
    assert all(c['steerwhy'] == '' for c in ai_with_steer), 'AI steer reason must be empty'
    for attempt in range(100):
        try:
            with urlopen('http://127.0.0.1:18080/health', timeout=1) as response:
                health=json.load(response)
            break
        except OSError:
            if run.poll() is not None:
                raise AssertionError(run.stdout.read())
            time.sleep(.02)
    else:
        raise AssertionError('synthetic server did not start')
    assert health['read_only'] and health['sample_sequence'] > 0
    assert health['listen_address']=='127.0.0.1' and health['port']==18080   # schema frozen at API 9; the BindAddress listener is tested below
    assert health['workbench_ui']['present'] is True
    assert health['workbench_ui']['sha256']==sidecar_expected
    assert health['workbench_ui']['expected']==sidecar_expected
    assert health['workbench_ui']['matches'] is True
    assert health['workbench_ui']['bytes']==len(sidecar_staged)
    assert health['viewer_ui']['present'] is True and health['viewer_ui']['matches'] is True, health['viewer_ui']
    assert health['viewer_ui']['sha256']==viewer_expected and health['viewer_ui']['bytes']==len(viewer_staged)
    # BACKEND 4 (api_version 3): native capture cost + rate and memory report.
    # api_version 5 (4.4.0 all-cars recorder): trace_mode/trace_bytes/trace_cars.
    assert health['api_version']=='10', health['api_version']
    assert health['trace_mode'] in ('selected','all','selected_fallback'), health['trace_mode']
    assert health['trace_cars']>=0 and health['trace_bytes']>=0
    assert health['capture_cost_us']['samples']>=1, health['capture_cost_us']
    assert health['capture_cost_us']['p95']<500.0, health['capture_cost_us']
    assert health['capture_cost_us']['rate_per_s']>=0
    assert health['memory']['working_set_kb']>0, health['memory']
    remote_base='http://'+remote_address+':18080'
    with urlopen(remote_base+'/health',timeout=2) as response:
        remote_health=json.load(response)
    assert remote_health['version']=='1.2.5' and remote_health['read_only']
    with urlopen(remote_base+'/',timeout=2) as response:
        remote_html=response.read().decode('utf-8')
    assert '1.2.5' in remote_html
    with urlopen(remote_base+'/events',timeout=3) as response:
        assert json.loads(response.readline()[6:])['version']=='1.2.5'
    try:
        urlopen(remote_base+'/action?type=retire&id=1',timeout=2)
        raise AssertionError('remote write accepted')
    except HTTPError as e:
        assert e.code==403
    print('PASS non-loopback HTTP health, dashboard, SSE and command refusal via '+remote_address+':18080',flush=True)
    with urlopen('http://127.0.0.1:18080/events',timeout=3) as response:
        frame=response.readline()
        assert frame.startswith(b'data: ')
        assert json.loads(frame[6:])['version']=='1.2.5'
        assert response.readline()==b'\r\n'
    checks=0
    for path in ['/telemetry','/track','/laps','/incidents','/pits','/driver?id=0']:
        with urlopen('http://127.0.0.1:18080'+path, timeout=2) as response:
            json.load(response)
        checks+=1
    # 4.4.0: "/" is the unified console shell. 4.4.0 batch 1: /classic and /viewer are retired.
    with urlopen('http://127.0.0.1:18080/',timeout=2) as response:
        console_html=response.read().decode('utf-8')
    assert '1.2.5' in console_html and 'F1 Challenge 99-02 Telemetry' in console_html  # generator meta: engine 1.2.0
    assert '/viewer.js' in console_html and '/workbench.js' in console_html and 'mountConsole' in console_html
    assert 'href="/classic"' not in console_html and 'href="/viewer"' not in console_html, 'console must not link the retired pages'
    with urlopen('http://127.0.0.1:18080/index.html',timeout=2) as response:
        assert response.read().decode('utf-8')==console_html
    # 4.4.0 batch 1: /classic and /viewer are retired (404). The classic page
    # footer, Engineering tab and cmpGrid checks read that page and are removed
    # with it; /health carries the bound port (checked by the port tests below).
    for retired in ('/classic', '/viewer'):
        try:
            urlopen('http://127.0.0.1:18080'+retired, timeout=2)
            raise AssertionError(retired+' is still served')
        except HTTPError as e:
            assert e.code == 404, (retired, e.code)
        checks+=1
    # ---- rc.11 weather: bounded session weather ring (10 s + change cadence) ----
    with urlopen('http://127.0.0.1:18080/weather', timeout=2) as response:
        weather=json.load(response)
    assert weather['api_version']=='10' and weather['interval_s']==10.0 and weather['capacity']==2048
    assert weather['tiers']=={'at':'CONFIRMED','trkt':'CONFIRMED','onw':'CONFIRMED','offw':'CONFIRMED','tgf':'DERIVED'}, weather['tiers']
    assert weather['fields']['st']=='s' and weather['fields']['tgf']=='raw'
    assert isinstance(weather['samples'],list) and weather['samples'] and all(len(row)==6 for row in weather['samples'])
    print('PASS weather session ring: capacity, cadence, units, tiers and rows',flush=True)
    # ---- engineering trace endpoints (synthetic traces seeded by runtime_test) ----
    with urlopen('http://127.0.0.1:18080/trace/status', timeout=2) as response:
        status=json.load(response)
    assert status['lap_len']>0 and len(status['cars'])==2
    assert status['mode']=='selected', status['mode']   # fixture: the all-cars store is not allocated
    assert status['cars'][0]['laps'], 'no recorded laps for side A'
    assert 'steering' in status['cars'][0]['unavailable'], status['cars'][0]['unavailable']
    # Fix 3: the unavailable channel carries a reason; player steering names
    # the pending live check, AI steering is not listed at all.
    assert 'player steering unavailable' in status['cars'][0]['unavailable_why']['steering']
    assert 'steering' not in status['cars'][1]['unavailable_why']
    # Fix 2: every completed lap carries its stable identity, and the eviction
    # log exists (empty here: the synthetic ring never overflowed).
    lap_a=status['cars'][0]['laps'][0]['lap']
    lap_b=status['cars'][1]['laps'][0]['lap']
    st0=status['cars'][0]; st1=status['cars'][1]
    lid0=st0['laps'][0]['lid']
    assert lid0=={'gen':status['gen'],'rev':st0['rev'],'slot':st0['slot'],'lap':lap_a,'src':'live'}, lid0
    assert isinstance(st0['evicted'],list) and isinstance(st1['evicted'],list)
    with urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d'%lap_a, timeout=2) as response:
        data=json.load(response)
    assert data['schema']=='f1c.trace/1' and data['count']>100
    assert data['lap']==2 and data['rev']>0 and data['track']=='Synthetic Track'
    assert data['samples'][5]['st'] is None, 'player steering must be unavailable'
    assert data['lid']==lid0, (data['lid'], lid0)
    # 4.4.0 all-cars API: a numeric car=<slot> addresses the same entry as its
    # side letter (selected-mode fallback resolves the recorded pair).
    with urlopen('http://127.0.0.1:18080/trace/data?car=%d&lap=%d'%(st0['slot'],lap_a), timeout=2) as response:
        data_slot=json.load(response)
    assert data_slot['lid']==lid0 and data_slot['slot']==st0['slot']
    with urlopen('http://127.0.0.1:18080/trace/open?car=%d'%st0['slot'], timeout=2) as response:
        open_slot=json.load(response)
    assert open_slot['schema']=='f1c.trace/1' and open_slot['open'] is True and open_slot['slot']==st0['slot']
    # Fix 2: pinned identity params serve the same lap; a stale rev/gen is an
    # explicit 409, and a never-recorded lap is an explicit evicted:false 404.
    with urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d&gen=%d&rev=%d'
                 %(lap_a,status['gen'],st0['rev']),timeout=2) as response:
        assert json.load(response)['lid']==lid0
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d&rev=%d'%(lap_a,st0['rev']+7),timeout=2)
        raise AssertionError('stale recorder revision accepted')
    except HTTPError as e:
        assert e.code==409 and json.load(e)['evicted'] is True
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d&gen=999999'%lap_a,timeout=2)
        raise AssertionError('stale generation accepted')
    except HTTPError as e:
        assert e.code==409 and json.load(e)['evicted'] is True
    with urlopen('http://127.0.0.1:18080/trace/data?car=B&lap=%d'%lap_b, timeout=2) as response:
        data_b=json.load(response)
    assert any(s['st'] is not None for s in data_b['samples']), 'AI steering expected'
    assert data_b['lid']['slot']==st1['slot'] and data_b['lid']['rev']==st1['rev']
    with urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d&fmt=csv'%lap_a, timeout=2) as response:
        csv_text=response.read().decode('utf-8')
    assert csv_text.startswith('# f1c.trace/1') and 'sim_t,dist_m' in csv_text and 'src=live' in csv_text
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=%d&carB=B&lapB=%d'%(lap_a,lap_b), timeout=2) as response:
        cmp_data=json.load(response)
    assert cmp_data['schema']=='f1c.compare/1' and cmp_data['n']>10
    assert cmp_data['a']['lid']['src']=='live' and cmp_data['b']['lid']['src']=='live'
    assert all(v is None for v in cmp_data['a_ch']['steer']), 'player steering column must be holes'
    assert any(v is not None for v in cmp_data['b_ch']['steer']), 'AI steering column expected'
    assert any(v is not None and v>0.1 for v in cmp_data['delta']), 'B is the slower synthetic lap'
    assert max(v for v in cmp_data['delta'] if v is not None)<11, 'session origin leaked into delta'
    with urlopen('http://127.0.0.1:18080/trace/export?car=A&lap=%d'%lap_a,timeout=2) as response:
        assert response.headers['Content-Type'].startswith('text/csv')
    try:
        urlopen('http://127.0.0.1:18080/trace/select?which=A&car=2&gen=999999',timeout=2)
        raise AssertionError('stale selection accepted')
    except HTTPError as e:
        assert e.code==409
    # selection stays on the same slot, so recorded data survives
    for q in ['/trace/select?which=A&car=0','/trace/select?which=B&car=1']:
        with urlopen('http://127.0.0.1:18080'+q, timeout=2) as response:
            json.load(response)
    for bad in ['/trace/select?which=Z&car=1','/trace/select?which=A&car=999','/trace/select?which=AB&car=1','/trace/select?which=A&car=',
                '/trace/data?car=C&lap=1','/trace/data?car=104&lap=1','/trace/data?car=A','/trace/compare?carA=A&lapA=1']:
        try:
            urlopen('http://127.0.0.1:18080'+bad,timeout=2)
            raise AssertionError('bad trace request was accepted: '+bad)
        except HTTPError as e:
            assert e.code==400, bad
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=99999',timeout=2)
        raise AssertionError('unknown lap accepted')
    except HTTPError as e:
        assert e.code==404 and json.load(e)['evicted'] is False
    checks+=6
    print('PASS engineering trace status/data/csv/compare/select endpoints',flush=True)
    # ---- 4.4.0 multi-driver: /workbench page retired (the console at / is the only UI); sidecar JS, open-lap serving and legacy import stay ----
    try:
        urlopen('http://127.0.0.1:18080/workbench',timeout=2)
        raise AssertionError('/workbench page still served')
    except HTTPError as e:
        assert e.code==404
    with urlopen('http://127.0.0.1:18080/workbench.js',timeout=2) as response:
        wb_js=response.read().decode('utf-8')
        assert response.headers['Content-Type'].startswith('application/javascript')
    assert 'radPerSecToRpm' in wb_js and '9.549296585513721' in wb_js and 'timeAtDistance' in wb_js
    # ---- 4.3.0 viewer page and sidecar ----
    with urlopen('http://127.0.0.1:18080/viewer.js',timeout=2) as response:
        tv_js=response.read()
        assert response.headers['Content-Type'].startswith('application/javascript')
    assert tv_js==viewer_staged, 'served viewer differs from the staged sidecar'
    assert b'mountConsole' in tv_js and b'CONSOLE_CSS' in tv_js, 'viewer sidecar must carry the 4.4.0 console'
    # v6 frame fields and trace sample tt/rx are present and gated.
    with urlopen('http://127.0.0.1:18080/telemetry',timeout=2) as response:
        tel6=json.load(response)
    for car6 in tel6['c']:
        assert isinstance(car6['vf6'],int) and len(car6['ttc'])==4 and len(car6['tcc'])==4 and len(car6['rx'])==16, car6
    with urlopen('http://127.0.0.1:18080/trace/open?car=A',timeout=2) as response:
        open_doc=json.load(response)
    assert open_doc['schema']=='f1c.trace/1' and open_doc['open'] is True and open_doc['count']>=1
    assert any(s.get('rpm') is not None for s in open_doc['samples']), 'open lap carries converted rpm'
    lap=build_synthetic_lap()
    with urlopen(Request('http://127.0.0.1:18080/trace/import/legacy?car=A',data=lap,method='POST',
                         headers={'Content-Type':'application/octet-stream'}),timeout=5) as response:
        imp=json.load(response)
    assert imp['schema']=='f1c.trace/1' and imp['source']=='legacy' and imp['format']=='lap'
    assert imp['partial'] is True and imp['count']==3
    assert any('mph->m/s' in c for c in imp['conversions'])
    assert any('g->m/s2' in c for c in imp['conversions'])
    s1=imp['samples'][1]
    assert abs(s1['s']-10.0)<1e-3, s1['s']                # 22.371365 mph
    assert abs(s1['rpm']-8000.0)<1e-6 and s1['ge']==3     # already RPM, float->int
    assert abs(s1['gl']-4.9)<1e-6                          # 0.5 g x 9.8
    assert abs(imp['samples'][2]['y']-1.5707963)<1e-3      # 90 deg -> rad
    assert abs(s1['tx']-120.0)<0.05 and abs(s1['tz']-6.0)<0.05
    with urlopen('http://127.0.0.1:18080/trace/import?car=A',timeout=2) as response:
        cached=json.load(response)
    assert cached['count']==imp['count']
    assert imp['lid']['src']=='import' and imp['lid']['serial']>=1 and imp['lid']['slot']==-1
    assert cached['lid']==imp['lid']
    # Fix 2: /trace/compare accepts an imported lap on either side.
    with urlopen(Request('http://127.0.0.1:18080/trace/import/legacy?car=B',data=lap,method='POST',
                         headers={'Content-Type':'application/octet-stream'}),timeout=5) as response:
        imp_b=json.load(response)
    assert imp_b['lid']['src']=='import' and imp_b['lid']['serial']>=1
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=import&carB=B&lapB=%d'%lap_b,timeout=2) as response:
        cmp_imp=json.load(response)
    assert cmp_imp['a']['lid']['src']=='import' and cmp_imp['a']['lid']['serial']==imp['lid']['serial']
    assert cmp_imp['b']['lid']['src']=='live'
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=%d&carB=B&lapB=import'%lap_a,timeout=2) as response:
        cmp_imp2=json.load(response)
    assert cmp_imp2['a']['lid']['src']=='live' and cmp_imp2['b']['lid']['src']=='import'
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=import&carB=B&lapB=import',timeout=2) as response:
        cmp_imp3=json.load(response)
    assert cmp_imp3['a']['lid']['src']=='import' and cmp_imp3['b']['lid']['src']=='import'
    try:
        urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=import&carB=B&lapB=1&gen=999999',timeout=2)
        raise AssertionError('stale compare generation accepted')
    except HTTPError as e:
        assert e.code==409
    try:
        urlopen(Request('http://127.0.0.1:18080/trace/import/legacy?car=A',
                        data=b'CHECKSUM=1\r\n\x02\x00\x00\x00abc',method='POST',
                        headers={'Content-Type':'application/octet-stream'}),timeout=3)
        raise AssertionError('malformed legacy import accepted')
    except HTTPError as e:
        assert e.code==400 and json.load(e)['error']
    # rc.6 cross-origin guard: only application/octet-stream is accepted, so a
    # foreign page must preflight; the preflight is refused without any allow
    # header. Wildcard CORS stays on GET /events only.
    try:
        urlopen(Request('http://127.0.0.1:18080/trace/import/legacy?car=A',data=lap,method='POST',
                        headers={'Content-Type':'text/plain'}),timeout=3)
        raise AssertionError('simple-content-type import accepted')
    except HTTPError as e:
        assert e.code==415 and e.headers.get('Access-Control-Allow-Origin') is None
    with socket.create_connection(('127.0.0.1',18080),timeout=2) as raw:
        raw.sendall(b'OPTIONS /trace/import/legacy HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n'
                    b'Origin: http://foreign.example\r\nAccess-Control-Request-Method: POST\r\n'
                    b'Access-Control-Request-Headers: content-type\r\n\r\n')
        data=b''
        while part:=raw.recv(65536): data+=part
    head=data.split(b'\r\n\r\n',1)[0]
    assert b'405' in head.split(b'\r\n',1)[0] and b'Access-Control-Allow-Origin' not in head
    # batch 4 (API 9): the 1.0.1 list (GET /sessions) and hard delete (POST /sessions/delete) are removed. The seeded
    # legacy archive is read from its folder on disk (review mode still answers ?session=<id>).
    import tempfile
    arroot = Path(tempfile.gettempdir()) / ('f1c-web-telemetry-arserve-%d' % run.pid) / 'telemetry_data'   # AR_ROOT_NAME
    legacy_ids = sorted(p.name for p in arroot.iterdir() if p.is_dir() and _re.fullmatch(r'\d{8}-\d{6}_s\d+_g\d+(_n\d+)?', p.name))
    assert len(legacy_ids) == 1, (str(arroot), legacy_ids)
    archive_id = legacy_ids[0]
    for bad in ('/laps?session=20261012-000000_s5_g1', '/trace/status?session=..%5C..%5Cx', '/telemetry?session=abc'):
        try:
            urlopen('http://127.0.0.1:18080' + bad, timeout=3)
            raise AssertionError('unknown archived session served: ' + bad)
        except HTTPError as e:
            assert e.code == 404, (bad, e.code)
    try:
        urlopen(Request('http://127.0.0.1:18080/sessions/delete', data=b'x', headers={'Content-Type': 'application/octet-stream', 'X-Telemetry-Action': 'delete'}, method='POST'), timeout=3)
        raise AssertionError('POST /sessions/delete is still served')
    except HTTPError as e:
        assert e.code == 405, e.code
    try:
        urlopen('http://127.0.0.1:18080/sessions', timeout=3)
        raise AssertionError('GET /sessions is still served')
    except HTTPError as e:
        assert e.code == 404, e.code
    # 8.2 guards on the write routes: this PC (peer), the action header, JSON, 4 KB body cap. Load needs no storage.
    for headers, body, code in (
        ({'Content-Type': 'application/json'}, b'{"recording":"x"}', 403),
        ({'Content-Type': 'text/plain', 'X-Telemetry-Action': 'load'}, b'{"recording":"x"}', 415),
        ({'Content-Type': 'application/json', 'X-Telemetry-Action': 'load'}, b'{"pad":"' + b'x' * 5000 + b'"}', 413),
        ({'Content-Type': 'application/json', 'X-Telemetry-Action': 'load'}, b'{"recording"', 400),
        ({'Content-Type': 'application/json', 'X-Telemetry-Action': 'load'}, b'{"recording":"..x"}', 422),
        ({'Content-Type': 'application/json', 'X-Telemetry-Action': 'load'}, b'{"recording":"No_Such_Recording"}', 404),
    ):
        try:
            urlopen(Request('http://127.0.0.1:18080/recordings/load', data=body, headers=headers, method='POST'), timeout=3)
            raise AssertionError('recordings guard passed: %r' % (headers,))
        except HTTPError as e:
            assert e.code == code, (headers, body[:40], e.code)
    try:
        urlopen(Request(remote_base + '/recordings/load', data=b'{"recording":"x"}', headers={'Content-Type': 'application/json', 'X-Telemetry-Action': 'load'}, method='POST'), timeout=3)
        raise AssertionError('recordings load accepted from a non-local peer')
    except HTTPError as e:
        assert e.code == 403, e.code
    with urlopen('http://127.0.0.1:18080/recordings', timeout=3) as response:
        recordings_doc = json.loads(response.read())
    assert recordings_doc['schema'] == 'f1c.recordings/1' and recordings_doc['loaded'] is None, recordings_doc
    print('PASS batch 4 HTTP: /sessions list and hard delete removed (405/404); write guards 403/415/413/400/422/404; remote peer refused; GET /recordings', flush=True)
    with urlopen('http://127.0.0.1:18080/health', timeout=3) as response:
        health_101 = json.loads(response.read())
        assert health_101['product'] == 'Telemetry 1.2.5' and health_101['archive']['enabled'] is True
    # review mode: the console endpoints answer from the archived session
    for path in ('/laps', '/pits', '/incidents', '/driver?id=0', '/trace/status', '/trace/data?car=0&lap=1', '/telemetry'):
        url = 'http://127.0.0.1:18080' + path + ('&' if '?' in path else '?') + 'session=' + archive_id
        with urlopen(url, timeout=3) as response:
            doc = json.loads(response.read())
        if path == '/laps':
            assert doc['archive'] == archive_id and doc['cars'][0]['laps'] == [83.25], doc
        if path == '/trace/status':
            assert doc['mode'] == 'archive' and doc['cars'][0]['laps'][0]['lid']['src'] == 'archive', doc
        if path.startswith('/trace/data'):
            assert doc['source'] == 'archive' and doc['count'] == 40 and doc['gen'] == 4242, doc['count']
    for path in ('/trace/open?car=0', '/trace/data?car=0&lap=9'):
        try:
            urlopen('http://127.0.0.1:18080' + path + '&session=' + archive_id, timeout=3)
            raise AssertionError('archived ' + path + ' answered')
        except HTTPError as e:
            assert e.code == 404, (path, e.code)
    print('PASS 1.0.1 session archive routes: list, unknown session 404, delete guards, health')
    # R1-B (API 10): a closed recording (v2) answers the review routes from the review index. These cases need a closed
    # recording on this PC (the owner's game run makes one); without one the block prints SKIP. No POST is sent here.
    import time as _time
    import contract_validate as cv
    review_base = 'http://127.0.0.1:18080'
    def review_get(path):
        try:
            with urlopen(review_base + path, timeout=5) as response:
                return response.status, response.headers, json.loads(response.read())
        except HTTPError as e:
            return e.code, e.headers, json.loads(e.read() or b'{}')
    with urlopen(review_base + '/recordings', timeout=3) as response:
        rec_listing = json.loads(response.read())
    v2_ids = [s['review_id'] for rec in rec_listing['recordings'] for s in rec['sessions']
              if s.get('review_id') and s['state'] == 'closed']
    if not v2_ids:
        print('SKIP R1-B v2 review cases: no closed recording with a review_id on this PC', flush=True)
    else:
        rid = v2_ids[0]
        deadline = _time.time() + 60
        while True:
            code, _, index_doc = review_get('/recordings/review?session=' + rid)
            assert code == 200, (code, index_doc)
            cv.validate('recordings_review.json', index_doc)
            if index_doc['state'] != 'building' or _time.time() > deadline:
                break
            _time.sleep(0.5)
        assert index_doc['state'] == 'ready', index_doc
        for path, schema in (('/laps', 'laps.json'), ('/trace/status', 'trace_status.json'), ('/telemetry', 'telemetry.json')):
            code, _, doc = review_get(path + '?session=' + rid)
            assert code == 200, (path, code, doc)
            cv.validate(schema, doc)
        code, _, status_doc = review_get('/trace/status?session=' + rid)
        car = next((c for c in status_doc['cars'] if c['laps']), None)
        assert car is not None, 'a closed recording must have at least one timed lap'
        lap = car['laps'][0]
        code, _, trace_doc = review_get('/trace/data?car=%d&lap=%d&session=%s' % (1000 + car['slot'], lap['lap'], rid))
        assert code == 200, (code, trace_doc)
        cv.validate('trace_data.json', trace_doc)
        assert trace_doc['source'] == 'recording' and trace_doc['count'] == len(trace_doc['samples']), trace_doc['count']
        # REPLAY 2.2 / TAPE 2.2: review /pits, /incidents, /track and /driver answer from the file; /weather is 404
        code, _, doc = review_get('/weather?session=' + rid)
        assert code == 404 and doc.get('error') == 'weather is not recorded in this recording', (code, doc)
        for path, schema in (('/pits', 'pits.json'), ('/incidents', 'incidents.json'), ('/track', 'track.json'),
                             ('/driver?id=%d' % car['slot'], 'driver.json')):
            sep = '&' if '?' in path else '?'
            code, _, doc = review_get(path + sep + 'session=' + rid)
            assert code == 200, (path, code, doc)
            cv.validate(schema, doc)
        # REPLAY 2.1 / 2.2: /replay/info and a 3 s window; a 31 s window is refused
        code, _, info = review_get('/replay/info?session=' + rid)
        assert code == 200, (code, info)
        cv.validate('replay_info.json', info)
        # TAPE2 N1 (2026-10-15): one name and team per car in every review doc (driver EVENT > season .veh by race number > #num > Car N)
        names = {c['slot']: (c['name'], c['team']) for c in info['cars']}
        for c in status_doc['cars']:
            assert (c['n'], c['cn']) == names[c['slot']], ('/trace/status names differ from /replay/info', c['slot'], c['n'], c['cn'], names[c['slot']])
        code, _, drv = review_get('/driver?id=%d&session=%s' % (car['slot'], rid))
        assert code == 200 and (drv['n'], drv['cn']) == names[car['slot']], ('/driver names differ', code, drv.get('n'), names[car['slot']])
        code, _, win = review_get('/replay?session=%s&t0=%.3f&t1=%.3f' % (rid, info['t_first'], info['t_first'] + 3.0))
        assert code == 200, (code, win)
        cv.validate('replay.json', win)
        # REPLAY 2.3 / TAPE 2.3: the frame at 10 s is the live /telemetry shape plus review and tape objects
        code, _, frame = review_get('/replay/frame?t=%.3f&session=%s' % (info['t_first'] + 10.0, rid))
        assert code == 200 and 'tape' in frame and 'review' in frame, (code, str(frame)[:200])
        cv.validate('telemetry.json', frame)
        if isinstance(frame.get('c'), list):
            for c in frame['c']:
                if c.get('id') in names:
                    assert c.get('n') == names[c['id']][0], ('/replay/frame name differs from /replay/info', c, names[c['id']])
        code, _, doc = review_get('/replay/frame?t=nan&session=' + rid)
        assert code == 400, (code, doc)
        code, _, doc = review_get('/replay?session=%s&t0=%.3f&t1=%.3f' % (rid, info['t_first'], info['t_first'] + 31.0))
        assert code == 400 and doc.get('error') == 'window too long', (code, doc)
        for bad in ('v2_0000000000000000', 'v2_ABCDEF0123456789', 'v2_short'):
            code, _, doc = review_get('/laps?session=' + bad)
            assert code == 404, (bad, code, doc)
            code, _, doc = review_get('/recordings/review?session=' + bad)
            assert code == 404, (bad, code, doc)
        with urlopen(review_base + '/?session=' + rid, timeout=5) as response:
            assert response.status == 200, response.status
        print('PASS R1-B v2 review: index ready, /laps /trace/status /trace/data /telemetry validate, not-recorded 404, unknown ids 404, page loads', flush=True)
    # 1.0.1: the SSE feed no longer carries a CORS wildcard (other sites must not read it).
    with urlopen('http://127.0.0.1:18080/events',timeout=3) as response:
        assert response.headers.get('Access-Control-Allow-Origin') is None
    with urlopen('http://127.0.0.1:18080/telemetry',timeout=2) as response:
        assert response.headers.get('Access-Control-Allow-Origin') is None
    # /action audit: GET-only, refused, no state change and no CORS wildcard.
    try:
        urlopen('http://127.0.0.1:18080/action?type=retire&id=1',timeout=2)
        raise AssertionError('action accepted')
    except HTTPError as e:
        assert e.code==403 and e.headers.get('Access-Control-Allow-Origin') is None
    # ---- BACKEND 1 endpoint audit (rc.7): allowlist, parsing, error shape ----
    for m in ['PUT','DELETE']:
        try:
            urlopen(Request('http://127.0.0.1:18080/telemetry',method=m),timeout=2)
            raise AssertionError('method accepted: '+m)
        except HTTPError as e:
            assert e.code==405 and json.load(e)['error']
    try:
        urlopen(Request('http://127.0.0.1:18080/telemetry',method='HEAD'),timeout=2)
        raise AssertionError('method accepted: HEAD')
    except HTTPError as e:
        assert e.code==405  # HEAD carries no body per HTTP; refusal is the check
    for bad in ['/trace/data?xcar=A&lap=1','/trace/data?car=A&xlap=1','/driver?xid=0']:
        try:
            urlopen('http://127.0.0.1:18080'+bad,timeout=2)
            raise AssertionError('substring key accepted: '+bad)
        except HTTPError as e:
            assert e.code==400, bad
    with urlopen('http://127.0.0.1:18080/trace/data?car=A&car=B&lap=%d'%lap_a,timeout=2) as response:
        dup=json.load(response)
    assert dup['which']=='A', 'duplicate keys must resolve to the first occurrence'
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=99999999999999999999',timeout=2)
        raise AssertionError('overflow lap accepted')
    except HTTPError as e:
        assert e.code==400
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=%d&carB=B&lapB=%d&step=abc'%(lap_a,lap_b),timeout=2) as response:
        cmpj=json.load(response)
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=%d&carB=B&lapB=%d'%(lap_a,lap_b),timeout=2) as response:
        cmpd=json.load(response)
    assert abs(cmpj['step']-cmpd['step'])<1e-9, 'junk step must fall back to the default'
    try:
        urlopen('http://127.0.0.1:18080/definitely-missing',timeout=2)
        raise AssertionError('missing path accepted')
    except HTTPError as e:
        assert e.code==404 and json.load(e)['error']=='not found'
    print('PASS endpoint audit: method allowlist, key boundaries, duplicates, overflow, step clamp, JSON 404',flush=True)
    # ---- BACKEND 2 contract: schemas validate every live document ----
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    import contract_validate as cv
    version=cv.check_contract_version(health['api_version'])
    with urlopen('http://127.0.0.1:18080/recordings',timeout=2) as response:
        cv.validate('recordings.json',json.load(response))
    with urlopen('http://127.0.0.1:18080/recordings/jobs',timeout=2) as response:
        cv.validate('recordings_jobs.json',json.load(response))
    cv.validate('health.json',health)
    for path,name in [('/telemetry','telemetry.json'),('/track','track.json'),
                      ('/laps','laps.json'),('/incidents','incidents.json'),
                      ('/pits','pits.json'),('/driver?id=0','driver.json'),
                      ('/trace/status','trace_status.json'),
                      ('/weather','weather.json')]:
        with urlopen('http://127.0.0.1:18080'+path,timeout=2) as response:
            cv.validate(name,json.load(response))
    for path in ['/trace/data?car=A&lap=%d'%lap_a,'/trace/open?car=A','/trace/import?car=A']:
        with urlopen('http://127.0.0.1:18080'+path,timeout=2) as response:
            cv.validate('trace_data.json',json.load(response))
    # 1.0.1 review documents use the same schemas (api_version 7 adds 'archive').
    review_id = archive_id
    for path,name in [('/laps','laps.json'),('/incidents','incidents.json'),('/pits','pits.json'),
                      ('/driver?id=0','driver.json'),('/trace/status','trace_status.json'),
                      ('/trace/data?car=0&lap=1','trace_data.json'),('/telemetry','telemetry.json')]:
        with urlopen('http://127.0.0.1:18080'+path+('&' if '?' in path else '?')+'session='+review_id,timeout=2) as response:
            cv.validate(name,json.load(response))
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=%d&carB=B&lapB=%d'%(lap_a,lap_b),timeout=2) as response:
        cv.validate('trace_compare.json',json.load(response))
    with urlopen('http://127.0.0.1:18080/trace/compare?carA=A&lapA=import&carB=B&lapB=import',timeout=2) as response:
        cv.validate('trace_compare.json',json.load(response))
    with urlopen('http://127.0.0.1:18080/events',timeout=3) as response:
        frame=response.readline()
    cv.validate('telemetry.json',json.loads(frame[6:]))
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=Z&lap=1',timeout=2)
        raise AssertionError('bad car accepted')
    except HTTPError as e:
        cv.validate('error.json',json.load(e))
    try:
        urlopen('http://127.0.0.1:18080/trace/data?car=A&lap=%d&rev=%d'%(lap_a,st0['rev']+7),timeout=2)
        raise AssertionError('stale revision accepted')
    except HTTPError as e:
        err=json.load(e)
        cv.validate('error.json',err)
        assert err['evicted'] is True
    assert version['api_version']=='10', version
    assert cv.check_schema_contract(), 'api_version 10 schema rules failed'
    assert cv.self_test(), 'contract validator self-test failed'
    print('PASS contract: schemas validate every live document, SSE event and error; api_version '+version['api_version']+' pinned',flush=True)
    with socket.create_connection(('127.0.0.1',18080),timeout=2) as raw:
        raw.sendall(b'POST /trace/import/legacy HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n'
                    b'Content-Type: application/octet-stream\r\nContent-Length: 20000000\r\n\r\n')
        data=b''
        while part:=raw.recv(65536): data+=part
    assert b'413' in data.split(b'\r\n',1)[0] and b'16 MB' in data
    try:
        urlopen(Request('http://127.0.0.1:18080/telemetry',data=b'x',method='POST'),timeout=2)
        raise AssertionError('POST accepted on /telemetry')
    except HTTPError as e:
        assert e.code==405
    checks+=6
    print('PASS workbench page/sidecar, open-lap serving, legacy import POST/cache/refusals',flush=True)
    # 4.4.0 batch 1: the classic page script (cmpGrid, Engineering controller)
    # is no longer served, so its Node checks are removed with it. The served
    # sidecars below keep their Node checks.
    import re, shutil
    node=shutil.which('node')
    if node:
        # rc.7: the served sidecar parses and keeps its RPM/DM#14 rules. The
        # served copy is written under a separate name with write_bytes: a
        # text-mode write translates LF to CRLF and would mutate the staged
        # sidecar the owner installs (the rc.6 staging defect).
        wb_path=build/'workbench_ui_served.js'
        wb_path.write_bytes(wb_js.encode('utf-8'))
        wb_test=build/'workbench_test.js'
        wb_test.write_text("""
const assert=require('assert');
const W=require(process.argv[2]);
assert.strictEqual(W.RPM_FACTOR,9.549296585513721);
assert(Math.abs(W.radPerSecToRpm(1803.7)-17224.066251291)<1e-6);
assert.strictEqual(W.formatRpm(1803.7),'17,224');
const pts=[{t:0,dist:0},{t:10,dist:100}];
assert(Math.abs(W.timeAtDistance(pts,50)-5)<1e-9);
assert(Math.abs(W.timeAtDistance([{t:0,dist:0},{t:1,dist:0.00005}],0.00002)-1)<1e-9);
assert(Math.abs((W.timeAtDistance([{t:0,dist:0},{t:11,dist:100}],50)-W.timeAtDistance(pts,50))-0.5)<1e-9);
// rc.11 old-viewer parity: all 23 legacy layouts embedded, Chassis Overview first.
assert.strictEqual(W.LAYOUTS.length,23);
assert.strictEqual(W.LAYOUTS[0].file,'overview.ini');
assert.strictEqual(W.channelInfo('clutch').get,null);
console.log('WORKBENCH PASS');
""",encoding='utf-8')
        out=subprocess.run([node,str(wb_test),str(wb_path)],check=True,capture_output=True,text=True)
        assert 'WORKBENCH PASS' in out.stdout
        print('PASS served workbench sidecar Node RPM/DM#14 test',flush=True)
        tv_path=build/'telemetry_viewer_served.js'
        tv_path.write_bytes(tv_js)
        out=subprocess.run([node,str(component/'viewer_test.js'),str(tv_path),str(wb_path)],check=True,capture_output=True,text=True)
        assert 'VIEWER PASS' in out.stdout, out.stdout
        print('PASS served viewer sidecar Node analysis test: '+out.stdout.strip(),flush=True)
        subprocess.run([node,str(component/'audit_regression_test.js'),str(wb_path)],check=True)
        # 4.4.0 multi-driver: audit_browser_test.js drove the retired /workbench page bootstrap; not run (see CONSOLE_BACKLOG multi-driver section).

    else:
        print('JavaScript syntax check unavailable: Node not on PATH',flush=True)
    try:
        urlopen('http://127.0.0.1:18080/action?type=retire&id=1',timeout=2)
        raise AssertionError('write command was accepted')
    except HTTPError as e:
        assert e.code==403 and json.load(e)['ok'] is False
    try:
        urlopen(Request('http://127.0.0.1:18080/',data=b'',method='POST'),timeout=2)
        raise AssertionError('POST was accepted')
    except HTTPError as e:
        assert e.code==405
    for bad in ['foo','104','-1','0abc','']:
        try:
            urlopen('http://127.0.0.1:18080/driver?id='+bad,timeout=2)
            raise AssertionError('bad driver id was accepted')
        except HTTPError as e:
            assert e.code==400
    with socket.create_connection(('127.0.0.1',18080),timeout=2) as fragmented:
        fragmented.sendall(b'GET /hea')
        time.sleep(.04)
        fragmented.sendall(b'lth HTTP/1.1\r\nHost: localhost:18080\r\n\r\n')
        data=b''
        while part:=fragmented.recv(65536):
            data+=part
        headers,body=data.split(b'\r\n\r\n',1)
        assert b'200 OK' in headers and json.loads(body)['read_only']
    print(f'PASS {checks} JSON endpoints, SSE frame, browser syntax, command/ID/POST refusal, fragmented request, system-only imports',flush=True)
    output=run.communicate(timeout=200 if os.environ.get("TELEMETRY_PREVIEW") else 18)[0]
    assert run.returncode==0,output
    perf=[line for line in output.splitlines() if line.startswith('PERF ')]
    assert perf, output
    print(perf[0],flush=True)
    perfg=[line for line in output.splitlines() if line.startswith('PERFG ')]
    assert perfg, output
    cb_p95=float(perfg[0].split('p95=')[1].split('us')[0])
    assert cb_p95<500.0, perfg[0]
    print(perfg[0],flush=True)

    # ---- configurable port: ini selection, occupied fallback, disabled path ----
    def start_serve(overrides=None):
        env=dict(os.environ)
        if overrides: env.update(overrides)
        proc=subprocess.Popen([str(build/'runtime_test.exe'),'serve',str(build/'web_telemetry.dll')],
                              cwd=build,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
        line=read_first_pass_line(proc)
        assert line.startswith('PASS '), line+proc.stdout.read()
        return proc
    def stop_serve(proc):
        if proc.poll() is None:
            proc.terminate(); proc.wait(timeout=5)
    def poll_health(port,proc):
        for _ in range(250):
            try:
                with urlopen('http://127.0.0.1:%d/health'%port,timeout=1) as response:
                    return json.load(response)
            except OSError:
                if proc.poll() is not None: return None
                time.sleep(.02)
        return None
    def hold_ports(ports):
        held=[]
        for port in ports:
            s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
            s.bind(('0.0.0.0',port)); held.append(s)
        return held
    # rc.17: without Remote=1 the listener is this PC only.
    proc=start_serve()
    try:
        health=poll_health(18080,proc)
        assert health and health['listen_address']=='127.0.0.1', health
        reached=False
        try:
            socket.create_connection((remote_address,18080),timeout=1).close(); reached=True
        except OSError:
            pass
        assert not reached, 'default listener accepted a connection on a non-loopback address'
        print('PASS default listener is 127.0.0.1 only; non-loopback refused',flush=True)
    finally:
        stop_serve(proc)
    held=hold_ports([18080])
    try:
        proc=start_serve()
        try:
            bound=None
            for _ in range(250):
                for port in range(18081,18086):
                    try:
                        with urlopen('http://127.0.0.1:%d/health'%port,timeout=.5) as response:
                            health=json.load(response); bound=port; break
                    except OSError: pass
                if bound: break
                time.sleep(.02)
            assert bound is not None, 'occupied 18080 did not produce a fallback listener'
            assert health['port']==bound, (health['port'],bound)
            print('PASS occupied-port fallback: 18080 held, listener bound %d, /health reports it'%bound,flush=True)
        finally:
            stop_serve(proc)
    finally:
        for s in held: s.close()
    held6=hold_ports(range(18080,18086))
    try:
        proc=start_serve()
        try:
            time.sleep(1.0)
            assert proc.poll() is None, 'process must survive with HTTP disabled (capture continues)'
            for port in range(18080,18086):
                probe=socket.socket(); probe.settimeout(.5)
                try:
                    probe.connect(('127.0.0.1',port))
                    raise AssertionError('port %d answered although all six candidates were held'%port)
                except OSError:
                    pass
                finally:
                    probe.close()
            print('PASS all-occupied path: no listener, no crash, capture process alive',flush=True)
        finally:
            stop_serve(proc)
    finally:
        for s in held6: s.close()
    ini=build/'port_config_test.ini'
    ini.write_text('[Mod]\nEnabled=1\nPort=18083\n',encoding='utf-8')
    try:
        proc=start_serve({'TELEMETRY_INI':str(ini)})
        try:
            health=poll_health(18083,proc)
            assert health and health['port']==18083, health
            print('PASS ini [Mod] Port selects the listener and /health',flush=True)
        finally:
            stop_serve(proc)
        ini.write_text('[Mod]\nEnabled=1\nPort=99\n',encoding='utf-8')
        proc=start_serve({'TELEMETRY_INI':str(ini)})
        try:
            health=poll_health(18080,proc)
            assert health and health['port']==18080, health
            print('PASS invalid ini Port falls back to the compiled default',flush=True)
        finally:
            stop_serve(proc)
    finally:
        if ini.exists(): ini.unlink()
    # ---- 1.0.1: the UI scripts are built into the DLL. The Lightloader manager
    # installs only .dll/.asi/.ini from a package, so 1.0.0 manager installs had
    # no scripts (player report 2026-10-12). Missing or stale files beside the
    # DLL must change nothing: the embedded pinned bytes are always served. ----
    sidecar=build/'workbench_ui.js'
    viewer_side=build/'telemetry_viewer.js'
    backup=build/'workbench_ui.js.missing_test_backup'
    viewer_backup=build/'telemetry_viewer.js.stale_test_backup'
    missing_ini=build/'missing_sidecar_test.ini'
    missing_ini.write_text('[Mod]\nPort=18084\n',encoding='utf-8')
    sidecar.replace(backup)
    viewer_side.replace(viewer_backup)
    viewer_side.write_text('/* stale private-build viewer */ var F1CViewer={};\n',encoding='utf-8')
    proc=None
    try:
        proc=start_serve({'TELEMETRY_INI':str(missing_ini)})
        health=poll_health(18084,proc)
        assert health and health['workbench_ui']['present'] is True and health['workbench_ui']['matches'] is True, health
        assert health['viewer_ui']['present'] is True and health['viewer_ui']['matches'] is True, health
        assert health['workbench_ui']['expected']==sidecar_expected
        with urlopen('http://127.0.0.1:18084/workbench.js',timeout=2) as response:
            wb=response.read()
        assert hashlib.sha256(wb).hexdigest().upper()==sidecar_expected
        with urlopen('http://127.0.0.1:18084/viewer.js',timeout=2) as response:
            vj=response.read()
        assert b'mountConsole' in vj and b'stale private-build' not in vj
        with urlopen('http://127.0.0.1:18084/',timeout=2) as response:
            page=response.read().decode('utf-8')
        assert 'mountConsole' in page
        try:
            urlopen('http://127.0.0.1:18084/workbench',timeout=2)
            raise AssertionError('/workbench page still served')
        except HTTPError as e:
            assert e.code==404
        print('PASS embedded UI: no workbench_ui.js and a stale telemetry_viewer.js beside the DLL still serve the pinned built-in scripts',flush=True)
    finally:
        if proc is not None: stop_serve(proc)
        backup.replace(sidecar)
        viewer_backup.replace(viewer_side)
        if missing_ini.exists(): missing_ini.unlink()
    # ---- BACKEND 1 robustness audit (rc.7): slow client, SSE cap, oversized line ----
    audit_ini=build/'endpoint_audit_test.ini'
    audit_ini.write_text('[Mod]\nPort=18085\n',encoding='utf-8')
    proc=None
    try:
        # Long-lived serve budget for this block: the slow-client test alone
        # consumes 5 s and the SSE-cap test needs the listener to stay up.
        proc=start_serve({'TELEMETRY_INI':str(audit_ini),'TELEMETRY_PREVIEW':'1'})
        health=poll_health(18085,proc)
        assert health and health['port']==18085, health
        from audit_http_test import run as audit_http_regressions
        audit_http_regressions(18085)
        # slowloris: a trickling header block is dropped at the 5 s deadline
        slow=socket.create_connection(('127.0.0.1',18085),timeout=2)
        slow.sendall(b'GET /hea')
        slow.settimeout(9)
        t0=time.time()
        data=b''
        try:
            while True:
                part=slow.recv(4096)
                if not part: break
                data+=part
        except socket.timeout:
            raise AssertionError('slow client never dropped')
        slow.close()
        elapsed=time.time()-t0
        assert elapsed < 8.5 and b'400' in data.split(b'\r\n',1)[0], (elapsed,data[:60])
        with urlopen('http://127.0.0.1:18085/health',timeout=3) as response:
            assert json.load(response)['read_only'], 'server unusable after slow client'
        # oversized request line: the bounded header buffer refuses it
        with socket.create_connection(('127.0.0.1',18085),timeout=2) as big:
            big.sendall(b'GET /'+b'a'*3000+b' HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
            data=b''
            while part:=big.recv(65536): data+=part
        assert b'400' in data.split(b'\r\n',1)[0] and b'incomplete request' in data
        # ---- BACKEND 3 SSE lifecycle: measured ~10 Hz, reconnect without
        # catch-up, and 15 readers keep streaming while one slow peer is reaped. ----
        def sse_frames(sock, want, deadline):
            buf=b''; frames=[]
            while len(frames)<want and time.time()<deadline:
                sock.settimeout(max(0.1, deadline-time.time()))
                try: part=sock.recv(65536)
                except socket.timeout: break
                if not part: break
                buf+=part
                while len(frames)<want:
                    i=buf.find(b'data: ')
                    j=buf.find(b'\r\n\r\n',i) if i>=0 else -1
                    if i<0 or j<0: break
                    frames.append(json.loads(buf[i+6:j])); buf=buf[j+4:]
            return frames
        c1=socket.create_connection(('127.0.0.1',18085),timeout=3)
        c1.sendall(b'GET /events HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
        t0=time.time(); f1=sse_frames(c1,3,t0+2.0); rate=len(f1)/max(1e-3,time.time()-t0)
        assert len(f1)==3 and 3.0<rate<25.0, (len(f1),rate)
        seqs=[int(f['sample_sequence']) for f in f1]
        assert seqs==sorted(seqs), seqs
        c1.close()
        c2=socket.create_connection(('127.0.0.1',18085),timeout=3)
        c2.sendall(b'GET /events HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
        f2=sse_frames(c2,2,time.time()+2.0)
        assert len(f2)>=1 and all(int(f['sample_sequence'])>=seqs[-1] for f in f2), (seqs,f2)
        c2.close()
        streams=[]
        for _ in range(16):
            s=socket.create_connection(('127.0.0.1',18085),timeout=3)
            s.sendall(b'GET /events HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
            streams.append(s)
        time.sleep(0.3)
        for s in streams[:15]:
            got=sse_frames(s,1,time.time()+1.5)
            assert got, 'SSE client starved behind a slow peer'
        with urlopen('http://127.0.0.1:18085/health',timeout=3) as response:
            assert json.load(response)['read_only']
        for s in streams: s.close()
        print('PASS BACKEND 3 SSE: measured %.1f Hz, reconnect no catch-up, 15 clients stream while one slow peer is reaped'%rate,flush=True)
        # SSE client cap: 16 held streams (kept drained so the server never
        # drops them), the 17th must be closed, the server lives.
        time.sleep(0.5)
        sse=[]
        for _ in range(16):
            s=socket.create_connection(('127.0.0.1',18085),timeout=3)
            s.sendall(b'GET /events HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
            sse.append(s)
        import threading
        stop=threading.Event()
        counts=[[] for _ in sse]
        def drain(sock,out):
            sock.settimeout(0.25)
            while not stop.is_set():
                try:
                    part=sock.recv(65536)
                    if not part: break
                    out.append(len(part))
                except socket.timeout:
                    continue
                except OSError:
                    break
        threads=[threading.Thread(target=drain,args=(s,counts[i]),daemon=True) for i,s in enumerate(sse)]
        for t in threads: t.start()
        time.sleep(0.8)
        assert all(sum(c)>0 for c in counts), 'held SSE streams are not receiving'
        extra=socket.create_connection(('127.0.0.1',18085),timeout=3)
        extra.sendall(b'GET /events HTTP/1.1\r\nHost: 127.0.0.1:18085\r\n\r\n')
        extra.settimeout(3)
        got=b''
        try:
            while True:
                part=extra.recv(65536)
                if not part: break      # EOF: the server refused the 17th
                got+=part
                if b'data: ' in got: break   # a streamed frame means it was accepted
        except socket.timeout:
            pass
        extra.close()
        # The refused client gets the SSE headers, then EOF without a frame;
        # any streamed `data:` frame means the cap let a 17th client in.
        assert b'data: ' not in got, '17th SSE client streamed frames (cap missing)'
        stop.set()
        for t in threads: t.join(timeout=2)
        with urlopen('http://127.0.0.1:18085/health',timeout=3) as response:
            assert json.load(response)['read_only']
        for s in sse: s.close()
        print('PASS robustness audit: slow-client deadline, oversized request line, 16-client SSE cap',flush=True)
    finally:
        if proc is not None: stop_serve(proc)
        if audit_ini.exists(): audit_ini.unlink()
    # ---- security review 2026-10-13 (M1 bind, M3 Host, M4 proxy headers, M5 connections, M6 send path) ----
    # Offline, synthetic listener. Remote=1 with a private BindAddress, the same address the owner would use on Tailscale.
    def sec_raw(addr, port, data, timeout=3):
        with socket.create_connection((addr, port), timeout=timeout) as sock:
            try:
                sock.sendall(data)
                sock.shutdown(socket.SHUT_WR)
            except OSError:
                return b''   # the listener refused the connection before reading it
            out = b''
            try:
                while True:
                    part = sock.recv(65536)
                    if not part: break
                    out += part
            except OSError:   # timeout, reset or abort: the listener closed a refused connection
                pass
        return out
    def sec_status(out):
        return int(out.split(b' ', 2)[1]) if out.startswith(b'HTTP/') else None
    # M1: Remote=1 without a private BindAddress, or with a public one, stays on this PC
    for ini_text, label in (('[Mod]\nPort=18087\nRemote=1\n', 'no BindAddress'),
                            ('[Mod]\nPort=18087\nRemote=1\nBindAddress=8.8.8.8\n', 'public BindAddress')):
        bad_ini = build/'security_bind_test.ini'
        bad_ini.write_text(ini_text, encoding='ascii')
        proc_bind = start_serve({'TELEMETRY_INI': str(bad_ini)})
        try:
            health_bind = poll_health(18087, proc_bind)
            assert health_bind and health_bind['listen_address'] == '127.0.0.1', (label, health_bind)
            reached = False
            try:
                socket.create_connection((remote_address, 18087), timeout=1).close(); reached = True
            except OSError:
                pass
            assert not reached, 'listener reached a non-loopback address with ' + label
        finally:
            stop_serve(proc_bind)
            if bad_ini.exists(): bad_ini.unlink()
    print('PASS M1: Remote=1 without a private BindAddress (absent or public) listens on this PC only', flush=True)
    sec_ini = build/'security_test.ini'
    sec_ini.write_text('[Mod]\nPort=18086\nRemote=1\nBindAddress=%s\n' % remote_address, encoding='ascii')
    proc = None
    try:
        proc = start_serve({'TELEMETRY_INI': str(sec_ini), 'TELEMETRY_PREVIEW': '1'})
        health = poll_health(18086, proc)
        assert health and health['port'] == 18086 and health['listen_address'] == '127.0.0.1', health
        # M3: Host allowlist on every route, reads included; the bound port and the configured address only
        get_health = b'GET /health HTTP/1.1\r\n'
        refused_hosts = {
            'absent Host': get_health + b'\r\n',
            'other name': get_health + b'Host: evil.example:18086\r\n\r\n',
            'other port': get_health + b'Host: 127.0.0.1:18080\r\n\r\n',
            'no port': get_health + b'Host: 127.0.0.1\r\n\r\n',
            'duplicate Host': get_health + b'Host: 127.0.0.1:18086\r\nHost: 127.0.0.1:18086\r\n\r\n',
            'rebinding name': get_health + b'Host: attacker.test:18086\r\n\r\n',
            'HTTP/1.0 without Host': b'GET /health HTTP/1.0\r\n\r\n',
            'events, other name': b'GET /events HTTP/1.1\r\nHost: evil.example:18086\r\n\r\n',
        }
        for label, req in refused_hosts.items():
            code = sec_status(sec_raw('127.0.0.1', 18086, req))
            assert code == 403, (label, code)
        assert sec_status(sec_raw('127.0.0.1', 18086, get_health + b'Host: 127.0.0.1:18086\r\n\r\n')) == 200
        assert sec_status(sec_raw('127.0.0.1', 18086, get_health + b'Host: localhost:18086\r\n\r\n')) == 200
        assert sec_status(sec_raw(remote_address, 18086, get_health + ('Host: %s:18086\r\n\r\n' % remote_address).encode('ascii'))) == 200
        print('PASS M3: Host allowlist refuses absent, foreign, wrong-port, portless, duplicate and HTTP/1.0 Host on every route', flush=True)
        # M4: a forwarded request is not local; the peer is 127.x in both cases
        body = b'{"recording":"No_Such_Recording"}'
        def recordings_load(extra=b''):
            return sec_raw('127.0.0.1', 18086, b'POST /recordings/load HTTP/1.1\r\nHost: 127.0.0.1:18086\r\n'
                           b'Content-Type: application/json\r\nX-Telemetry-Action: load\r\n'
                           b'Content-Length: ' + str(len(body)).encode('ascii') + b'\r\n' + extra + b'\r\n' + body)
        assert sec_status(recordings_load()) == 404
        out = recordings_load(b'X-Forwarded-For: 203.0.113.9\r\n')
        assert sec_status(out) == 403 and b'only from the game PC' in out, out[:160]
        def director_state(extra=b''):
            return sec_raw('127.0.0.1', 18086, b'GET /director/state HTTP/1.1\r\nHost: 127.0.0.1:18086\r\n' + extra + b'\r\n')
        assert b'"viewer_local":true' in director_state()
        assert b'"viewer_local":false' in director_state(b'Via: 1.1 tailscale\r\n')
        print('PASS M4: X-Forwarded-For, Via and Forwarded make a loopback request non-local (recordings refused, viewer_local false)', flush=True)
        # M5: a trickling header does not hold the listener; a second client is answered at once
        slow = socket.create_connection(('127.0.0.1', 18086), timeout=3)
        slow.sendall(b'GET /hea')
        time.sleep(0.2)
        t0 = time.time()
        code = sec_status(sec_raw('127.0.0.1', 18086, get_health + b'Host: 127.0.0.1:18086\r\n\r\n', timeout=2))
        elapsed = time.time() - t0
        slow.close()
        assert code == 200 and elapsed < 1.0, (code, elapsed)
        # M5: per-peer cap (4) for a non-loopback peer: the fifth connection from the same address is refused
        held = []
        for _ in range(4):
            s = socket.create_connection((remote_address, 18086), timeout=3)
            s.sendall(b'GET /hea')
            held.append(s)
        time.sleep(0.2)
        fifth = socket.create_connection((remote_address, 18086), timeout=3)
        got = b''
        try:
            fifth.sendall(('GET /health HTTP/1.1\r\nHost: %s:18086\r\n\r\n' % remote_address).encode('ascii'))
        except OSError:
            pass
        fifth.settimeout(3)
        try:
            while True:
                part = fifth.recv(65536)
                if not part: break
                got += part
        except OSError:
            pass
        fifth.close()
        assert b'200 OK' not in got, got[:80]
        for s in held: s.close()
        time.sleep(0.3)
        assert sec_status(sec_raw(remote_address, 18086, ('GET /health HTTP/1.1\r\nHost: %s:18086\r\n\r\n' % remote_address).encode('ascii'), timeout=3)) == 200
        # M5: total cap (32): with 32 idle connections open the 33rd is refused; the listener recovers after they close
        idle = []
        for _ in range(32):
            idle.append(socket.create_connection(('127.0.0.1', 18086), timeout=3))
        time.sleep(0.3)
        got = sec_raw('127.0.0.1', 18086, get_health + b'Host: 127.0.0.1:18086\r\n\r\n', timeout=3)
        assert b'200 OK' not in got, got[:80]
        for s in idle: s.close()
        time.sleep(0.3)
        assert sec_status(sec_raw('127.0.0.1', 18086, get_health + b'Host: 127.0.0.1:18086\r\n\r\n', timeout=3)) == 200
        print('PASS M5: a trickling header does not delay a second client (<1 s); 5th connection from one non-loopback peer and 33rd connection in total refused', flush=True)
        # M6: a slow reader gets the whole body (no 200 ms truncation); SO_RCVBUF is small so the server must wait for it
        slow_reader = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        slow_reader.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        slow_reader.settimeout(5)
        slow_reader.connect(('127.0.0.1', 18086))
        slow_reader.sendall(b'GET /telemetry HTTP/1.1\r\nHost: 127.0.0.1:18086\r\n\r\n')
        raw_all = b''
        while True:
            part = slow_reader.recv(2048)
            if not part: break
            raw_all += part
            time.sleep(0.03)
        slow_reader.close()
        head, _, body_bytes = raw_all.partition(b'\r\n\r\n')
        length = None
        for line in head.split(b'\r\n'):
            if line.lower().startswith(b'content-length:'):
                length = int(line.split(b':', 1)[1])
        assert length is not None and length == len(body_bytes), (length, len(body_bytes))
        json.loads(body_bytes)
        print('PASS M6: slow reader receives the complete /telemetry body (%d bytes)' % length, flush=True)
        with urlopen('http://127.0.0.1:18086/health', timeout=3) as response:
            assert json.load(response)['read_only']
    finally:
        if proc is not None: stop_serve(proc)
        if sec_ini.exists(): sec_ini.unlink()
    # ---- BACKEND 3 resource gates (measured; all logical cores per the owner CPU rule) ----
    b3=subprocess.run([sys.executable,str(component/'robustness_backend3.py'),str(build)],
                      check=True, capture_output=True, text=True)
    for line in b3.stdout.splitlines():
        if line.startswith(('PASS BACKEND 3','RESOURCE ')):
            print(line,flush=True)
    resource_line=next((l for l in b3.stdout.splitlines() if l.startswith('RESOURCE ')),None)
    assert resource_line and 'crashes=0' in resource_line, b3.stdout[-2000:]
finally:
    if run.poll() is None:
        run.terminate();run.wait(timeout=5)
    # 1.0.1: serve harnesses that were terminated cannot remove their archived
    # test session; drop those %TEMP% folders (test output only, never workspace).
    import shutil, tempfile
    for stale in Path(tempfile.gettempdir()).glob('f1c-web-telemetry-arserve-*'):
        shutil.rmtree(stale, ignore_errors=True)
