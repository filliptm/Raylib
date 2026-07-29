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
#include "vfx.h"
#include "vfx_catalog.h"
#include "character_animation.h"
#include "config.h"
#include "map_content.h"
#include "game_commands.h"
#include "content_catalog.h"
#include "ui_system.h"
#include "raymath.h"
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    TAB_MATCH = 0,
    TAB_BOTS,
    TAB_PLAYER,
    TAB_KIT,
    TAB_STYLE,
    TAB_WORLD,
    TAB_PREVIEW,
    TAB_COUNT
} PanelTab;

static const char *TAB_NAMES[TAB_COUNT] = {
    "MATCH", "BOTS", "PLAYER", "KIT", "VISUAL", "WORLD", "PREVIEW / UI"
};

typedef struct CommandCenterState {
    bool open;
    PanelTab tab;
    float scroll[TAB_COUNT];
    float contentHeight[TAB_COUNT];
    int vfxPreview;
    int actionPreview;
} CommandCenterState;

// Explicit long-lived editor state. Match resets cannot erase navigation or scroll.
static CommandCenterState g_command = {
    .open = false,
    .tab = TAB_MATCH,
    .vfxPreview = VFX_SCRAPPER_CAST
};

#define g_open (g_command.open)
#define g_tab (g_command.tab)
#define g_scroll (g_command.scroll)
#define g_contentHeight (g_command.contentHeight)

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
        CommandUiSliderI(ui,
                         k->mainKind == ATTACK_RETURNING
                             ? "Damage / leg" : "Damage / pellet",
                         &k->damage, 20, 4000);
        if (k->healing > 0) CommandUiSliderI(ui, "Healing / ally", &k->healing, 20, 4000);
        if (k->mainKind != ATTACK_RETURNING)
        {
            CommandUiSliderI(ui, "Pellets", &k->pellets, 1, 20);
            CommandUiSliderF(ui, "Spread", &k->spreadDeg,
                             0.0f, 90.0f, "%.0f deg");
        }
        CommandUiSliderF(ui,
                         k->mainKind == ATTACK_RETURNING
                             ? "Outbound speed" : "Projectile speed",
                         &k->speed, 6.0f, 90.0f, "%.0f");
        CommandUiSliderF(ui, "Range", &k->range, 2.0f, 40.0f, "%.1f");
        CommandUiSliderF(ui, "Shot size", &k->projRadius, 0.05f, 4.0f, "%.2f");
        if (k->mainKind == ATTACK_RETURNING)
            CommandUiSliderF(ui, "Return speed", &k->returnSpeed,
                             6.0f, 120.0f, "%.1f");
    }
    CommandUiSliderF(ui, "Cooldown", &k->cooldown, 0.05f, 2.0f, "%.2fs");
    CommandUiSliderF(ui, "Reload / ammo", &k->reloadPerAmmo, 0.15f, 5.0f, "%.2fs");
    CommandUiSliderI(ui, "Ammo capacity", &k->maxAmmo, 1, 8);
    CommandUiSliderF(ui, k->mainKind == ATTACK_RAIN ? "Super gain / pulse" : "Super gain / hit",
              &k->superPerHit, 0.0f, 1.0f, "%.2f");
    if (k->mainKind == ATTACK_PROJECTILE || k->mainKind == ATTACK_LOB)
        CommandUiSliderF(ui, "Self-heal / damage", &k->selfHealRatio, 0.0f, 1.0f, "%.2f");

    if (k->secondaryKind != SECONDARY_NONE)
    {
        CommandUiSection(ui, "SECONDARY");
        CommandUiText(ui, k->mobilityName, COMMAND_TEXT_MAIN);
        if (k->secondaryKind == SECONDARY_DASH)
        {
            CommandUiSliderF(ui, "Cooldown", &k->mobilityCooldown,
                             0.25f, 10.0f, "%.2fs");
            CommandUiSliderF(ui, "Duration", &k->mobilityDuration,
                             0.05f, 1.0f, "%.2fs");
            CommandUiSliderF(ui, "Speed", &k->mobilitySpeed,
                             4.0f, 60.0f, "%.1f");
            CommandUiText(ui, "Shift boosts along movement, then aim.",
                          COMMAND_TEXT_DIM);
        }
        else if (k->secondaryKind == SECONDARY_SHIELD)
        {
            CommandUiSliderI(ui, "Capacity", &k->secondaryCapacity, 100, 5000);
            CommandUiSliderF(ui, "Move multiplier",
                             &k->secondaryMoveMultiplier,
                             0.10f, 1.0f, "%.2fx");
            CommandUiSliderF(ui, "Healing / absorbed",
                             &k->secondaryHealRatio,
                             0.0f, 1.0f, "%.2f");
            CommandUiSliderF(ui, "Recharge delay",
                             &k->secondaryRechargeDelay,
                             0.25f, 10.0f, "%.2fs");
            CommandUiSliderF(ui, "Recharge / second",
                             &k->secondaryRechargeRate,
                             25.0f, 2000.0f, "%.0f");
            CommandUiSliderF(ui, "Break lockout",
                             &k->secondaryBreakLockout,
                             0.25f, 12.0f, "%.2fs");
            CommandUiText(ui, "Hold Shift/LB for a 360-degree damage shell.",
                          COMMAND_TEXT_DIM);
        }
        else if (k->secondaryKind == SECONDARY_GRAPPLE)
        {
            CommandUiSliderF(ui, "Cooldown", &k->mobilityCooldown,
                             0.25f, 15.0f, "%.2fs");
            CommandUiSliderF(ui, "Pull duration", &k->mobilityDuration,
                             0.10f, 1.5f, "%.2fs");
            CommandUiSliderF(ui, "Max range", &k->secondaryRange,
                             2.0f, 24.0f, "%.1f");
            CommandUiSliderF(ui, "Launch delay", &k->secondaryDelay,
                             0.0f, 0.8f, "%.2fs");
            CommandUiText(ui, "Hold Shift/LB to preview; release to launch.",
                          COMMAND_TEXT_DIM);
        }
        else if (k->secondaryKind == SECONDARY_MINE)
        {
            CommandUiSliderF(ui, "Cooldown", &k->mobilityCooldown,
                             0.25f, 15.0f, "%.2fs");
            CommandUiSliderF(ui, "Arm time", &k->secondaryDelay,
                             0.05f, 3.0f, "%.2fs");
            CommandUiSliderF(ui, "Trigger radius",
                             &k->secondaryTriggerRadius,
                             0.5f, 8.0f, "%.1f");
            CommandUiSliderF(ui, "Blast radius", &k->secondaryRadius,
                             0.5f, 10.0f, "%.1f");
            CommandUiSliderI(ui, "Damage", &k->secondaryDamage,
                             20, 5000);
            CommandUiSliderF(ui, "Knockback", &k->secondaryKnockback,
                             0.0f, 12.0f, "%.1f");
            CommandUiText(ui, "Shift/LB replaces the owner's mine at their feet.",
                          COMMAND_TEXT_DIM);
        }
    }

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
        CommandUiSliderI(ui,
                         k->superKind == SUPER_RETURNING
                             ? "Damage / leg" : "Damage",
                         &k->sDamage, 50, 6000);
        if (k->superKind != SUPER_RETURNING)
        {
            CommandUiSliderI(ui, "Pellets", &k->sPellets, 1, 24);
            CommandUiSliderF(ui, "Spread", &k->sSpreadDeg,
                             0.0f, 90.0f, "%.0f deg");
        }
        CommandUiSliderF(ui, "Range", &k->sRange, 2.0f, 45.0f, "%.1f");
        if (k->superKind == SUPER_RETURNING)
        {
            CommandUiSliderF(ui, "Outbound speed", &k->sSpeed,
                             6.0f, 120.0f, "%.1f");
            CommandUiSliderF(ui, "Return speed", &k->sReturnSpeed,
                             6.0f, 120.0f, "%.1f");
            CommandUiSliderF(ui, "Outbound pull", &k->sOutboundPull,
                             0.0f, 8.0f, "%.2f");
            CommandUiSliderF(ui, "Return knockback", &k->sReturnKnockback,
                             0.0f, 8.0f, "%.2f");
        }
    }
    else
    {
        CommandUiSliderI(ui, "Impact damage", &k->sDamage, 50, 6000);
        CommandUiText(ui, "Dash length follows Dash speed on the PLAYER tab.", COMMAND_TEXT_DIM);
    }

    CommandUiSection(ui, "");
    int kitOverrides = ConfigKitOverrideCount(w, cls);
    CommandUiText(ui, TextFormat("%d local project-value changes", kitOverrides), COMMAND_TEXT_DIM);
    if (CommandUiButton(ui, "Reset kit + showcase to PROJECT"))
    {
        ConfigResetKitToProject(w, cls);
        GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SYNC_CLASS_HEALTH, cls });
    }
    if (CommandUiButton(ui, "SAVE KIT + SHOWCASE AS PROJECT DEFAULT"))
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
    CommandUiSliderF(ui, "Exposure", &t->styleExposure, 0.5f, 2.0f, "%.2f");
    CommandUiSliderF(ui, "Filmic tonemap", &t->styleTonemap, 0.0f, 1.0f, "%.2f");
    CommandUiSliderF(ui, "Bloom", &t->bloom, 0.0f, 3.0f, "%.2f");
    CommandUiSliderF(ui, "Vignette", &t->styleVignette, 0.0f, 1.5f, "%.2f");
    CommandUiSliderF(ui, "Space backdrop", &t->backdropBrightness, 0.0f, 2.0f, "%.2f");

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
        t->styleExposure = p->styleExposure;
        t->styleTonemap = p->styleTonemap;
        t->backdropBrightness = p->backdropBrightness;
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

    CommandUiSection(ui, "COMBAT RECOVERY");
    CommandUiSliderF(ui, "Regen delay", &t->healthRegenDelay, 0.0f, 10.0f, "%.2fs");
    CommandUiSliderF(ui, "Pulse interval", &t->healthRegenInterval,
                     0.05f, 5.0f, "%.2fs");
    CommandUiSliderF(ui, "Max health / pulse", &t->healthRegenRatio,
                     0.0f, 0.5f, "%.2f");
    CommandUiText(ui, "0.13 restores 13% max health. Zero disables regeneration.",
                  COMMAND_TEXT_DIM);

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
    CommandUiSliderF(ui, "Match camera distance", &t->matchCameraDistance,
                     20.0f, 60.0f, "%.1f");
    CommandUiSliderF(ui, "Bloom strength", &t->bloom, 0.0f, 3.0f, "%.2f");

    CommandUiSection(ui, "DEBUG");
    CommandUiToggle(ui, "Show ranges + sight", &t->showDebug);

    int projectiles = 0, particles = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) if (w->session.projectiles[i].active) projectiles++;
    for (int i = 0; i < MAX_PARTICLES; i++) if (w->presentation.particles[i].active) particles++;

    CommandUiText(ui, TextFormat("%d FPS   %d shots   %d particles", GetFPS(), projectiles, particles), COMMAND_TEXT_DIM);
    CommandUiText(
        ui,
        TextFormat("VFX atlases %d/%d   layers %d/%d   dropped %d",
                   RenderLoadedVfxAtlasCount(), VFX_ATLAS_COUNT,
                   VfxActiveCount(&w->presentation), MAX_VFX_INSTANCES,
                   w->presentation.droppedVfx),
        RenderLoadedVfxAtlasCount() == VFX_ATLAS_COUNT
            ? COMMAND_TEXT_DIM : COMMAND_WARN);
    const VfxRecipeDefinition *last =
        VfxCatalogGet(w->presentation.lastVfxEffect);
    CommandUiText(
        ui,
        TextFormat("Consumed %d events / spawned %d layers / last: %s",
                   w->presentation.vfxEventsConsumed,
                   w->presentation.vfxLayersSpawned,
                   last ? last->name : "none"),
        COMMAND_TEXT_DIM);

    CommandUiSection(ui, "VFX PREVIEW");
    if (g_command.vfxPreview <= VFX_NONE ||
        g_command.vfxPreview >= VFX_EFFECT_COUNT)
        g_command.vfxPreview = VFX_SCRAPPER_CAST;
    const VfxRecipeDefinition *preview =
        VfxCatalogGet((VfxEffectId)g_command.vfxPreview);
    CommandUiText(ui, preview ? preview->name : "invalid recipe",
                  COMMAND_TEXT_MAIN);
    if (CommandUiButton(ui, "Previous VFX"))
    {
        g_command.vfxPreview--;
        if (g_command.vfxPreview <= VFX_NONE)
            g_command.vfxPreview = VFX_EFFECT_COUNT - 1;
    }
    if (CommandUiButton(ui, "Next VFX"))
    {
        g_command.vfxPreview++;
        if (g_command.vfxPreview >= VFX_EFFECT_COUNT)
            g_command.vfxPreview = VFX_NONE + 1;
    }
    if (CommandUiButton(ui, "Spawn selected VFX"))
    {
        Brawler *player = &w->session.brawlers[w->session.playerIdx];
        Vector3 direction = { sinf(player->renderYaw), 0.0f,
                              cosf(player->renderYaw) };
        Vector3 at = Vector3Add(player->position,
                                Vector3Scale(direction, 3.0f));
        at.y = 0.85f;
        Vector3 end = Vector3Add(at, Vector3Scale(direction, 4.0f));
        const VfxRecipeDefinition *recipe =
            VfxCatalogGet((VfxEffectId)g_command.vfxPreview);
        int spawned = VfxSpawnPreview(
            &w->presentation, (VfxEffectId)g_command.vfxPreview,
            at, end, player->renderYaw,
            recipe ? recipe->defaultSize : 1.0f,
            (Color){ 96, 224, 255, 255 });
        w->presentation.vfxLayersSpawned += spawned;
        w->presentation.lastVfxEffect =
            (VfxEffectId)g_command.vfxPreview;
    }

    static const char *ACTION_NAMES[] = {
        "MAIN", "SUPER", "CAST", "MOBILITY", "GUARD", "GRAPPLE",
        "MINE DEPLOY"
    };
    CommandUiCycler(ui, "Character action", &g_command.actionPreview,
                    7, ACTION_NAMES);
    if (CommandUiButton(ui, "Play selected action"))
        CharacterActionStart(
            &w->presentation, w->session.playerIdx,
            (CharacterActionId)(g_command.actionPreview + CHARACTER_ACTION_MAIN));
    if (w->session.playerIdx >= 0 &&
        w->session.playerIdx < w->session.brawlerCount)
    {
        const CharacterActionState *active =
            &w->presentation.actions[w->session.playerIdx];
        CommandUiText(
            ui,
            TextFormat("Active: %s   %.0f%%   blend %.0f%%",
                       CharacterActionName(active->action),
                       CharacterActionProgress(active)*100.0f,
                       CharacterActionBlendWeight(active)*100.0f),
            active->active ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);
    }

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

