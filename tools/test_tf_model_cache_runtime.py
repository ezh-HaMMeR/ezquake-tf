"""Offline regression runner. Writes only isolated assets/tf-v7-regression."""
from pathlib import Path
import sys,shutil,subprocess,json,re
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
case=sys.argv[1];runtime=ROOT/'assets/tf-v7-regression';out=ROOT/'.codex_tmp'
s=(ROOT/'tools/tf_buildings_fixture.py').read_text().split("if __name__=='__main__':")[0]
s=s.replace('column*3+(f//3)%3','column*3+(f%2 if column==0 else f%3)').replace('entity(number,5,x,y,-303','entity(number,5,x,y,-311')
s=s.replace('for f in range(1,161):','for f in range(1,81):\n        if f in [26,51]:demo+=record(init)')
s=s.replace('if f==1:', 'if f in [1,27,52]:').replace('if f==110:','if f==75:')
if case == 'vid_restart':
 s=s.replace('if f==25:',f"if f==20:msg+=bytes([9])+z('{case}\\n')\n        if f==25:")
if case=='fs_restart':
 s=s.replace('if f==25:',"if f==20:msg+=bytes([9])+z('exec cache_fs.cfg\\n')\n        if f==25:")
if case=='new_models':
 s=s.replace('if f in [26,51]:demo+=record(init)',"if f in [26,51]:demo+=record(init.replace(z('progs/disp.mdl')+b'\\0\\0',z('progs/disp.mdl')+z('progs/dgib1.mdl')+b'\\0\\0') if f==26 else init)")
ns={'__name__':'regression'};exec(compile(s,'fixture','exec'),ns)
renderer=1 if case in ['modern','disk_cold','disk_warm','disk_corrupt'] else 0
ns['build'](runtime,Path('C:/Games/qwtf'),ROOT/'assets/tf-buildings-v7-release',ROOT/'build-msbuild-x64/Release/ezquake.exe',renderer,True,False,True)
for p in (ROOT/'assets/tf-buildings-v7-release/fortress').rglob('*'):
 if p.is_file():(runtime/'fortress'/p.relative_to(ROOT/'assets/tf-buildings-v7-release/fortress')).unlink()
config=runtime/'qw/fixture.cfg';cfg=config.read_text();opts=['r_modelcache 1','r_modelcache_stats 1','r_lerpframes 1','gl_program_aliasmodels '+str(int(case!='immediate')),'r_modelcache_mb '+('0' if case=='zero_budget' else '256'),'r_modelcache_disk '+str(int(case.startswith('disk_')))]
cfg=cfg.replace('playdemo tf_buildings','\n'.join(opts)+'\nversion\nplaydemo tf_buildings');
if case=='fs_restart':
 command=b'exec cache_fs.cfg\n';raw=(runtime/'qw/tf_buildings.mvd').read_bytes()
 (runtime/'qw/cache_after.mvd').write_bytes(raw.replace(command,b'echo AFTER_FS_OK\n'.ljust(len(command),b' ')))
 (runtime/'qw/cache_fs.cfg').write_text('fs_restart\nplaydemo cache_after\n')
config.write_text(cfg)
if case=='disk_corrupt':
 slots=list((runtime/'ezquake/cache/md3-v1').glob('*.bin'));assert slots
 for p in slots:
  with p.open('r+b') as f:f.seek(100);f.write(b'corrupt')
log=runtime/'qw/qconsole.log'
if log.exists():log.unlink()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
p=subprocess.Popen([str(runtime/'ezquake.exe'),'-basedir',str(runtime),'-nohome','-window','-condebug','+exec','fixture.cfg'],startupinfo=si)
try:p.wait(timeout=55)
except subprocess.TimeoutExpired:p.kill();raise
text=log.read_text(errors='replace');(out/f'v7-regression-{case}.log').write_text(text)
assert 'TF_BUILDINGS_FIXTURE_COMPLETE' in text,(case,p.returncode,text[-2000:])
assert p.returncode==0,(case,p.returncode)
if case not in ['immediate']:assert 'ModelCache GPU hit:' in text,case
if case=='disk_warm':assert 'ModelCache disk hit:' in text
if case=='disk_corrupt':assert 'ModelCache MD3 build: progs/turrgun.mdl' in text
if case=='fs_restart':
 s=s.replace('if f==25:',"if f==20:msg+=bytes([9])+z('exec cache_fs.cfg\\n')\n        if f==25:")
if case=='new_models':assert 'ModelCache MD3 build: progs/dgib1.mdl' in text
if case in ['vid_restart','fs_restart']:assert text.count('ModelCache MD3 build: progs/turrgun.mdl')>=2
for n in ['buildings_teams','buildings_swap']:
 image=runtime/f'fortress/{n}.png'
 if image.exists():shutil.copyfile(image,out/f'v7-{case}-{n}.png')
print(case,'PASS',flush=True)
for line in text.splitlines():
 if 'ModelCache' in line:print(line,flush=True)
