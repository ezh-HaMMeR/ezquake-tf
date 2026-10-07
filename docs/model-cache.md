# Model loading cache

`r_modelcache 1` (default) reuses the existing alias/MD3 GPU buffer across
map changes when all requested models remain in that buffer. A subset of
the previous model set is also reusable. Newly needed or reloaded models
cause a rebuild. Brushes, lightmaps and map-specific data are not cached by
this feature. The cache does not preload every file in every PAK.

`r_modelcache_mb 256` limits additional retained CPU vertex buffers to
256 MiB (range 0–2048). Buffers for the current GPU generation get priority;
unused older generations are dropped after rebuilding. This is not a limit
on the client's total RAM, decoded models or GPU memory. A zero budget still
allows GPU reuse. Classic immediate mode needs its CPU geometry for drawing
and is outside this optional retention budget. `r_modelcache 0` disables
GPU reuse and optional CPU retention, restoring the previous reload policy.

`Cache_Flush`, gamedir changes, `fs_restart`, and renderer shutdown invalidate
GPU reuse. `vid_restart` reloads models and texture bindings for the new
context. Changing a PAK on disk requires the usual filesystem restart or
client restart; this cache does not add a file watcher.

`r_modelcache_disk 1` optionally caches prepared MD3 vertices between client
runs. Default is 0: on fast CPUs/SSDs, checksum and disk I/O costs can outweigh
the benefit. Files live in `<basedir>/ezquake/cache/md3-v1`, with eight
replaceable slots and a maximum of 128 MiB per file (1 GiB total, plus a
transient write file). Slots are selected by model name; collisions produce
misses, not incorrect data. The format checks source content, model name,
render flags, format version, endianness, expected payload length, exact EOF
and payload checksum. Invalid or unwritable files fall back to normal
preparation. No textures, pointers, entity state or executable code is saved.
The folder may be removed while the client is closed.

`r_modelcache_stats 1` prints CPU MD3 preparation, total MD3 load, model
reload and GPU build/reuse timings to the console. These timings do not
include the full BSP/lightmap/sound/network loading sequence. They must not
be presented as total map loading times.

MD3 packed-normal decoding uses a 256-entry angle lookup table instead of
repeating trigonometric calculations for every vertex of every frame.

Validation: `tools/test_tf_model_cache_runtime.py CASE` runs an isolated
Windows fixture under `assets/tf-v7-regression` using local Quake assets.
Cases: classic, immediate, modern, zero_budget, new_models, vid_restart,
fs_restart, disk_cold, disk_warm, disk_corrupt. The disk cases must run in
that order. This requires the v7 model staging folder and a built client.
