#include "ui_system.h"
#include "content_catalog.h"
#include "hud.h"
#include "phone_layout.h"
#include "player_touch.h"
#include "raymath.h"
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

static bool Contains(Rectangle outer, Rectangle inner)
{
    return inner.x >= outer.x - 0.01f &&
           inner.y >= outer.y - 0.01f &&
           inner.x + inner.width <= outer.x + outer.width + 0.01f &&
           inner.y + inner.height <= outer.y + outer.height + 0.01f;
}

static bool EndsBefore(Rectangle left, Rectangle right)
{
    return left.x + left.width <= right.x + 0.01f;
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

    UiViewportInsets phoneInsets = { 0.0f, 59.0f, 21.0f, 59.0f };
    Rectangle phoneSafe = UiReferenceSafeRectWithInsets(
        874, 402, 1.0f, phoneInsets);
    CHECK(phoneSafe.x >= phoneInsets.left &&
          phoneSafe.y >= phoneInsets.top &&
          phoneSafe.x + phoneSafe.width <= 874.0f - phoneInsets.right + 0.01f &&
          phoneSafe.y + phoneSafe.height <= 402.0f - phoneInsets.bottom + 0.01f,
          "notched-phone UI safe rectangle escaped its usable viewport");

    UiPhoneFrame phoneFrame =
        UiPhoneFrameForViewport(874, 402, phoneInsets);
    CHECK(Contains((Rectangle){ 0, 0, 874, 402 }, phoneFrame.safe),
          "phone composition escaped the viewport");
    CHECK(phoneFrame.safe.x >= phoneInsets.left &&
          phoneFrame.safe.y >= phoneInsets.top &&
          phoneFrame.safe.x + phoneFrame.safe.width <=
              874.0f - phoneInsets.right + 0.01f &&
          phoneFrame.safe.y + phoneFrame.safe.height <=
              402.0f - phoneInsets.bottom + 0.01f,
          "phone composition ignored the device safe area");
    CHECK(phoneFrame.referenceWidth > 960.0f,
          "phone composition did not gain usable landscape width");

    UiPhoneHomeLayout phoneHome =
        UiPhoneHomeLayoutForFrame(phoneFrame);
    CHECK(Contains(phoneFrame.safe, phoneHome.rail) &&
          phoneHome.rail.width >= phoneFrame.safe.width*0.99f,
          "phone launch rail does not use the safe landscape width");
    CHECK(Contains(phoneFrame.safe, phoneHome.controls) &&
          Contains(phoneFrame.safe, phoneHome.settings) &&
          Contains(phoneFrame.safe, phoneHome.roster) &&
          Contains(phoneFrame.safe, phoneHome.mode) &&
          Contains(phoneFrame.safe, phoneHome.practice) &&
          Contains(phoneFrame.safe, phoneHome.deploy),
          "phone launch controls escaped the safe composition");
    CHECK(EndsBefore(phoneHome.roster, phoneHome.mode) &&
          EndsBefore(phoneHome.mode, phoneHome.practice) &&
          EndsBefore(phoneHome.practice, phoneHome.deploy),
          "phone launch controls overlap");
    CHECK(phoneHome.controls.height >= 44.0f &&
          phoneHome.settings.height >= 44.0f &&
          phoneHome.roster.height >= 44.0f &&
          phoneHome.practice.height >= 44.0f &&
          phoneHome.deploy.height >= 44.0f,
          "phone launch control fell below the physical target floor");

    UiPhoneRosterLayout phoneRoster =
        UiPhoneRosterLayoutForFrame(phoneFrame);
    CHECK(Contains(phoneFrame.safe, phoneRoster.header) &&
          Contains(phoneFrame.safe, phoneRoster.identity) &&
          Contains(phoneFrame.safe, phoneRoster.stage) &&
          Contains(phoneFrame.safe, phoneRoster.telemetry) &&
          Contains(phoneFrame.safe, phoneRoster.candidateRail),
          "phone roster region escaped the safe composition");
    CHECK(EndsBefore(phoneRoster.identity, phoneRoster.stage) &&
          EndsBefore(phoneRoster.stage, phoneRoster.telemetry),
          "phone roster columns overlap");
    for (int i = 0; i < CLASS_COUNT; i++)
        CHECK(Contains(phoneFrame.safe, phoneRoster.candidates[i]) &&
              phoneRoster.candidates[i].height >= 44.0f,
              "phone roster candidate escaped or became too small");

    UiPhoneResultLayout phoneResult =
        UiPhoneResultLayoutForFrame(phoneFrame);
    CHECK(Contains(phoneFrame.safe, phoneResult.panel),
          "phone result panel escaped the safe composition");
    for (int i = 0; i < 3; i++)
        CHECK(Contains(phoneFrame.safe, phoneResult.actions[i]) &&
              phoneResult.actions[i].height >= 44.0f,
              "phone result action escaped or became too small");
    CHECK(EndsBefore(phoneResult.actions[0], phoneResult.actions[1]) &&
          EndsBefore(phoneResult.actions[1], phoneResult.actions[2]),
          "phone result actions overlap");

    UiSystem frameUi = { 0 };
    frameUi.layout.scale = 1.25f;
    frameUi.layout.viewportScale = 1.10f;
    frameUi.layout.origin = (Vector2){ 7.0f, 9.0f };
    UiFrameLayout previousFrame = UiPhoneApplyFrame(&frameUi, phoneFrame);
    CHECK(Near(frameUi.layout.scale, phoneFrame.scale) &&
          Near(frameUi.layout.origin.x, phoneFrame.origin.x) &&
          Near(frameUi.layout.origin.y, phoneFrame.origin.y),
          "phone frame did not become the active UI composition");
    UiPhoneRestoreFrame(&frameUi, previousFrame);
    CHECK(Near(frameUi.layout.scale, 1.25f) &&
          Near(frameUi.layout.viewportScale, 1.10f) &&
          Near(frameUi.layout.origin.x, 7.0f) &&
          Near(frameUi.layout.origin.y, 9.0f),
          "phone frame did not restore the shared UI layout");

    Rectangle smallTarget = { 20, 30, 18, 24 };
    Rectangle touchTarget = UiTouchTargetBounds(smallTarget, 44.0f);
    CHECK(Near(touchTarget.width, 44.0f) && Near(touchTarget.height, 44.0f) &&
          Near(touchTarget.x + touchTarget.width*0.5f,
               smallTarget.x + smallTarget.width*0.5f) &&
          Near(touchTarget.y + touchTarget.height*0.5f,
               smallTarget.y + smallTarget.height*0.5f),
          "touch target expansion changed the visual target center");

    MobileControlLayout mobile = PlayerTouchLayout(
        874, 402, (AppSafeInsets){ 0.0f, 59.0f, 21.0f, 59.0f });
    CHECK(mobile.safe.x >= 59.0f &&
          mobile.safe.x + mobile.safe.width <= 815.01f &&
          mobile.safe.y + mobile.safe.height <= 381.01f,
          "mobile controls escaped the iPhone safe area");
    CHECK(mobile.mainRadius >= 54.0f && mobile.actionRadius >= 38.0f,
          "mobile controls fell below their physical target floor");
    CHECK(Vector2Distance(mobile.attackHome, mobile.secondaryHome) >
              mobile.mainRadius + mobile.actionRadius,
          "secondary control overlaps the attack stick");

    Camera3D phoneCamera = {
        .position = { 0.0f, 10.0f, 10.0f },
        .target = { 0.0f, 0.0f, 0.0f },
        .up = { 0.0f, 1.0f, 0.0f },
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    Vector3 touchRight = PlayerTouchCameraIntent(
        phoneCamera, (Vector2){ 1.0f, 0.0f });
    Vector3 touchForward = PlayerTouchCameraIntent(
        phoneCamera, (Vector2){ 0.0f, -1.0f });
    CHECK(touchRight.x > 0.99f && fabsf(touchRight.z) < 0.01f &&
          touchForward.z < -0.99f && fabsf(touchForward.x) < 0.01f,
          "touch stick directions no longer follow the arena camera");
    Vector3 fullSpeed = PlayerFullSpeedMoveIntent(
        (Vector3){ 0.20f, 0.70f, 0.10f }, 0.14f);
    Vector3 stopped = PlayerFullSpeedMoveIntent(
        (Vector3){ 0.10f, 0.00f, 0.00f }, 0.14f);
    CHECK(Near(Vector3Length(fullSpeed), 1.0f) && Near(fullSpeed.y, 0.0f),
          "partial stick movement no longer resolves to full ground speed");
    CHECK(Near(Vector3Length(stopped), 0.0f),
          "movement dead zone no longer resolves to a full stop");

    UiSystem touchUi = { 0 };
    touchUi.modality = UI_INPUT_TOUCH;
    touchUi.glyphMode = UI_GLYPH_AUTO;
    UiSystemSetActive(&touchUi);
    CHECK(strcmp(UiBindingLabel("LMB", "RT", "ATTACK"), "ATTACK") == 0,
          "automatic glyph mode did not select touch language");

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

    puts("UI phone layout, binary movement, skin, focus, motion, motifs, bars, and results passed");
    return 0;
}
