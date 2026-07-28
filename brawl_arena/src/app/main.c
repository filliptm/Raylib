/*******************************************************************************************
*   BRAWL ARENA
*
*   A top-down 3D arena brawler in the Brawl Stars mould: aim-and-release shooting,
*   an ammo/reload economy, a super charged by landing hits, and bushes that hide you.
*
*   Bots stand still by default so you can dial the feel in. Press TAB for the command
*   center; edits autosave to a local draft and can be promoted to tracked project truth.
********************************************************************************************/

#include "raylib.h"
#include "app_types.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "ai.h"
#include "player.h"
#include "render.h"
#include "camera.h"
#include "effects.h"
#include "hud.h"
#include "command_center.h"
#include "assets.h"
#include "character_animation.h"
#include "config.h"
#include "gems.h"
#include "menu.h"
#include "map_content.h"
#include "game_random.h"
#include "ui_system.h"
#include <string.h>
#include <math.h>
#include "raymath.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800

static App world;
static Assets assets;
static UiSystem ui;

static const Color SKY_COLOR = { 22, 26, 38, 255 };

// Finds a spot near `want` that is neither solid nor a bush, spiralling outward if the
// first choice is blocked. Sandbox targets must be reachable and, above all, visible.
static Vector3 ClearSpotNear(App *w, Vector3 want)
{
    for (float radius = 0.0f; radius <= 6.0f; radius += 1.0f)
    {
        int steps = (radius < 0.5f) ? 1 : 8;
        for (int i = 0; i < steps; i++)
        {
            float a = (i/(float)steps)*PI*2.0f;
            Vector3 p = { want.x + sinf(a)*radius, 0.0f, want.z + cosf(a)*radius };

            if (ArenaSolidAt(&w->session.arena, p.x, p.z)) continue;
            if (ArenaBushAt(&w->session.arena, p.x, p.z)) continue;
            return ArenaResolveCircle(&w->session.arena, p, BRAWLER_RADIUS);
        }
    }
    return ArenaResolveCircle(&w->session.arena, want, BRAWLER_RADIUS);
}

// Rebuilds only match-owned and short-lived presentation/controller state. Application
// configuration, content, navigation, and profile state have separate owners and are
// therefore impossible to erase accidentally during a match reset.
static void ResetMatch(App *w, BrawlerClass playerClass)
{
    bool sandbox = w->session.sandbox;
    memset(&w->session, 0, sizeof(w->session));
    memset(&w->controller, 0, sizeof(w->controller));
    memset(&w->presentation, 0, sizeof(w->presentation));
    w->session.sandbox = sandbox;
    GameRandomSeed(&w->session.random, 0xB4A71E5u);
    GameContext game = AppGameContext(w);

    ArenaLoad(&w->session.arena, MapCatalogSelected(&w->content), w->tune.crateHealth);
    w->session.playerIdx = 0;

    if (w->tune.gemGrab && !w->session.sandbox)
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
            int idx = w->session.brawlerCount++;
            BrawlerSpawn(game, idx, TEAM_PLAYER, (slot == 0) ? playerClass : kit,
                         ArenaSpawnFor(&w->session.arena, TEAM_PLAYER, slot), slot == 0);
            w->session.brawlers[idx].spawnSlot = slot;
        }

        for (int slot = 0; slot < perSide; slot++)
        {
            BrawlerClass kit = (BrawlerClass)((slot + 1) % CLASS_COUNT);
            int idx = w->session.brawlerCount++;
            BrawlerSpawn(game, idx, TEAM_ENEMY, kit,
                         ArenaSpawnFor(&w->session.arena, TEAM_ENEMY, slot), false);
            w->session.brawlers[idx].spawnSlot = slot;
        }
    }
    else
    {
        BrawlerSpawn(game, 0, TEAM_PLAYER, playerClass,
                     ArenaSpawnFor(&w->session.arena, TEAM_PLAYER, 0), true);
        w->session.brawlerCount = 1;

        int bots = w->tune.botCount;
        if (bots > MAX_BRAWLERS - 1) bots = MAX_BRAWLERS - 1;
        if (bots < 0) bots = 0;

        Vector3 home = ArenaSpawnFor(&w->session.arena, TEAM_PLAYER, 0);

        for (int i = 0; i < bots; i++)
        {
            BrawlerClass kit = w->tune.botMixedKits ? (BrawlerClass)(i % CLASS_COUNT) : w->tune.botKit;
            int idx = 1 + i;

            Vector3 spot;
            if (w->session.sandbox)
            {
                // A firing range: targets fanned out ahead of you at stepped distances,
                // so weapon range and damage falloff can be read off directly. Spawning
                // them at the far end of the map made testing a long walk.
                const float dist[4] = { 5.0f, 9.5f, 14.0f, 18.5f };
                const float side[4] = { 0.0f, -4.0f, 4.0f, -1.5f };
                spot = ClearSpotNear(w, (Vector3){ home.x + side[i % 4], 0.0f,
                                                   home.z - dist[i % 4] });
            }
            else spot = ArenaSpawnFor(&w->session.arena, TEAM_ENEMY, i);

            BrawlerSpawn(game, idx, TEAM_ENEMY, kit, spot, false);
            w->session.brawlers[idx].spawnSlot = i;
            w->session.brawlerCount++;
        }
    }

    MatchReset(game);
    RenderBuildGrass(w);
    CameraInit(w);
}

