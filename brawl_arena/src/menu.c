/*******************************************************************************************
*   MENU
*
*   The shell around the match: a main screen with the selected brawler on a podium and
*   everything else arranged around the edges, plus a character select built on the same
*   podium scene so switching between them costs nothing.
*
*   Every card here does something real. Nothing is a placeholder, because a menu full of
*   dead buttons teaches you to stop clicking things.
********************************************************************************************/
#include "menu.h"
#include "render.h"
#include "weapons.h"
#include "effects.h"
#include "config.h"
#include "command_center.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

#define FADE_SPEED 3.6f

//------------------------------------------------------------------------------------
// Palette
//------------------------------------------------------------------------------------
static const Color BG_TOP      = {  22,  30,  52, 255 };
static const Color BG_BOTTOM   = {  10,  14,  26, 255 };
static const Color CARD_BG     = {  26,  34,  54, 232 };
static const Color CARD_EDGE   = {  58,  76, 110, 255 };
static const Color CARD_HOVER  = {  40,  58,  92, 245 };
static const Color TEXT_MAIN   = { 232, 240, 252, 255 };
static const Color TEXT_DIM    = { 132, 148, 176, 255 };
static const Color ACCENT      = {  92, 178, 255, 255 };
static const Color PLAY_GOLD   = { 255, 196,  62, 255 };
static const Color PLAY_GOLD_D = { 196, 140,  30, 255 };

static Assets *g_assets = NULL;
static Camera3D g_podium;
static Brawler g_preview;
static float g_spin = 0.0f;
static float g_time = 0.0f;
static int g_hoverKit = -1;
static bool g_showControls = false;
static bool g_blockCards = false;

// Character select: the roster scrolls forever by wrapping, so there is never a hard
// stop at either end of a short list.
#define ENTRY_H 172
#define SELECT_MODEL_X 3.1f     // world offset that puts the podium on the screen's left

static float g_scroll = 0.0f;
static bool g_dragging = false;
static bool g_dragMoved = false;
static float g_dragStartY = 0.0f;
static float g_dragStartScroll = 0.0f;

// One accent per kit, used for the podium model and the select cards so the two agree.
static const Color KIT_ACCENT[CLASS_COUNT] = {
    {  74, 142, 236, 255 },     // SCRAPPER
    { 104, 200, 255, 255 },     // LONGSHOT
    { 172, 118, 250, 255 },     // MORTAR
    { 250, 146,  84, 255 },     // TANK
    {  70, 244, 166, 255 }      // GUARDIAN
};

// The podium brawler is rebuilt whenever the selection changes.
static BrawlerClass g_previewKit = CLASS_COUNT;

//------------------------------------------------------------------------------------
// Small widget layer. Separate from the command center's on purpose: this one is chunky
// and animated, that one is dense and precise, and merging them would ruin both.
//------------------------------------------------------------------------------------
typedef struct CardStyle {
    Color fill, edge, text;
    int fontSize;
    bool primary;
} CardStyle;

