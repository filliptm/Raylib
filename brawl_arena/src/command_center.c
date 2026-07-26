/*******************************************************************************************
*   COMMAND CENTER
*
*   A live tuning panel. Everything gameplay reads at runtime lives in World.tune or the
*   WEAPONS[] table, so every control here takes effect on the next frame with no restart.
*
*   The widgets are a tiny immediate-mode set written against raylib's shape and text
*   calls, which keeps the project dependency-free.
********************************************************************************************/
#include "command_center.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "render.h"
#include "effects.h"
#include "config.h"
#include "raymath.h"
#include <stddef.h>
#include <math.h>

#define PANEL_X 16
#define PANEL_TOP 74
#define PANEL_W 372
#define ROW_H 27
#define LABEL_W 132
#define VALUE_W 62      // right-hand column reserved for a slider's numeric readout

typedef enum { TAB_BOTS = 0, TAB_PLAYER, TAB_KIT, TAB_WORLD, TAB_COUNT } PanelTab;

static const char *TAB_NAMES[TAB_COUNT] = { "BOTS", "PLAYER", "KIT", "WORLD" };

// Open on launch: this build is a sandbox, so the tuning panel is the point of entry.
static bool g_open = true;
static PanelTab g_tab = TAB_BOTS;
static const void *g_activeSlider = NULL;

// Palette
static const Color PANEL_BG     = { 14, 18, 28, 238 };
static const Color PANEL_EDGE   = { 60, 74, 98, 255 };
static const Color TEXT_MAIN    = { 226, 232, 242, 255 };
static const Color TEXT_DIM     = { 138, 150, 170, 255 };
static const Color ACCENT       = { 92, 178, 255, 255 };
static const Color ACCENT_DIM   = { 44, 88, 130, 255 };
static const Color TRACK_BG     = { 34, 40, 52, 255 };
static const Color WARN         = { 255, 176, 80, 255 };

typedef struct UI {
    World *world;
    int x, y, width;
} UI;

//------------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------------
static Rectangle PanelRect(void)
{
    return (Rectangle){ PANEL_X, PANEL_TOP, PANEL_W, GetScreenHeight() - PANEL_TOP - 16 };
}

static bool MouseIn(Rectangle r)
{
    Vector2 m = GetMousePosition();
    return (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
}

static void uiSection(UI *ui, const char *title)
{
    ui->y += 8;
    DrawText(title, ui->x, ui->y, 13, ACCENT);
    ui->y += 17;
    DrawRectangle(ui->x, ui->y, ui->width, 1, (Color){ 48, 60, 80, 255 });
    ui->y += 8;
}

static void uiText(UI *ui, const char *text, Color c)
{
    DrawText(text, ui->x, ui->y + 4, 13, c);
    ui->y += ROW_H - 6;
}

static bool uiButton(UI *ui, const char *label)
{
    Rectangle r = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool hover = MouseIn(r);
    bool click = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangleRounded(r, 0.28f, 6, hover ? ACCENT_DIM : (Color){ 34, 42, 56, 255 });
    DrawRectangleRoundedLines(r, 0.28f, 6, hover ? ACCENT : PANEL_EDGE);

    int tw = MeasureText(label, 13);
    DrawText(label, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + 5), 13, hover ? TEXT_MAIN : TEXT_DIM);

    ui->y += ROW_H;
    return click;
}

static bool uiToggle(UI *ui, const char *label, bool *value)
{
    Rectangle r = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool hover = MouseIn(r);
    bool click = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (click) { *value = !*value; ConfigMarkDirty(); }

    DrawText(label, ui->x, ui->y + 5, 13, hover ? TEXT_MAIN : TEXT_DIM);

    // Pill switch on the right
    float pw = 44, ph = 18;
    Rectangle pill = { ui->x + ui->width - pw, ui->y + 2, pw, ph };
    DrawRectangleRounded(pill, 0.9f, 8, *value ? ACCENT : TRACK_BG);

    float knobX = *value ? (pill.x + pw - ph + 2) : (pill.x + 2);
    DrawCircle((int)(knobX + (ph - 4) / 2), (int)(pill.y + ph / 2), (ph - 4) / 2,
               *value ? (Color){ 12, 20, 30, 255 } : TEXT_DIM);

    ui->y += ROW_H;
    return click;
}

