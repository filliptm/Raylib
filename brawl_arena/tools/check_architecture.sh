#!/bin/sh
set -eu

# Exit code 1 from rg means "no matches" (a pass); anything else — including a
# missing binary — must fail the build instead of silently skipping every check.
command -v rg >/dev/null 2>&1 || {
    printf '%s\n' 'Architecture check failed: ripgrep (rg) is required but not installed.'
    exit 1
}

fail_if_found()
{
    pattern=$1
    paths=$2
    message=$3

    status=0
    matches=$(rg -n "$pattern" $paths) || status=$?
    if [ "$status" -eq 0 ]; then
        printf '%s\n' "Architecture check failed: $message"
        printf '%s\n' "$matches"
        exit 1
    elif [ "$status" -ne 1 ]; then
        printf '%s\n' "Architecture check failed: rg exited with status $status."
        exit 1
    fi
}

# Simulation may depend on core/content contracts, but never on the application
# aggregate or anything that renders, reads devices, or owns UI state.
fail_if_found \
    '^#include "(app_types|presentation_types|assets|render|effects|camera|environment|ability_visuals|hud|menu|command_center|command_widgets|player|game_commands)\.h"' \
    'src/game src/core' \
    'core/game code imported an outer-layer header.'

fail_if_found \
    '\bApp[[:space:]]*\*' \
    'src/game src/core' \
    'core/game code accepts the whole App instead of GameContext.'

fail_if_found \
    '\b(IsKey|IsMouse|GetMouse|GetScreenToWorldRay|GetRandomValue)\w*\(' \
    'src/game src/core' \
    'simulation reads platform input or nondeterministic raylib random state.'

fail_if_found \
    '\b(FxSpawn|FxConsume|Draw[A-Z]|BeginMode|EndMode|CameraUpdate)\w*\(' \
    'src/game src/core' \
    'simulation directly performs presentation work instead of emitting an event.'

# rlgl batches billboards and immediate geometry. Direct depth-mask changes do not flush
# that batch, so transparent quads can accidentally write rectangular depth silhouettes
# for the post-process outline. render_state.h is the single safe owner of these calls.
status=0
matches=$(rg -n '\brl(Enable|Disable)DepthMask\(' src/presentation \
        -g '*.[ch]' -g '!render_state.h') || status=$?
if [ "$status" -eq 0 ]; then
    printf '%s\n' \
        'Architecture check failed: use RenderBeginNoDepthWrite/RenderEndNoDepthWrite.'
    printf '%s\n' "$matches"
    exit 1
elif [ "$status" -ne 1 ]; then
    printf '%s\n' "Architecture check failed: rg exited with status $status."
    exit 1
fi

printf '%s\n' 'architecture and render-state checks passed'
