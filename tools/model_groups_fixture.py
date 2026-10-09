"""Prepare isolated real-client A/B fixtures for model lighting and outline groups.
Requires an installed Quake/TF game and the optional HD building/grenade packs.
Run ezquake.exe -basedir <runtime> -game fortress +exec fixture.cfg.
"""
from pathlib import Path
import argparse, struct, shutil
from tf_buildings_fixture import build, entity, record, z, user, player

def prepare(runtime, source, pack, executable, renderer, immediate=False, grenade_pak=None):
    build(runtime,source,pack,executable,renderer,True,False,True)
    if grenade_pak: shutil.copyfile(grenade_pak,runtime/'fortress/pak4.pak')
    p=runtime/'fortress/maps/fixture.bsp';b=bytearray(p.read_bytes())
    o,n=struct.unpack_from('<2i',b,4+8*8);b[o:o+n]=bytes([32])*n;p.write_bytes(b)
    models=['maps/fixture.bsp','progs/player.mdl','progs/turrbase.mdl','progs/turrgun.mdl','progs/disp.mdl',
            'progs/hgren2.mdl','progs/missile.mdl','progs/backpack.mdl','progs/tf_stan.mdl','progs/dtree.mdl','progs/detpack.mdl']
    init=bytes([11])+struct.pack('<2i',28,1)+z('fortress')+struct.pack('<f',0)+z('Model group regression')
    init+=struct.pack('<10f',800,100,320,500,10,.7,10,4,4,1)
    init+=bytes([9])+z('fullserverinfo "\\*gamedir\\fortress\\teamplay\\1\\maxclients\\32\\fpd\\512"\n')
    init+=bytes([46,0,0,0,45,0])+b''.join(z(m) for m in models)+b'\0\0'
    init+=user(0,'RedEngineer','red',4,'tf_eng')+user(1,'BlueEngineer','blue',13,'tf_eng')
    init+=bytes([12,0])+z('m')+bytes([9])+z('skins\n')
    init+=bytes([30])+struct.pack('<3h',260*8,350*8,-215*8)+bytes([14,192,0,31,0])
    all_groups='players grenades buildings projectiles pickups objectives props'
    phases=[
        ('baseline','gl_outline 0; gl_fb_tfmodels 1; gl_fb_tfmodels_filter ""'),
        ('styles','gl_outline 1; gl_outline_group1 "grenades pickups objectives"; gl_outline_group1_color "255 0 0"; gl_outline_group1_scale 3; gl_outline_group2 "buildings"; gl_outline_group2_color "0 80 255"; gl_outline_group2_scale 2; gl_outline_group3 "players projectiles"; gl_outline_group3_color "0 255 0"; gl_outline_group3_scale 1; gl_outline_group4 "props"; gl_outline_group4_color "255 255 0"; gl_outline_group4_scale 0.5'),
        ('thin','gl_outline_group1_scale 0.3'),
        ('priority','gl_outline_group1_scale 3; gl_outline_group1 "grenades pickups objectives buildings"'),
        ('disabled','gl_outline_group1_enable 0'),
        ('players_only','gl_outline_group1 "grenades buildings projectiles pickups objectives props"; gl_outline_group1_enable 0; gl_outline_group2 "players"; gl_outline_group2_color "0 255 0"'),
        ('global_off','gl_outline 0'),
        ('dark0','gl_fb_tfmodels 0; gl_fb_tfmodels_filter ""'),
        ('bright0','gl_fb_tfmodels_filter "'+all_groups+'"'),
        ('dark1','gl_fb_tfmodels 1'),
        ('bright1','gl_fb_tfmodels_filter ""'),
        ('empty_slots','gl_outline 1; gl_outline_group1 ""; gl_outline_group2 ""; gl_outline_group3 ""; gl_outline_group4 ""'),
    ]
    demo=record(init)
    for f in range(1,25+len(phases)*15):
        msg=player(0,2,1000,skin=0)+bytes([47])
        for i,model in enumerate([2,6,5,7,8,9,10]):
            msg+=entity(40+i,model,390-i*45,160,-303,0,1,yaw=48)
        msg+=entity(50,11,240,95,-303,0,1,yaw=48)
        msg+=b'\0\0'
        if f==5:msg+=bytes([9])+z('toggleconsole\n')
        for i,(name,cmd) in enumerate(phases):
            if f==10+i*15:msg+=bytes([9])+z(cmd+'\n')
            if f==20+i*15:msg+=bytes([9])+z('screenshot groups_'+name+'\n')
        if f==22+len(phases)*15:msg+=bytes([9])+z('echo MODEL_GROUPS_COMPLETE; quit\n')
        demo+=record(msg,100)
    (runtime/'qw/tf_buildings.mvd').write_bytes(demo)
    p=runtime/'qw/fixture.cfg';cfg=p.read_text().replace('playdemo tf_buildings','gl_clear 1\nversion\nr_fullbrightSkins 1\n'+('gl_program_aliasmodels '+str(int(not immediate))+'\n' if renderer == 0 else '')+'playdemo tf_buildings');p.write_text(cfg)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--source',type=Path,default=Path('C:/Games/qwtf'))
    p.add_argument('--pack',type=Path,default=Path('assets/tf-buildings-v7a-release'))
    p.add_argument('--executable',type=Path,default=Path('build-msbuild-x64/Release/ezquake.exe'))
    p.add_argument('--renderer',type=int,default=0)
    p.add_argument('--immediate',action='store_true')
    p.add_argument('--grenade-pak',type=Path)
    a=p.parse_args();prepare(a.runtime.resolve(),a.source,a.pack,a.executable,a.renderer,a.immediate,a.grenade_pak)