static void TabPreview(CommandUi *ui)
{
    App *w = ui->world;
    BrawlerClass cls = w->session.brawlers[w->session.playerIdx].cls;
    CharacterShowcaseDefinition *showcase = &w->content.showcase;

    CommandUiSection(ui, "SHARED CHARACTER SHOWCASE");
    CommandUiText(ui, "One transform and camera for every model and menu screen.",
                  COMMAND_TEXT_MAIN);
    CommandUiText(ui, "Changing candidates never moves or restarts the stage.",
                  COMMAND_TEXT_DIM);
    CommandUiSliderF(ui, "Yaw", &showcase->yawDegrees,
                     -360.0f, 360.0f, "%.0f deg");
    CommandUiSliderF(ui, "Scale", &showcase->scale, 0.50f, 1.50f, "%.2f");
    CommandUiSliderF(ui, "Offset X", &showcase->offset.x, -8.0f, 8.0f, "%.2f");
    CommandUiSliderF(ui, "Offset Y", &showcase->offset.y, -4.0f, 4.0f, "%.2f");
    CommandUiSliderF(ui, "Camera X", &showcase->cameraPosition.x,
                     -20.0f, 20.0f, "%.2f");
    CommandUiSliderF(ui, "Camera Y", &showcase->cameraPosition.y,
                     0.20f, 12.0f, "%.2f");
    CommandUiSliderF(ui, "Camera Z", &showcase->cameraPosition.z,
                     -20.0f, -2.0f, "%.2f");
    CommandUiSliderF(ui, "Target X", &showcase->cameraTarget.x,
                     -8.0f, 8.0f, "%.2f");
    CommandUiSliderF(ui, "Target Y", &showcase->cameraTarget.y,
                     0.20f, 4.0f, "%.2f");
    CommandUiSliderF(ui, "Target Z", &showcase->cameraTarget.z,
                     -8.0f, 8.0f, "%.2f");
    CommandUiSliderF(ui, "Vertical FOV", &showcase->verticalFov,
                     20.0f, 80.0f, "%.0f deg");

    CommandUiSection(ui, "PERSONAL UI PROFILE");
    CommandUiText(ui, "These values save to profile.cfg, never gameplay.cfg.",
                  COMMAND_TEXT_DIM);
    CommandUiSliderF(ui, "UI scale", &w->uiPreferences.scale, 0.75f, 1.50f, "%.2fx");
    CommandUiToggle(ui, "Reduced motion", &w->uiPreferences.reducedMotion);
    CommandUiToggle(ui, "High contrast", &w->uiPreferences.highContrast);
    CommandUiToggle(ui, "Tutorial hints", &w->uiPreferences.showTutorialHints);
    static const char *GLYPH_NAMES[] = { "AUTO", "KEYBOARD", "GAMEPAD" };
    CommandUiCycler(ui, "Input glyphs", &w->uiPreferences.inputGlyphMode,
                    3, GLYPH_NAMES);
    if (CommandUiButton(ui, "Reset tutorial progress"))
    {
        w->uiPreferences.tutorialFlags = 0;
        ConfigMarkDirty();
    }

    CommandUiSection(ui, "PROJECT AUTHORING");
    CommandUiText(ui, TextFormat("%d kit/showcase values differ from PROJECT",
                                 ConfigKitOverrideCount(w, cls)),
                  ConfigKitOverrideCount(w, cls) > 0 ? COMMAND_WARN : COMMAND_TEXT_DIM);
    if (CommandUiButton(ui, "Reset kit + showcase to PROJECT"))
        ConfigResetKitToProject(w, cls);
    if (CommandUiButton(ui, "SAVE KIT + SHOWCASE AS PROJECT DEFAULT"))
        ConfigPromoteKit(w, cls);
}

