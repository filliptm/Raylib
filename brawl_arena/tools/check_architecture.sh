#!/bin/sh
set -eu

fail_if_found()
{
    pattern=$1
    paths=$2
    message=$3

    if matches=$(rg -n "$pattern" $paths); then
        printf '%s\n' "Architecture check failed: $message"
        printf '%s\n' "$matches"
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
if matches=$(rg -n '\brl(Enable|Disable)DepthMask\(' src/presentation \
        -g '*.[ch]' -g '!render_state.h'); then
    printf '%s\n' \
        'Architecture check failed: use RenderBeginNoDepthWrite/RenderEndNoDepthWrite.'
    printf '%s\n' "$matches"
    exit 1
fi

printf '%s\n' 'architecture and render-state checks passed'
