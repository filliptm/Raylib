/*******************************************************************************************
*   ARENA INK MENU
*
*   Player-facing shell built from the shared UI system. Gameplay is only reached through
*   screen requests and existing application flags; the 3D preview is owned by MenuScene.
********************************************************************************************/
#include "menu.h"

#include "config.h"
#include "content_catalog.h"
#include "menu_scene.h"
#include "ui_system.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>

#define FADE_SPEED 3.6f

typedef enum MenuOverlay {
    MENU_OVERLAY_NONE = 0,
    MENU_OVERLAY_CONTROLS,
    MENU_OVERLAY_SETTINGS
} MenuOverlay;

static UiSystem *g_ui;
static MenuScene g_scene;
static MenuOverlay g_overlay;
static BrawlerClass g_candidate = CLASS_SHOTGUNNER;
static AppScreen g_lastScreen = SCREEN_COUNT;
static BrawlerClass g_lastPreview = CLASS_COUNT;
static float g_screenAge;
static float g_characterAge;

static const char *ROLE_NAMES[] = {
    "Damage", "Marksman", "Artillery", "Tank", "Support"
};

static Color CharacterAccent(const App *w, BrawlerClass cls)
{
    const CharacterDefinition *character = ContentCharacter(&w->content, cls);
    const CharacterUiStyle *style = ContentCharacterUiStyle(cls);
    return character && style ? style->primary : g_ui->theme->blue;
}

static Rectangle ScaleRectCentered(Rectangle bounds, float scale)
{
    float width = bounds.width*scale;
    float height = bounds.height*scale;
    return (Rectangle){
        bounds.x + (bounds.width - width)*0.5f,
        bounds.y + (bounds.height - height)*0.5f,
        width, height
    };
}

static Rectangle RefRectOffsetY(float x, float y, float width, float height,
                                float referenceOffset)
{
    Rectangle bounds = UiRefRect(x, y, width, height);
    bounds.y += UiScale(referenceOffset);
    return bounds;
}

static Vector2 RefPointOffsetY(float x, float y, float referenceOffset)
{
    Vector2 point = UiRefPoint(x, y);
    point.y += UiScale(referenceOffset);
    return point;
}

static void DrawBackdrop(void)
{
    UiDrawComicBackdrop();
}

static void DrawStationHeader(const App *w, const char *section)
{
    const UiTheme *t = g_ui->theme;
    Rectangle rail = UiRefRect(24, 20, 1232, 64);
    UiDrawFeaturePanel(rail, t->ink, t->paper, true);
    UiDrawSignalRail(rail, t->yellow, false);

    UiDrawTextOutline(UI_TEXT_HEADING, "BRAWL ARENA", UiRefPoint(50, 29),
                      t->yellow, t->ink, 2.0f);
    UiDrawText(UI_TEXT_CAPTION, "HELIOS-9 // LIVE DEPLOYMENT",
               UiRefPoint(50, 61), t->textSecondary);

    Rectangle sectionRect = UiRefRect(490, 28, 300, 42);
    UiDrawControlSurface(sectionRect, t->yellow, t->ink, false);
    UiDrawTextFit(UI_TEXT_LABEL, section, sectionRect, UI_ALIGN_CENTER, t->ink);

    char profile[96];
    snprintf(profile, sizeof(profile), "%d W  /  %d L  /  %d KO",
             w->tune.statWins, w->tune.statLosses, w->tune.statKos);
    UiDrawText(UI_TEXT_DATA, profile, UiRefPoint(852, 42), t->paper);
}

static void DrawMetric(Rectangle row, UiIcon icon, const char *label,
                       const char *value, Color color)
{
    const UiTheme *t = g_ui->theme;
    UiIconDraw(icon, (Vector2){ row.x + UiScale(16), row.y + row.height*0.5f },
               UiScale(16), color);
    Rectangle labelBounds = row;
    labelBounds.x += UiScale(34);
    labelBounds.width *= 0.52f;
    UiDrawTextFit(UI_TEXT_LABEL, label, labelBounds, UI_ALIGN_LEFT, t->textMuted);
    Rectangle valueBounds = row;
    valueBounds.x += row.width*0.56f;
    valueBounds.width = row.width*0.40f;
    UiDrawTextFit(UI_TEXT_DATA, value, valueBounds, UI_ALIGN_RIGHT, t->paper);
}

