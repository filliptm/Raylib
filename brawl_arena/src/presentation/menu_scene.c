#include "menu_scene.h"

#include "content_catalog.h"
#include "render.h"
#include "raymath.h"
#include <math.h>

static const Color KIT_ACCENT[CLASS_COUNT] = {
    { 74, 142, 236, 255 },
    { 104, 200, 255, 255 },
    { 172, 118, 250, 255 },
    { 250, 146, 84, 255 },
    { 70, 244, 166, 255 }
};

static void RebuildPreview(MenuScene *scene, const App *app, BrawlerClass kit)
{
    scene->preview = (Brawler){ 0 };
    scene->preview.team = TEAM_PLAYER;
    scene->preview.cls = kit;
    scene->preview.isPlayer = true;
    scene->preview.alive = true;
    scene->preview.visible = true;
    scene->preview.spawnScale = 1.0f;
    scene->preview.maxHealth = ContentCharacter(&app->content, kit)->maxHealth;
    scene->preview.health = scene->preview.maxHealth;
    scene->previewKit = kit;
}

void MenuSceneInit(MenuScene *scene, Assets *assets)
{
    *scene = (MenuScene){ 0 };
    scene->assets = assets;
    scene->previewKit = CLASS_COUNT;
    scene->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    scene->camera.fovy = 40.0f;
    scene->camera.projection = CAMERA_PERSPECTIVE;
}

void MenuSceneUpdate(MenuScene *scene, const App *app, BrawlerClass candidate,
                     AppScreen screen, float dt)
{
    if (candidate < 0 || candidate >= CLASS_COUNT) candidate = CLASS_SHOTGUNNER;
    if (scene->previewKit != candidate) RebuildPreview(scene, app, candidate);
    scene->time += dt;
    scene->preview.bobPhase += dt*3.4f;

    const CharacterPresentationDefinition *profile =
        ContentCharacterPresentation(&app->content, candidate);
    bool select = screen == SCREEN_BRAWLERS;
    Vector2 offset = select ? profile->selectOffset : profile->homeOffset;
    float yaw = (select ? profile->selectYawDegrees : profile->homeYawDegrees)*DEG2RAD;
    scene->preview.position = (Vector3){ offset.x, offset.y, 0.0f };
    scene->preview.renderYaw = yaw;
    scene->preview.aimAngle = yaw;
    scene->camera.position = (Vector3){ 0.0f, 2.7f, -profile->cameraDistance };
    scene->camera.target = (Vector3){ 0.0f, profile->cameraTargetY, 0.0f };
}

static void DrawHangar(MenuScene *scene)
{
    Assets *a = scene->assets;

    Matrix floor = MatrixMultiply(MatrixScale(13.5f, 0.12f, 8.0f),
                                  MatrixTranslate(0.0f, -0.36f, 1.8f));
    DrawLit(a, a->cube, floor, a->texMetal, (Color){ 30, 48, 66, 255 },
            (Vector2){ 8, 5 }, 0.0f);

    Matrix rear = MatrixMultiply(MatrixScale(12.0f, 5.2f, 0.20f),
                                 MatrixTranslate(0.0f, 2.15f, 3.5f));
    DrawLit(a, a->cube, rear, a->texWall, (Color){ 28, 42, 60, 255 },
            (Vector2){ 7, 3 }, 0.0f);

    for (int side = -1; side <= 1; side += 2)
    {
        Matrix pillar = MatrixMultiply(MatrixScale(0.35f, 5.0f, 0.45f),
                                       MatrixTranslate(side*4.8f, 2.05f, 2.9f));
        DrawLit(a, a->cube, pillar, a->texMetal, (Color){ 48, 68, 88, 255 },
                (Vector2){ 1, 4 }, 0.0f);
        Matrix rail = MatrixMultiply(MatrixScale(0.10f, 0.10f, 5.4f),
                                     MatrixTranslate(side*3.9f, 1.05f, 0.7f));
        DrawLit(a, a->cube, rail, a->texGlow, (Color){ 100, 185, 255, 150 },
                (Vector2){ 1, 1 }, 0.65f);
    }

    for (int i = -4; i <= 4; i++)
    {
        Matrix stripe = MatrixMultiply(MatrixScale(0.08f, 0.015f, 3.2f),
                                       MatrixTranslate(i*1.15f, -0.27f, -0.3f));
        DrawLit(a, a->cube, stripe, a->texFlat,
                (i & 1) ? (Color){ 255, 157, 66, 110 } :
                          (Color){ 42, 68, 92, 100 },
                (Vector2){ 1, 1 }, 0.2f);
    }
}

void MenuSceneDraw(MenuScene *scene, const App *app, BrawlerClass candidate,
                   AppScreen screen)
{
    Assets *a = scene->assets;
    if (!a || candidate < 0 || candidate >= CLASS_COUNT) return;
    const CharacterPresentationDefinition *profile =
        ContentCharacterPresentation(&app->content, candidate);
    bool select = screen == SCREEN_BRAWLERS;
    float scale = select ? profile->selectScale : profile->homeScale;
    float yaw = (select ? profile->selectYawDegrees : profile->homeYawDegrees)*DEG2RAD;

    Vector3 lightPos[2] = { { -2.6f, 3.0f, -2.4f }, { 2.8f, 2.4f, 1.6f } };
    Vector3 lightCol[2] = { { 0.85f, 0.72f, 0.45f }, { 0.32f, 0.48f, 0.80f } };
    AssetsSetCamera(a, scene->camera.position);
    AssetsSetToon(a, app->tune.toon, app->tune.toonBands);
    AssetsSetLights(a, lightPos, lightCol, 2);

    BeginMode3D(scene->camera);
        DrawHangar(scene);

        float px = scene->preview.position.x;
        Matrix base = MatrixMultiply(MatrixScale(1.50f, 0.22f, 1.50f),
                                     MatrixTranslate(px, -0.22f, 0));
        DrawLit(a, a->cylinder, base, a->texMetal, (Color){ 66, 82, 106, 255 },
                (Vector2){ 1, 1 }, 0.0f);
        Matrix lip = MatrixMultiply(MatrixScale(1.62f, 0.07f, 1.62f),
                                    MatrixTranslate(px, -0.07f, 0));
        DrawLit(a, a->cylinder, lip, a->texMetal, (Color){ 111, 139, 164, 255 },
                (Vector2){ 1, 1 }, 0.0f);
        Matrix glow = MatrixMultiply(MatrixScale(1.34f, 0.02f, 1.34f),
                                     MatrixTranslate(px, 0.02f, 0));
        Color teamGlow = TEAM_COLORS[TEAM_PLAYER];
        teamGlow.a = (unsigned char)(75 + (0.5f + 0.5f*sinf(scene->time*1.6f))*45);
        DrawLit(a, a->cylinder, glow, a->texGlow, teamGlow, (Vector2){ 1, 1 }, 1.0f);

        RiggedCharacter *character = &a->characters[candidate];
        if (character->ok && app->tune.modelCharacter)
        {
            AssetsDrawCharacter(a, candidate, scene->preview.position, yaw, scale,
                                character->clipIdle, scene->time*60.0f, true, WHITE,
                                0.0f, 0.0f, lightPos, lightCol, 2, scene->camera.position);
        }
        else
        {
            Brawler fallback = scene->preview;
            fallback.renderYaw = yaw;
            fallback.aimAngle = yaw;
            fallback.spawnScale = scale;
            Color accent = KIT_ACCENT[candidate];
            RenderBrawlerModel(a, &fallback, scene->time, 0.0f, &accent);
        }
    EndMode3D();
}
