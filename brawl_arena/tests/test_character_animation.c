#include "character_animation.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

enum {
    CLIP_IDLE = 10,
    CLIP_COMBAT,
    CLIP_WALK,
    CLIP_RUN_F,
    CLIP_RUN_B,
    CLIP_RUN_FL,
    CLIP_RUN_FR,
    CLIP_RUN_BL,
    CLIP_RUN_BR,
    CLIP_DEATH
};

static RiggedCharacter TestCharacter(void)
{
    return (RiggedCharacter){
        .clipIdle = CLIP_IDLE,
        .clipCombat = CLIP_COMBAT,
        .clipWalk = CLIP_WALK,
        .clipRunF = CLIP_RUN_F,
        .clipRunB = CLIP_RUN_B,
        .clipRunFL = CLIP_RUN_FL,
        .clipRunFR = CLIP_RUN_FR,
        .clipRunBL = CLIP_RUN_BL,
        .clipRunBR = CLIP_RUN_BR,
        .clipDeath = CLIP_DEATH
    };
}

int main(void)
{
    RiggedCharacter character = TestCharacter();
    Brawler brawler = {
        .alive = true,
        .renderYaw = 0.0f
    };

    CharacterAnimationSelection selected =
        CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_IDLE && selected.loop,
          "a stationary living brawler did not select idle");

    // Regression: firing sets revealTimer for concealment. It must not trigger the
    // generic looping combat stance or make its duration follow bush-reveal tuning.
    brawler.revealTimer = 20.0f;
    brawler.aimHold = 4.0f;
    brawler.deliberateAim = true;
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_IDLE,
          "stationary attack/reveal state selected the combat stance");

    brawler.velocity = (Vector3){ 0.0f, 0.0f, 3.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_WALK,
          "low-speed forward movement did not select walk");

    brawler.velocity = (Vector3){ 0.0f, 0.0f, 11.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_RUN_F && fabsf(selected.playbackRate - 1.0f) < 0.001f,
          "full-speed forward movement did not select a one-times run");

    brawler.velocity = (Vector3){ 8.0f, 0.0f, 8.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_RUN_FL,
          "forward-left movement did not select its directional clip");

    brawler.velocity = (Vector3){ 0.0f, 0.0f, -11.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_RUN_B && selected.playbackRate > 1.0f,
          "backpedaling did not select the accelerated backward clip");

    brawler.dashTimer = 0.2f;
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_RUN_F &&
          fabsf(selected.playbackRate - 1.35f) < 0.001f,
          "dash did not select its forward-run fallback");

    brawler.dashTimer = 0.0f;
    brawler.alive = false;
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_DEATH && !selected.loop,
          "downed brawler did not select the non-looping death clip");
    brawler.alive = true;

    // Walk/run hysteresis: a speed just inside the band keeps the clip that is
    // already playing instead of flipping - and restarting - every frame. The
    // stateless threshold is 0.60*moveSpeed = 6.6.
    CharacterAnimState runState = {
        .clip = CLIP_RUN_F, .clipAge = 1.0f, .valid = true
    };
    brawler.velocity = (Vector3){ 0.0f, 0.0f, 6.2f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, &runState);
    CHECK(selected.clip == CLIP_RUN_F,
          "speed inside the hysteresis band did not keep the running clip");
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, NULL);
    CHECK(selected.clip == CLIP_WALK,
          "sub-threshold speed with no history did not select walk");
    CharacterAnimState walkState = {
        .clip = CLIP_WALK, .clipAge = 1.0f, .valid = true
    };
    brawler.velocity = (Vector3){ 0.0f, 0.0f, 7.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, &walkState);
    CHECK(selected.clip == CLIP_WALK,
          "speed inside the hysteresis band did not keep the walking clip");

    // Minimum dwell: grazing a direction-sector edge must not restart the cycle
    // every frame, but an established clip still hands over immediately.
    CharacterAnimState freshTurn = {
        .clip = CLIP_RUN_FL, .clipAge = 0.05f, .valid = true
    };
    brawler.velocity = (Vector3){ 0.0f, 0.0f, 11.0f };
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, &freshTurn);
    CHECK(selected.clip == CLIP_RUN_FL,
          "a clip only 0.05s old was replaced across a sector edge");
    freshTurn.clipAge = 0.5f;
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, &freshTurn);
    CHECK(selected.clip == CLIP_RUN_F,
          "an established clip did not hand over after the dwell time");

    // Death must never be delayed by locomotion dwell.
    freshTurn.clipAge = 0.01f;
    brawler.alive = false;
    selected = CharacterAnimationSelect(&character, &brawler, 11.0f, &freshTurn);
    CHECK(selected.clip == CLIP_DEATH,
          "the death clip was delayed by locomotion dwell");
    brawler.alive = true;

    PresentationState presentation = { 0 };
    CharacterActionStart(&presentation, 2, CHARACTER_ACTION_MAIN);
    CHECK(presentation.actions[2].active &&
          presentation.actions[2].action == CHARACTER_ACTION_MAIN,
          "starting a one-shot did not populate presentation action state");
    CHECK(CharacterActionDuration(CHARACTER_ACTION_SUPER) >
          CharacterActionDuration(CHARACTER_ACTION_MAIN),
          "super action is not authored as the longer readable gesture");
    CharacterActionsUpdate(&presentation, 0.10f);
    CHECK(CharacterActionProgress(&presentation.actions[2]) > 0.0f &&
          CharacterActionBlendWeight(&presentation.actions[2]) > 0.0f,
          "one-shot did not advance into its blended pose");
    CharacterActionsUpdate(&presentation, 1.0f);
    CHECK(!presentation.actions[2].active,
          "one-shot action did not return to locomotion after its duration");

    // The shared per-brawler update: state becomes valid on the first tick, time
    // advances on the gameplay clock, and only a real clip change restarts it.
    static Assets assets;
    assets.characters[0] = character;
    assets.characters[0].ok = true;
    Brawler roster[1] = { 0 };
    roster[0].alive = true;

    PresentationState animState = { 0 };
    CharacterAnimationsUpdate(&animState, &assets, roster, 1, 11.0f, 0.016f);
    CHECK(animState.anim[0].valid && animState.anim[0].clip == CLIP_IDLE,
          "first update did not establish the idle clip");
    float previousTime = animState.anim[0].time;
    CharacterAnimationsUpdate(&animState, &assets, roster, 1, 11.0f, 0.016f);
    CHECK(animState.anim[0].time > previousTime,
          "an unchanged clip did not keep advancing");

    roster[0].velocity = (Vector3){ 0.0f, 0.0f, 11.0f };
    CharacterAnimationsUpdate(&animState, &assets, roster, 1, 11.0f, 0.016f);
    CHECK(animState.anim[0].clip == CLIP_RUN_F &&
          animState.anim[0].time <= 0.017f,
          "a clip change did not restart the cycle");

    printf("Character animation selection and action blending passed\n");
    return 0;
}
