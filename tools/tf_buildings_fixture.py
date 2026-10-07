"""Offline MVD fixture showing both teams, all levels and every firing frame."""
import argparse
from pathlib import Path
import shutil
import struct

from tf_model_skin_fixture import z, record, user, player


def write_pak(folder, path):
    data=bytearray(b'PACK'+b'\0'*8); entries=[]
    for file in sorted(folder.rglob('*')):
        if not file.is_file(): continue
        name=file.relative_to(folder).as_posix().encode('ascii')
        assert len(name)<56
        content=file.read_bytes(); entries.append(struct.pack('<56s2i',name,len(data),len(content)))
        data.extend(content)
    offset=len(data); directory=b''.join(entries); data.extend(directory)
    struct.pack_into('<2i',data,4,offset,len(directory))
    path.write_bytes(data)


def entity(number, model, x, y, height, frame, owner, skin=0,yaw=64):
    flags=number | (7<<9) | (1<<12) | (1<<13) | (1<<15)
    return struct.pack('<H',flags)+bytes([4|8|16,model,frame,owner,skin])+struct.pack('<2hbh',x*8,y*8,yaw,height*8)


def build(runtime, source, pack, executable, renderer,showcase=False,world=False,reference_layout=False,load_benchmark=False,debris=False):
    for folder in ['id1','fortress/maps','fortress/progs','fortress/gfx','qw','ezquake/configs']:
        (runtime/folder).mkdir(parents=True,exist_ok=True)
    for name in ['id1/pak0.pak','fortress/pak0.pak','fortress/pak1.pak']:
        shutil.copyfile(source/name,runtime/name)
    shutil.copytree(pack/'fortress',runtime/'fortress',dirs_exist_ok=True)
    write_pak(pack/'fortress',runtime/'fortress/pak2.pak')
    shutil.copyfile(executable,runtime/'ezquake.exe')
    bsp=bytearray((source/'fortress/maps/caverns.bsp').read_bytes())
    for lump,stride,field in ([] if world else [(5,24,22),(10,28,22),(14,64,60)]):
        offset,length=struct.unpack_from('<2i',bsp,4+lump*8)
        for pos in range(offset,offset+length,stride):
            struct.pack_into('<i' if lump==14 else '<H',bsp,pos+field,0)
    (runtime/'fortress/maps/fixture.bsp').write_bytes(bsp)
    models=['maps/fixture.bsp','progs/player.mdl','progs/turrbase.mdl','progs/turrgun.mdl','progs/disp.mdl']
    if debris: models += ['progs/'+n+'.mdl' for n in ['dgib1','dgib2','dgib3','tgib1','tgib2','tgib3']]
    init=bytes([11])+struct.pack('<2i',28,1)+z('fortress')+struct.pack('<f',0)+z('TF HD buildings')
    init+=struct.pack('<10f',800,100,320,500,10,.7,10,4,4,1)
    init+=bytes([9])+z('fullserverinfo "\\*gamedir\\fortress\\teamplay\\1\\maxclients\\32\\fpd\\512"\n')
    init+=bytes([46,0,0,0,45,0])+b''.join(z(m) for m in models)+b'\0\0'
    init+=user(0,'RedEngineer','red',4,'tf_eng')+user(1,'BlueEngineer','blue',13,'tf_eng')
    init+=bytes([12,0])+z('m')
    init+=bytes([9])+z('skins\n')
    # Camera faces diagonally towards +X gun fronts; world is hidden for clarity.
    init+=bytes([30])+struct.pack('<3h',260*8,350*8,-215*8)+bytes([14,192,0,31,0])
    (runtime/'fortress/gfx/finale.lmp').write_bytes(struct.pack('<2i',1,1)+b'\xff')
    (runtime/'fixture-overlay/gfx').mkdir(parents=True,exist_ok=True)
    shutil.copyfile(runtime/'fortress/gfx/finale.lmp',runtime/'fixture-overlay/gfx/finale.lmp')
    write_pak(runtime/'fixture-overlay',runtime/'fortress/pak3.pak')
    demo=record(init)
    for f in range(1,161):
        # A player update completes the QW signon; without it the client keeps
        # its startup console open and never reaches the active demo state.
        msg=player(0,2,1000,skin=0)+bytes([47])
        number=40
        if debris:
            for i in range(6):
                msg+=entity(number+i,6+i,385-i*50,160,-288,0,0,yaw=48)
        for row,owner in ([] if debris else ([(1,1)] if showcase else [(0,1),(1,2)])):
            for column in range(4):
                x=(359-column*68) if reference_layout else (155+column*68); y=65+row*95
                yaw=48 if reference_layout else (80 if showcase else 64)
                if column<3:
                    msg+=entity(number,3,x,y,-311,0,owner,yaw=yaw); number+=1
                    msg+=entity(number,4,x,y,-303,column*3+(f//3)%3,owner,column,yaw=yaw); number+=1
                else:
                    msg+=entity(number,5,x,y,-303,f%2,owner,yaw=yaw); number+=1
        msg+=b'\0\0'
        if f==1: msg+=bytes([9])+z('echo TF_BUILDINGS_LOAD_READY\n')
        if f==4 and load_benchmark:msg+=bytes([9])+z('quit\n')
        if f==5: msg+=bytes([9])+z('toggleconsole\n')
        if f==25: msg+=bytes([9])+z('screenshot buildings_teams\n')
        if f==50: msg+=user(0,'RedEngineer','blue',13,'tf_eng')+user(1,'BlueEngineer','red',4,'tf_eng')
        if f==65: msg+=bytes([9])+z('screenshot buildings_swap\n')
        if f==80: msg+=bytes([40,0])+struct.pack('<i',0)+b'\0'
        if f==95: msg+=bytes([9])+z('screenshot buildings_disconnect\n')
        if f==110: msg+=bytes([9])+z('echo TF_BUILDINGS_FIXTURE_COMPLETE; quit\n')
        demo+=record(msg,100)
    (runtime/'qw/tf_buildings.mvd').write_bytes(demo)
    (runtime/'ezquake/configs/config.cfg').write_text(
        f'cfg_save_onquit 0\nautoupdate 0\nvid_renderer {renderer}\nvid_fullscreen 0\n'
        'vid_width 1400\nvid_height 900\nvid_win_width 1400\nvid_win_height 900\ncl_onload ""\n',encoding='ascii')
    (runtime/'fortress/autoexec.cfg').write_text('',encoding='ascii')
    (runtime/'qw/fixture.cfg').write_text('\n'.join([
        'cfg_save_onquit 0','autoupdate 0','viewsize 120','fov 70','scr_newhud 0',
        'scr_consize 0','scr_conalpha 0','con_notifylines 0','con_notifytime 0',
        'r_drawworld '+str(int(world)),'gl_spec_xray_distance 9999','r_drawviewmodel 0','gl_fb_models 1',
        'gl_outline 0','gl_no24bit 0','r_drawhud 0',
        'cl_nolerp 1','gl_texturemode GL_LINEAR_MIPMAP_LINEAR','sshot_format png',
        'enemycolor off','teamcolor off','echo TF_BUILDINGS_LOAD_BEGIN','playdemo tf_buildings','']),encoding='ascii')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--source',type=Path,default=Path('C:/Games/qwtf'))
    parser.add_argument('--pack',type=Path,default=Path('assets/tf-buildings'))
    parser.add_argument('--executable',type=Path,default=Path('build-msbuild-x64/Release/ezquake.exe'))
    parser.add_argument('--renderer',type=int,default=0)
    parser.add_argument('--showcase',action='store_true')
    parser.add_argument('--world',action='store_true')
    parser.add_argument('--reference-layout',action='store_true')
    parser.add_argument('--load-benchmark',action='store_true')
    parser.add_argument('--debris',action='store_true')
    args=parser.parse_args()
    build(args.runtime,args.source,args.pack,args.executable,args.renderer,args.showcase,args.world,args.reference_layout,args.load_benchmark,args.debris)
