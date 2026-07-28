#ifndef BRAWL_CHARACTER_ANIMATION_H
#define BRAWL_CHARACTER_ANIMATION_H

#include "assets.h"
#include "game_types.h"
#include "presentation_types.h"

// raylib samples glTF clips every 17 ms (GLTF_ANIMDELAY), i.e. ~58.82 frames per
// second. Advancing at a flat 60 played every clip ~2% fast.
#define CHARACTER_CLIP_FPS (1000.0f/17.0f)

// Seconds over which an outgoing locomotion clip blends beneath its replacement,
// so a clip change no longer snaps the skeleton to frame zero.
#define CHARACTER_CROSSFADE_DURATION 0.10f

typedef struct CharacterAnimationSelection {
    int clip;
    float playbackRate;
    bool loop;
} CharacterAnimationSelection;

// Selects a rigged-character clip from physical movement only. Concealment reveal,
// attack cooldowns, and other gameplay timers must not double as animation state.
// `state` is the brawler's current clip state (NULL for a stateless pick): it feeds
// the walk/run and idle hysteresis plus a minimum dwell time, so a speed hovering on
// a threshold or a heading grazing a sector edge cannot restart the clip every frame.
CharacterAnimationSelection CharacterAnimationSelect(
    const RiggedCharacter *character,
    const Brawler *brawler,
    float moveSpeed,
    const CharacterAnimState *state
);

// Advances every brawler's clip state on the clamped gameplay clock, drawn or not,
// so bush-concealed brawlers do not freeze mid-stride and animation cannot outrun
// movement during a frame hitch. Call once per match update, before rendering.
void CharacterAnimationsUpdate(
    PresentationState *presentation,
    const Assets *assets,
    const Brawler *brawlers, int brawlerCount,
    float moveSpeed, float dt
);

float CharacterActionDuration(CharacterActionId action);
const char *CharacterActionName(CharacterActionId action);
void CharacterActionStart(PresentationState *presentation, int brawlerIndex,
                          CharacterActionId action);
void CharacterActionsUpdate(PresentationState *presentation, float dt);
float CharacterActionProgress(const CharacterActionState *state);
float CharacterActionBlendWeight(const CharacterActionState *state);

#endif