static bool MouseIn(Rectangle r)
{
    Vector2 m = GetMousePosition();
    return (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
}

static void DrawLabel(const char *text, int cx, int y, int size, Color c)
{
    int tw = MeasureText(text, size);
    DrawText(text, cx - tw/2 + 2, y + 2, size, (Color){ 0, 0, 0, 170 });
    DrawText(text, cx - tw/2, y, size, c);
}

// Returns true on click. Hovering lifts the card slightly, which is the cheapest way to
// make a flat interface feel responsive.
static bool Card(World *w, Rectangle r, const char *title, const char *sub,
                 CardStyle style, int badge)
{
    (void)w;
    bool hover = MouseIn(r) && !g_blockCards;
    bool click = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool held = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    Rectangle draw = r;
    if (hover) { draw.y -= 3.0f; draw.height += 1.0f; }
    if (held) draw.y += 4.0f;

    // Drop shadow anchors the card to the background.
    DrawRectangleRounded((Rectangle){ draw.x + 3, draw.y + 5, draw.width, draw.height },
                         0.22f, 8, (Color){ 0, 0, 0, 120 });

    Color fill = hover ? (style.primary ? PLAY_GOLD : CARD_HOVER) : style.fill;
    DrawRectangleRounded(draw, 0.22f, 8, fill);
    DrawRectangleRoundedLines(draw, 0.22f, 8, hover ? ACCENT : style.edge);

    if (style.primary)
    {
        // A lit top edge so the primary action reads as raised.
        DrawRectangleRounded((Rectangle){ draw.x + 6, draw.y + 4, draw.width - 12, draw.height*0.34f },
                             0.5f, 6, (Color){ 255, 255, 255, hover ? 60 : 38 });
    }

    int cx = (int)(draw.x + draw.width/2);
    int titleY = sub ? (int)(draw.y + draw.height/2 - style.fontSize) : (int)(draw.y + draw.height/2 - style.fontSize/2);
    DrawLabel(title, cx, titleY, style.fontSize, style.primary ? (Color){ 40, 26, 4, 255 } : style.text);
    if (sub) DrawLabel(sub, cx, titleY + style.fontSize + 4, 13, style.primary ? (Color){ 90, 62, 12, 255 } : TEXT_DIM);

    if (badge > 0)
    {
        DrawCircle((int)(draw.x + draw.width - 6), (int)draw.y + 6, 11.0f, (Color){ 232, 72, 72, 255 });
        DrawLabel(TextFormat("%d", badge), (int)(draw.x + draw.width - 6), (int)draw.y - 1, 15, WHITE);
    }
    return click;
}

static CardStyle StyleNormal(void)
{
    return (CardStyle){ CARD_BG, CARD_EDGE, TEXT_MAIN, 17, false };
}

static CardStyle StylePrimary(void)
{
    return (CardStyle){ PLAY_GOLD_D, PLAY_GOLD, (Color){ 40, 26, 4, 255 }, 34, true };
}

//------------------------------------------------------------------------------------
// Podium scene
//------------------------------------------------------------------------------------
// ESC has to close an open overlay before it means "back", or dismissing the controls
// panel would also quit the game.
bool MenuConsumeEscape(void)
{
    if (!g_showControls) return false;
    g_showControls = false;
    return true;
}

void MenuInit(Assets *a)
{
    g_assets = a;

    // Framed so the brawler is about half the screen height, leaving room above for the
    // name badge and below for the mode and play cards.
    g_podium.position = (Vector3){ 0.0f, 2.7f, -7.6f };
    g_podium.target = (Vector3){ 0.0f, 1.30f, 0.0f };
    g_podium.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    g_podium.fovy = 40.0f;
    g_podium.projection = CAMERA_PERSPECTIVE;
}

static void RebuildPreview(World *w)
{
    BrawlerClass kit = (BrawlerClass)Clamp((float)w->tune.selectedKit, 0.0f, CLASS_COUNT - 1);

    g_preview = (Brawler){ 0 };
    g_preview.team = TEAM_PLAYER;
    g_preview.cls = kit;
    g_preview.isPlayer = true;
    g_preview.alive = true;
    g_preview.visible = true;
    g_preview.spawnScale = 1.0f;
    g_preview.maxHealth = WEAPONS[kit].maxHealth;
    g_preview.health = g_preview.maxHealth;
    g_preview.position = (Vector3){ 0.0f, 0.0f, 0.0f };
    g_previewKit = kit;
}

void MenuUpdate(World *w, float dt)
{
    g_time += dt;
    g_spin += dt*0.45f;

    if (g_previewKit != (BrawlerClass)w->tune.selectedKit) RebuildPreview(w);

    // Select puts the model on the left and the roster on the right; the main menu
    // keeps it centred.
    g_preview.position.x = (w->screen == SCREEN_BRAWLERS) ? SELECT_MODEL_X : 0.0f;

    // Face the camera, with a slow sway either side so the silhouette is never static.
    // The camera looks down +Z, so PI turns the model around to meet it.
    g_preview.bobPhase += dt*3.4f;
    g_preview.renderYaw = PI + sinf(g_spin)*0.55f;
    g_preview.aimAngle = g_preview.renderYaw;
}

static void DrawPodiumScene(World *w)
{
    Assets *a = g_assets;
    if (!a) return;

    // Two warm key lights either side of the podium, so the model has shape against the
    // flat background rather than reading as a silhouette.
    Vector3 lightPos[2] = { { -2.6f, 3.0f, -2.4f }, { 2.8f, 2.4f, 1.6f } };
    Vector3 lightCol[2] = { { 0.85f, 0.72f, 0.45f }, { 0.32f, 0.48f, 0.80f } };
    (void)lightCol;
    AssetsSetCamera(a, g_podium.position);
    AssetsSetToon(a, w->tune.toon, w->tune.toonBands);
    AssetsSetLights(a, lightPos, lightCol, 2);

    Camera3D cam = g_podium;
    if (w->screen == SCREEN_BRAWLERS)
    {
        cam.position.z -= 0.4f;     // a touch closer, the model is the left-hand feature
    }

    BeginMode3D(cam);

        // Platform: a low disc, deliberately smaller than the model so it frames the
        // brawler instead of competing with it.
        float px = g_preview.position.x;

        Matrix base = MatrixMultiply(MatrixScale(1.45f, 0.22f, 1.45f), MatrixTranslate(px, -0.22f, 0));
        DrawLit(a, a->cylinder, base, a->texMetal, (Color){ 84, 96, 124, 255 }, (Vector2){ 1, 1 }, 0.0f);

        Matrix lip = MatrixMultiply(MatrixScale(1.58f, 0.07f, 1.58f), MatrixTranslate(px, -0.07f, 0));
        DrawLit(a, a->cylinder, lip, a->texMetal, (Color){ 142, 158, 192, 255 }, (Vector2){ 1, 1 }, 0.0f);

        Matrix glow = MatrixMultiply(MatrixScale(1.30f, 0.02f, 1.30f), MatrixTranslate(px, 0.02f, 0));
        float pulse = 0.5f + 0.5f*sinf(g_time*1.6f);
        Color teamGlow = TEAM_COLORS[TEAM_PLAYER];
        teamGlow.a = (unsigned char)(70 + pulse*60);
        DrawLit(a, a->cylinder, glow, a->texGlow, teamGlow, (Vector2){ 1, 1 }, 1.0f);

        Color accent = KIT_ACCENT[g_preview.cls];

        RiggedCharacter *character = &a->characters[g_preview.cls];
        if (character->ok && w->tune.modelCharacter)
        {
            // Frames advance at 60Hz, the rate the source clips were sampled at. Tint
            // stays white so the model's own texture reads - the kit accent lives on
            // the podium ring instead.
            AssetsDrawCharacter(a, g_preview.cls, g_preview.position, g_preview.renderYaw, 1.0f,
                                character->clipIdle, g_time*60.0f, true, WHITE, 0.0f, 0.0f,
                                lightPos, lightCol, 2, cam.position);
        }
        else RenderBrawlerModel(a, &g_preview, g_time, 0.0f, &accent);

    EndMode3D();
}

static void DrawBackdrop(void)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangleGradientV(0, 0, sw, sh, BG_TOP, BG_BOTTOM);

    // A faint vignette focuses attention on the podium.
    DrawRectangleGradientH(0, 0, sw/4, sh, (Color){ 0, 0, 0, 90 }, (Color){ 0, 0, 0, 0 });
    DrawRectangleGradientH(sw - sw/4, 0, sw/4, sh, (Color){ 0, 0, 0, 0 }, (Color){ 0, 0, 0, 90 });
}

