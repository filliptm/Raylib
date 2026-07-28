#ifndef BRAWL_MENU_SCENE_H
#define BRAWL_MENU_SCENE_H

#include "app_types.h"
#include "assets.h"

typedef struct MenuScene {
    Assets *assets;
    Camera3D camera;
    Brawler preview;
    BrawlerClass previewKit;
    float time;
} MenuScene;

void MenuSceneInit(MenuScene *scene, Assets *assets);
void MenuSceneUpdate(MenuScene *scene, const App *app, BrawlerClass candidate,
                     AppScreen screen, float dt);
void MenuSceneDrawStage(MenuScene *scene, const App *app, BrawlerClass candidate,
                        AppScreen screen);
void MenuSceneDrawBrawler(MenuScene *scene, const App *app, BrawlerClass candidate,
                          AppScreen screen);

#endif
