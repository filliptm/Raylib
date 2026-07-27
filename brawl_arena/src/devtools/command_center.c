/*******************************************************************************************
*   COMMAND CENTER
*
*   A live tuning panel. Controls edit App.tune and its owned content catalog, autosave
*   to an ignored draft, and expose explicit promotion into tracked project defaults.
*
*   The widgets are a tiny immediate-mode set written against raylib's shape and text
*   calls, which keeps the project dependency-free.
********************************************************************************************/
#include "command_center.h"
#include "command_widgets.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "render.h"
#include "gems.h"
#include "effects.h"
#include "config.h"
#include "map_content.h"
#include "game_commands.h"
#include "content_catalog.h"
#include "raymath.h"
#include <stddef.h>
#include <math.h>

typedef enum { TAB_MATCH = 0, TAB_BOTS, TAB_PLAYER, TAB_KIT, TAB_STYLE, TAB_WORLD, TAB_COUNT } PanelTab;

static const char *TAB_NAMES[TAB_COUNT] = { "MATCH", "BOTS", "PLAYER", "KIT", "STYLE", "WORLD" };

// Open on launch: this build is a sandbox, so the tuning panel is the point of entry.
static bool g_open = true;
static PanelTab g_tab = TAB_MATCH;
static float g_scroll[TAB_COUNT] = { 0 };
static float g_contentHeight[TAB_COUNT] = { 0 };

//------------------------------------------------------------------------------------
// Actions
//------------------------------------------------------------------------------------
// Some rules only take effect on a rebuild; flag it and let the game loop handle it.
static void NeedsRestart(App *w)
{
    w->matchRestartPending = true;
    ConfigMarkDirty();
}

//------------------------------------------------------------------------------------
// Tabs
//------------------------------------------------------------------------------------
static void TabMatch(CommandUi *ui)
{
    App *w = ui->world;
    Tuning *t = &w->tune;

    CommandUiSection(ui, "MODE");

    // The toggle is the rule PLAY will use. While a sandbox session is running it is
    // deliberately not in effect, and saying so avoids the panel looking like it lies.
    if (w->session.sandbox)
    {
        CommandUiText(ui, "SANDBOX SESSION - no objective", (Color){ 120, 200, 255, 255 });
        CommandUiText(ui, "Rules below apply to PLAY, not to now.", COMMAND_TEXT_DIM);
    }

    if (CommandUiToggle(ui, "Gem Grab", &t->gemGrab)) NeedsRestart(w);
    CommandUiText(ui, t->gemGrab ? "Two teams race to hold the target count."
                          : "Free-form: no teams, no objective.", COMMAND_TEXT_DIM);

    if (!t->gemGrab)
    {
        CommandUiSection(ui, "");
        CommandUiText(ui, "Turn Gem Grab on for team rules.", COMMAND_TEXT_DIM);
        return;
    }

    CommandUiSection(ui, "RULES");
    if (CommandUiSliderI(ui, "Team size", &t->teamSize, 1, MAX_BRAWLERS/2)) NeedsRestart(w);
    CommandUiSliderI(ui, "Gems to win", &t->gemsToWin, 1, 30);
    CommandUiSliderF(ui, "Countdown", &t->gemCountdown, 3.0f, 40.0f, "%.0fs");
    CommandUiSliderF(ui, "Gem every", &t->gemVentInterval, 1.0f, 20.0f, "%.1fs");

    CommandUiSection(ui, "STATE");
    if (w->session.sandbox)
    {
        CommandUiText(ui, TextFormat("SANDBOX    bots %s", BOT_MODE_NAMES[t->botMode]), COMMAND_TEXT_MAIN);
    }
    else
    {
        const char *phase = (w->session.match.phase == MATCH_OVER) ? "OVER"
                          : (w->session.match.phase == MATCH_COUNTDOWN) ? "COUNTDOWN" : "PLAYING";
        CommandUiText(ui, TextFormat("%s    %d - %d", phase, w->session.match.teamGems[0], w->session.match.teamGems[1]), COMMAND_TEXT_MAIN);
    }

    int loose = 0;
    for (int i = 0; i < MAX_GEMS; i++) if (w->session.gems[i].active) loose++;
    CommandUiText(ui, TextFormat(loose == 1 ? "%d gem on the floor" : "%d gems on the floor", loose), COMMAND_TEXT_DIM);

    CommandUiSection(ui, "ACTIONS");
    if (CommandUiButton(ui, "Restart match"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESET_OBJECTIVE, 0 });
    if (CommandUiButton(ui, "Drop a gem at the vent"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SPAWN_GEM, 0 });
    if (CommandUiButton(ui, "Clear the floor"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_CLEAR_GEMS, 0 });
}

