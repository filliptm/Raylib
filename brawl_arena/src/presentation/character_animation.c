#include "character_animation.h"
#include "raymath.h"

#include <math.h>
#include <stddef.h>

static float ClampPlayback(float value)
{
    if (value < 0.6f) return 0.6f;
    if (value > 1.3f) return 1.3f;
    return value;
}

// A clip change restarts its cycle (there is no crossfade), so switches between
// looping locomotion clips are debounced: a clip must have played this long before
// a boundary-grazing heading or speed may replace it.
#define LOCOMOTION_MIN_DWELL 0.15f

static bool IsLocomotionClip(const RiggedCharacter *character, int clip)
{
    return clip >= 0 &&
           (clip == character->clipIdle || clip == character->clipWalk ||
            clip == character->clipRunF || clip == character->clipRunB ||
            clip == character->clipRunFL || clip == character->clipRunFR ||
            clip == character->clipRunBL || clip == character->clipRunBR);
}

CharacterAnimationSelection CharacterAnimationSelect(
    const RiggedCharacter *character,
    const Brawler *brawler,
    float moveSpeed,
    const CharacterAnimState *state
)
{
    CharacterAnimationSelection selection = {
        .clip = -1,
        .playbackRate = 1.0f,
        .loop = true
    };
    if (!character || !brawler) return selection;

    int previous = (state && state->valid) ? state->clip : -1;

    if (!brawler->alive)
    {
        selection.clip = character->clipDeath;
        selection.loop = false;
        return selection;
    }

    if (brawler->dashTimer > 0.0f)
    {
        selection.clip = character->clipRunF;
        selection.playbackRate = 1.35f;
        return selection;
    }

    float speed = sqrtf(brawler->velocity.x*brawler->velocity.x +
                        brawler->velocity.z*brawler->velocity.z);

    // Idle hysteresis: leaving idle demands more speed than returning to it, so a
    // brawler oscillating around a near-stationary speed does not flicker clips.
    float idleThreshold = (previous == character->clipIdle) ? 0.75f : 0.45f;
    if (speed <= idleThreshold)
    {
        // The canonical library does not contain a shoot one-shot. Keep stationary
        // actors in their authored idle instead of coupling a looping combat stance
        // to the unrelated bush-reveal timer.
        selection.clip = character->clipIdle;
        return selection;
    }

    float relative = atan2f(brawler->velocity.x, brawler->velocity.z) -
                     brawler->renderYaw;
    while (relative > PI) relative -= 2.0f*PI;
    while (relative < -PI) relative += 2.0f*PI;
    float degrees = relative*RAD2DEG;

    float nominalSpeed = moveSpeed > 0.001f ? moveSpeed : 1.0f;

    if (fabsf(degrees) <= 35.0f)
    {
        // The walk/run split tracks the tuned move speed rather than a fixed value:
        // a hard constant sat at the top speed of some tunings, flipping the clip -
        // and restarting it - on nearly every frame. The band around the threshold
        // sticks with whichever of the two clips is already playing.
        float runThreshold = 0.60f*nominalSpeed;
        if (previous == character->clipRunF) runThreshold *= 0.88f;
        else if (previous == character->clipWalk) runThreshold *= 1.12f;
        selection.clip = (speed <= runThreshold) ? character->clipWalk
                                                 : character->clipRunF;
    }
    else if (degrees > 35.0f && degrees <= 105.0f)
        selection.clip = character->clipRunFL;
    else if (degrees < -35.0f && degrees >= -105.0f)
        selection.clip = character->clipRunFR;
    else if (degrees > 105.0f && degrees < 155.0f)
        selection.clip = character->clipRunBL;
    else if (degrees < -105.0f && degrees > -155.0f)
        selection.clip = character->clipRunBR;
    else
        selection.clip = character->clipRunB;

    // Minimum dwell between looping locomotion clips, so circling across a sector
    // edge cannot restart the cycle every frame.
    if (state && state->valid && previous != selection.clip &&
        state->clipAge < LOCOMOTION_MIN_DWELL &&
        IsLocomotionClip(character, previous) &&
        IsLocomotionClip(character, selection.clip) &&
        previous != character->clipIdle)
        selection.clip = previous;

    if (selection.clip == character->clipRunB)
        selection.playbackRate = 1.30f;

    // Feet track ground speed. The walk clip is authored around a stroll, not the
    // class top speed, so it is normalised against a fraction of it.
    float reference = (selection.clip == character->clipWalk)
                    ? 0.55f*nominalSpeed : nominalSpeed;
    selection.playbackRate *= ClampPlayback(speed/reference);
    return selection;
}