// Shared slider body. Returns true while the value is being changed.
// Layout is label | track | value, all inside one row, so nothing overlaps its neighbours.
//
// `id` identifies which slider is being dragged and MUST be the address of the value
// being edited - those live in World.tune or WEAPONS[], so they are stable and unique.
// Keying on &norm instead looked fine but gave every slider the same stack address, so
// grabbing any one of them dragged all of them onto the same value.
static bool SliderTrack(UI *ui, const void *id, const char *label, float *norm, const char *valueText)
{
    Rectangle row = { ui->x, ui->y, ui->width, ROW_H - 5 };
    Rectangle track = { ui->x + LABEL_W, ui->y + 7, ui->width - LABEL_W - VALUE_W, 9 };

    bool hover = MouseIn(row);
    bool changed = false;

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_activeSlider = id;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && g_activeSlider == id) g_activeSlider = NULL;

    if (g_activeSlider == id)
    {
        float t = (GetMousePosition().x - track.x) / track.width;
        *norm = Clamp(t, 0.0f, 1.0f);
        changed = true;
        ConfigMarkDirty();
    }

    DrawText(label, ui->x, ui->y + 4, 13, hover ? TEXT_MAIN : TEXT_DIM);
    DrawRectangleRounded(track, 0.9f, 6, TRACK_BG);

    Rectangle fill = track;
    fill.width = track.width * (*norm);
    if (fill.width > 2) DrawRectangleRounded(fill, 0.9f, 6, hover ? ACCENT : ACCENT_DIM);

    DrawCircle((int)(track.x + track.width * (*norm)), (int)(track.y + track.height / 2), 7,
               hover ? TEXT_MAIN : ACCENT);

    int tw = MeasureText(valueText, 12);
    DrawText(valueText, ui->x + ui->width - tw, ui->y + 5, 12, WARN);

    ui->y += ROW_H;
    return changed;
}

static bool uiSliderF(UI *ui, const char *label, float *value, float lo, float hi, const char *fmt)
{
    float norm = (hi > lo) ? (*value - lo) / (hi - lo) : 0.0f;
    norm = Clamp(norm, 0.0f, 1.0f);

    bool changed = SliderTrack(ui, value, label, &norm, TextFormat(fmt, *value));
    if (changed) *value = lo + norm * (hi - lo);
    return changed;
}

static bool uiSliderI(UI *ui, const char *label, int *value, int lo, int hi)
{
    float norm = (hi > lo) ? (float)(*value - lo) / (float)(hi - lo) : 0.0f;
    norm = Clamp(norm, 0.0f, 1.0f);

    bool changed = SliderTrack(ui, value, label, &norm, TextFormat("%d", *value));
    if (changed) *value = lo + (int)(norm * (hi - lo) + 0.5f);
    return changed;
}

static bool uiCycler(UI *ui, const char *label, int *value, int count, const char **names)
{
    DrawText(label, ui->x, ui->y + 5, 13, TEXT_DIM);

    int boxW = ui->width - LABEL_W;
    Rectangle left  = { ui->x + LABEL_W, ui->y + 1, 24, ROW_H - 7 };
    Rectangle right = { ui->x + ui->width - 24, ui->y + 1, 24, ROW_H - 7 };
    bool changed = false;

    bool lh = MouseIn(left), rh = MouseIn(right);

    DrawRectangleRounded(left, 0.3f, 5, lh ? ACCENT_DIM : TRACK_BG);
    DrawRectangleRounded(right, 0.3f, 5, rh ? ACCENT_DIM : TRACK_BG);
    DrawText("<", (int)left.x + 9, (int)left.y + 3, 13, TEXT_MAIN);
    DrawText(">", (int)right.x + 9, (int)right.y + 3, 13, TEXT_MAIN);

    if (lh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *value = (*value - 1 + count) % count; changed = true; }
    if (rh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *value = (*value + 1) % count; changed = true; }
    if (changed) ConfigMarkDirty();

    const char *name = names[*value];
    int tw = MeasureText(name, 14);
    DrawText(name, (int)(left.x + boxW / 2 - tw / 2), ui->y + 4, 14, ACCENT);

    ui->y += ROW_H;
    return changed;
}

//------------------------------------------------------------------------------------
// Actions
//------------------------------------------------------------------------------------
static BrawlerClass BotKitFor(World *w, int slot)
{
    return w->tune.botMixedKits ? (BrawlerClass)(slot % CLASS_COUNT) : w->tune.botKit;
}