static void TabBots(CommandUi *ui)
{
    App *w = ui->world;
    Tuning *t = &w->tune;

    CommandUiSection(ui, "BEHAVIOUR");

    int mode = (int)t->botMode;
    if (CommandUiCycler(ui, "Mode", &mode, BOT_MODE_COUNT, BOT_MODE_NAMES))
        t->botMode = (BotMode)mode;

    const char *hint =
        (t->botMode == BOT_STATIC) ? "Inert targets. They will not move or shoot." :
        (t->botMode == BOT_ROAM)   ? "They wander the arena but never open fire." :
                                     "Full combat AI: chase, strafe, shoot, retreat.";
    CommandUiText(ui, hint, COMMAND_TEXT_DIM);

    CommandUiSection(ui, "ROSTER");

    // Bound straight to the tuning field: a local would reintroduce the shared-address
    // problem and would not survive a save.
    if (CommandUiSliderI(ui, "Bot count", &t->botCount, 0, MAX_BRAWLERS - 1))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SET_BOT_COUNT, t->botCount });

    if (CommandUiToggle(ui, "Mixed kits", &t->botMixedKits))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESPAWN_BOTS, 0 });

    if (!t->botMixedKits)
    {
        int kit = (int)t->botKit;
        if (CommandUiCycler(ui, "Bot kit", &kit, CLASS_COUNT, CLASS_NAMES))
        {
            t->botKit = (BrawlerClass)kit;
            GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESPAWN_BOTS, 0 });
        }
    }

    CommandUiSliderF(ui, "Respawn delay", &t->enemyRespawn, 0.5f, 15.0f, "%.1fs");

    CommandUiSection(ui, "COMBAT DECISIONS");
    CommandUiSliderF(ui, "Retreat below", &t->aiRetreatHealth, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Heal ally below", &t->aiSupportHealth, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Super ally below", &t->aiSupportSuperHealth, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Obstacle probe", &t->aiProbeAhead, 0.2f, 5.0f, "%.2f");

    CommandUiSection(ui, "ACTIONS");
    if (CommandUiButton(ui, "Respawn all bots"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESPAWN_BOTS, 0 });

    if (CommandUiButton(ui, "Kill all bots"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_KILL_BOTS, 0 });

    if (CommandUiButton(ui, "Heal all bots"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_HEAL_BOTS, 0 });
}

static void TabPlayer(CommandUi *ui)
{
    App *w = ui->world;
    Tuning *t = &w->tune;
    Brawler *p = &w->session.brawlers[w->session.playerIdx];

    CommandUiSection(ui, "KIT");
    int kit = (int)p->cls;
    if (CommandUiCycler(ui, "Active kit", &kit, CLASS_COUNT, CLASS_NAMES))
    {
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SET_PLAYER_CLASS, kit });
        w->tune.selectedKit = kit;      // so the choice survives a restart like the rest
        ConfigMarkDirty();
    }

    CommandUiSection(ui, "SANDBOX");
    CommandUiToggle(ui, "God mode", &t->godMode);
    CommandUiToggle(ui, "Infinite ammo", &t->infiniteAmmo);

    CommandUiSection(ui, "MOVEMENT");
    CommandUiSliderF(ui, "Move speed", &t->moveSpeed, 2.0f, 30.0f, "%.1f");
    CommandUiSliderF(ui, "Acceleration", &t->moveAccel, 4.0f, 80.0f, "%.0f");
    CommandUiSliderF(ui, "Dash speed", &t->dashSpeed, 8.0f, 60.0f, "%.0f");
    CommandUiSliderF(ui, "Respawn delay", &t->playerRespawn, 0.5f, 15.0f, "%.1fs");

    CommandUiSection(ui, "ACTIONS");
    if (CommandUiButton(ui, "Charge super"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_CHARGE_PLAYER_SUPER, 0 });
    if (CommandUiButton(ui, "Heal to full"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_HEAL_PLAYER, 0 });
    if (CommandUiButton(ui, "Respawn player"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESPAWN_PLAYER, 0 });
}