//------------------------------------------------------------------------------------
// Main menu
//------------------------------------------------------------------------------------
static void DrawMainMenu(World *w)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Tuning *t = &w->tune;
    const WeaponDef *kit = &WEAPONS[g_preview.cls];

    //--- Top left: profile, built from numbers actually earned -------------------
    DrawRectangleRounded((Rectangle){ 18, 16, 236, 58 }, 0.2f, 8, CARD_BG);
    DrawRectangleRoundedLines((Rectangle){ 18, 16, 236, 58 }, 0.2f, 8, CARD_EDGE);
    DrawRectangleRounded((Rectangle){ 27, 25, 40, 40 }, 0.28f, 6, TEAM_COLORS[TEAM_PLAYER]);
    DrawText("BRAWLER", 78, 25, 17, TEXT_MAIN);
    DrawText(TextFormat("%d W   %d L   %d KO", t->statWins, t->statLosses, t->statKos),
             78, 47, 13, TEXT_DIM);

    //--- Top right: settings and quit -------------------------------------------
    // The single entry to the practice range: static targets to shoot at with every
    // parameter to hand, without touching the mode PLAY will use. TAB hides the panel
    // if you just want the range.
    if (Card(w, (Rectangle){ sw - 250, 16, 108, 46 }, "PRACTICE", NULL, StyleNormal(), 0))
    {
        w->sandbox = true;
        w->matchRestartPending = true;
        ShellRequestScreen(w, SCREEN_MATCH);
        CommandCenterForceOpen();
    }
    if (Card(w, (Rectangle){ sw - 132, 16, 108, 46 }, "QUIT", NULL, StyleNormal(), 0))
        w->quitRequested = true;

    //--- Centre top: the selected kit's name badge -------------------------------
    int badgeW = 300;
    DrawRectangleRounded((Rectangle){ sw/2 - badgeW/2, 88, badgeW, 54 }, 0.3f, 8, CARD_BG);
    DrawRectangleRoundedLines((Rectangle){ sw/2 - badgeW/2, 88, badgeW, 54 }, 0.3f, 8, CARD_EDGE);
    DrawLabel(kit->name, sw/2, 95, 26, TEXT_MAIN);
    DrawLabel(kit->flavor, sw/2, 122, 13, TEXT_DIM);

    //--- Left column -------------------------------------------------------------
    if (Card(w, (Rectangle){ 24, 232, 150, 74 }, "BRAWLERS", "choose a kit", StyleNormal(), 0))
        ShellRequestScreen(w, SCREEN_BRAWLERS);

    if (Card(w, (Rectangle){ 24, 320, 150, 74 }, "CONTROLS", "how to play", StyleNormal(), 0))
        g_showControls = true;

    //--- Right column: kit stats at a glance -------------------------------------
    int rx = sw - 214;
    DrawRectangleRounded((Rectangle){ rx, 232, 190, 178 }, 0.14f, 8, CARD_BG);
    DrawRectangleRoundedLines((Rectangle){ rx, 232, 190, 178 }, 0.14f, 8, CARD_EDGE);
    DrawText("KIT", rx + 16, 244, 13, ACCENT);

    // Drawn one at a time, for the TextFormat buffer reason noted in DrawRosterEntry.
    DrawText(TextFormat("HEALTH      %d", kit->maxHealth), rx + 16, 272, 14, TEXT_MAIN);
    if (kit->mainKind == ATTACK_RAIN)
        DrawText(TextFormat("PULSE D/H   %d / %d", kit->damage, kit->healing),
                 rx + 16, 298, 14, TEXT_MAIN);
    else if (kit->healing > 0)
        DrawText(TextFormat("DMG / HEAL  %d / %d", kit->damage*kit->pellets,
                            kit->healing*kit->pellets), rx + 16, 298, 14, TEXT_MAIN);
    else
        DrawText(TextFormat("DAMAGE      %d", kit->damage*kit->pellets), rx + 16, 298, 14, TEXT_MAIN);
    DrawText(TextFormat("RANGE       %.0f", kit->range), rx + 16, 324, 14, TEXT_MAIN);
    DrawText(TextFormat("RELOAD      %.2fs", kit->reloadPerAmmo), rx + 16, 350, 14, TEXT_MAIN);

    DrawText("SUPER", rx + 16, 380, 13, ACCENT);
    DrawText(kit->superName, rx + 66, 379, 15, PLAY_GOLD);

    //--- Bottom centre: the mode card, which toggles the rules --------------------
    Rectangle modeCard = { sw/2 - 250, sh - 104, 340, 78 };
    bool gg = t->gemGrab;
    if (Card(w, modeCard, gg ? "GEM GRAB" : "SANDBOX",
             gg ? TextFormat("%dv%d   first to %d", t->teamSize, t->teamSize, t->gemsToWin)
                : "free-form, no objective",
             StyleNormal(), 0))
    {
        t->gemGrab = !t->gemGrab;
        ConfigMarkDirty();
    }
    DrawText("MODE  -  click to change", (int)modeCard.x + 6, (int)modeCard.y - 17, 12, TEXT_DIM);

    //--- Bottom right: play ------------------------------------------------------
    if (Card(w, (Rectangle){ sw/2 + 106, sh - 104, 200, 78 }, "PLAY", NULL, StylePrimary(), 0))
    {
        w->sandbox = false;              // a real match, whatever the sandbox was doing
        w->matchRestartPending = true;
        w->matchResultBanked = false;
        ShellRequestScreen(w, SCREEN_MATCH);
    }
}

