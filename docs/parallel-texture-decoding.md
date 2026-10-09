# Parallel model skin decoding

`r_texture_decode_threads 4` is the default. External PNG and true-color
24/32-bit TGA skins (including RLE) of MDL/MD3 models can be decoded in parallel.
Animated MDL skin groups and luma textures are included. MD3 preparation queues
the primary surface skin names; alternative shader/folder paths retain serial
loading when the primary name cannot be decoded.

```text
r_texture_decode_threads 4   // Default; up to four decoding threads
r_texture_decode_threads 2   // Lower CPU concurrency
r_texture_decode_threads 0   // Original synchronous image decoding
r_texture_decode_threads 1   // Memory decoder, one thread (diagnostics)
r_modelcache_stats 1         // Includes TextureDecode batch timing
```

The effective concurrency is limited by the CPU count, the number of queued
images and a maximum of eight. The main thread participates in decoding, so
the setting counts all decoding threads, not just background workers. Negative
values act as zero and values above eight are capped. The setting affects newly
loaded models; already cached models are not discarded. Compare fresh client
processes with the same resources to measure the first-map effect.

The main thread resolves filenames with the normal format/loose-file priority,
reads compressed bytes from the VFS, then starts a bounded batch for one model.
Workers use isolated libpng state or the memory TGA decoder, malloc/free and
an atomic work index. They never call VFS, engine allocators, console/error
handlers or OpenGL. All workers join before model processing resumes. Gamma,
luma merging, mip generation, texture CRC and GPU upload retain their existing
main-thread paths.

Up to 32 images and 128 MiB of combined encoded and decoded buffers are queued.
This is a batch budget, not a cap on total process RAM or libpng's internal
working memory. Excess images, unsupported image types, `.link` targets,
allocation/decode failures and JPEG/PCX use the ordinary loader. The queue is
deduplicated, consumed once and cleared at model completion or host abort/map
clear. This is not a persistent disk cache and does not preload every PAK.

The output remains RGBA8 at the original dimensions. No texture compression,
resolution reduction, geometry changes or changes to model lighting are made.
Both classic and modern renderers use the same preparation path.

## Verification

`image_decode_memory_tests` checks PNG RGB/RGBA/grayscale/palette, transparency,
1/8/16-bit inputs, interlacing, TGA origins/alpha/raw/RLE, truncated packets and
concurrent decoding. `tools/verify_texture_decode_pack.py` compares all PNG/TGA
textures in a supplied PAK byte-for-byte against Pillow's decoded RGBA output.

Three alternating fresh-process runs per setting (`0` and `4`), same executable,
isolated nine-model grenade fixture, disk model cache disabled, Windows x64:

| Renderer / pack | Serial median | Parallel median | Difference |
| --- | ---: | ---: | ---: |
| Classic / original PNG | 3.021 s | 2.518 s | -0.503 s (16.6%) |
| Classic / lossless TGA RLE | 2.411 s | 2.289 s | -0.121 s (5.0%) |
| Modern / original PNG | 3.121 s | 2.618 s | -0.504 s (16.1%) |
| Modern / lossless TGA RLE | 2.489 s | 2.370 s | -0.119 s (4.8%) |

These measure the fixture's load markers, not just PNG decode time. OS file
cache was not flushed. They are local measurements, not a guaranteed saving
for other hardware/maps. All 30 PNG textures and 24 lossless TGA replacements
were byte-exact against the independent decoder. Larger gains on the original
PNG pack are expected because the TGA pack has already reduced decoding cost.

The classic-renderer visual fixture was also compared with decoding disabled
and enabled, both for the original pack and with loose JPG/PCX/paletted-TGA and
`.link` overrides. All six screenshot comparisons differed by less than 0.002
on average per 8-bit channel (animated fixture timing). Modern rendering was
exercised by the loading benchmark. This is fixture coverage, not a live-server
gameplay test.
