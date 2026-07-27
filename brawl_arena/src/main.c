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
#include <math.h>
#include "raymath.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800

static World world;
static Assets assets;

static const Color SKY_COLOR = { 22, 26, 38, 255 };

// Finds a spot near `want` that is neither solid nor a bush, spiralling outward if the
// first choice is blocked. Sandbox targets must be reachable and, above all, visible.
static Vector3 ClearSpotNear(World *w, Vector3 want)
{
    for (float radius = 0.0f; radius <= 6.0f; radius += 1.0f)
    {
        int steps = (radius < 0.5f) ? 1 : 8;
        for (int i = 0; i < steps; i++)
        {
            float a = (i/(float)steps)*PI*2.0f;
            Vector3 p = { want.x + sinf(a)*radius, 0.0f, want.z + cosf(a)*radius };

            if (ArenaSolidAt(&w->arena, p.x, p.z)) continue;
            if (ArenaBushAt(&w->arena, p.x, p.z)) continue;
            return ArenaResolveCircle(&w->arena, p, BRAWLER_RADIUS);
        }
    }
    return ArenaResolveCircle(&w->arena, want, BRAWLER_RADIUS);
}

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
    bool keepSandbox = w->sandbox;

    memset(w, 0, sizeof(World));

    w->tune = keepTuning;
    w->screen = keepScreen;
    w->pending = keepPending;
    w->fade = keepFade;
    w->fadingOut = keepFadingOut;
    w->quitRequested = keepQuit;
    w->matchResultBanked = keepBanked;
    w->sandbox = keepSandbox;

    ArenaLoad(&w->arena);
    w->playerIdx = 0;

    if (w->tune.gemGrab && !w->sandbox)
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

        Vector3 home = ArenaSpawnFor(&w->arena, TEAM_PLAYER, 0);

        for (int i = 0; i < bots; i++)
        {
            BrawlerClass kit = w->tune.botMixedKits ? (BrawlerClass)(i % CLASS_COUNT) : w->tune.botKit;
            int idx = 1 + i;

            Vector3 spot;
            if (w->sandbox)
            {
                // A firing range: targets fanned out ahead of you at stepped distances,
                // so weapon range and damage falloff can be read off directly. Spawning
                // them at the far end of the map made testing a long walk.
                const float dist[4] = { 5.0f, 9.5f, 14.0f, 18.5f };
                const float side[4] = { 0.0f, -4.0f, 4.0f, -1.5f };
                spot = ClearSpotNear(w, (Vector3){ home.x + side[i % 4], 0.0f,
                                                   home.z - dist[i % 4] });
            }
            else spot = ArenaSpawnFor(&w->arena, TEAM_ENEMY, i);

            BrawlerSpawn(w, idx, TEAM_ENEMY, kit, spot, false);
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
            // An open overlay gets first refusal, so closing it never also quits.
            if (MenuConsumeEscape()) { }
            else if (world.screen == SCREEN_BRAWLERS) ShellRequestScreen(&world, SCREEN_MENU);
            else if (world.screen == SCREEN_MATCH) ShellRequestScreen(&world, SCREEN_MENU);
            else world.quitRequested = true;
        }

        if (world.screen == SCREEN_MATCH)
        {
            float dt = realDt*world.tune.timeScale;

            CommandCenterUpdate(&world);

            bool decided = MatchIsOver(&world);

            if ((IsKeyPressed(KEY_R) && !decided) || world.matchRestartPending)
            {
                world.matchRestartPending = false;
                world.matchResultBanked = false;
                ResetMatch(&world, (BrawlerClass)world.tune.selectedKit);
            }

            world.time += dt;

            // Input is suppressed mid-transition so a menu click cannot also fire a shot,
            // and once the match is decided nothing moves at all. Effects and the camera
            // keep running so the deciding blow finishes playing out.
            if (!locked && !decided) PlayerUpdate(&world, dt);

            if (!decided)
            {
                AIUpdate(&world, dt);
                BrawlersUpdate(&world, dt);
                ProjectilesUpdate(&world, dt);
            }

            ArenaUpdate(&world.arena, dt);
            MatchUpdate(&world, dt);
            FxUpdate(&world, dt);
            CameraUpdate(&world, dt);

            BankResult(&world);

            // Hand the player back to the menu, either when they have read the result or
            // as soon as they click through it.
            if (decided && !ShellIsTransitioning(&world))
            {
                bool skipped = world.match.overTimer > 0.9f &&
                               (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE));

                if (world.match.overTimer > MATCH_RESULT_HOLD || skipped)
                    ShellRequestScreen(&world, SCREEN_MENU);
            }
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

    ConfigFlush(&world);
    ConfigSave(&world);
    AssetsUnload(&assets);
    CloseWindow();
    return 0;
}