static void DrawControlsModal(World *w)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 195 });

    Rectangle panel = { sw/2.0f - 270, sh/2.0f - 205, 540, 400 };
    DrawRectangleRounded(panel, 0.07f, 8, CARD_BG);
    DrawRectangleRoundedLines(panel, 0.07f, 8, CARD_EDGE);
    DrawLabel("CONTROLS", sw/2, (int)panel.y + 20, 25, TEXT_MAIN);

    static const char *KEYS[] = {
        "WASD / arrows", "Hold LMB", "Release LMB", "Tap LMB or SPACE",
        "RMB", "1 - 5", "TAB", "R", "ESC"
    };
    static const char *WHAT[] = {
        "Move", "Aim - draws the shot on the ground", "Fire along the preview",
        "Quick shot, auto-aimed at the nearest enemy", "Super, once charged",
        "Swap kit on the spot", "Command center", "Restart the match",
        "Back a screen, or quit from here"
    };

    int rows = (int)(sizeof(KEYS)/sizeof(KEYS[0]));
    for (int i = 0; i < rows; i++)
    {
        int y = (int)panel.y + 66 + i*32;
        DrawText(KEYS[i], (int)panel.x + 26, y, 15, ACCENT);
        DrawText(WHAT[i], (int)panel.x + 205, y, 15, TEXT_MAIN);
    }

    Rectangle close = { panel.x + panel.width/2 - 60, panel.y + panel.height - 52, 120, 38 };
    if (Card(w, close, "CLOSE", NULL, StyleNormal(), 0)) g_showControls = false;

    // Clicking anywhere off the panel dismisses it too.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !MouseIn(panel)) g_showControls = false;
}

