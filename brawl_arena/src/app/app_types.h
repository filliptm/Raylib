#ifndef BRAWL_APP_TYPES_H
#define BRAWL_APP_TYPES_H

#include "content_types.h"
#include "game_types.h"
#include "presentation_types.h"

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_BRAWLERS,
    SCREEN_MATCH,
    SCREEN_COUNT
} AppScreen;

typedef struct PlayerController {
    Vector3 aimPoint;
    float aimDist;
    bool charging;
    float chargeTime;
    bool aimingSuper;
} PlayerController;

typedef struct AppFlow {
    AppScreen screen;
    AppScreen pending;
    float fade;
    bool fadingOut;
    bool matchResultBanked;
    bool quitRequested;
} AppFlow;

typedef struct App {
    GameSession session;
    PlayerController controller;
    PresentationState presentation;
    AppFlow flow;
    Tuning tune;
    ContentCatalog content;
    ConfigState config;
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