static void AbilitySummary(const AbilityDefinition *ability, char *buffer, int size)
{
    if (ability->behavior == ABILITY_BEHAVIOR_RAIN)
        snprintf(buffer, size, "Growing rain // %d damage + %d healing per pulse",
                 ability->damage, ability->healing);
    else if (ability->behavior == ABILITY_BEHAVIOR_SOUND_WAVE)
        snprintf(buffer, size, "Wide resonance cone // %d damage + %d healing per tick",
                 ability->damage, ability->healing);
    else if (ability->behavior == ABILITY_BEHAVIOR_DASH)
        snprintf(buffer, size, "Armored drive // %d impact damage", ability->damage);
    else if (ability->behavior == ABILITY_BEHAVIOR_SHIELD)
        snprintf(buffer, size,
                 "Hold 360 shell // %d capacity // %.0f%% absorb healing",
                 ability->data.shield.capacity,
                 ability->data.shield.healRatio*100.0f);
    else if (ability->behavior == ABILITY_BEHAVIOR_GRAPPLE)
        snprintf(buffer, size, "Aim + release // %.0f range // %.1fs cooldown",
                 ability->range, ability->cooldown);
    else if (ability->behavior == ABILITY_BEHAVIOR_MINE)
        snprintf(buffer, size, "%d damage // %.1f trigger // %.1fs cooldown",
                 ability->damage, ability->data.mine.triggerRadius,
                 ability->cooldown);
    else if (ability->behavior == ABILITY_BEHAVIOR_RETURNING)
        snprintf(buffer, size, "%d outbound + %d return // piercing disc",
                 ability->damage, ability->damage);
    else if (ability->behavior == ABILITY_BEHAVIOR_LOB)
        snprintf(buffer, size, "Arcing payload // %.1f splash radius", ability->radius);
    else if (ability->healing > 0)
        snprintf(buffer, size, "%d damage to hostiles // %d healing to allies",
                 ability->damage, ability->healing);
    else if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE &&
             ability->data.projectile.pellets > 1)
    {
        if (ability->data.projectile.spreadDegrees < 0.1f)
            snprintf(buffer, size, "Tight twin bolts // %d combined damage",
                     ability->damage*ability->data.projectile.pellets);
        else
            snprintf(buffer, size, "%d projectiles // %.0f degree spread",
                     ability->data.projectile.pellets,
                     ability->data.projectile.spreadDegrees);
    }
    else snprintf(buffer, size, "Precision projectile // %.0f range", ability->range);
}

static void DrawAbilityBlock(Rectangle bounds, const char *eyebrow,
                             const AbilityDefinition *ability, Color accent)
{
    const UiTheme *t = g_ui->theme;
    UiDrawPanel(bounds, t->surface, t->ink, true);
    UiDrawSignalRail(bounds, accent, false);
    UiDrawText(UI_TEXT_CAPTION, eyebrow,
               (Vector2){ bounds.x + UiScale(18), bounds.y + UiScale(10) }, accent);
    Rectangle name = { bounds.x + UiScale(18), bounds.y + UiScale(23),
                       bounds.width - UiScale(32), UiScale(28) };
    UiDrawTextFit(UI_TEXT_EMPHASIS, ability->name, name, UI_ALIGN_LEFT, t->paper);
    char summary[160];
    AbilitySummary(ability, summary, (int)sizeof(summary));
    Rectangle copy = { bounds.x + UiScale(18), bounds.y + UiScale(50),
                       bounds.width - UiScale(32), UiScale(24) };
    UiDrawTextFit(UI_TEXT_CAPTION, summary, copy, UI_ALIGN_LEFT, t->textSecondary);
}

static void OpenOverlay(MenuOverlay overlay, UiId returnFocus)
{
    g_overlay = overlay;
    g_ui->restoreFocus = returnFocus;
    g_ui->activatePressed = false;
    UiFocus(overlay == MENU_OVERLAY_CONTROLS
        ? UiHash("controls.close") : UiHash("settings.scale.down"));
}

static void CloseOverlay(void)
{
    g_overlay = MENU_OVERLAY_NONE;
    if (g_ui->restoreFocus) UiFocus(g_ui->restoreFocus);
    g_ui->restoreFocus = 0;
}