//------------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------------
bool CommandCenterIsOpen(void) { return g_open; }

void CommandCenterForceOpen(void) { g_open = true; }

bool CommandCenterConsumeEscape(void)
{
    if (!g_open) return false;
    g_open = false;
    CommandUiResetInteraction();
    return true;
}

bool CommandCenterCapturesMouse(void)
{
    if (!g_open) return false;
    return CommandUiMouseIn(CommandPanelRect()) || CommandUiHasActiveSlider();
}

void CommandCenterUpdate(App *w)
{
    (void)w;
    if (IsKeyPressed(KEY_TAB)) g_open = !g_open;
    if (g_open && UiBackPressed()) g_open = false;
    if (g_open && (UiSystemActive()->previousPressed || UiSystemActive()->nextPressed))
    {
        int direction = UiSystemActive()->nextPressed ? 1 : -1;
        g_tab = (PanelTab)((g_tab + direction + TAB_COUNT)%TAB_COUNT);
    }
    if (!g_open) CommandUiResetInteraction();
}

void CommandCenterDraw(App *w)
{
    const UiTheme *theme = UiSystemActive()->theme;
    if (!g_open)
    {
        UiDrawTextAligned(UI_TEXT_CAPTION, "TAB  COMMAND CENTER",
                          UiRefRect(500, 772, 280, 24), UI_ALIGN_CENTER, theme->muted);
        return;
    }

    Rectangle panel = CommandPanelRect();
    UiDrawFeaturePanel(panel, COMMAND_PANEL_BG, COMMAND_PANEL_EDGE, true);
    UiDrawSignalRail(panel, COMMAND_WARN, false);

    float inset = UiScale(16);
    float headerHeight = UiScale(78);
    float footerHeight = UiScale(76);
    float railWidth = UiScale(126);

    UiDrawText(UI_TEXT_HEADING, "COMMAND CENTER",
               (Vector2){ panel.x + inset, panel.y + UiScale(10) }, COMMAND_TEXT_MAIN);
    UiDrawTextAligned(UI_TEXT_CAPTION, "TAB TO CLOSE",
                      (Rectangle){ panel.x + panel.width - UiScale(116),
                                   panel.y + UiScale(12), UiScale(98), UiScale(24) },
                      UI_ALIGN_RIGHT, COMMAND_TEXT_DIM);
    int projectChanges = ConfigProjectOverrideCount(w);
    char source[96];
    if (w->config.recoveryDefaults) snprintf(source, sizeof(source), "CONFIG RECOVERY MODE");
    else if (projectChanges > 0)
        snprintf(source, sizeof(source), "PROJECT + LOCAL DRAFT  //  %d CHANGED", projectChanges);
    else snprintf(source, sizeof(source), "PROJECT DEFAULTS  //  CLEAN");
    UiDrawText(UI_TEXT_CAPTION, source,
               (Vector2){ panel.x + inset, panel.y + UiScale(48) },
               w->config.recoveryDefaults ? COMMAND_WARN :
               (projectChanges > 0 ? theme->safety : theme->ally));
    DrawLineEx((Vector2){ panel.x + inset, panel.y + headerHeight },
               (Vector2){ panel.x + panel.width - inset, panel.y + headerHeight },
               UiScale(1), COMMAND_PANEL_EDGE);

    Rectangle rail = {
        panel.x + inset,
        panel.y + headerHeight + UiScale(10),
        railWidth,
        panel.height - headerHeight - footerHeight - UiScale(20)
    };
    UiDrawPanel(rail, theme->voidBg, theme->hull, false);
    UiDrawText(UI_TEXT_CAPTION, "GAMEPLAY",
               (Vector2){ rail.x + UiScale(10), rail.y + UiScale(10) }, theme->muted);

    float tabY = rail.y + UiScale(34);
    for (int i = 0; i < TAB_COUNT; i++)
    {
        if (i == TAB_KIT)
        {
            UiDrawText(UI_TEXT_CAPTION, "CONTENT",
                       (Vector2){ rail.x + UiScale(10), tabY + UiScale(2) }, theme->muted);
            tabY += UiScale(24);
        }
        if (i == TAB_STYLE)
        {
            UiDrawText(UI_TEXT_CAPTION, "PRESENTATION",
                       (Vector2){ rail.x + UiScale(10), tabY + UiScale(2) }, theme->muted);
            tabY += UiScale(24);
        }
        Rectangle r = { rail.x + UiScale(6), tabY, rail.width - UiScale(12), UiScale(38) };
        UiResponse response = UiInteract(UiHash(TAB_NAMES[i]), r, true);
        bool hover = response.hovered;
        bool active = (g_tab == (PanelTab)i);
        if (response.activated) g_tab = (PanelTab)i;
        DrawRectangleRec(r, active ? COMMAND_ACCENT_DIM :
                         (hover ? theme->deckRaised : theme->voidBg));
        if (active) UiDrawSignalRail(r, COMMAND_ACCENT, false);
        UiDrawTextFit(UI_TEXT_CAPTION, TAB_NAMES[i],
                      (Rectangle){ r.x + UiScale(12), r.y,
                                   r.width - UiScale(18), r.height },
                      UI_ALIGN_LEFT, active ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);
        if (response.focused && UiSystemActive()->focusVisible)
            DrawRectangleLinesEx(r, UiScale(1), COMMAND_TEXT_MAIN);
        tabY += UiScale(42);
    }

    int contentTop = (int)(panel.y + headerHeight + UiScale(10));
    int contentBottom = (int)(panel.y + panel.height - footerHeight - UiScale(8));
    Rectangle contentClip = {
        rail.x + rail.width + UiScale(12), (float)contentTop,
        panel.x + panel.width - inset - (rail.x + rail.width + UiScale(12)),
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
        .x = (int)contentClip.x + (int)UiScale(6),
        .y = contentTop - (int)g_scroll[g_tab],
        .width = (int)contentClip.width - (int)UiScale(14),
        .clip = contentClip
    };
    int logicalStart = ui.y;
    BrawlerClass activeClass = w->session.brawlers[w->session.playerIdx].cls;
    WeaponDef weaponBefore = w->content.weapons[activeClass];

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
        case TAB_PREVIEW: TabPreview(&ui); break;
        default: break;
    }
    EndScissorMode();
    if (memcmp(&weaponBefore, &w->content.weapons[activeClass],
               sizeof(weaponBefore)) != 0)
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

    Rectangle footer = {
        panel.x + inset,
        panel.y + panel.height - footerHeight + UiScale(8),
        panel.width - inset*2.0f,
        footerHeight - UiScale(16)
    };
    DrawLineEx((Vector2){ footer.x, footer.y - UiScale(6) },
               (Vector2){ footer.x + footer.width, footer.y - UiScale(6) },
               UiScale(1), COMMAND_PANEL_EDGE);
    UiResponse discard = UiButton(UiHash("command.discard"),
        (Rectangle){ footer.x, footer.y, footer.width*0.42f, footer.height },
        "RESTORE PROJECT", UI_BUTTON_DANGER, UI_ICON_BACK);
    UiResponse save = UiButton(UiHash("command.save"),
        (Rectangle){ footer.x + footer.width*0.44f, footer.y,
                     footer.width*0.56f, footer.height },
        "SAVE ALL TO PROJECT", UI_BUTTON_STANDARD, UI_ICON_NEXT);
    if (discard.activated)
    {
        ConfigResetAllToProject(w);
        for (int i = 0; i < CLASS_COUNT; i++)
            GameCommandExecute(w, (GameCommand){ GAME_COMMAND_SYNC_CLASS_HEALTH, i });
        w->matchRestartPending = true;
    }
    if (save.activated) ConfigPromoteAll(w);
}
