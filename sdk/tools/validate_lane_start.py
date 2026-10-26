import re, glob, os, math

GAMEDIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "..",
    "F1 Challenge V10", "SeasonData", "Circuits")

def parse_aiw(path):
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    wps = []
    cur = None
    pits = []
    idx = 0
    for ln in lines:
        s = ln.strip()
        if s.startswith('[Waypoint]'):
            cur = {}
            continue
        if cur is not None:
            m = re.match(r'wp_(\w+)=(.*?)(//.*)?$', s)
            if m:
                key = m.group(1).strip()
                val = m.group(2).strip()
                if key == 'pos':
                    cur['pos'] = tuple(float(x) for x in val.strip('()').split(','))
                elif key == 'score':
                    p = val.strip('()').split(',')
                    cur['sector'] = int(p[0])
                    cur['lapdist'] = float(p[1])
                elif key == 'branchID':
                    cur['branch'] = int(val.strip('()'))
                    cur['idx'] = idx
                    wps.append(cur)
                    cur = {}
                    idx += 1
        if s.startswith('PitPos='):
            v = s.split('=')[1].split('//')[0].strip()
            pits.append(tuple(float(x) for x in v.strip('()').split(',')))
    return wps, pits

def nearest_lane(wps, xyz):
    best, bd = None, 1e18
    for w in wps:
        if w.get('branch') == 1:
            p = w['pos']
            d = (p[0]-xyz[0])**2 + (p[1]-xyz[1])**2 + (p[2]-xyz[2])**2
            if d < bd:
                bd, best = d, w
    return best

results = []
for path in sorted(glob.glob(os.path.join(GAMEDIR, "**", "*.aiw"), recursive=True)):
    name = os.path.basename(path)[:-4]
    try:
        wps, pits = parse_aiw(path)
    except Exception as e:
        results.append((name, 'PARSE-ERR', str(e)[:40]))
        continue
    lane = [w for w in wps if w.get('branch') == 1]
    if not lane:
        results.append((name, 'NO-LANE', ''))
        continue
    lane.sort(key=lambda w: w['idx'])
    first = lane[0]
    if not pits:
        results.append((name, 'NO-PITS', 'lane-start=wp%d' % first['idx']))
        continue
    stall0 = pits[0]
    near = nearest_lane(lane, stall0)
    margin = near['idx'] - first['idx']
    flag = 'OK' if margin > 5 else ('EDGE' if margin > 0 else 'FAIL')
    results.append((name, flag, 'laneStart=wp%d(Z%.0f) stallNear=wp%d(Z%.0f) stallZ=%.0f margin=%dwp' % (
        first['idx'], first['pos'][2], near['idx'], near['pos'][2], stall0[2], margin)))

print('%-28s %-6s %s' % ('track', 'flag', 'detail'))
for r in results:
    print('%-28s %-6s %s' % (r[0], r[1], r[2]))
fails = [r for r in results if r[1] == 'FAIL']
edges = [r for r in results if r[1] == 'EDGE']
ok = [r for r in results if r[1] == 'OK']
print()
print('tracks: %d  OK: %d  EDGE: %d  FAIL: %d  NO-LANE: %d' % (
    len(results), len(ok), len(edges), len(fails), len([r for r in results if r[1] == 'NO-LANE'])))
for r in fails:
    print('  FAIL:', r[0], r[2])
for r in edges:
    print('  EDGE:', r[0], r[2])
