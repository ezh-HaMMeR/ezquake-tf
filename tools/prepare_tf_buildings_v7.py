"""Lossless surface split of v6, retaining legacy poses and three rig poses."""
from pathlib import Path
import struct,shutil,json,argparse
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=ROOT/'assets/tf-buildings-v6-release')
parser.add_argument('--output',type=Path,default=ROOT/'assets/tf-buildings-v7-release')
args=parser.parse_args();SRC=args.source;OUT=args.output
shutil.copytree(SRC/'fortress',OUT/'fortress',dirs_exist_ok=True)
p=OUT/'fortress/progs/turrgun.md3';d=p.read_bytes();h=struct.unpack_from('<4si64s9i',d);sh=struct.unpack_from('<4s64s10i',d,h[10]);n=sh[5]
frames=list(range(10))+[10,34,58]
a=np.frombuffer(d,dtype=np.dtype([('xyz','<i2',(3,)),('normal','<u2')]),offset=h[10]+sh[10],count=82*n).reshape(82,n)
tri=np.frombuffer(d,dtype='<i4',offset=h[10]+sh[7],count=sh[6]*3).reshape(-1,3)
uv=np.frombuffer(d,dtype='<f4',offset=h[10]+sh[9],count=n*2).reshape(n,2)
moving=np.any(a[58]['xyz']!=a[59]['xyz'],axis=1)|(a[58]['normal']!=a[59]['normal'])
rotor=np.any(moving[tri],axis=1)
left=np.mean(a[58]['xyz'][tri][:,:,1],axis=1)<0
parts=[('body',~rotor),('tfrotor_l',rotor&left),('tfrotor_r',rotor&~left)]
fixed=lambda x,n:x.encode().ljust(n,b'\0')
surfaces=[];stats={}
for name,mask in parts:
 t=tri[mask];ids=np.unique(t);remap=np.full(n,-1,dtype=np.int32);remap[ids]=np.arange(len(ids));t=remap[t].astype('<i4')
 tb=t.tobytes();shader=d[h[10]+sh[8]:h[10]+sh[8]+68];st=uv[ids].tobytes();xyz=a[frames][:,ids].tobytes()
 ot=108;osh=ot+len(tb);ost=osh+68;ox=ost+len(st);end=ox+len(xyz)
 surfaces.append(struct.pack('<4s64s10i',b'IDP3',fixed(name,64),0,13,1,len(ids),len(t),ot,osh,ost,ox,end)+tb+shader+st+xyz)
 # Every triangle retains exact UVs, normals and positions in every selected pose.
 assert np.array_equal(a[frames][:,ids][:,t],a[frames][:,tri[mask]])
 stats[name]={'vertices':len(ids),'triangles':len(t)}
fb=[];tags=[]
for i,f in enumerate(frames):
 raw=bytearray(d[h[8]+f*56:h[8]+(f+1)*56])
 if i>=10:
  raw[40:56]=fixed(f'tfrig{i-9}_x',16)
  allpos=a[10+(i-10)*24:34+(i-10)*24]['xyz'].astype(float)/64
  struct.pack_into('<6f',raw,0,*allpos.min(axis=(0,1)),*allpos.max(axis=(0,1)))
 fb.append(raw)
 level=i-9 if i>=10 else (1 if i==9 else i//3+1)
 for name,cy in [('tfrotor_l',-11),('tfrotor_r',11)]:
  if level==1 and name=='tfrotor_l':cy=0
  cz=25.1 if level==1 and name=='tfrotor_l' else 23.7
  tags.append(struct.pack('<64s12f',fixed(name,64),0,cy,cz,1,0,0,0,1,0,0,0,1))
ft=b''.join(fb);tagdata=b''.join(tags);ofs=108+len(ft)+len(tagdata);body=b''.join(surfaces)
hdr=struct.pack('<4si64s9i',b'IDP3',15,fixed('TF Buildings v7 rigid rotors',64),0,13,2,3,3,108,108+len(ft),ofs,ofs+len(body))
p.write_bytes(hdr+ft+tagdata+body)
for level in range(3):
 tex='textures/tfbuildv5/turrgun'+(f'_l{level+1}' if level<2 else '')
 (p.parent/f'turrgun_{level}.skin').write_text(''.join(name+','+tex+'\n' for name,_ in parts))
for file in SRC.glob('*.blend'):shutil.copyfile(file,OUT/file.name)
result={'old_bytes':len(d),'new_bytes':p.stat().st_size,'frames':13,'surfaces':stats,'vbo_old_mib':sh[6]*3*82*64/1048576,'vbo_new_mib':sh[6]*3*13*64/1048576}
(OUT/'rig-verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
