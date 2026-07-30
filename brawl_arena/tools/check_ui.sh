#!/bin/sh
set -eu

if matches=$(rg -n '(^|[^A-Za-z])DrawText(Ex)?\(' src/ui/menu.c src/ui/hud.c); then
    printf '%s\n' 'UI policy failed: migrated player screens bypass UiDrawText.'
    printf '%s\n' "$matches"
    exit 1
fi

if matches=$(rg -n '(^|[^A-Za-z])MeasureText(Ex)?\(' src/ui/menu.c src/ui/hud.c); then
    printf '%s\n' 'UI policy failed: migrated player screens bypass UiMeasureText.'
    printf '%s\n' "$matches"
    exit 1
fi

if matches=$(rg -n 'LoadFont(Ex)?\(' src --glob '!ui_system.c'); then
    printf '%s\n' 'UI policy failed: font lifetime escaped ui_system.c.'
    printf '%s\n' "$matches"
    exit 1
fi

if matches=$(rg -n 'LoadTexture(Ex)?\(' src/ui --glob '!ui_skin.c'); then
    printf '%s\n' 'UI policy failed: texture lifetime escaped ui_skin.c.'
    printf '%s\n' "$matches"
    exit 1
fi

if matches=$(rg -n 'DrawTexture(Pro|Rec|NPatch)?\(' src/ui/menu.c src/ui/hud.c); then
    printf '%s\n' 'UI policy failed: player screens bypass the shared UI skin.'
    printf '%s\n' "$matches"
    exit 1
fi

for required in \
    resources/fonts/BarlowCondensed-Bold.ttf \
    resources/fonts/Barlow-Regular.ttf \
    resources/fonts/Barlow-SemiBold.ttf \
    resources/fonts/IBMPlexMono-Medium.ttf \
    resources/fonts/OFL-Barlow.txt \
    resources/fonts/OFL-IBMPlexMono.txt \
    resources/fonts/SOURCE.md
do
    if [ ! -f "$required" ]; then
        printf '%s\n' "UI policy failed: missing $required"
        exit 1
    fi
done

python3 tools/check_ui_assets.py

printf '%s\n' 'Arena Ink UI policy checks passed'