static void StartMatch(App *w, bool practice)
{
    w->session.sandbox = practice;
    w->matchRestartPending = true;
    w->flow.matchResultBanked = false;
    ShellRequestScreen(w, SCREEN_MATCH);
}

static void DrawHome(App *w, float logoScale, float railOffset)
{
    const UiTheme *t = g_ui->theme;
    BrawlerClass selected = (BrawlerClass)w->tune.selectedKit;
    const CharacterDefinition *character = ContentCharacter(&w->content, selected);
    Color accent = CharacterAccent(w, selected);

    // The logo and sticker-like brawler are the authored signature; utility
    // controls and launch choices stay on a disciplined poster grid.
    UiDrawArenaLogo(ScaleRectCentered(UiRefRect(22, 12, 330, 174), logoScale),
                    "HELIOS-9 // LIVE DEPLOYMENT");

    // Sits in the free strip between the title block and the utility cluster; the
    // bottom row is fully occupied by the roster/mode/deploy panels.
    UiResponse studio = UiButton(UiHash("home.studio"), UiRefRect(660, 34, 148, 52),
                                 "VFX Studio", UI_BUTTON_YELLOW, UI_ICON_STUDIO);
    if (studio.activated) ShellRequestScreen(w, SCREEN_STUDIO);

    UiResponse controls = UiButton(UiHash("home.controls"), UiRefRect(824, 34, 132, 52),
                                   "Controls", UI_BUTTON_BLUE, UI_ICON_CONTROLS);
    if (controls.activated) OpenOverlay(MENU_OVERLAY_CONTROLS, UiHash("home.controls"));
    UiResponse settings = UiButton(UiHash("home.settings"), UiRefRect(968, 34, 132, 52),
                                   "Settings", UI_BUTTON_YELLOW, UI_ICON_SETTINGS);
    if (settings.activated) OpenOverlay(MENU_OVERLAY_SETTINGS, UiHash("home.settings"));
    UiResponse quit = UiButton(UiHash("home.quit"), UiRefRect(1112, 34, 120, 52),
                               "Quit", UI_BUTTON_DANGER, UI_ICON_QUIT);
    if (quit.activated) w->flow.quitRequested = true;

    Rectangle launch = RefRectOffsetY(24, 646, 1232, 130, railOffset);
    UiDrawFeaturePanel(launch, t->ink, t->paper, true);
    UiDrawSignalRail(launch, t->yellow, false);

    UiDrawText(UI_TEXT_CAPTION, "ACTIVE BRAWLER // CHANGE",
               RefPointOffsetY(48, 658, railOffset), accent);

    UiResponse roster = UiButton(UiHash("home.roster"),
                                 RefRectOffsetY(46, 678, 276, 72, railOffset),
                                 character->displayName, UI_BUTTON_BLUE, UI_ICON_NEXT);
    if (roster.activated)
    {
        ShellRequestScreen(w, SCREEN_BRAWLERS);
    }
    Rectangle mode = RefRectOffsetY(344, 660, 398, 92, railOffset);
    UiDrawPanel(mode, t->ink, t->paper, false);
    UiDrawTextAligned(UI_TEXT_CAPTION, "ACTIVE GAME MODE",
                      RefRectOffsetY(410, 666, 266, 20, railOffset),
                      UI_ALIGN_CENTER, t->paper);
    UiResponse previous = UiIconButton(UiHash("home.mode.previous"),
                                       RefRectOffsetY(358, 692, 48, 48, railOffset),
                                       UI_ICON_PREVIOUS, "Previous mode");
    UiResponse next = UiIconButton(UiHash("home.mode.next"),
                                   RefRectOffsetY(680, 692, 48, 48, railOffset),
                                   UI_ICON_NEXT, "Next mode");
    const char *modeName = w->tune.gemGrab ? "GEM GRAB" : "SKIRMISH";
    Rectangle modeSlab = RefRectOffsetY(414, 688, 260, 56, railOffset);
    UiDrawControlSurface(modeSlab,
                         w->tune.gemGrab ? t->yellow : t->blue,
                         t->ink, true);
    UiDrawTextFit(UI_TEXT_HEADING, modeName,
                  RefRectOffsetY(418, 688, 252, 56, railOffset),
                  UI_ALIGN_CENTER, w->tune.gemGrab ? t->ink : t->paper);
    if (previous.activated || next.activated || g_ui->previousPressed || g_ui->nextPressed)
    {
        w->tune.gemGrab = !w->tune.gemGrab;
        ConfigMarkDirty();
    }

    UiResponse practice = UiButton(UiHash("home.practice"),
                                   RefRectOffsetY(764, 662, 154, 88, railOffset),
                                   "Practice", UI_BUTTON_BLUE, UI_ICON_PRACTICE);
    if (practice.activated) StartMatch(w, true);

    UiResponse deploy = UiButton(UiHash("home.deploy"),
                                 RefRectOffsetY(940, 656, 292, 100, railOffset),
                                 "DEPLOY", UI_BUTTON_PRIMARY, UI_ICON_NEXT);
    if (deploy.activated) StartMatch(w, false);
}

