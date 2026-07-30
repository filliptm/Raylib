#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
brawl_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
build_root="$brawl_root/apple/build"

"$script_dir/bootstrap_ios.sh"
make -C "$brawl_root" character-assets vfx-assets

cmake -S "$brawl_root/apple" -B "$build_root" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.6 \
    ${BRAWL_DEVELOPMENT_TEAM:+-DBRAWL_DEVELOPMENT_TEAM="$BRAWL_DEVELOPMENT_TEAM"}

printf 'Generated %s/BrawlArenaIOS.xcodeproj\n' "$build_root"
