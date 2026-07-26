/*******************************************************************************************
*   BRAWL ARENA
*
*   A top-down 3D arena brawler in the Brawl Stars mould: aim-and-release shooting,
*   an ammo/reload economy, a super charged by landing hits, and bushes that hide you.
*
*   Bots stand still by default so you can dial the feel in. Press TAB for the command
*   center; anything you change there is saved to tuning.cfg and reloaded next launch.
********************************************************************************************/

#include "raylib.h"
#include "types.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "ai.h"
#include "player.h"
#include "render.h"
#include "effects.h"
#include "hud.h"
#include "command_center.h"
#include "assets.h"
#include "config.h"
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800

static World world;
static Assets assets;

static const Color SKY_COLOR = { 22, 26, 38, 255 };

// Rebuilds the match while carrying the current tuning across, so a reset never
// throws away what you have dialled in.
static void ResetMatch(World *w, BrawlerClass playerClass)
{
    Tuning keepTuning = w->tune;

    memset(w, 0, sizeof(World));
    w->tune = keepTuning;

    ArenaLoad(&w->arena);

    w->playerIdx = 0;
    BrawlerSpawn(w, 0, TEAM_PLAYER, playerClass, w->arena.playerSpawn, true);
    w->brawlerCount = 1;

    int bots = w->tune.botCount;
    if (bots > MAX_BRAWLERS - 1) bots = MAX_BRAWLERS - 1;
    if (bots < 0) bots = 0;

    for (int i = 0; i < bots; i++)
    {
        BrawlerClass kit = w->tune.botMixedKits ? (BrawlerClass)(i % CLASS_COUNT) : w->tune.botKit;
        Vector3 pos = w->arena.enemySpawns[i % w->arena.enemySpawnCount];
        BrawlerSpawn(w, 1 + i, TEAM_ENEMY, kit, pos, false);
        w->brawlerCount++;
    }

    CameraInit(w);
}

static void DrawOverlays(World *w)
{
    HudDrawBars(w);
    FxDrawScreen(w);
    HudDrawPanel(w);
    CommandCenterDraw(w);
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Brawl Arena");
    SetTargetFPS(60);

    // Weapon table and tuning must exist before anything spawns: spawning reads maxHealth.
    WeaponsResetAll();
    TuningSetDefaults(&world.tune);
    ConfigLoad(&world);

    AssetsLoad(&assets, SCREEN_WIDTH, SCREEN_HEIGHT);
    RenderSetAssets(&assets);

    ResetMatch(&world, CLASS_SHOTGUNNER);

    while (!WindowShouldClose())
    {
        float realDt = GetFrameTime();
        if (realDt > 0.05f) realDt = 0.05f;   // survive a hitch without teleporting anyone

        float dt = realDt*world.tune.timeScale;

        CommandCenterUpdate(&world);

        if (IsKeyPressed(KEY_R))
            ResetMatch(&world, world.brawlers[world.playerIdx].cls);

        world.time += dt;

        PlayerUpdate(&world, dt);
        AIUpdate(&world, dt);
        BrawlersUpdate(&world, dt);
        ProjectilesUpdate(&world, dt);
        ArenaUpdate(&world.arena, dt);
        FxUpdate(&world, dt);
        CameraUpdate(&world, dt);

        // Saving runs on real time so slow-mo does not stall it.
        ConfigAutoSave(&world, realDt);

        bool usePost = world.tune.postFx && assets.postOk;

        if (usePost)
        {
            BeginTextureMode(assets.sceneTarget);
                ClearBackground(SKY_COLOR);
                RenderWorld(&world);
            EndTextureMode();

            float bloom = world.tune.bloom;
            float vignette = 0.85f;
            SetShaderValue(assets.post, assets.locBloom, &bloom, SHADER_UNIFORM_FLOAT);
            SetShaderValue(assets.post, assets.locVignette, &vignette, SHADER_UNIFORM_FLOAT);

            BeginDrawing();
                ClearBackground(BLACK);

                BeginShaderMode(assets.post);
                    // Render textures come out vertically flipped, hence the negative height.
                    DrawTextureRec(assets.sceneTarget.texture,
                                   (Rectangle){ 0, 0, (float)assets.sceneTarget.texture.width,
                                                -(float)assets.sceneTarget.texture.height },
                                   (Vector2){ 0, 0 }, WHITE);
                EndShaderMode();

                DrawOverlays(&world);
            EndDrawing();
        }
        else
        {
            BeginDrawing();
                ClearBackground(SKY_COLOR);
                RenderWorld(&world);
                DrawOverlays(&world);
            EndDrawing();
        }
    }

    ConfigSave(&world);
    AssetsUnload(&assets);
    CloseWindow();
    return 0;
}
