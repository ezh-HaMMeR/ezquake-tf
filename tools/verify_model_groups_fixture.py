"""Validate screenshots produced by model_groups_fixture.py (Pillow, numpy)."""
from pathlib import Path
import argparse,json
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('runtime',type=Path);a=p.parse_args()
root=a.runtime/'fortress'
shots={f.stem.removeprefix('groups_'):np.asarray(Image.open(f).convert('RGB')) for f in root.glob('groups_*.png')}
colors=[(255,0,0),(0,80,255),(0,255,0),(255,255,0)]
counts={n:[int(np.all(im==c,axis=2).sum()) for c in colors] for n,im in shots.items()}
assert all(c>100 for c in counts['styles'][:3]),counts
assert counts['priority'][0]>100 and counts['priority'][1]==0,counts
assert counts['disabled'][0]==0 and counts['disabled'][1]==0,counts
assert counts['players_only'][0]==0 and counts['players_only'][1]==0 and counts['players_only'][2]>100,counts
for n in ['baseline','global_off','dark0','bright0','dark1','bright1','empty_slots']:
 assert counts[n][:3]==[0,0,0],(n,counts[n])
if 'thin' in shots:
 assert counts['styles'][3]>100,counts
 assert counts['thin'][0]<counts['styles'][0]*.7,counts
 assert counts['players_only'][3]==0,counts
# Static dispenser body: inversion works for either master value.
brightness={n:float(shots[n][350:520,470:580].mean()) for n in ['dark0','bright0','dark1','bright1']}
assert brightness['bright0']>brightness['dark0']*1.5,brightness
assert brightness['bright1']>brightness['dark1']*1.5,brightness
log=(a.runtime/'test.log').read_text(errors='replace')
assert 'MODEL_GROUPS_COMPLETE' in log
assert 'Unknown command' not in log and 'Host_Error' not in log
report={'color_pixels':counts,'dispenser_brightness':brightness,'passed':True}
(a.runtime/'verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'runtime':str(a.runtime),'passed':True,'dispenser_brightness':brightness}))