static void DrawRosterRow(App *w, BrawlerClass cls, Rectangle bounds)
{
    const UiTheme *t = g_ui->theme;
    const CharacterDefinition *character = ContentCharacter(&w->content, cls);
    bool candidate = g_candidate == cls;
    UiResponse response = UiButton(UiHash(character->id), bounds, character->displayName,
                                   candidate ? UI_BUTTON_YELLOW : UI_BUTTON_STANDARD,
                                   (UiIcon)-1);
    if (candidate)
    {
        UiDrawSignalRail(bounds, CharacterAccent(w, cls), false);
        UiDrawTextAligned(UI_TEXT_CAPTION, "PREVIEW",
                          (Rectangle){ bounds.x,
                                       bounds.y + bounds.height - UiScale(22),
                                       bounds.width, UiScale(16) },
                          UI_ALIGN_CENTER, candidate ? t->ink : t->gold);
    }
    if (response.activated)
        g_candidate = cls;
}

static void DrawRoster(App *w)
{
    const UiTheme *t = g_ui->theme;
    DrawStationHeader(w, "BRAWLER SELECT");

    UiResponse back = UiButton(UiHash("roster.back"), UiRefRect(28, 100, 142, 50),
                               "Back", UI_BUTTON_BLUE, UI_ICON_BACK);
    if (back.activated) ShellRequestScreen(w, SCREEN_MENU);

    const CharacterDefinition *candidate =
        ContentCharacter(&w->content, g_candidate);
    const AbilityDefinition *main = ContentMainAbility(&w->content, g_candidate);
    const AbilityDefinition *super = ContentSuperAbility(&w->content, g_candidate);
    const AbilityDefinition *secondary =
        ContentSecondaryAbility(&w->content, g_candidate);
    Color accent = CharacterAccent(w, g_candidate);

    // The former launch-deck readout belongs here: this is the moment where
    // comparative character information helps the player make a decision.
    Rectangle identity = UiRefRect(24, 166, 300, 454);
    UiDrawFeaturePanel(identity, t->inkSoft, t->paper, true);
    UiDrawSignalRail(identity, accent, false);
    UiDrawText(UI_TEXT_CAPTION, ROLE_NAMES[candidate->role],
               UiRefPoint(48, 184), accent);
    UiDrawTextFit(UI_TEXT_TITLE, candidate->displayName, UiRefRect(46, 200, 252, 55),
                  UI_ALIGN_LEFT, t->paper);
    UiDrawTextFit(UI_TEXT_BODY, candidate->flavor, UiRefRect(48, 255, 248, 34),
                  UI_ALIGN_LEFT, t->textSecondary);
    DrawAbilityBlock(UiRefRect(46, 306, 252, 88), "MAIN ATTACK", main, t->blue);
    if (secondary)
        DrawAbilityBlock(UiRefRect(46, 404, 252, 88),
                         "SECONDARY", secondary, t->yellow);
    DrawAbilityBlock(UiRefRect(46, secondary ? 502 : 404, 252, 88),
                     "ULTIMATE", super, t->purple);

    Rectangle telemetry = UiRefRect(956, 166, 300, 454);
    UiDrawFeaturePanel(telemetry, t->inkSoft, t->paper, true);
    UiDrawSignalRail(telemetry, t->yellow, true);
    UiDrawText(UI_TEXT_CAPTION, "FIELD TELEMETRY", UiRefPoint(980, 186), t->yellow);
    UiDrawText(UI_TEXT_HEADING, "KIT READOUT", UiRefPoint(980, 208), t->paper);

    char value[48];
    snprintf(value, sizeof(value), "%d", candidate->maxHealth);
    DrawMetric(UiRefRect(980, 258, 250, 42), UI_ICON_HEALTH,
               "Hull integrity", value, t->ally);
    int totalDamage = main->damage;
    if (main->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
        main->behavior == ABILITY_BEHAVIOR_LOB)
        totalDamage *= main->data.projectile.pellets;
    else if (main->behavior == ABILITY_BEHAVIOR_RETURNING)
        totalDamage *= 2;
    snprintf(value, sizeof(value), "%d", totalDamage);
    DrawMetric(UiRefRect(980, 306, 250, 42), UI_ICON_DAMAGE,
               "Attack output", value, t->enemy);
    snprintf(value, sizeof(value), "%.1f m", main->range);
    DrawMetric(UiRefRect(980, 354, 250, 42), UI_ICON_RANGE,
               "Effective range", value, t->blue);
    snprintf(value, sizeof(value), "%.2f s", main->reloadPerAmmo);
    DrawMetric(UiRefRect(980, 402, 250, 42), UI_ICON_RELOAD,
               "Ammo reload", value, t->yellow);
    snprintf(value, sizeof(value), "%d", candidate->maxAmmo);
    DrawMetric(UiRefRect(980, 450, 250, 42), UI_ICON_SUPER,
               "Ammo cells", value, t->gold);

    UiDrawPanel(UiRefRect(980, 510, 250, 84), t->inkSoft, t->surfaceStrong, false);
    UiDrawText(UI_TEXT_CAPTION, "SELECTION STATUS", UiRefPoint(996, 524), t->textMuted);
    UiDrawTextFit(UI_TEXT_BODY,
                  w->tune.selectedKit == (int)g_candidate
                    ? "Currently equipped." : "Preview only until confirmed.",
                  UiRefRect(996, 544, 218, 34), UI_ALIGN_LEFT,
                  w->tune.selectedKit == (int)g_candidate ? t->ally : t->textSecondary);

    Rectangle roster = UiRefRect(24, 640, 1232, 136);
    UiDrawPanel(roster, t->ink, t->paper, true);
    UiDrawText(UI_TEXT_CAPTION, "CHOOSE A BRAWLER", UiRefPoint(48, 652), t->paper);
    for (int i = 0; i < CLASS_COUNT; i++)
        DrawRosterRow(w, (BrawlerClass)i, UiRefRect(48 + i*142, 678, 132, 72));

    char selectLabel[96];
    snprintf(selectLabel, sizeof(selectLabel), "SELECT %s", candidate->displayName);
    UiResponse select = UiButton(UiHash("roster.confirm"), UiRefRect(790, 674, 442, 80),
                                 selectLabel, UI_BUTTON_YELLOW, UI_ICON_NEXT);
    if (select.activated)
    {
        w->tune.selectedKit = g_candidate;
        ConfigMarkDirty();
        ShellRequestScreen(w, SCREEN_MENU);
    }
}