static void DrawOverlays(App *w)
{
    HudDrawBars(w);
    FxDrawScreen(w);
    HudDrawPanel(w);
    CommandCenterDraw(w);
}

// Banks a finished match into the profile exactly once.
static void BankResult(App *w)
{
    if (w->flow.matchResultBanked) return;
    if (!w->tune.gemGrab || w->session.match.phase != MATCH_OVER) return;

    w->flow.matchResultBanked = true;
    if (w->session.match.winner == TEAM_PLAYER) w->tune.statWins++;
    else w->tune.statLosses++;
    w->tune.statKos += w->session.kills;
    ConfigMarkDirty();
}

int main(void)
{
    // Configuration loads before the window exists so presentation flags can follow
    // it: with post-processing on, the scene renders into a non-multisampled FBO,
    // where an MSAA backbuffer only taxes the fullscreen blit.
    ConfigInitialize(&world);

    unsigned int flags = FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE;
    if (!world.tune.postFx) flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Brawl Arena");
    SetWindowMinSize(960, 600);

    // Vsync owns frame pacing on its own. Layering the CPU-side frame limiter on
    // top of the blocking buffer swap made the two clocks beat against each other,
    // producing alternating short/long frames.
    SetTargetFPS(0);

    // ESC must mean "back", not "quit", now that there are screens to back out of.
    SetExitKey(KEY_NULL);

    UiSystemLoad(&ui);
    char mapStatus[256];
    if (!MapCatalogLoad(&world.content, "data/maps/manifest.cfg",
                        mapStatus, (int)sizeof(mapStatus)))
    {
        TraceLog(LOG_ERROR, "Map catalog: %s", mapStatus);
        CloseWindow();
        return 1;
    }

    AssetsLoad(&assets, GetScreenWidth(), GetScreenHeight());
    RenderSetAssets(&assets);
    MenuInit(&assets, &ui);

    ResetMatch(&world, (BrawlerClass)world.tune.selectedKit);
    world.flow.screen = SCREEN_MENU;

    while (!WindowShouldClose() && !world.flow.quitRequested)
    {
        float realDt = GetFrameTime();
        if (realDt > 0.05f) realDt = 0.05f;
        UiSystemBeginFrame(&ui, &world.uiPreferences,
                           GetScreenWidth(), GetScreenHeight(), realDt);

        // A live drag-resize reports a new size every frame; rebuilding the scene
        // framebuffer each time hitched the whole drag. The target is recreated once
        // the size has settled, with direct rendering (no post) covering the gap.
        static float resizeSettle = -1.0f;
        if (IsWindowResized()) resizeSettle = 0.15f;
        if (resizeSettle >= 0.0f)
        {
            resizeSettle -= realDt;
            if (resizeSettle < 0.0f)
                AssetsResizeViewport(&assets, GetScreenWidth(), GetScreenHeight());
        }

        ShellUpdate(&world, realDt);
        bool locked = ShellIsTransitioning(&world);

        //--- Back / escape -------------------------------------------------------
        if (IsKeyPressed(KEY_ESCAPE) && !locked)
        {
            // An open overlay gets first refusal, so closing it never also quits.
            if (MenuConsumeEscape()) { }
            else if (world.flow.screen == SCREEN_MATCH &&
                     CommandCenterConsumeEscape()) { }
            else if (world.flow.screen == SCREEN_BRAWLERS) ShellRequestScreen(&world, SCREEN_MENU);
            else if (world.flow.screen == SCREEN_MATCH) ShellRequestScreen(&world, SCREEN_MENU);
            else world.flow.quitRequested = true;
        }

        if (world.flow.screen == SCREEN_MATCH)
        {
            float dt = realDt*world.tune.timeScale;

            CommandCenterUpdate(&world);

            GameContext game = AppGameContext(&world);
            bool decided = MatchIsOver(game);

            if ((IsKeyPressed(KEY_R) && !decided) || world.matchRestartPending)
            {
                world.matchRestartPending = false;
                world.flow.matchResultBanked = false;
                ResetMatch(&world, (BrawlerClass)world.tune.selectedKit);
            }

            world.session.time += dt;

            // Input is suppressed mid-transition so a menu click cannot also fire a shot,
            // and once the match is decided nothing moves at all. Effects and the camera
            // keep running so the deciding blow finishes playing out.
            if (!locked && !decided)
            {
                PlayerInput input = PlayerCaptureInput(&world);
                HudObservePlayerInput(&world, &input);
                PlayerUpdate(&world, &input, dt);
            }

            if (!decided)
            {
                AIUpdate(game, dt);
                BrawlersUpdate(game, dt);
                ProjectilesUpdate(game, dt);
                BrawlersUpdateRegeneration(game);
            }

            ArenaUpdate(&world.session.arena, dt);
            MatchUpdate(game, dt);
            FxConsumeGameEvents(&world);
            FxUpdate(&world, dt);
            CharacterAnimationsUpdate(&world.presentation, &assets,
                                      world.session.brawlers,
                                      world.session.brawlerCount,
                                      world.tune.moveSpeed, dt);
            CameraUpdate(&world, dt);

            BankResult(&world);

            // Hand the player back to the menu, either when they have read the result or
            // as soon as they click through it.
            if (decided && !ShellIsTransitioning(&world))
            {
                if (world.session.match.overTimer > world.tune.matchResultHold ||
                    HudConsumeContinue())
                    ShellRequestScreen(&world, SCREEN_MENU);
            }
        }
        else
        {
            world.session.time += realDt;
            if (!locked) MenuUpdate(&world, realDt);
            else MenuUpdate(&world, 0.0f);
        }

        ConfigAutoSave(&world, realDt);

        //--- Present -------------------------------------------------------------
        if (world.flow.screen == SCREEN_MATCH)
        {
            bool usePost = world.tune.postFx && assets.postOk &&
                           assets.sceneTarget.texture.id > 0 &&
                           resizeSettle < 0.0f;

            if (usePost)
            {
                BeginTextureMode(assets.sceneTarget);
                    ClearBackground(SKY_COLOR);
                    RenderWorld(&world);
                EndTextureMode();

                float outline = (world.tune.toon && assets.depthOk) ? world.tune.toonOutline : 0.0f;
                AssetsSetStyle(&assets, &world.tune, world.session.time, outline);

                BeginDrawing();
                    ClearBackground(BLACK);
                    BeginShaderMode(assets.post);
                        if (assets.depthOk)
                            SetShaderValueTexture(assets.post, assets.locDepthTex, assets.sceneTarget.depth);
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
        UiSystemEndFrame(&ui);
    }

    ConfigFlush(&world);
    UiSystemUnload(&ui);
    AssetsUnload(&assets);
    CloseWindow();
    return 0;
}
