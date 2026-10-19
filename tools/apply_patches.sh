#!/usr/bin/env bash
# Fetch the framework and launcher at their pinned commits and apply this
# port's patches on top (framework_patches/, recomp_ui_patches/).
#
#   bash tools/apply_patches.sh
#
# Safe to run again: each submodule is reset to its pinned commit first.
set -euo pipefail
cd "$(dirname "$0")/.."

git submodule update --init --recursive

apply() {
  local dir="$1" patches="$2"
  local pin
  pin=$(git ls-tree HEAD "$dir" | awk '{print $3}')
  git -C "$dir" checkout -q --detach "$pin"
  git -C "$dir" submodule update --init --recursive -q
  echo "== $dir @ ${pin:0:10}: applying $(ls "$patches"/*.patch | wc -l) patch(es)"
  git -C "$dir" -c user.name="HarvestMoonSNESRecomp" -c user.email="build@localhost" \
      am -q --3way "$PWD/$patches"/*.patch
}

apply snesrecomp framework_patches
apply recomp-ui recomp_ui_patches
echo "Done. Next: bash tools/regen.sh --rom /path/to/your/rom.sfc"
