# Opt-in TF sentry rotor animation

The MD3 replacement for `progs/turrgun.mdl` can opt into independent rotor
animation without changing the server protocol. Only that model name (or
`progs/turrgun.md3`) is eligible. Ordinary models retain existing behavior.

The model must contain exactly 82 frames. Frames 0–9 retain the original TF
poses. Frames 10–33, 34–57 and 58–81 contain one full rotor revolution for
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

Older clients can use frames 0–9 as before, subject to their MD3 limits.
The additional geometry increases model memory and loading cost.

Build `tf_sentry_animation_tests` with `BUILD_CFG_EDITOR_TESTS=ON` and run
`ctest -C Release -R tf_sentry_animation --output-on-failure` for marker,
frame-boundary, idle/reset, phase and loop tests.