static void DrawControlRow(float x, float y, const char *binding, const char *action)
{
    const UiTheme *t = g_ui->theme;
    UiDrawKeycap(UiRefRect(x, y, 124, 38), binding, false);
    UiDrawText(UI_TEXT_BODY, action, UiRefPoint(x + 142, y + 8), t->paper);
}

static void DrawControlsOverlay(App *w)
{
    (void)w;
    const UiTheme *t = g_ui->theme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), t->scrim);
    Rectangle panel = UiRefRect(190, 76, 900, 648);
    UiDrawFeaturePanel(panel, t->inkSoft, t->paper, true);
    UiDrawDecoration(UI_DECORATION_HALFTONE, UiRefRect(824, 94, 230, 170),
                     t->blue, 0.16f);
    UiDrawSignalRail(panel, t->yellow, false);
    UiDrawTextOutline(UI_TEXT_TITLE, "CONTROLS", UiRefPoint(230, 100),
                      t->yellow, t->ink, 2.0f);
    UiDrawText(UI_TEXT_CAPTION, "ACTIVE INPUT GLYPHS FOLLOW THE LAST USED DEVICE",
               UiRefPoint(232, 150), t->textMuted);

    UiDrawText(UI_TEXT_LABEL, "MOVEMENT", UiRefPoint(232, 192), t->blue);
    DrawControlRow(232, 218, UiBindingLabel("WASD / ARROWS", "LEFT STICK"), "Move");
    DrawControlRow(232, 264, UiBindingLabel("LEFT SHIFT", "LEFT BUMPER"),
                   "Secondary — press dash / hold shield");

    UiDrawText(UI_TEXT_LABEL, "COMBAT", UiRefPoint(232, 326), t->yellow);
    DrawControlRow(232, 352, UiBindingLabel("HOLD LMB", "HOLD RT"), "Aim main attack");
    DrawControlRow(232, 398, UiBindingLabel("RELEASE LMB", "RELEASE RT"), "Fire");
    DrawControlRow(232, 444, UiBindingLabel("RMB", "RIGHT BUMPER"), "Aim and use ultimate");

    UiDrawText(UI_TEXT_LABEL, "SYSTEM", UiRefPoint(680, 192), t->purple);
    DrawControlRow(680, 218, UiBindingLabel("TAB", "KEYBOARD"), "Command center in practice");
    DrawControlRow(680, 264, UiBindingLabel("R", "KEYBOARD"), "Restart match");
    DrawControlRow(680, 310, UiBindingLabel("ESC", "B"), "Back or close");
    DrawControlRow(680, 356, UiBindingLabel("1 - 5", "KEYBOARD"), "Swap active brawler");

    UiResponse close = UiButton(UiHash("controls.close"), UiRefRect(504, 644, 272, 56),
                                "Close controls", UI_BUTTON_YELLOW, UI_ICON_CLOSE);
    if (close.activated) CloseOverlay();
}

