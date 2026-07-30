#include "ui_system.h"
#include "content_catalog.h"
#include "hud.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static bool Near(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

static bool SameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int main(void)
{
    struct {
        int width;
        int height;
        float expectedScale;
    } viewports[] = {
        { 960, 600, 0.75f },
        { 1280, 800, 1.00f },
        { 1920, 1080, 1.35f },
        { 2560, 1440, 1.80f }
    };

    for (int i = 0; i < 4; i++)
    {
        float scale = UiReferenceScaleForViewport(viewports[i].width,
                                                  viewports[i].height, 1.0f);
        CHECK(Near(scale, viewports[i].expectedScale),
              "reference scale changed for a supported viewport");
        Rectangle safe = UiReferenceSafeRect(viewports[i].width,
                                             viewports[i].height, 1.0f);
        CHECK(safe.x >= 0 && safe.y >= 0 &&
              safe.x + safe.width <= viewports[i].width + 0.01f &&
              safe.y + safe.height <= viewports[i].height + 0.01f,
              "safe rectangle escaped the viewport");
        CHECK(44.0f*scale >= 33.0f,
              "minimum player target became too small at a supported viewport");
    }

    UiFocusNode nodes[] = {
        { 1, { 0, 0, 80, 50 }, true },
        { 2, { 100, 0, 80, 50 }, true },
        { 3, { 0, 80, 80, 50 }, true },
        { 4, { 100, 80, 80, 50 }, false }
    };
    CHECK(UiFocusNeighbor(nodes, 4, 1, 1, 0) == 2,
          "focus did not move right");
    CHECK(UiFocusNeighbor(nodes, 4, 1, 0, 1) == 3,
          "focus did not move down");
    CHECK(UiFocusNeighbor(nodes, 4, 3, 1, 0) != 4,
          "focus entered a disabled target");
    CHECK(UiHash("stable.focus.id") == UiHash("stable.focus.id"),
          "UI IDs are not deterministic");

    CHECK(Near(UiMotionDuration(0.22f, false), 0.22f),
          "normal motion duration changed");
    CHECK(Near(UiMotionDuration(0.22f, true), 0.0f),
          "reduced motion did not remove decorative duration");
    CHECK(Near(UiEaseOutCubic(0.0f), 0.0f) &&
          Near(UiEaseOutCubic(1.0f), 1.0f) &&
          UiEaseOutCubic(0.5f) > 0.5f,
          "shared cubic entrance easing changed");
    CHECK(UiEaseOutBack(0.70f) > 1.0f &&
          Near(UiEaseOutBack(1.0f), 1.0f),
          "shared poster-pop easing lost its controlled overshoot");
    CHECK(Near(UiMotionProgress(0.10f, 0.20f, 0.40f, false), 0.0f) &&
          Near(UiMotionProgress(0.40f, 0.20f, 0.40f, false), 0.5f) &&
          Near(UiMotionProgress(0.0f, 1.0f, 4.0f, true), 1.0f),
          "delayed/reduced motion progress changed");
    CHECK(HUD_RESULT_CONTINUE != HUD_RESULT_REMATCH &&
          HUD_RESULT_REMATCH != HUD_RESULT_CHANGE_BRAWLER,
          "result actions are no longer distinct");

    const UiTheme *theme = UiThemeArenaInk();
    CHECK(theme->surface.a == 255 && theme->surfaceRaised.a == 255,
          "structural UI surfaces became transparent");
    CHECK(UiThemeContrastRatio(theme->paper, theme->surface) >= 4.5f,
          "primary text contrast fell below WCAG AA");
    CHECK(UiThemeContrastRatio(theme->textSecondary, theme->surface) >= 3.0f,
          "secondary text contrast fell below the approved floor");
    CHECK(UiThemeContrastRatio(theme->ink, theme->yellow) >= 4.5f,
          "ink on the yellow action color fell below WCAG AA");
    CHECK(UiThemeContrastRatio(theme->paper, theme->blue) >= 4.5f,
          "paper on the blue action color fell below WCAG AA");
    CHECK(UiThemeContrastRatio(theme->paper, theme->enemy) >= 4.5f,
          "paper on the red action color fell below WCAG AA");
    CHECK(UiThemeContrastRatio(theme->paper, theme->purple) >= 4.5f,
          "paper on the purple action color fell below WCAG AA");
    CHECK(SameColor(HudHealthBarColor(theme, TEAM_PLAYER, false), theme->ally),
          "player-team health bars lost the stable ally color");
    CHECK(SameColor(HudHealthBarColor(theme, TEAM_ENEMY, false), theme->enemy),
          "enemy health bars lost the stable enemy color");
    Color contrastedAlly = HudHealthBarColor(theme, TEAM_PLAYER, true);
    Color contrastedEnemy = HudHealthBarColor(theme, TEAM_ENEMY, true);
    CHECK(contrastedAlly.g > contrastedAlly.r &&
          contrastedEnemy.r > contrastedEnemy.g,
          "high contrast mode obscured health-bar allegiance");

    UiSkin proceduralSkin = { 0 };
    CHECK(UiSkinLoad(&proceduralSkin) && proceduralSkin.ready,
          "procedural Arena Ink skin did not become ready");
    UiSkinUnload(&proceduralSkin);
    CHECK(!proceduralSkin.ready,
          "procedural Arena Ink skin retained state after unload");

    UiSkin missingSkin = { 0 };
    Rectangle skinBounds = { 0, 0, 240, 80 };
    CHECK(!UiSkinDrawPanel(&missingSkin, skinBounds, theme->surface, theme->border,
                           true, false),
          "missing panel texture did not request the geometry fallback");
    CHECK(!UiSkinDrawButton(&missingSkin, skinBounds, theme->surface,
                            theme->ink, theme->border, false),
          "missing button texture did not request the geometry fallback");
    CHECK(!UiSkinDrawProgress(&missingSkin, skinBounds, 0.5f, theme->surfaceMuted,
                              theme->blue, false, 0, 3.0f),
          "missing progress texture did not request the geometry fallback");
    CHECK(!UiSkinDrawDecoration(&missingSkin, UI_DECORATION_BURST,
                                skinBounds, theme->blue),
          "missing decoration texture did not request the geometry fallback");

    ContentCatalog catalog = { 0 };
    ContentCatalogResetAll(&catalog);
    CHECK(ContentShowcaseValid(&catalog.showcase),
          "compiled character showcase is invalid");
    CHECK(Near(catalog.showcase.yawDegrees, 180.0f) &&
          Near(catalog.showcase.scale, 0.90f) &&
          Near(catalog.showcase.offset.x, 0.0f) &&
          Near(catalog.showcase.offset.y, 0.0f),
          "shared character transform changed");
    CHECK(Near(catalog.showcase.cameraPosition.x, 0.0f) &&
          Near(catalog.showcase.cameraPosition.y, 2.70f) &&
          Near(catalog.showcase.cameraPosition.z, -7.60f) &&
          Near(catalog.showcase.cameraTarget.x, 0.0f) &&
          Near(catalog.showcase.cameraTarget.y, 1.40f) &&
          Near(catalog.showcase.cameraTarget.z, 0.0f) &&
          Near(catalog.showcase.verticalFov, 40.0f),
          "shared showcase camera changed");
    bool motifs[5] = { false };
    for (int i = 0; i < CLASS_COUNT; i++)
    {
        const CharacterUiStyle *style = ContentCharacterUiStyle((BrawlerClass)i);
        CHECK(style->motif >= CHARACTER_UI_SAW &&
              style->motif <= CHARACTER_UI_GROWTH,
              "character poster motif escaped its supported range");
        CHECK(!motifs[style->motif],
              "two characters lost their distinct poster motif");
        motifs[style->motif] = true;
        CHECK(style->primary.a == 255 && style->secondary.a == 255 &&
              style->impactLabel && strlen(style->impactLabel) > 0,
              "character UI identity is incomplete");
    }

    puts("UI layout, skin, focus, motion, motifs, team bars, and result actions passed");
    return 0;
}