//------------------------------------------------------------------------------------
// Character select
//------------------------------------------------------------------------------------
// Attack and super descriptions are derived from the weapon data rather than written
// out, so they stay true after the numbers are tuned in the command center.
static const char *AttackSummary(const WeaponDef *k)
{
    if (k->mainKind == ATTACK_RAIN)
        return TextFormat("Growing rain, %d damage/healing per pulse", k->damage);
    if (k->healing > 0) return TextFormat("%d damage to foes, %d healing to allies",
                                          k->damage, k->healing);
    if (k->mainKind == ATTACK_LOB)
        return TextFormat("Arcing lob, %.1f splash, clears walls", k->projRadius);
    if (k->rangeScaled) return "Single shot, damage grows with distance";
    if (k->pellets > 1) return TextFormat("%d pellets, %.0f degree spread", k->pellets, k->spreadDeg);
    return "Single shot";
}

static const char *SuperSummary(const WeaponDef *k)
{
    if (k->superKind == SUPER_SOUND_WAVE)
        return TextFormat("Wide cone: %d damage, %d healing per tick",
                          k->sDamage, k->sHealing);
    if (k->superKind == SUPER_HEALING_BURST)
        return TextFormat("Heals nearby allies %d within %.0f", k->sHealing, k->sRange);
    if (k->superKind == SUPER_DASH)
        return TextFormat("Charge: %d on contact, smashes crates", k->sDamage);
    if (k->sPiercing) return TextFormat("Piercing shot: %d, hits everyone in line", k->sDamage);
    if (k->mainKind == ATTACK_LOB)
        return TextFormat("%d shells, %d each, breaks walls", k->sPellets, k->sDamage);
    return TextFormat("%d pellets, %d each, breaks walls", k->sPellets, k->sDamage);
}