static void PreferenceToggle(UiId id, Rectangle bounds, const char *label, bool *value)
{
    UiResponse response = UiToggle(id, bounds, label, *value);
    if (response.activated)
    {
        *value = !*value;
        ConfigMarkDirty();
    }
}

static void DrawSettingsOverlay(App *w)
{
    const UiTheme *t = g_ui->theme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), t->scrim);
    Rectangle panel = UiRefRect(330, 78, 620, 600);
    UiDrawFeaturePanel(panel, t->inkSoft, t->paper, true);
    UiDrawDecoration(UI_DECORATION_HALFTONE, UiRefRect(720, 96, 196, 150),
                     t->purple, 0.15f);
    UiDrawSignalRail(panel, t->yellow, false);
    UiDrawTextOutline(UI_TEXT_TITLE, "SETTINGS", UiRefPoint(372, 102),
                      t->yellow, t->ink, 2.0f);
    UiDrawText(UI_TEXT_CAPTION, "PROFILE-SCOPED // SAVED AUTOMATICALLY",
               UiRefPoint(374, 152), t->textMuted);

    UiDrawText(UI_TEXT_LABEL, "INTERFACE SCALE", UiRefPoint(374, 198), t->blue);
    UiDrawPanel(UiRefRect(374, 224, 532, 62), t->inkSoft, t->border, false);
    UiResponse smaller = UiIconButton(UiHash("settings.scale.down"),
                                      UiRefRect(388, 233, 44, 44),
                                      UI_ICON_PREVIOUS, "Decrease UI scale");
    UiResponse larger = UiIconButton(UiHash("settings.scale.up"),
                                     UiRefRect(848, 233, 44, 44),
                                     UI_ICON_NEXT, "Increase UI scale");
    char scale[32];
    snprintf(scale, sizeof(scale), "%.0f%%", w->uiPreferences.scale*100.0f);
    UiDrawTextAligned(UI_TEXT_DATA, scale, UiRefRect(448, 233, 384, 44),
                      UI_ALIGN_CENTER, t->paper);
    if (smaller.activated || larger.activated)
    {
        w->uiPreferences.scale = Clamp(w->uiPreferences.scale +
            (larger.activated ? 0.10f : -0.10f), 0.75f, 1.50f);
        ConfigMarkDirty();
    }

    PreferenceToggle(UiHash("settings.motion"), UiRefRect(374, 310, 532, 50),
                     "Reduced decorative motion", &w->uiPreferences.reducedMotion);
    PreferenceToggle(UiHash("settings.contrast"), UiRefRect(374, 372, 532, 50),
                     "High-contrast gameplay cues", &w->uiPreferences.highContrast);
    PreferenceToggle(UiHash("settings.hints"), UiRefRect(374, 434, 532, 50),
                     "Action tutorial hints", &w->uiPreferences.showTutorialHints);

    UiDrawText(UI_TEXT_LABEL, "INPUT GLYPHS", UiRefPoint(374, 506), t->purple);
    const char *glyphs[UI_GLYPH_MODE_COUNT] = { "AUTO", "KEYBOARD / MOUSE", "GAMEPAD" };
    UiResponse prev = UiIconButton(UiHash("settings.glyph.prev"),
                                   UiRefRect(374, 528, 44, 44),
                                   UI_ICON_PREVIOUS, "Previous input glyph mode");
    UiResponse next = UiIconButton(UiHash("settings.glyph.next"),
                                   UiRefRect(862, 528, 44, 44),
                                   UI_ICON_NEXT, "Next input glyph mode");
    UiDrawTextAligned(UI_TEXT_DATA, glyphs[w->uiPreferences.inputGlyphMode],
                      UiRefRect(432, 528, 416, 44), UI_ALIGN_CENTER, t->paper);
    if (prev.activated || next.activated)
    {
        int direction = next.activated ? 1 : -1;
        w->uiPreferences.inputGlyphMode =
            (w->uiPreferences.inputGlyphMode + direction + UI_GLYPH_MODE_COUNT) %
            UI_GLYPH_MODE_COUNT;
        ConfigMarkDirty();
    }

    UiResponse reset = UiButton(UiHash("settings.reset.hints"),
                                UiRefRect(374, 602, 250, 46),
                                "Reset tutorial", UI_BUTTON_BLUE, UI_ICON_CONTROLS);
    if (reset.activated)
    {
        w->uiPreferences.tutorialFlags = 0;
        ConfigMarkDirty();
    }
    UiResponse close = UiButton(UiHash("settings.close"), UiRefRect(642, 602, 264, 46),
                                "Save and close", UI_BUTTON_YELLOW, UI_ICON_CLOSE);
    if (close.activated)
    {
        ConfigFlush(w);
        CloseOverlay();
    }
}