static void SetBotCount(World *w, int n)
{
    if (n < 0) n = 0;
    if (n > MAX_BRAWLERS - 1) n = MAX_BRAWLERS - 1;

    int current = w->brawlerCount - 1;
    if (n == current) return;

    if (n > current)
    {
        for (int slot = current; slot < n; slot++)
        {
            Vector3 pos = w->arena.enemySpawns[slot % w->arena.enemySpawnCount];
            BrawlerSpawn(w, 1 + slot, TEAM_ENEMY, BotKitFor(w, slot), pos, false);
        }
    }
    else
    {
        // Drop any shots still owned by bots that no longer exist.
        for (int i = 0; i < MAX_PROJECTILES; i++)
            if (w->projectiles[i].active && w->projectiles[i].owner > n)
                w->projectiles[i].active = false;
    }

    w->brawlerCount = n + 1;
}

static void RespawnAllBots(World *w)
{
    for (int i = 1; i < w->brawlerCount; i++)
    {
        Vector3 pos = w->arena.enemySpawns[(i - 1) % w->arena.enemySpawnCount];
        BrawlerSpawn(w, i, TEAM_ENEMY, BotKitFor(w, i - 1), pos, false);
    }
}

// Health lives on the brawler, so editing a kit's health has to reach the living copies.
static void SyncMaxHealth(World *w, BrawlerClass cls)
{
    int target = WEAPONS[cls].maxHealth;
    if (target < 1) target = 1;

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (b->cls != cls || b->maxHealth == target) continue;

        float ratio = (b->maxHealth > 0) ? (float)b->health / (float)b->maxHealth : 1.0f;
        b->maxHealth = target;
        b->health = (int)(target * ratio);
        if (b->health < 1) b->health = 1;
    }
}

//------------------------------------------------------------------------------------
// Tabs
//------------------------------------------------------------------------------------
static void TabBots(UI *ui)
{
    World *w = ui->world;
    Tuning *t = &w->tune;

    uiSection(ui, "BEHAVIOUR");

    int mode = (int)t->botMode;
    if (uiCycler(ui, "Mode", &mode, BOT_MODE_COUNT, BOT_MODE_NAMES))
        t->botMode = (BotMode)mode;

    const char *hint =
        (t->botMode == BOT_STATIC) ? "Inert targets. They will not move or shoot." :
        (t->botMode == BOT_ROAM)   ? "They wander the arena but never open fire." :
                                     "Full combat AI: chase, strafe, shoot, retreat.";
    uiText(ui, hint, TEXT_DIM);

    uiSection(ui, "ROSTER");

    // Bound straight to the tuning field: a local would reintroduce the shared-address
    // problem and would not survive a save.
    if (uiSliderI(ui, "Bot count", &t->botCount, 0, MAX_BRAWLERS - 1))
        SetBotCount(w, t->botCount);

    if (uiToggle(ui, "Mixed kits", &t->botMixedKits)) RespawnAllBots(w);

    if (!t->botMixedKits)
    {
        int kit = (int)t->botKit;
        if (uiCycler(ui, "Bot kit", &kit, CLASS_COUNT, CLASS_NAMES))
        {
            t->botKit = (BrawlerClass)kit;
            RespawnAllBots(w);
        }
    }

    uiSliderF(ui, "Respawn delay", &t->enemyRespawn, 0.5f, 15.0f, "%.1fs");

    uiSection(ui, "ACTIONS");
    if (uiButton(ui, "Respawn all bots")) RespawnAllBots(w);

    if (uiButton(ui, "Kill all bots"))
    {
        for (int i = 1; i < w->brawlerCount; i++)
            if (w->brawlers[i].alive)
                BrawlerApplyDamage(w, i, w->brawlers[i].health, w->playerIdx, w->brawlers[i].position);
    }

    if (uiButton(ui, "Heal all bots"))
        for (int i = 1; i < w->brawlerCount; i++)
            w->brawlers[i].health = w->brawlers[i].maxHealth;
}

