"""Build an offline MVD rendering fixture with real TF2003 appearance assets.

Left to right: red Soldier, blue Spy disguised as a red Soldier,
red Soldier corpse (colormap 0), red headless corpse, blue Soldier.
The owner changes class/colors, then disconnects; corpses keep their skin.
All configuration commands are authored here, not read from demo input.
"""
from pathlib import Path
import argparse
import shutil
import struct


def z(s):
    return s.encode("ascii") + b"\0"


def record(payload, msec=0):
    return bytes([msec, 6]) + struct.pack("<i", len(payload)) + payload


def user(slot, name, team, color, skin):
    info = f"\\name\\{name}\\team\\{team}\\topcolor\\{color}\\bottomcolor\\{color}\\skin\\{skin}"
    return bytes([40, slot]) + struct.pack("<i", slot+1) + z(info)


def player(slot, model, x, skin=3):
    flags = 7 | (7 << 3) | (1 << 7) | (1 << 11)
    return bytes([42, slot]) + struct.pack("<H", flags) + bytes([0]) + struct.pack("<3h3H", x*8, 80*8, -311*8, 0, 16384, 0) + bytes([model, skin])


def corpse(number, model, x):
    flags = number | (7 << 9) | (1 << 12) | (1 << 13) | (1 << 15)
    return struct.pack("<H", flags) + bytes([4 | 8 | 16, model, 178, 0, 3]) + struct.pack("<2hbh", x*8, 80*8, 64, -311*8)


def build(runtime, source, tf_root):
    for directory in ["id1", "fortress/maps", "fortress/progs", "fortress/skins", "qw"]:
        (runtime / directory).mkdir(parents=True, exist_ok=True)
    for name in ["id1/pak0.pak", "fortress/pak0.pak", "fortress/pak1.pak", "fortress/maps/caverns.bsp",
                 "fortress/progs/player.mdl", "fortress/progs/headless.mdl"]:
        shutil.copyfile(source / name, runtime / name)
    for path in (source / "fortress/skins").glob("tf_*.pcx"):
        shutil.copyfile(path, runtime / "fortress/skins" / path.name)
    for path in (tf_root / "assets/fortress/progs").glob("tf*.mdl"):
        shutil.copyfile(path, runtime / "fortress/progs" / path.name)
    # Remove world draw surfaces while retaining a valid BSP and its collision/visibility data.
    bsp = bytearray((runtime / "fortress/maps/caverns.bsp").read_bytes())
    for lump, stride, field in [(5, 24, 22), (10, 28, 22), (14, 64, 60)]:
        offset, length = struct.unpack_from("<2i", bsp, 4+lump*8)
        for pos in range(offset, offset+length, stride):
            struct.pack_into("<i" if lump == 14 else "<H", bsp, pos+field, 0)
    (runtime / "fortress/maps/fixture.bsp").write_bytes(bsp)
    models = ["maps/fixture.bsp", "progs/player.mdl", "progs/tfbody2.mdl", "progs/tfheadless2.mdl"]
    init = bytes([11]) + struct.pack("<2i", 28, 1) + z("fortress") + struct.pack("<f", 0) + z("TF model skin fixture")
    init += struct.pack("<10f", 800, 100, 320, 500, 10, .7, 10, 4, 4, 1)
    init += bytes([9]) + z('fullserverinfo "\\*gamedir\\fortress\\teamplay\\1\\maxclients\\32\\fpd\\512"\n')
    init += bytes([46, 0, 0, 0, 45, 0]) + b"".join(z(m) for m in models) + b"\0\0"
    for slot, name, team, color, skin in [(0, "Soldier", "red", 4, "tf_sold"),
                                           (1, "Spy", "blue", 13, "tf_spy"),
                                           (2, "BlueSoldier", "blue", 13, "tf_sold"),
                                           (3, "CorpseOwner", "blue", 13, "tf_spy")]:
        init += user(slot, name, team, color, skin)
    init += bytes([12, 0]) + z("m") + bytes([9]) + z("skins\n")
    # Fixed camera, with finale instead of the automatic intermission scoreboard.
    init += bytes([30]) + struct.pack("<3h", 226*8, 220*8, -273*8) + bytes([7, 192, 0, 31, 0])
    demo = record(init)
    stages = {
        5: 'menu_ingame 0; menu_main; togglemenu\n',
        30: 'enemycolor 00FFFF; teamcolor FF00FF; echo TF_STAGE_RGB; screenshot rgb\n',
        70: 'enemycolor FFFF00; teamcolor 00FF00; echo TF_STAGE_CHANGED; screenshot changed\n',
        110: 'red_team_color 3366FF; echo TF_STAGE_INDIVIDUAL; screenshot individual\n',
        150: 'red_team_color ""; enemycolor off; teamcolor off; echo TF_STAGE_OFF; screenshot off\n',
        190: 'enemycolor FF0000; teamcolor 0000FF; gl_nocolors 1; echo TF_STAGE_NOCOLORS; screenshot nocolors\n',
        230: 'gl_nocolors 0; echo TF_STAGE_REENABLED; screenshot reenabled\n',
        270: 'echo TF_FIXTURE_COMPLETE; quit\n',
    }
    for frame in range(1, 281):
        msg = player(0, 2, 158) + player(1, 3, 192) + player(2, 2, 294)
        msg += bytes([47]) + corpse(40, 3, 226) + corpse(41, 4, 260) + b"\0\0"
        if frame == 90: msg += user(3, "CorpseOwner", "green", 11, "tf_medic")
        if frame == 91: msg += bytes([40, 3]) + struct.pack("<i", 0) + b"\0"
        if frame in stages:
            command = stages[frame]
            if "; screenshot" in command: command = command.split("; screenshot")[0] + "\n"
            msg += bytes([9]) + z(command)
        if frame-2 in stages and "; screenshot" in stages[frame-2]:
            msg += bytes([9]) + z("screenshot " + stages[frame-2].split("; screenshot ")[1])
        demo += record(msg, 100)
    (runtime / "qw/tf_model_skin.mvd").write_bytes(demo)
    (runtime / "qw/fixture.cfg").write_text("\n".join([
        "cfg_save_onquit 0", "autoupdate 0", "vid_fullscreen 0", "vid_width 1000", "vid_height 600",
        "vid_win_width 1000", "vid_win_height 600", "viewsize 120", "fov 90", "scr_newhud 0",
        "r_drawworld 0", "gl_spec_xray_distance 9999", "r_drawviewmodel 0", "r_fullbrightSkins 1", "gl_fb_models 1", "cl_nolerp 1", "teamlock red",
        'red_team_color ""', 'blue_team_color ""', 'green_team_color ""', 'yellow_team_color ""',
        "enemycolor 00FFFF", "teamcolor FF00FF", "sshot_format png",
        "playdemo tf_model_skin", "\n"]), encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--tf-root", type=Path, required=True)
    args = parser.parse_args()
    build(args.runtime, args.source, args.tf_root)
