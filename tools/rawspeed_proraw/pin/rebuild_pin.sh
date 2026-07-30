#!/usr/bin/env bash
#
# Rebuild the rawspeed commit that src/external/rawspeed is pinned to, using
# only refs that darktable-org/rawspeed publishes.
#
# Why this exists: the pinned commit (see PINNED_COMMIT below) is not reachable
# from any branch of darktable-org/rawspeed, so a fresh clone of this fork
# cannot "git submodule update --init" it. This script reconstructs the same
# source tree from the public 'stable' branch plus the patch series next to it.
#
# The rebuilt commit's SHA will NOT equal PINNED_COMMIT -- committer identity
# and timestamps differ -- but its *tree* is byte-for-byte identical, which is
# what the build consumes. The script verifies that and refuses to finish if
# the tree does not match.
#
# Usage:
#   tools/rawspeed_proraw/pin/rebuild_pin.sh [<target-dir>]
#
# With no argument it operates on src/external/rawspeed relative to the repo
# root. Afterwards, point the submodule at the rebuilt commit it prints.

set -euo pipefail

UPSTREAM="https://github.com/darktable-org/rawspeed.git"
BASE_REF="refs/heads/stable"
BASE_COMMIT="4c511d611c1beee9aa97a2ec50b0838c6c7be52e"
PINNED_COMMIT="2c3dfc5779b604b647956ef2f4c292e943ab1d79"
EXPECTED_TREE="099b577e836a53d8e5de87149aa0ea13b20130a1"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "$script_dir" rev-parse --show-toplevel)"
target="${1:-$repo_root/src/external/rawspeed}"

shopt -s nullglob
patches=("$script_dir"/[0-9]*.patch)
shopt -u nullglob
if [ ${#patches[@]} -eq 0 ]; then
  echo "error: no patches found next to $0" >&2
  exit 1
fi
echo "Using ${#patches[@]} patches from $script_dir"

if [ ! -d "$target/.git" ]; then
  echo "Cloning $UPSTREAM into $target"
  mkdir -p "$target"
  git clone --quiet "$UPSTREAM" "$target"
fi

cd "$target"

echo "Fetching $BASE_REF from upstream"
git fetch --quiet "$UPSTREAM" "$BASE_REF"

if ! git cat-file -e "${BASE_COMMIT}^{commit}" 2>/dev/null; then
  echo "error: base commit $BASE_COMMIT not found after fetching $BASE_REF." >&2
  echo "       Upstream may have moved 'stable'; see ../README.md." >&2
  exit 1
fi

# Refuse to clobber uncommitted work.
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "error: $target has uncommitted changes; refusing to continue." >&2
  exit 1
fi

git am --abort >/dev/null 2>&1 || true
echo "Checking out base $BASE_COMMIT"
git checkout --quiet --detach "$BASE_COMMIT"

echo "Applying patch series"
if ! git am "${patches[@]}"; then
  echo "error: 'git am' failed. Resolve, then run 'git am --continue'." >&2
  exit 1
fi

rebuilt_commit="$(git rev-parse HEAD)"
rebuilt_tree="$(git rev-parse 'HEAD^{tree}')"

echo
if [ "$rebuilt_tree" != "$EXPECTED_TREE" ]; then
  echo "FAILED: rebuilt tree $rebuilt_tree != expected $EXPECTED_TREE" >&2
  exit 1
fi

echo "OK: rebuilt tree matches the pinned tree ($EXPECTED_TREE)"
echo "    rebuilt commit : $rebuilt_commit"
echo "    recorded pin   : $PINNED_COMMIT (content-identical, different SHA)"
echo
echo "The working tree now matches the pin and is ready to build."
echo "To make this reproducible for others, push the rebuilt commit to a fork"
echo "you control and repoint the submodule -- see ../README.md."