static void TabPlayer(UI *ui)
{
    World *w = ui->world;
    Tuning *t = &w->tune;
    Brawler *p = &w->brawlers[w->playerIdx];

    uiSection(ui, "KIT");
    int kit = (int)p->cls;
    if (uiCycler(ui, "Active kit", &kit, CLASS_COUNT, CLASS_NAMES))
    {
        Vector3 pos = p->alive ? p->position : w->arena.playerSpawn;
        float keep = p->superCharge;
        BrawlerSpawn(w, w->playerIdx, TEAM_PLAYER, (BrawlerClass)kit, pos, true);
        w->brawlers[w->playerIdx].superCharge = keep;
    }

    uiSection(ui, "SANDBOX");
    uiToggle(ui, "God mode", &t->godMode);
    uiToggle(ui, "Infinite ammo", &t->infiniteAmmo);

    uiSection(ui, "MOVEMENT");
    uiSliderF(ui, "Move speed", &t->moveSpeed, 2.0f, 30.0f, "%.1f");
    uiSliderF(ui, "Acceleration", &t->moveAccel, 4.0f, 80.0f, "%.0f");
    uiSliderF(ui, "Dash speed", &t->dashSpeed, 8.0f, 60.0f, "%.0f");
    uiSliderF(ui, "Respawn delay", &t->playerRespawn, 0.5f, 15.0f, "%.1fs");

    uiSection(ui, "ACTIONS");
    if (uiButton(ui, "Charge super")) p->superCharge = 1.0f;
    if (uiButton(ui, "Heal to full")) p->health = p->maxHealth;
    if (uiButton(ui, "Respawn player")) BrawlerRespawn(w, w->playerIdx);
}

static void TabKit(UI *ui)
{
    World *w = ui->world;
    BrawlerClass cls = w->brawlers[w->playerIdx].cls;
    WeaponDef *k = &WEAPONS[cls];

    uiSection(ui, "EDITING");
    uiText(ui, TextFormat("%s  -  %s", k->name, k->flavor), TEXT_MAIN);

    uiSection(ui, "MAIN ATTACK");
    if (uiSliderI(ui, "Max health", &k->maxHealth, 500, 12000)) SyncMaxHealth(w, cls);
    uiSliderI(ui, "Damage / pellet", &k->damage, 20, 4000);
    uiSliderI(ui, "Pellets", &k->pellets, 1, 20);
    uiSliderF(ui, "Spread", &k->spreadDeg, 0.0f, 90.0f, "%.0f deg");
    uiSliderF(ui, "Projectile speed", &k->speed, 6.0f, 90.0f, "%.0f");
    uiSliderF(ui, "Range", &k->range, 2.0f, 40.0f, "%.1f");
    uiSliderF(ui, "Shot size", &k->projRadius, 0.05f, 4.0f, "%.2f");
    uiSliderF(ui, "Cooldown", &k->cooldown, 0.05f, 2.0f, "%.2fs");
    uiSliderF(ui, "Reload / ammo", &k->reloadPerAmmo, 0.15f, 5.0f, "%.2fs");
    uiSliderF(ui, "Super per hit", &k->superPerHit, 0.0f, 1.0f, "%.0f%%");

    uiSection(ui, "SUPER");
    uiText(ui, k->superName, TEXT_MAIN);
    if (!k->sDash)
    {
        uiSliderI(ui, "Damage", &k->sDamage, 50, 6000);
        uiSliderI(ui, "Pellets", &k->sPellets, 1, 24);
        uiSliderF(ui, "Spread", &k->sSpreadDeg, 0.0f, 90.0f, "%.0f deg");
        uiSliderF(ui, "Range", &k->sRange, 2.0f, 45.0f, "%.1f");
    }
    else
    {
        uiSliderI(ui, "Impact damage", &k->sDamage, 50, 6000);
        uiText(ui, "Dash length follows Dash speed on the PLAYER tab.", TEXT_DIM);
    }

    uiSection(ui, "");
    if (uiButton(ui, "Reset this kit"))
    {
        WeaponsResetKit(cls);
        SyncMaxHealth(w, cls);
        ConfigMarkDirty();
    }
}

