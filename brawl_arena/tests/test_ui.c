#include "ui_system.h"
#include "content_catalog.h"
#include "hud.h"
#include <math.h>
#include <stdio.h>

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

    const UiTheme *theme = UiThemeHelios();
    CHECK(theme->deck.a == 255 && theme->deckRaised.a == 255,
          "structural UI surfaces became transparent");
    CHECK(UiThemeContrastRatio(theme->paper, theme->deck) >= 4.5f,
          "primary text contrast fell below WCAG AA");
    CHECK(UiThemeContrastRatio(theme->mist, theme->deck) >= 3.0f,
          "secondary text contrast fell below the approved floor");
    CHECK(SameColor(HudHealthBarColor(theme, TEAM_PLAYER, false), theme->ally),
          "player-team health bars lost the stable ally color");
    CHECK(SameColor(HudHealthBarColor(theme, TEAM_ENEMY, false), theme->enemy),
          "enemy health bars lost the stable enemy color");
    Color contrastedAlly = HudHealthBarColor(theme, TEAM_PLAYER, true);
    Color contrastedEnemy = HudHealthBarColor(theme, TEAM_ENEMY, true);
    CHECK(contrastedAlly.g > contrastedAlly.r &&
          contrastedEnemy.r > contrastedEnemy.g,
          "high contrast mode obscured health-bar allegiance");

    NPatchInfo patch = UiSkinNinePatchInfo(384, 128, 24, 20, 24, 20);
    CHECK(Near(patch.source.width, 384.0f) && Near(patch.source.height, 128.0f),
          "UI skin patch lost its source dimensions");
    CHECK(patch.left == 24 && patch.top == 20 &&
          patch.right == 24 && patch.bottom == 20,
          "UI skin patch margins changed");
    CHECK(patch.layout == NPATCH_NINE_PATCH,
          "UI skin patch no longer scales from nine slices");

    UiSkin missingSkin = { 0 };
    Rectangle skinBounds = { 0, 0, 240, 80 };
    CHECK(!UiSkinDrawPanel(&missingSkin, skinBounds, theme->deck, theme->line,
                           true, false),
          "missing panel texture did not request the geometry fallback");
    CHECK(!UiSkinDrawButton(&missingSkin, skinBounds, theme->deck, theme->line, false),
          "missing button texture did not request the geometry fallback");
    CHECK(!UiSkinDrawProgress(&missingSkin, skinBounds, 0.5f, theme->hull,
                              theme->ion, false, 0, 3.0f),
          "missing progress texture did not request the geometry fallback");
    CHECK(!UiSkinDrawDecoration(&missingSkin, UI_DECORATION_ORBITAL_RING,
                                skinBounds, theme->ion),
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

    puts("UI layout, skin, focus, motion, team bars, and shared showcase passed");
    return 0;
}
