#ifndef BRAWL_APP_TYPES_H
#define BRAWL_APP_TYPES_H

#include "content_types.h"
#include "game_types.h"
#include "presentation_types.h"

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_BRAWLERS,
    SCREEN_MATCH,
    SCREEN_STUDIO,
    SCREEN_COUNT
} AppScreen;

typedef struct PlayerController {
    Vector3 aimPoint;
    float aimDist;
    bool charging;
    float chargeTime;
    bool aimingSuper;
    // Longshot's grapple is a deliberate hold-to-preview, release-to-launch
    // secondary. Other secondaries remain press or hold activated.
    bool aimingSecondary;
    // Edge-detection memory for gamepad triggers; owned here so a match reset also
    // resets it instead of leaking state through file-scope statics.
    bool gamepadAttackHeld;
    bool gamepadSecondaryHeld;
    // A release that lands during the tail of a cooldown is remembered briefly and
    // retried, so near-edge clicks fire instead of being eaten.
    float attackBufferTimer;
    bool attackBufferTap;
} PlayerController;

typedef struct MobileStickState {
    int touchId;
    bool active;
    bool pressed;
    bool released;
    Vector2 origin;
    Vector2 position;
    Vector2 value;
} MobileStickState;

// Process-lifetime touch identities belong to the application input layer. Keeping
// them outside deterministic GameSession state lets the same simulation consume
// touch, gamepad, keyboard, or replayed PlayerInput without platform dependencies.
typedef struct MobileControlsState {
    MobileStickState move;
    MobileStickState attack;
    MobileStickState superAbility;
    MobileStickState secondary;
    bool pausePressed;
    float idleAge;
} MobileControlsState;

typedef struct AppFlow {
    AppScreen screen;
    AppScreen pending;
    float fade;
    bool fadingOut;
    bool matchResultBanked;
    bool quitRequested;
} AppFlow;

// Personal presentation preferences live beside the application shell rather than in
// deterministic match state or project gameplay tuning. Config persists these fields
// only to the ignored profile file.
typedef struct UiPreferences {
    float scale;
    bool reducedMotion;
    bool highContrast;
    bool showTutorialHints;
    int inputGlyphMode;
    int tutorialFlags;
} UiPreferences;

typedef struct App {
    GameSession session;
    PlayerController controller;
    PresentationState presentation;
    AppFlow flow;
    Tuning tune;
    ContentCatalog content;
    ConfigState config;
    UiPreferences uiPreferences;
    MobileControlsState mobileControls;
    bool matchRestartPending;
} App;

static inline GameContext AppGameContext(App *app)
{
    return (GameContext){
        .session = &app->session,
        .tuning = &app->tune,
        .content = &app->content
    };
}

#endif
