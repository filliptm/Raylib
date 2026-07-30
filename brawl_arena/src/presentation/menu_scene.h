#ifndef BRAWL_MENU_SCENE_H
#define BRAWL_MENU_SCENE_H

#include "app_types.h"
#include "assets.h"

typedef struct MenuScene {
    Assets *assets;
    Camera3D camera;
    Brawler preview;
    BrawlerClass previewKit;
    float stageTime;
    float previewTime;
    RenderTexture2D stickerTarget;
    Shader stickerShader;
    int stickerWidth;
    int stickerHeight;
    int stickerResolutionLoc;
    int stickerInnerLoc;
    int stickerOuterLoc;
    int stickerInkLoc;
    int stickerPaperLoc;
    bool stickerReady;
} MenuScene;

void MenuSceneInit(MenuScene *scene, Assets *assets);
void MenuSceneUnload(MenuScene *scene);
void MenuSceneUpdate(MenuScene *scene, const App *app, BrawlerClass candidate,
                     AppScreen screen, float dt);
void MenuSceneDrawStage(MenuScene *scene, const App *app, BrawlerClass candidate,
                        AppScreen screen);
void MenuSceneRenderBrawler(MenuScene *scene, const App *app, BrawlerClass candidate,
                            AppScreen screen, int width, int height);
void MenuSceneCompositeBrawler(MenuScene *scene, const App *app,
                               BrawlerClass candidate, AppScreen screen,
                               float entranceScale);

#endif