static void TabWorld(UI *ui)
{
    World *w = ui->world;
    Tuning *t = &w->tune;

    uiSection(ui, "PACE");
    uiSliderF(ui, "Time scale", &t->timeScale, 0.05f, 2.0f, "%.2fx");
    uiSliderF(ui, "Super gain mult", &t->superMult, 0.0f, 6.0f, "%.2fx");

    uiSection(ui, "STEALTH");
    uiSliderF(ui, "Bush reveal range", &t->bushReveal, 0.0f, 14.0f, "%.1f");
    uiSliderF(ui, "Reveal on fire", &t->fireReveal, 0.0f, 6.0f, "%.1fs");
    uiSliderF(ui, "Conceal ghosting", &t->concealDither, 0.0f, 0.95f, "%.2f");

    uiSection(ui, "GRASS");
    uiSliderF(ui, "Height", &t->grassHeight, 0.2f, 4.0f, "%.2f");
    uiSliderF(ui, "Wind strength", &t->windStrength, 0.0f, 1.5f, "%.2f");
    uiSliderF(ui, "Wind speed", &t->windSpeed, 0.0f, 6.0f, "%.2f");
    uiSliderF(ui, "Bend radius", &t->grassBendRadius, 0.2f, 6.0f, "%.2f");
    uiSliderF(ui, "Bend strength", &t->grassBendStrength, 0.0f, 4.0f, "%.2f");

    uiSection(ui, "LOOK");
    uiToggle(ui, "Bloom + vignette", &t->postFx);
    uiSliderF(ui, "Bloom strength", &t->bloom, 0.0f, 3.0f, "%.2f");

    uiSection(ui, "DEBUG");
    uiToggle(ui, "Show ranges + sight", &t->showDebug);

    int projectiles = 0, particles = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) if (w->projectiles[i].active) projectiles++;
    for (int i = 0; i < MAX_PARTICLES; i++) if (w->particles[i].active) particles++;

    uiText(ui, TextFormat("%d FPS   %d shots   %d particles", GetFPS(), projectiles, particles), TEXT_DIM);

    uiSection(ui, "ACTIONS");
    if (uiButton(ui, "Rebuild arena")) { ArenaLoad(&w->arena); RenderBuildGrass(w); }

    if (uiButton(ui, "Reset score")) { w->kills = 0; w->deaths = 0; }

    if (uiButton(ui, "Reset ALL tuning"))
    {
        TuningSetDefaults(t);
        WeaponsResetAll();
        for (int i = 0; i < CLASS_COUNT; i++) SyncMaxHealth(w, (BrawlerClass)i);
        SetBotCount(w, t->botCount);
        ConfigMarkDirty();
    }
}

//------------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------------
bool CommandCenterIsOpen(void) { return g_open; }

bool CommandCenterCapturesMouse(void)
{
    if (!g_open) return false;
    return MouseIn(PanelRect()) || g_activeSlider != NULL;
}

void CommandCenterUpdate(World *w)
{
    (void)w;
    if (IsKeyPressed(KEY_TAB)) g_open = !g_open;
    if (!g_open) g_activeSlider = NULL;
}

void CommandCenterDraw(World *w)
{
    if (!g_open)
    {
        // Centered, so it clears the kit panel on the left and the super meter on the right.
        const char *hint = "TAB  command center";
        int tw = MeasureText(hint, 14);
        DrawText(hint, GetScreenWidth() / 2 - tw / 2, GetScreenHeight() - 26, 14, (Color){ 120, 132, 152, 200 });
        return;
    }

    Rectangle panel = PanelRect();
    DrawRectangleRounded(panel, 0.02f, 8, PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.02f, 8, PANEL_EDGE);

    // Title
    DrawText("COMMAND CENTER", (int)panel.x + 16, (int)panel.y + 12, 18, TEXT_MAIN);
    DrawText("TAB to close", (int)(panel.x + panel.width - 92), (int)panel.y + 17, 12, TEXT_DIM);
    DrawText("auto-saved to tuning.cfg", (int)panel.x + 16, (int)panel.y + 32, 11, (Color){ 96, 108, 128, 255 });

    // Tabs
    int tabY = (int)panel.y + 50;
    int tabW = (int)(panel.width - 32) / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++)
    {
        Rectangle r = { panel.x + 16 + i * tabW, tabY, tabW - 4, 26 };
        bool hover = MouseIn(r);
        bool active = (g_tab == (PanelTab)i);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_tab = (PanelTab)i;

        DrawRectangleRounded(r, 0.3f, 6, active ? ACCENT_DIM : (hover ? (Color){ 30, 38, 50, 255 } : TRACK_BG));
        int tw = MeasureText(TAB_NAMES[i], 13);
        DrawText(TAB_NAMES[i], (int)(r.x + r.width / 2 - tw / 2), (int)r.y + 6, 13,
                 active ? TEXT_MAIN : TEXT_DIM);

        if (active) DrawRectangle((int)r.x, (int)(r.y + r.height - 2), (int)r.width, 2, ACCENT);
    }

    UI ui = { .world = w, .x = (int)panel.x + 16, .y = tabY + 34, .width = (int)panel.width - 32 };

    switch (g_tab)
    {
        case TAB_BOTS:   TabBots(&ui); break;
        case TAB_PLAYER: TabPlayer(&ui); break;
        case TAB_KIT:    TabKit(&ui); break;
        case TAB_WORLD:  TabWorld(&ui); break;
        default: break;
    }
}