bool MenuConsumeEscape(void)
{
    if (g_overlay == MENU_OVERLAY_NONE) return false;
    CloseOverlay();
    return true;
}

void MenuInit(Assets *assets, UiSystem *ui)
{
    g_ui = ui;
    g_screenAge = 0.0f;
    g_characterAge = 0.0f;
    g_lastPreview = CLASS_COUNT;
    MenuSceneInit(&g_scene, assets);
}

void MenuUnload(void)
{
    MenuSceneUnload(&g_scene);
    g_ui = NULL;
}

void MenuUpdate(App *w, float dt)
{
    if (g_lastScreen != w->flow.screen)
    {
        g_lastScreen = w->flow.screen;
        g_screenAge = 0.0f;
        if (w->flow.screen == SCREEN_BRAWLERS)
        {
            g_candidate = (BrawlerClass)w->tune.selectedKit;
            UiFocus(UiHash("roster.back"));
            if (UiCurrentModality() == UI_INPUT_POINTER) g_ui->focusVisible = false;
        }
        else if (w->flow.screen == SCREEN_MENU)
        {
            UiFocus(UiHash("home.roster"));
            if (UiCurrentModality() == UI_INPUT_POINTER) g_ui->focusVisible = false;
        }
    }

    if (UiBackPressed())
    {
        if (g_overlay != MENU_OVERLAY_NONE) CloseOverlay();
        else if (w->flow.screen == SCREEN_BRAWLERS) ShellRequestScreen(w, SCREEN_MENU);
        else w->flow.quitRequested = true;
    }

    BrawlerClass preview = w->flow.screen == SCREEN_BRAWLERS
        ? g_candidate : (BrawlerClass)w->tune.selectedKit;
    if (preview != g_lastPreview)
    {
        g_lastPreview = preview;
        g_characterAge = 0.0f;
    }
    g_screenAge += fmaxf(0.0f, dt);
    g_characterAge += fmaxf(0.0f, dt);
    float sceneDt = w->uiPreferences.reducedMotion ? 0.0f : dt;
    MenuSceneUpdate(&g_scene, w, preview, w->flow.screen, sceneDt);
}