static void TabKit(CommandUi *ui)
{
    App *w = ui->world;
    BrawlerClass cls = w->session.brawlers[w->session.playerIdx].cls;
    WeaponDef *k = &w->content.weapons[cls];

    CommandUiSection(ui, "EDITING");
    CommandUiText(ui, TextFormat("%s  -  %s", k->name, k->flavor), COMMAND_TEXT_MAIN);

    CommandUiSection(ui, "MAIN ATTACK");
    if (CommandUiSliderI(ui, "Max health", &k->maxHealth, 500, 12000))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SYNC_CLASS_HEALTH, cls });
    if (k->mainKind == ATTACK_RAIN)
    {
        CommandUiSliderI(ui, "Damage / pulse", &k->damage, 20, 4000);
        CommandUiSliderI(ui, "Healing / pulse", &k->healing, 20, 4000);
        CommandUiSliderF(ui, "Cast range", &k->range, 2.0f, 40.0f, "%.1f");
        CommandUiSliderF(ui, "Rain radius", &k->projRadius, 0.5f, 8.0f, "%.2f");
        CommandUiSliderF(ui, "Duration", &k->duration, 0.2f, 8.0f, "%.2fs");
        CommandUiSliderF(ui, "Pulse interval", &k->tickRate, 0.05f, k->duration, "%.2fs");
        CommandUiSliderF(ui, "Growth time", &k->growTime, 0.05f, k->duration, "%.2fs");
    }
    else
    {
        CommandUiSliderI(ui, "Damage / pellet", &k->damage, 20, 4000);
        if (k->healing > 0) CommandUiSliderI(ui, "Healing / ally", &k->healing, 20, 4000);
        CommandUiSliderI(ui, "Pellets", &k->pellets, 1, 20);
        CommandUiSliderF(ui, "Spread", &k->spreadDeg, 0.0f, 90.0f, "%.0f deg");
        CommandUiSliderF(ui, "Projectile speed", &k->speed, 6.0f, 90.0f, "%.0f");
        CommandUiSliderF(ui, "Range", &k->range, 2.0f, 40.0f, "%.1f");
        CommandUiSliderF(ui, "Shot size", &k->projRadius, 0.05f, 4.0f, "%.2f");
    }
    CommandUiSliderF(ui, "Cooldown", &k->cooldown, 0.05f, 2.0f, "%.2fs");
    CommandUiSliderF(ui, "Reload / ammo", &k->reloadPerAmmo, 0.15f, 5.0f, "%.2fs");
    CommandUiSliderI(ui, "Ammo capacity", &k->maxAmmo, 1, 8);
    CommandUiSliderF(ui, k->mainKind == ATTACK_RAIN ? "Super gain / pulse" : "Super gain / hit",
              &k->superPerHit, 0.0f, 1.0f, "%.2f");

    CommandUiSection(ui, "SUPER");
    CommandUiText(ui, k->superName, COMMAND_TEXT_MAIN);
    if (k->superKind == SUPER_SOUND_WAVE)
    {
        CommandUiSliderI(ui, "Damage / tick", &k->sDamage, 20, 6000);
        CommandUiSliderI(ui, "Healing / tick", &k->sHealing, 20, 6000);
        CommandUiSliderF(ui, "Cone width", &k->sSpreadDeg, 20.0f, 150.0f, "%.0f deg");
        CommandUiSliderF(ui, "Range", &k->sRange, 2.0f, 45.0f, "%.1f");
        CommandUiSliderF(ui, "Effect duration", &k->sDuration, 0.2f, 12.0f, "%.2fs");
        CommandUiSliderF(ui, "Tick interval", &k->sTickRate, 0.05f, k->sDuration, "%.2fs");
        CommandUiSliderF(ui, "Wave lifetime", &k->sVisualDuration, 0.1f, 4.0f, "%.2fs");
    }
    else if (k->superKind == SUPER_HEALING_BURST)
    {
        CommandUiSliderI(ui, "Healing", &k->sHealing, 50, 6000);
        CommandUiSliderF(ui, "Radius", &k->sRange, 2.0f, 20.0f, "%.1f");
    }
    else if (k->superKind != SUPER_DASH)
    {
        CommandUiSliderI(ui, "Damage", &k->sDamage, 50, 6000);
        CommandUiSliderI(ui, "Pellets", &k->sPellets, 1, 24);
        CommandUiSliderF(ui, "Spread", &k->sSpreadDeg, 0.0f, 90.0f, "%.0f deg");
        CommandUiSliderF(ui, "Range", &k->sRange, 2.0f, 45.0f, "%.1f");
    }
    else
    {
        CommandUiSliderI(ui, "Impact damage", &k->sDamage, 50, 6000);
        CommandUiText(ui, "Dash length follows Dash speed on the PLAYER tab.", COMMAND_TEXT_DIM);
    }

    CommandUiSection(ui, "");
    int kitOverrides = ConfigKitOverrideCount(w, cls);
    CommandUiText(ui, TextFormat("%d local project-value changes", kitOverrides), COMMAND_TEXT_DIM);
    if (CommandUiButton(ui, "Reset kit to PROJECT default"))
    {
        ConfigResetKitToProject(w, cls);
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SYNC_CLASS_HEALTH, cls });
    }
    if (CommandUiButton(ui, "SAVE KIT AS PROJECT DEFAULT"))
        ConfigPromoteKit(w, cls);
}