void CharacterAnimationsUpdate(
    PresentationState *presentation,
    const Assets *assets,
    const Brawler *brawlers, int brawlerCount,
    float moveSpeed, float dt
)
{
    if (!presentation || !assets || !brawlers || dt < 0.0f) return;
    if (brawlerCount > MAX_BRAWLERS) brawlerCount = MAX_BRAWLERS;

    for (int i = 0; i < brawlerCount; i++)
    {
        const Brawler *b = &brawlers[i];
        CharacterAnimState *state = &presentation->anim[i];

        if (b->cls < 0 || b->cls >= CLASS_COUNT ||
            !assets->characters[b->cls].ok)
        {
            *state = (CharacterAnimState){ 0 };
            continue;
        }

        const RiggedCharacter *character = &assets->characters[b->cls];
        CharacterAnimationSelection selection =
            CharacterAnimationSelect(character, b, moveSpeed,
                                     state->valid ? state : NULL);

        if (!state->valid || selection.clip != state->clip ||
            state->cls != (int)b->cls)
        {
            state->clip = selection.clip;
            state->cls = (int)b->cls;
            state->time = 0.0f;
            state->clipAge = 0.0f;
            state->valid = true;
        }
        state->loop = selection.loop;
        state->clipAge += dt;
        state->time += dt*selection.playbackRate;
    }
}

float CharacterActionDuration(CharacterActionId action)
{
    switch (action)
    {
        case CHARACTER_ACTION_MAIN: return 0.42f;
        case CHARACTER_ACTION_SUPER: return 0.82f;
        case CHARACTER_ACTION_CAST: return 0.72f;
        case CHARACTER_ACTION_MOBILITY: return 0.58f;
        case CHARACTER_ACTION_GUARD: return 0.78f;
        default: return 0.0f;
    }
}

const char *CharacterActionName(CharacterActionId action)
{
    switch (action)
    {
        case CHARACTER_ACTION_MAIN: return "MAIN";
        case CHARACTER_ACTION_SUPER: return "SUPER";
        case CHARACTER_ACTION_CAST: return "CAST";
        case CHARACTER_ACTION_MOBILITY: return "MOBILITY";
        case CHARACTER_ACTION_GUARD: return "GUARD";
        default: return "NONE";
    }
}

void CharacterActionStart(PresentationState *presentation, int brawlerIndex,
                          CharacterActionId action)
{
    if (!presentation || brawlerIndex < 0 || brawlerIndex >= MAX_BRAWLERS)
        return;
    float duration = CharacterActionDuration(action);
    if (duration <= 0.0f) return;
    presentation->actions[brawlerIndex] = (CharacterActionState){
        .action = action,
        .duration = duration,
        .active = true
    };
}

void CharacterActionsUpdate(PresentationState *presentation, float dt)
{
    if (!presentation || dt < 0.0f) return;
    for (int i = 0; i < MAX_BRAWLERS; i++)
    {
        CharacterActionState *state = &presentation->actions[i];
        if (!state->active) continue;
        state->age += dt;
        if (state->age >= state->duration)
            *state = (CharacterActionState){ 0 };
    }
}

float CharacterActionProgress(const CharacterActionState *state)
{
    if (!state || !state->active || state->duration <= 0.0f) return 0.0f;
    return Clamp(state->age/state->duration, 0.0f, 1.0f);
}

float CharacterActionBlendWeight(const CharacterActionState *state)
{
    float progress = CharacterActionProgress(state);
    if (progress <= 0.0f || progress >= 1.0f) return 0.0f;

    // The pose lands quickly, holds long enough to read at the match camera, then
    // eases back into locomotion. This is also the crossfade envelope used when an
    // authored optional action clip is present.
    if (progress < 0.16f) return progress/0.16f;
    if (progress < 0.58f) return 1.0f;
    float out = 1.0f - (progress - 0.58f)/0.42f;
    return out*out*(3.0f - 2.0f*out);
}
