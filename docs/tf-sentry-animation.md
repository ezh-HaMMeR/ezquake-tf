# Opt-in TF sentry rotor animation

The MD3 replacement for `progs/turrgun.mdl` can opt into independent rotor
animation without changing the server protocol. Only that model name (or
`progs/turrgun.md3`) is eligible. Ordinary models retain existing behavior.

The model must contain exactly 82 frames. Frames 0â€“9 retain the original TF
poses. Frames 10â€“33, 34â€“57 and 58â€“81 contain one full rotor revolution for
levels I, II and III respectively. Each cycle contains 24 evenly spaced
poses, with fixed topology and stationary non-rotating geometry. Their
16-byte, zero-padded MD3 frame names must be `tfspin1_00` through
`tfspin1_23`, then the corresponding `tfspin2_*` and `tfspin3_*` names.
The loader checks every marker before enabling the feature.

Network frames 1/2, 4/5 and 7/8 indicate firing. Frames 0, 3 and 6 are shared
between idle and firing: a transition from a firing pose at the same level
continues spinning for at most 125 ms. Entity resets and level changes do
not carry that idle grace period. Frame 9 remains the construction pose.
This follows the TF2003 100 ms firing cadence; other server frame layouts
are not automatically recognized.

The phase uses game time at two revolutions per second. `r_lerpframes 1`
interpolates between the 24 poses, including the last-to-first transition
within each level. Both renderer paths use the same pose selection; classic
shader VBO direction generation also wraps each level separately. Demo
pause and playback speed follow game time. No extra network messages,
gameplay changes, acceleration or deceleration are introduced.

Older clients can use frames 0â€“9 as before, subject to their MD3 limits.
The additional geometry increases model memory and loading cost.

Build `tf_sentry_animation_tests` with `BUILD_CFG_EDITOR_TESTS=ON` and run
`ctest -C Release -R tf_sentry_animation --output-on-failure` for marker,
frame-boundary, idle/reset, phase and loop tests.

## Compact rigid rotors (v2026.10.08.1)

TF Buildings v7 retains legacy frames 0–9, but replaces the 72 sampled
poses with three stationary poses at 10–12, named `tfrig1_x`, `tfrig2_x`,
`tfrig3_x` (zero-padded to 16 bytes). There are three surfaces: `body`,
`tfrotor_l`, `tfrotor_r`, and two tags per frame, named `tfrotor_l` and
`tfrotor_r`. Each tag's origin is the pivot of rotation about local X.
The tag axes are currently unused. The loader validates frame markers,
tag names, finite bounded origins, counts and tag/frame file ranges.

With `r_lerpframes 1`, the same firing/idle policy selects pose 10, 11 or 12.
The renderer rotates only the two rotor surfaces at 720 degrees per game
second. The modelview transform applies to regular, outline and shell passes
in classic and modern rendering. There is no frame-count-dependent stepping.
With interpolation disabled, network frames 0–9 remain usable. The v7
package requires the new client because older ezquake-tf builds have
multi-surface skin/additive-rendering bugs. Use the v6 package on older
clients. The 82-frame v6 format also remains supported by the new client.

The compact exporter preserves every selected triangle's coordinates,
normals and UVs byte-for-byte and leaves all textures unchanged.