static void TabStyle(CommandUi *ui)
{
    App *w = ui->world;
    Tuning *t = &w->tune;

    CommandUiSection(ui, "MASTER");
    CommandUiToggle(ui, "Post effects", &t->postFx);
    if (!t->postFx)
    {
        CommandUiText(ui, "Everything below needs the post pass on.", COMMAND_TEXT_DIM);
        return;
    }

    CommandUiSection(ui, "ILLUSTRATED");
    CommandUiToggle(ui, "Toon light bands", &t->toon);
    CommandUiSliderF(ui, "Bands", &t->toonBands, 2.0f, 4.0f, "%.0f");
    CommandUiSliderF(ui, "Ink outline", &t->toonOutline, 0.0f, 1.0f, "%.2f");

    CommandUiSection(ui, "CANVAS");
    CommandUiSliderF(ui, "Painterly", &t->stylePainterly, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Pixelate", &t->stylePixelate, 0.0f, 1.0f, "%.2f");

    CommandUiSection(ui, "PRINT");
    CommandUiSliderF(ui, "Halftone dots", &t->styleHalftone, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Posterize", &t->stylePosterize, 0.0f, 1.0f, "%.2f");

    CommandUiSection(ui, "GRADE");
    CommandUiSliderF(ui, "Saturation", &t->styleSaturation, 0.0f, 2.0f, "%.2f");
    CommandUiSliderF(ui, "Brightness", &t->styleBrightness, 0.6f, 1.5f, "%.2f");
    CommandUiSliderF(ui, "Bloom", &t->bloom, 0.0f, 3.0f, "%.2f");
    CommandUiSliderF(ui, "Vignette", &t->styleVignette, 0.0f, 1.5f, "%.2f");

    CommandUiSection(ui, "FILM");
    CommandUiSliderF(ui, "Grain", &t->styleGrain, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Chromatic fringe", &t->styleCA, 0.0f, 1.0f, "%.2f");

    CommandUiSection(ui, "");
    if (CommandUiButton(ui, "Reset style to PROJECT default"))
    {
        const Tuning *p = &w->config.projectTuning;
        t->postFx = p->postFx;
        t->toon = p->toon;
        t->toonBands = p->toonBands;
        t->toonOutline = p->toonOutline;
        t->stylePixelate = p->stylePixelate;
        t->stylePainterly = p->stylePainterly;
        t->styleHalftone = p->styleHalftone;
        t->stylePosterize = p->stylePosterize;
        t->styleGrain = p->styleGrain;
        t->styleCA = p->styleCA;
        t->styleSaturation = p->styleSaturation;
        t->styleBrightness = p->styleBrightness;
        t->styleVignette = p->styleVignette;
        t->bloom = p->bloom;
        ConfigMarkDirty();
    }
}

static void TabWorld(CommandUi *ui)
{
    App *w = ui->world;
    Tuning *t = &w->tune;

    CommandUiSection(ui, "PACE");
    CommandUiSliderF(ui, "Time scale", &t->timeScale, 0.05f, 2.0f, "%.2fx");
    CommandUiSliderF(ui, "Super gain mult", &t->superMult, 0.0f, 6.0f, "%.2fx");
    if (CommandUiSliderI(ui, "Crate health", &t->crateHealth, 100, 10000)) NeedsRestart(w);
    CommandUiSliderF(ui, "Result hold", &t->matchResultHold, 0.5f, 15.0f, "%.1fs");

    CommandUiSection(ui, "STEALTH");
    CommandUiSliderF(ui, "Bush reveal range", &t->bushReveal, 0.0f, 14.0f, "%.1f");
    CommandUiSliderF(ui, "Reveal on fire", &t->fireReveal, 0.0f, 6.0f, "%.1fs");
    CommandUiSliderF(ui, "Conceal ghosting", &t->concealDither, 0.0f, 0.95f, "%.2f");

    CommandUiSection(ui, "GRASS");
    CommandUiSliderF(ui, "Height", &t->grassHeight, 0.2f, 4.0f, "%.2f");
    CommandUiSliderF(ui, "Wind strength", &t->windStrength, 0.0f, 1.5f, "%.2f");
    CommandUiSliderF(ui, "Wind speed", &t->windSpeed, 0.0f, 6.0f, "%.2f");
    CommandUiSliderF(ui, "Bend radius", &t->grassBendRadius, 0.2f, 6.0f, "%.2f");
    CommandUiSliderF(ui, "Bend strength", &t->grassBendStrength, 0.0f, 4.0f, "%.2f");

    CommandUiSection(ui, "LOOK");
    CommandUiToggle(ui, "Rigged character models", &t->modelCharacter);
    CommandUiSliderF(ui, "Bloom strength", &t->bloom, 0.0f, 3.0f, "%.2f");

    CommandUiSection(ui, "DEBUG");
    CommandUiToggle(ui, "Show ranges + sight", &t->showDebug);

    int projectiles = 0, particles = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) if (w->session.projectiles[i].active) projectiles++;
    for (int i = 0; i < MAX_PARTICLES; i++) if (w->presentation.particles[i].active) particles++;

    CommandUiText(ui, TextFormat("%d FPS   %d shots   %d particles", GetFPS(), projectiles, particles), COMMAND_TEXT_DIM);

    CommandUiSection(ui, "ACTIONS");
    const MapDefinition *selectedMap = MapCatalogSelected(&w->content);
    CommandUiText(ui, TextFormat("Map: %s", selectedMap ? selectedMap->name : "none"), COMMAND_TEXT_DIM);
    if (w->content.mapCount > 1 && CommandUiButton(ui, "Next map"))
    {
        w->content.selectedMap = (w->content.selectedMap + 1)%w->content.mapCount;
        w->matchRestartPending = true;
    }
    if (CommandUiButton(ui, "Rebuild arena")) w->matchRestartPending = true;

    if (CommandUiButton(ui, "Reset score"))
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_RESET_SCORE, 0 });

    CommandUiSection(ui, "PROJECT CONFIG");
    CommandUiText(ui, ConfigStatus(w), COMMAND_TEXT_DIM);
    CommandUiText(ui, TextFormat("%d values differ from PROJECT", ConfigProjectOverrideCount(w)),
           ConfigProjectOverrideCount(w) > 0 ? COMMAND_WARN : COMMAND_TEXT_DIM);

    if (CommandUiButton(ui, "Discard draft / restore PROJECT"))
    {
        ConfigResetAllToProject(w);
        for (int i = 0; i < CLASS_COUNT; i++)
            GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SYNC_CLASS_HEALTH, i });
        w->matchRestartPending = true;
    }
    if (CommandUiButton(ui, "SAVE ALL AS PROJECT DEFAULTS"))
        ConfigPromoteAll(w);
}