void MenuDraw(App *w)
{
    DrawBackdrop();
    BrawlerClass preview = w->flow.screen == SCREEN_BRAWLERS
        ? g_candidate : (BrawlerClass)w->tune.selectedKit;
    const CharacterDefinition *character = ContentCharacter(&w->content, preview);
    const CharacterUiStyle *style = ContentCharacterUiStyle(preview);
    float screenT = UiMotionProgress(g_screenAge, 0.0f, 0.42f,
                                     w->uiPreferences.reducedMotion);
    float characterT = UiMotionProgress(g_characterAge, 0.03f, 0.40f,
                                        w->uiPreferences.reducedMotion);
    float logoScale = 0.78f + 0.22f*UiEaseOutBack(screenT);
    float stickerScale = 0.84f + 0.16f*UiEaseOutBack(characterT);
    float railOffset = 54.0f*(1.0f - UiEaseOutCubic(screenT));
    MenuSceneDrawStage(&g_scene, w, preview, w->flow.screen);
    Color accent = CharacterAccent(w, preview);
    if (w->flow.screen == SCREEN_BRAWLERS)
        UiDrawDecoration(UI_DECORATION_HALFTONE, UiRefRect(356, 112, 568, 568),
                         g_ui->theme->ink, 0.14f);
    else
        UiDrawDecoration(UI_DECORATION_BURST, UiRefRect(356, 104, 568, 568),
                         g_ui->theme->yellow, 0.34f);
    UiDrawDecoration(UI_DECORATION_SPEED_LINES, UiRefRect(372, 120, 536, 510),
                     accent, 0.13f);
    if (character && style)
        UiDrawCharacterMotif(style->motif, UiRefRect(358, 112, 564, 548),
                             style->primary, style->secondary, 0.26f);
    MenuSceneCompositeBrawler(&g_scene, w, preview, w->flow.screen, stickerScale);

    bool entranceReady = w->uiPreferences.reducedMotion || screenT >= 0.98f;
    UiSetInteractionsEnabled(g_overlay == MENU_OVERLAY_NONE && entranceReady);
    if (w->flow.screen == SCREEN_BRAWLERS) DrawRoster(w);
    else DrawHome(w, logoScale, railOffset);

    UiSetInteractionsEnabled(true);
    if (g_overlay == MENU_OVERLAY_CONTROLS) DrawControlsOverlay(w);
    else if (g_overlay == MENU_OVERLAY_SETTINGS) DrawSettingsOverlay(w);
}

void MenuPrepareDraw(App *w)
{
    BrawlerClass preview = w->flow.screen == SCREEN_BRAWLERS
        ? g_candidate : (BrawlerClass)w->tune.selectedKit;
    MenuSceneRenderBrawler(&g_scene, w, preview, w->flow.screen,
                           GetScreenWidth(), GetScreenHeight());
}

bool ShellIsTransitioning(const App *w)
{
    return w->flow.fadingOut || w->flow.fade > 0.001f;
}

void ShellRequestScreen(App *w, AppScreen screen)
{
    if (w->flow.fadingOut || screen == w->flow.screen) return;
    ConfigFlush(w);
    w->flow.pending = screen;
    w->flow.fadingOut = true;
}

void ShellUpdate(App *w, float dt)
{
    if (w->flow.fadingOut)
    {
        w->flow.fade += dt*FADE_SPEED;
        if (w->flow.fade >= 1.0f)
        {
            w->flow.fade = 1.0f;
            w->flow.fadingOut = false;
            w->flow.screen = w->flow.pending;
            if (w->flow.screen != SCREEN_MATCH) w->session.sandbox = false;
        }
    }
    else if (w->flow.fade > 0.0f)
    {
        w->flow.fade -= dt*FADE_SPEED;
        if (w->flow.fade < 0.0f) w->flow.fade = 0.0f;
    }
}

void ShellDrawFade(App *w)
{
    if (w->flow.fade <= 0.001f) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 0, 0, 0,
                    (unsigned char)(255*Clamp(w->flow.fade, 0.0f, 1.0f)) });
}
