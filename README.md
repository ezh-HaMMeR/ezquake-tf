# ezQuake — Modern QuakeWorld Client

This repository is the ezquake-tf client, based on
[MeMcCree/ezquake-source](https://github.com/MeMcCree/ezquake-source).

GitHub releases provide Windows and Linux clients. Release tags use the
`vYYYY.MM.DD` format without a `-test` suffix. Packages are built locally by
default; the manual GitHub Actions workflow is retained only as a fallback.
Every client archive includes the complete `qw` directory with ezquake-tf
custom assets.

## TF teammate status HUD

The `teammates` HUD element displays live status received from a compatible
teamplay server. It accepts the existing ezQuake `//tinfo` protocol, never
offers enemy display, and hides the local player by default.

```text
hud_teammates_show 1
hud_teammates_place speed
hud_teammates_align_x center
hud_teammates_align_y before
hud_teammates_pos_y -20
hud_teammates_frame 0.47059
hud_teammates_frame_color "10 0 0"
hud_teammates_layout "%p%n $x10%l$x11 $x10 %g $x11 %a/%H %w"
hud_teammates_grow_up 1
hud_teammates_weapon_style 0
hud_teammates_weapon_icon_scale 0.65
hud_teammates_show_ammo 1
```

`hud_teammates_weapon_style 0` uses weapon icons; value `1` switches to the
classic letter abbreviations. `hud_teammates_weapon_icon_scale` controls only
the icon size. When `hud_teammates_grow_up` is enabled, additional teammates
are added above the existing rows.

With the extended TF2003 `//tinfo` format, `%w` shows the active weapon and its
active ammo count instead of guessing the best owned weapon. `%g` displays both
grenade types and counts. The location (`%l`) and grenade (`%g`) columns in
`hud_teammates` automatically size themselves to the visible contents.

The original right-side team-status panel is hidden by default and is
independent from the new HUD element. Enable it explicitly with:

```text
show_teammates_status 1
```

The legacy `scr_teaminfo 1` setting remains supported for configurations shared
with an unmodified ezQuake client. Either setting enables the original panel.

The game server/mod must send `//tinfo` only to eligible teammates; the client
does not infer other players' private health or inventory from network traffic.
The supported payload is:

```text
//tinfo <0-based-client> <x> <y> <z> <health> <armor> <items> <nick> <shells> <nails> <rockets> <cells>
```

TF2003 may append active-weapon, active-ammo and two grenade inventory groups;
older 12-field servers remain supported.

## Modifier key bindings

Bindings can use one native `Ctrl`, `Alt` or `Shift` modifier with any regular
bindable key. Modifier names are case-insensitive, and combination bindings
take precedence over the unmodified key while their modifier is held:

```text
bind Ctrl+c "cmd sg_reload"
bind Alt+x "shownick"
bind Shift+MOUSE3 "+zoom"
```

Combination bindings are preserved by `cfg_save`, listed by `bindlist`, and
removed with the usual `unbind Ctrl+c` syntax.

## TF Scout scanner beams

`new_scanmode` enables thin, team-colored Scout scanner beams without changing
the server-controlled scan distance. The value is the maximum number of
nearest detected players shown for each team:

```text
new_scanmode 0 // original scanner rendering (default)
new_scanmode 1 // nearest player from each team
new_scanmode 2 // two nearest players from each team
new_scanmode 3 // three nearest players from each team
gl_lightning_size 10 // scanner beam width (default)
```

Blue targets use a blue beam and red targets use a red beam. Yellow and green
are also supported for four-team games. For Scouts on TF2003 servers,
`new_scanmode` enables the server-provided friendly scan mode so both allies and
enemies can be shown. Target selection and scan range remain server-controlled.
Regular Lightning Gun beams retain their existing rendering and color settings.

Homepage: [https://ezquake.com][homepage]

Community discord: [http://discord.quake.world][discord]

This is the right place to start playing QuakeWorld&reg; — the fastest first
person shooter action game ever.

Combining the features of all modern QuakeWorld® clients, ezQuake makes
QuakeWorld&reg; easier to start and play. The immortal first person shooter
Quake&reg; in the brand new skin with superb graphics and extremely fast
gameplay.

## Features

 * Modern graphics
 * [QuakeTV][qtv] support
 * Rich menus
 * Multiview support
 * Tons of features to serve latest pro-gaming needs
 * Built in server browser & MP3 player control
 * Recorded games browser
 * Customization of all possible graphics elements of the game including Heads Up Display
 * All sorts of scripting possibilities
 * Windows, Linux, MacOSX and FreeBSD platforms supported (SDL2).

Our client comes only with bare minimum of game media. If you want to
experience ezQuake with modern graphics and other additional media including
custom configurations, maps, textures and more, try using the [nQuake][nQuake]-installer.

## Support

Need help with using ezQuake? Try #dev-corner on [discord][discord]

Or (less populated these days) visit us on IRC at QuakeNet, channel #ezQuake: [webchat][webchat] or [IRC][IRC].

Sometimes help from other users of ezQuake might be more useful to you so you
can also try visiting the [quakeworld.nu Client Talk-forums][forum].

If you have found a bug, please report it [here][issues]

## Installation guide

To play Quakeworld you need the files *pak0.pak* and *pak1.pak* from the original Quake-game.

### Install ezQuake to an existing Quake-installation
If you have an existing Quake-installation simply extract the ezQuake executable into your Quake-directory.

A typical error message when installing ezQuake into a pre-existing directory is about *glide2x.dll* missing.
To get rid of this error, remove the file *opengl32.dll* from your Quake directory.

### Upgrade an nQuake-installation
If you have a version of [nQuake][nQuake] already installed you can upgrade ezQuake by extracting the new executable into the nQuake-directory.

### Minimal clean installation
If you want to make a clean installation of ezQuake you can do this by following these steps:

1. Create a new directory
2. Extract the ezQuake-executable into this directory
3. Create a subdirectory called *id1*
4. Copy *pak0.pak* and *pak1.pak* into this subdirectory

## Compiling

On Linux, `./build-linux.sh` produces an ezQuake binary in the top directory. 

For a more in-depth description of how to build on all platforms, have a look at 
[BUILD.md](BUILD.md).

## Nightly builds

Nightly builds can be found [here][nightly]

 [nQuake]: http://nquake.com/
 [webchat]: http://webchat.quakenet.org/?channels=#ezquake
 [IRC]: irc://irc.quakenet.org/#ezquake
 [forum]: http://www.quakeworld.nu/forum/8
 [qtv]: http://qtv.quakeworld.nu/
 [nightly]: https://builds.quakeworld.nu/ezquake/snapshots/
 [releases]: https://github.com/ezQuake/ezquake-source/releases
 [issues]: https://github.com/ezQuake/ezquake-source/issues
 [homepage]: https://ezquake.com
 [discord]: http://discord.quake.world/
