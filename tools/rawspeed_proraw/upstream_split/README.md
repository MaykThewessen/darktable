# Splitting rawspeed#963 into reviewable pull requests

darktable-org/rawspeed#963 bundles four unrelated concerns into one 9-file
diff, which turns one review into four. This directory splits it into five
series that apply independently, so each can be reviewed on its own merits.

All five were rebased onto upstream `develop`
(`c835b05aecfacb7343f7c424abd620aa12116c3f`, "Merge pull request #979"),
cherry-picked cleanly with zero conflicts, and **each compiles and links on
its own**. Original authorship (Philipp Lutz) is preserved in every patch.

## The five series

| Dir | Content | Files | Commits | Depends on |
|---|---|---|---|---|
| `01_ljpeg_predictor_modes` | Predictor modes 2 to 7, the inverted tile reshape, and their follow-up cleanups | `LJpegDecoder.cpp`, `LJpegDecompressor.{cpp,h}`, fuzz harness | 7 | none |
| `02_dng_unique_camera_model` | Honour the `UniqueCameraModel` Exif tag for DNG | `DngDecoder.cpp` | 1 | none |
| `03_dng_12bit_jpeg_errmsg` | Clearer error for 12-bit lossy-JPEG DNGs | `AbstractDngDecompressor.cpp`, `JpegDecompressor.cpp` | 1 | none |
| `04_analyzer_hardening` | Static-analyzer / sanitizer / fuzzer fixes with no LJpeg content | `SimpleTiffDecoder.h`, `AbstractDngDecompressor.cpp`, `FileReader.cpp` | 1 | none |
| `05_jpegxl_dng17` | JPEG XL decompressor (DNG 1.7, compression 52546) | 8 files, mostly new | 1 | none |

Series 02, 03 and 04 are each a single small commit and should be quick
reviews. Landing them first shrinks #963 to the part that actually needs
decoder expertise.

Series 05 is not part of #963 at all. It is separate local work, listed here
because it sits on the same submodule pin.

## The one non-obvious dependency

Upstream commit `cf87137` ("Address clang-tidy, sanitizer and fuzzer
findings") looks like a self-contained cleanup, but it is not: one of its
hunks adds

```c
if (predictorMode < 1 || predictorMode > 7)
  ThrowRDE("Unsupported predictor mode: %i", predictorMode);
```

to the `LJpegDecompressor` constructor. `LJpegDecompressor` is `final` and
does not inherit `AbstractLJpegDecoder`, so it has no `predictorMode` member
on `develop`. That member is introduced by the predictor-mode work itself.

The trap: `cf87137` cherry-picks onto `develop` **without conflict**, because
the surrounding context lines all exist. It then fails to compile:

```
LJpegDecompressor.cpp:118:28: error: 'predictorMode' was not declared in this scope
```

So a naive commit-by-commit split of #963 produces a series that looks clean
and does not build. This split therefore folds that single hunk into
`01_ljpeg_predictor_modes` (as `0007-LJpeg-validate-predictor-mode-range`)
and keeps the other three hunks, which touch no LJpeg code, in
`04_analyzer_hardening`.

## Applying a series

```
git clone https://github.com/darktable-org/rawspeed.git
cd rawspeed
git checkout -b ljpeg_predictor_modes c835b05a
git am /path/to/01_ljpeg_predictor_modes/*.patch
```

Patch filenames are left as `git format-patch` generated them: `git am`
relies on the numeric prefix for ordering.

## How this was verified

Configured with CMake + Ninja, `Release`, testing/benchmarking/tools/fuzzers
off, and built to completion:

| Series | Result |
|---|---|
| 01 | builds, links, 0 errors |
| 02 | builds, 0 errors |
| 03 | builds, 0 errors |
| 04 | builds, 0 errors |
| 05 | builds, 0 errors, with `-DWITH_JPEGXL=ON` |

Series 05 was built against libjxl **0.7.0**, so it does not require the
0.11.2 that the submodule bump notes mention.

Not covered: no raw sample corpus was decoded here, so this verifies
compilation and separability, not decode correctness. Decode correctness for
predictor modes 1 to 7 and for ProRAW was checked separately on real files
(iPhone 12 and 15 Pro Max) and reported in the #963 thread.
