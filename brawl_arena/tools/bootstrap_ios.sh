#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
brawl_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
vendor_root="$brawl_root/apple/ThirdParty"
raylib_root="$vendor_root/raylib-ios"
raylib_repository="https://github.com/ghera/raylib-iOS.git"
raylib_revision="0fa3228e90c7f2717b6ad7858b3438326d957059"
compatibility_patch="$brawl_root/apple/patches/raylib-ios-es3.patch"

mkdir -p "$vendor_root"

if [ ! -d "$raylib_root/.git" ]; then
    git clone --filter=blob:none --no-checkout "$raylib_repository" "$raylib_root"
fi

git -C "$raylib_root" fetch --depth 1 origin "$raylib_revision"
git -C "$raylib_root" checkout --detach "$raylib_revision"

if git -C "$raylib_root" apply --unidiff-zero --check "$compatibility_patch" 2>/dev/null; then
    git -C "$raylib_root" apply --unidiff-zero "$compatibility_patch"
elif ! git -C "$raylib_root" apply --unidiff-zero --reverse --check "$compatibility_patch" 2>/dev/null; then
    printf 'raylib-iOS compatibility patch does not apply cleanly\n' >&2
    exit 1
fi

printf 'raylib-iOS ready at %s (%s)\n' "$raylib_root" "$raylib_revision"