//------------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------------
bool CommandCenterIsOpen(void) { return g_open; }

void CommandCenterForceOpen(void) { g_open = true; }

bool CommandCenterCapturesMouse(void)
{
    if (!g_open) return false;
    return CommandUiMouseIn(CommandPanelRect()) || CommandUiHasActiveSlider();
}

void CommandCenterUpdate(App *w)
{
    (void)w;
    if (IsKeyPressed(KEY_TAB)) g_open = !g_open;
    if (!g_open) CommandUiResetInteraction();
}

void CommandCenterDraw(App *w)
{
    if (!g_open)
    {
        // Centered, so it clears the kit panel on the left and the super meter on the right.
        const char *hint = "TAB  command center";
        int tw = MeasureText(hint, 14);
        DrawText(hint, GetScreenWidth() / 2 - tw / 2, GetScreenHeight() - 26, 14, (Color){ 120, 132, 152, 200 });
        return;
    }

    Rectangle panel = CommandPanelRect();
    DrawRectangleRounded(panel, 0.02f, 8, COMMAND_PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.02f, 8, COMMAND_PANEL_EDGE);

    // Title
    DrawText("COMMAND CENTER", (int)panel.x + 16, (int)panel.y + 12, 18, COMMAND_TEXT_MAIN);
    DrawText("TAB to close", (int)(panel.x + panel.width - 92), (int)panel.y + 17, 12, COMMAND_TEXT_DIM);
    int projectChanges = ConfigProjectOverrideCount(w);
    const char *source = w->config.recoveryDefaults ? "CONFIG RECOVERY MODE"
                       : (projectChanges > 0 ? TextFormat("PROJECT + LOCAL DRAFT  (%d)", projectChanges)
                                             : "PROJECT DEFAULTS");
    DrawText(source, (int)panel.x + 16, (int)panel.y + 32, 11,
             w->config.recoveryDefaults ? COMMAND_WARN : (Color){ 96, 148, 188, 255 });

    // Tabs
    int tabY = (int)panel.y + 50;
    int tabW = (int)(panel.width - 32) / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++)
    {
        Rectangle r = { panel.x + 16 + i * tabW, tabY, tabW - 4, 26 };
        bool hover = CommandUiMouseIn(r);
        bool active = (g_tab == (PanelTab)i);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_tab = (PanelTab)i;

        DrawRectangleRounded(r, 0.3f, 6, active ? COMMAND_ACCENT_DIM : (hover ? (Color){ 30, 38, 50, 255 } : COMMAND_TRACK_BG));
        int tw = MeasureText(TAB_NAMES[i], 13);
        DrawText(TAB_NAMES[i], (int)(r.x + r.width / 2 - tw / 2), (int)r.y + 6, 13,
                 active ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);

        if (active) DrawRectangle((int)r.x, (int)(r.y + r.height - 2), (int)r.width, 2, COMMAND_ACCENT);
    }

    int contentTop = tabY + 34;
    int contentBottom = (int)(panel.y + panel.height) - 10;
    Rectangle contentClip = {
        panel.x + 8, (float)contentTop, panel.width - 16,
        (float)(contentBottom - contentTop)
    };

    float maxScroll = fmaxf(0.0f, g_contentHeight[g_tab] - contentClip.height);
    if (CommandUiMouseIn(contentClip))
    {
        g_scroll[g_tab] -= GetMouseWheelMove()*42.0f;
        g_scroll[g_tab] = Clamp(g_scroll[g_tab], 0.0f, maxScroll);
    }

    CommandUi ui = {
        .world = w,
        .x = (int)panel.x + 16,
        .y = contentTop - (int)g_scroll[g_tab],
        .width = (int)panel.width - 32,
        .clip = contentClip
    };
    int logicalStart = ui.y;

    BeginScissorMode((int)contentClip.x, (int)contentClip.y,
                     (int)contentClip.width, (int)contentClip.height);

    switch (g_tab)
    {
        case TAB_MATCH:  TabMatch(&ui); break;
        case TAB_BOTS:   TabBots(&ui); break;
        case TAB_PLAYER: TabPlayer(&ui); break;
        case TAB_KIT:    TabKit(&ui); break;
        case TAB_STYLE:  TabStyle(&ui); break;
        case TAB_WORLD:  TabWorld(&ui); break;
        default: break;
    }
    EndScissorMode();
    ContentCatalogRebuildTyped(&w->content);

    g_contentHeight[g_tab] = (float)(ui.y - logicalStart);
    maxScroll = fmaxf(0.0f, g_contentHeight[g_tab] - contentClip.height);
    g_scroll[g_tab] = Clamp(g_scroll[g_tab], 0.0f, maxScroll);

    if (maxScroll > 0.0f)
    {
        float trackH = contentClip.height;
        float thumbH = fmaxf(28.0f, trackH*(trackH/g_contentHeight[g_tab]));
        float travel = trackH - thumbH;
        float thumbY = contentClip.y + (maxScroll > 0.0f ? g_scroll[g_tab]/maxScroll*travel : 0.0f);
        DrawRectangle((int)(contentClip.x + contentClip.width - 3), (int)contentClip.y,
                      2, (int)trackH, COMMAND_TRACK_BG);
        DrawRectangle((int)(contentClip.x + contentClip.width - 4), (int)thumbY,
                      4, (int)thumbH, COMMAND_ACCENT_DIM);
    }
}