static void DrawRosterEntry(World *w, Rectangle r, int kitIndex, bool selected, bool hover)
{
    (void)w;
    const WeaponDef *k = &WEAPONS[kitIndex];
    Color accent = KIT_ACCENT[kitIndex];

    Color fill = selected ? (Color){ 34, 56, 92, 248 } : (hover ? CARD_HOVER : CARD_BG);
    DrawRectangleRounded(r, 0.10f, 8, fill);
    DrawRectangleRoundedLines(r, 0.10f, 8, selected ? accent : CARD_EDGE);

    // Accent spine down the left edge, the only colour that identifies the kit here.
    DrawRectangleRounded((Rectangle){ r.x + 6, r.y + 10, 6, r.height - 20 }, 0.9f, 5, accent);

    DrawText(k->name, (int)r.x + 24, (int)r.y + 12, 24, TEXT_MAIN);
    DrawText(k->flavor, (int)r.x + 24, (int)r.y + 40, 13, TEXT_DIM);

    if (selected)
    {
        const char *tag = "SELECTED";
        int tw = MeasureText(tag, 12);
        DrawText(tag, (int)(r.x + r.width) - tw - 18, (int)r.y + 16, 12, accent);
    }

    // Stats in two columns so the block scans quickly.
    //
    // Each string is drawn immediately after it is formatted. TextFormat hands back a
    // pointer into a small rotating buffer, so holding several results at once silently
    // corrupts the earliest ones.
    int sx = (int)r.x + 24, rx = (int)r.x + 210, sy = (int)r.y + 66;

    DrawText(TextFormat("HEALTH   %d", k->maxHealth), sx, sy, 13, TEXT_MAIN);
    if (k->mainKind == ATTACK_RAIN)
        DrawText(TextFormat("PULSE D/H %d/%d", k->damage, k->healing),
                 sx, sy + 19, 13, TEXT_MAIN);
    else if (k->healing > 0)
        DrawText(TextFormat("D/H      %d/%d", k->damage*k->pellets,
                            k->healing*k->pellets), sx, sy + 19, 13, TEXT_MAIN);
    else
        DrawText(TextFormat("DAMAGE   %d", k->damage*k->pellets), sx, sy + 19, 13, TEXT_MAIN);
    DrawText(TextFormat("RANGE    %.0f", k->range), sx, sy + 38, 13, TEXT_MAIN);

    DrawText(TextFormat("RELOAD   %.2fs", k->reloadPerAmmo), rx, sy, 13, TEXT_MAIN);
    DrawText(TextFormat("COOLDOWN %.2fs", k->cooldown), rx, sy + 19, 13, TEXT_MAIN);
    DrawText(TextFormat("AMMO     %d", k->maxAmmo), rx, sy + 38, 13, TEXT_MAIN);

    // Descriptions share one column, set wide enough for the longest super name.
    const int descX = 112;

    DrawText("ATTACK", (int)r.x + 24, (int)r.y + 128, 11, ACCENT);
    DrawText(AttackSummary(k), (int)r.x + descX, (int)r.y + 127, 13, TEXT_MAIN);

    DrawText(k->superName, (int)r.x + 24, (int)r.y + 148, 11, PLAY_GOLD);
    DrawText(SuperSummary(k), (int)r.x + descX, (int)r.y + 147, 13, TEXT_MAIN);
}

