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
#include "gems.h"
#include "menu.h"
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
    // The wipe clears match state only. Anything owned by the application rather than
    // by the match has to survive it, or starting a game would throw away which screen
    // we are on and bounce straight back to the menu.
    Tuning keepTuning = w->tune;
    AppScreen keepScreen = w->screen;
    AppScreen keepPending = w->pending;
    float keepFade = w->fade;
    bool keepFadingOut = w->fadingOut;
    bool keepQuit = w->quitRequested;
    bool keepBanked = w->matchResultBanked;

    memset(w, 0, sizeof(World));

    w->tune = keepTuning;
    w->screen = keepScreen;
    w->pending = keepPending;
    w->fade = keepFade;
    w->fadingOut = keepFadingOut;
    w->quitRequested = keepQuit;
    w->matchResultBanked = keepBanked;

    ArenaLoad(&w->arena);
    w->playerIdx = 0;

    if (w->tune.gemGrab)
    {
        // Even sides. Slot 0 of the player team is the human; everyone else is a bot,
        // and allies fall out of the existing AI for free because it only ever asks
        // whether a brawler is on the other team.
        int perSide = w->tune.teamSize;
        if (perSide < 1) perSide = 1;
        if (perSide > MAX_BRAWLERS/2) perSide = MAX_BRAWLERS/2;

        for (int slot = 0; slot < perSide; slot++)
        {
            BrawlerClass kit = (BrawlerClass)(slot % CLASS_COUNT);
            int idx = w->brawlerCount++;
            BrawlerSpawn(w, idx, TEAM_PLAYER, (slot == 0) ? playerClass : kit,
                         ArenaSpawnFor(&w->arena, TEAM_PLAYER, slot), slot == 0);
            w->brawlers[idx].spawnSlot = slot;
        }

        for (int slot = 0; slot < perSide; slot++)
        {
            BrawlerClass kit = (BrawlerClass)((slot + 1) % CLASS_COUNT);
            int idx = w->brawlerCount++;
            BrawlerSpawn(w, idx, TEAM_ENEMY, kit,
                         ArenaSpawnFor(&w->arena, TEAM_ENEMY, slot), false);
            w->brawlers[idx].spawnSlot = slot;
        }
    }
    else
    {
        BrawlerSpawn(w, 0, TEAM_PLAYER, playerClass,
                     ArenaSpawnFor(&w->arena, TEAM_PLAYER, 0), true);
        w->brawlerCount = 1;

        int bots = w->tune.botCount;
        if (bots > MAX_BRAWLERS - 1) bots = MAX_BRAWLERS - 1;
        if (bots < 0) bots = 0;

        for (int i = 0; i < bots; i++)
        {
            BrawlerClass kit = w->tune.botMixedKits ? (BrawlerClass)(i % CLASS_COUNT) : w->tune.botKit;
            int idx = 1 + i;
            BrawlerSpawn(w, idx, TEAM_ENEMY, kit, ArenaSpawnFor(&w->arena, TEAM_ENEMY, i), false);
            w->brawlers[idx].spawnSlot = i;
            w->brawlerCount++;
        }
    }

    MatchReset(w);
    RenderBuildGrass(w);
    CameraInit(w);
}

static void DrawOverlays(World *w)
{
    HudDrawBars(w);
    FxDrawScreen(w);
    HudDrawPanel(w);
    CommandCenterDraw(w);
}

// Banks a finished match into the profile exactly once.
static void BankResult(World *w)
{
    if (w->matchResultBanked) return;
    if (!w->tune.gemGrab || w->match.phase != MATCH_OVER) return;

    w->matchResultBanked = true;
    if (w->match.winner == TEAM_PLAYER) w->tune.statWins++;
    else w->tune.statLosses++;
    w->tune.statKos += w->kills;
    ConfigMarkDirty();
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Brawl Arena");
    SetTargetFPS(60);

    // ESC must mean "back", not "quit", now that there are screens to back out of.
    SetExitKey(KEY_NULL);

    WeaponsResetAll();
    TuningSetDefaults(&world.tune);
    ConfigLoad(&world);

    AssetsLoad(&assets, SCREEN_WIDTH, SCREEN_HEIGHT);
    RenderSetAssets(&assets);
    MenuInit(&assets);

    ResetMatch(&world, (BrawlerClass)world.tune.selectedKit);
    world.screen = SCREEN_MENU;

    while (!WindowShouldClose() && !world.quitRequested)
    {
        float realDt = GetFrameTime();
        if (realDt > 0.05f) realDt = 0.05f;

        ShellUpdate(&world, realDt);
        bool locked = ShellIsTransitioning(&world);

        //--- Back / escape -------------------------------------------------------
        if (IsKeyPressed(KEY_ESCAPE) && !locked)
        {
            if (world.screen == SCREEN_BRAWLERS) ShellRequestScreen(&world, SCREEN_MENU);
            else if (world.screen == SCREEN_MATCH) ShellRequestScreen(&world, SCREEN_MENU);
            else world.quitRequested = true;
        }

        if (world.screen == SCREEN_MATCH)
        {
            float dt = realDt*world.tune.timeScale;

            CommandCenterUpdate(&world);

            if (IsKeyPressed(KEY_R) || world.matchRestartPending)
            {
                world.matchRestartPending = false;
                world.matchResultBanked = false;
                ResetMatch(&world, (BrawlerClass)world.tune.selectedKit);
            }

            world.time += dt;

            // Input is suppressed mid-transition so a menu click cannot also fire a shot.
            if (!locked) PlayerUpdate(&world, dt);
            AIUpdate(&world, dt);
            BrawlersUpdate(&world, dt);
            ProjectilesUpdate(&world, dt);
            ArenaUpdate(&world.arena, dt);
            MatchUpdate(&world, dt);
            FxUpdate(&world, dt);
            CameraUpdate(&world, dt);

            BankResult(&world);
        }
        else
        {
            world.time += realDt;
            if (!locked) MenuUpdate(&world, realDt);
            else MenuUpdate(&world, 0.0f);
        }

        ConfigAutoSave(&world, realDt);

        //--- Present -------------------------------------------------------------
        if (world.screen == SCREEN_MATCH)
        {
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
                        DrawTextureRec(assets.sceneTarget.texture,
                                       (Rectangle){ 0, 0, (float)assets.sceneTarget.texture.width,
                                                    -(float)assets.sceneTarget.texture.height },
                                       (Vector2){ 0, 0 }, WHITE);
                    EndShaderMode();
                    DrawOverlays(&world);
                    ShellDrawFade(&world);
                EndDrawing();
            }
            else
            {
                BeginDrawing();
                    ClearBackground(SKY_COLOR);
                    RenderWorld(&world);
                    DrawOverlays(&world);
                    ShellDrawFade(&world);
                EndDrawing();
            }
        }
        else
        {
            BeginDrawing();
                ClearBackground(BLACK);
                MenuDraw(&world);
                ShellDrawFade(&world);
            EndDrawing();
        }
    }

    ConfigSave(&world);
    AssetsUnload(&assets);
    CloseWindow();
    return 0;
}
