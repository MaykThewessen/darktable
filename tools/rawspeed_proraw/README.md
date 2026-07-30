# rawspeed ProRAW / predictor-mode support

This directory holds two related things:

1. `pin/`: how to reproduce the exact rawspeed source tree that
   `src/external/rawspeed` is pinned to, using only refs that
   darktable-org/rawspeed publishes.
2. `upstream_split/`: the same work carved into five independent series that
   can each go upstream as a small pull request.

Nothing here is built as part of darktable. These are maintenance aids.

## Background

darktable's rawspeed submodule is pinned to a commit that carries:

- darktable-org/rawspeed#963 (LJpeg predictor modes 2 to 7), which unblocks
  Apple ProRAW DNGs and DJI / Blackmagic files that use predictor mode 6.
- A libjxl-backed JPEG XL decompressor (DNG 1.7, compression 52546) for
  iPhone 16 ProRAW. This is local work, not part of #963.

Upstream #963 is still open. The maintainer is focused on a Rust port and is
not currently merging features, so the pin is expected to live here a while.

## 1. Reproducing the pin (`pin/`)

### The problem

The pinned commit `2c3dfc5779b604b647956ef2f4c292e943ab1d79` is not reachable
from any branch of darktable-org/rawspeed. It was built locally. So a fresh
clone of this fork cannot do:

```
git submodule update --init src/external/rawspeed
```

The commit that `git submodule` wants simply is not published anywhere.

### The fix

`pin/rebuild_pin.sh` reconstructs the tree from public refs only:

- base: `refs/heads/stable` = `4c511d611c1beee9aa97a2ec50b0838c6c7be52e`
  (published by upstream)
- plus the 10 patches in `pin/`, applied with `git am`

```
tools/rawspeed_proraw/pin/rebuild_pin.sh
```

The rebuilt commit SHA will not equal the recorded pin, because committer
identity and timestamps differ. The *tree* is byte-for-byte identical, and
that is what the compiler sees. The script verifies this and fails loudly if
the tree hash does not come out as
`099b577e836a53d8e5de87149aa0ea13b20130a1`.

Verified: applying the series to public `stable` yields exactly the pinned
tree, and `git diff` against the pinned commit is empty.

### Making it reproducible for everyone

The script gets a *contributor* to a working tree. It does not make
`git submodule update --init` work for a stranger, because the pinned SHA is
still unpublished. To close that gap properly, push the rebuilt commit to a
fork you control and repoint the submodule:

```
# in src/external/rawspeed, after running rebuild_pin.sh
git push git@github.com:<you>/rawspeed.git HEAD:refs/heads/proraw_predictor7_on_v3.6
```

Then update `.gitmodules` to that fork's URL and record the pushed SHA with
`git add src/external/rawspeed`. After that a fresh clone initialises without
any of this.

Until then, note that the recorded pin is only resolvable by someone who
already has the objects, which is the gap this directory documents.

## 2. Upstream split (`upstream_split/`)

See `upstream_split/README.md`.