static void DrawBrawlerSelect(World *w)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Tuning *t = &w->tune;

    if (Card(w, (Rectangle){ 24, 24, 130, 52 }, "BACK", NULL, StyleNormal(), 0))
        ShellRequestScreen(w, SCREEN_MENU);

    Rectangle panel = { sw*0.46f, 92.0f, sw*0.50f, sh - 150.0f };

    DrawText("ROSTER", (int)panel.x + 2, (int)panel.y - 26, 16, ACCENT);
    DrawText("scroll to browse", (int)panel.x + 78, (int)panel.y - 23, 12, TEXT_DIM);

    //--- Scroll input --------------------------------------------------------
    float total = CLASS_COUNT*(float)ENTRY_H;
    Vector2 mouse = GetMousePosition();
    bool overPanel = MouseIn(panel);

    if (overPanel) g_scroll -= GetMouseWheelMove()*58.0f;

    // Dragging the list is the natural gesture here, so a press only counts as a
    // selection if the pointer barely moved.
    if (overPanel && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        g_dragging = true;
        g_dragMoved = false;
        g_dragStartY = mouse.y;
        g_dragStartScroll = g_scroll;
    }
    if (g_dragging)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            float delta = mouse.y - g_dragStartY;
            if (fabsf(delta) > 6.0f) g_dragMoved = true;
            g_scroll = g_dragStartScroll - delta;
        }
        else g_dragging = false;
    }

    // Wrap rather than clamp: the roster has no ends.
    while (g_scroll < 0.0f) g_scroll += total;
    while (g_scroll >= total) g_scroll -= total;

    //--- Entries -------------------------------------------------------------
    BeginScissorMode((int)panel.x, (int)panel.y, (int)panel.width, (int)panel.height);

    int first = (int)floorf(g_scroll/ENTRY_H) - 1;
    int visible = (int)(panel.height/ENTRY_H) + 3;

    for (int n = 0; n < visible; n++)
    {
        int slot = first + n;
        int kit = ((slot % CLASS_COUNT) + CLASS_COUNT) % CLASS_COUNT;

        Rectangle r = { panel.x + 6, panel.y + slot*(float)ENTRY_H - g_scroll,
                        panel.width - 12, ENTRY_H - 12.0f };

        bool hover = !g_blockCards && MouseIn(r) && MouseIn(panel);
        bool selected = (t->selectedKit == kit);

        DrawRosterEntry(w, r, kit, selected, hover);

        if (hover) g_hoverKit = kit;

        // Released without dragging, over this entry: that is a pick.
        if (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !g_dragMoved && !selected)
        {
            t->selectedKit = kit;
            ConfigMarkDirty();
            RebuildPreview(w);
        }
    }

    EndScissorMode();

    // Fades top and bottom, so entries dissolve at the edges instead of being sliced.
    DrawRectangleGradientV((int)panel.x, (int)panel.y, (int)panel.width, 34,
                           (Color){ BG_TOP.r, BG_TOP.g, BG_TOP.b, 235 },
                           (Color){ BG_TOP.r, BG_TOP.g, BG_TOP.b, 0 });
    DrawRectangleGradientV((int)panel.x, (int)(panel.y + panel.height) - 34, (int)panel.width, 34,
                           (Color){ BG_BOTTOM.r, BG_BOTTOM.g, BG_BOTTOM.b, 0 },
                           (Color){ BG_BOTTOM.r, BG_BOTTOM.g, BG_BOTTOM.b, 235 });

    //--- Name of the model on the left ---------------------------------------
    const WeaponDef *shown = &WEAPONS[t->selectedKit];
    int nameX = (int)(sw*0.22f);
    DrawLabel(shown->name, nameX, 96, 34, TEXT_MAIN);
    DrawLabel(shown->flavor, nameX, 134, 14, TEXT_DIM);
}

//------------------------------------------------------------------------------------
void MenuDraw(World *w)
{
    DrawBackdrop();
    DrawPodiumScene(w);

    if (w->screen == SCREEN_BRAWLERS)
    {
        g_showControls = false;             // the modal belongs to the main menu only
        DrawBrawlerSelect(w);
        return;
    }

    // Underlying cards are drawn inert while the modal is up, then the modal on top.
    g_blockCards = g_showControls;
    DrawMainMenu(w);
    g_blockCards = false;

    if (g_showControls) DrawControlsModal(w);
}

//------------------------------------------------------------------------------------
// Screen flow
//------------------------------------------------------------------------------------
bool ShellIsTransitioning(const World *w) { return w->fadingOut || w->fade > 0.001f; }

void ShellRequestScreen(World *w, AppScreen screen)
{
    if (w->fadingOut) return;               // ignore double clicks mid-transition
    if (screen == w->screen) return;

    // Get any pending tweak onto disk now rather than relying on the debounce, so a
    // change made a moment before quitting from the menu still survives.
    ConfigFlush(w);

    w->pending = screen;
    w->fadingOut = true;
}

void ShellUpdate(World *w, float dt)
{
    if (w->fadingOut)
    {
        w->fade += dt*FADE_SPEED;
        if (w->fade >= 1.0f)
        {
            w->fade = 1.0f;
            w->fadingOut = false;
            w->screen = w->pending;

            // Entering the menu should always show the current selection, and leaving a
            // sandbox should not leave the flag set for the next thing you start.
            if (w->screen != SCREEN_MATCH) { w->sandbox = false; RebuildPreview(w); }
        }
    }
    else if (w->fade > 0.0f)
    {
        w->fade -= dt*FADE_SPEED;
        if (w->fade < 0.0f) w->fade = 0.0f;
    }
}

void ShellDrawFade(World *w)
{
    if (w->fade <= 0.001f) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 0, 0, 0, (unsigned char)(255*Clamp(w->fade, 0.0f, 1.0f)) });
}
