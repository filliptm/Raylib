#include "mobile_controls.h"

#include "content_catalog.h"
#include "player_touch.h"
#include "ui_system.h"
#include "raymath.h"

#include <math.h>

#if defined(BRAWL_MOBILE)
#define MOBILE_STICK_ALPHA 48
#define MOBILE_ACTION_VISUAL_SCALE 0.75f

static Color Alpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

static Vector2 KnobPosition(const MobileStickState *stick, Vector2 home,
                            float radius)
{
    Vector2 origin = stick->active ? stick->origin : home;
    Vector2 offset = Vector2Scale(stick->value, radius*0.72f);
    return stick->active ? Vector2Add(origin, offset) : origin;
}

static void DrawStickBase(Vector2 center, float radius, Color accent,
                          unsigned char alpha)
{
    DrawCircleV(Vector2Add(center, (Vector2){ 4, 6 }), radius + 4.0f,
                Alpha(BLACK, (unsigned char)(alpha*0.55f)));
    DrawCircleV(center, radius, Alpha((Color){ 3, 15, 27, 255 }, alpha));
    DrawRing(center, radius - 3.0f, radius, 0.0f, 360.0f, 64,
             Alpha((Color){ 255, 247, 218, 255 },
                   (unsigned char)(alpha*1.50f)));
    DrawRing(center, radius - 8.0f, radius - 5.0f, -48.0f, 78.0f, 24,
             Alpha(accent, (unsigned char)(alpha*1.40f)));
    DrawLineEx((Vector2){ center.x - radius*0.46f, center.y },
               (Vector2){ center.x + radius*0.46f, center.y }, 1.5f,
               Alpha(accent, (unsigned char)(alpha*1.05f)));
    DrawLineEx((Vector2){ center.x, center.y - radius*0.46f },
               (Vector2){ center.x, center.y + radius*0.46f }, 1.5f,
               Alpha(accent, (unsigned char)(alpha*1.05f)));
}

static void DrawStick(const MobileStickState *stick, Vector2 home, float radius,
                      Color accent, const char *label)
{
    Vector2 base = stick->active ? stick->origin : home;
    Vector2 knob = KnobPosition(stick, home, radius);
    unsigned char alpha = MOBILE_STICK_ALPHA;
    DrawStickBase(base, radius, accent, alpha);

    float knobRadius = radius*0.42f;
    DrawCircleV(Vector2Add(knob, (Vector2){ 3, 4 }), knobRadius + 2.0f,
                Alpha(BLACK, (unsigned char)(alpha*0.78f)));
    DrawCircleV(knob, knobRadius,
                Alpha(accent, (unsigned char)(alpha*1.53f)));
    DrawRing(knob, knobRadius - 3.0f, knobRadius, 0.0f, 360.0f, 32,
             Alpha((Color){ 255, 247, 218, 255 },
                   (unsigned char)(alpha*1.58f)));

    int fontSize = (int)Clamp(radius*0.22f, 11.0f, 15.0f);
    int width = MeasureText(label, fontSize);
    DrawText(label, (int)(base.x - width*0.5f),
             (int)(base.y + radius + 7.0f), fontSize,
             Alpha((Color){ 255, 247, 218, 255 },
                   (unsigned char)(alpha*1.65f)));
}

static void DrawAction(const MobileStickState *stick, Vector2 home,
                       float touchRadius, Color accent, const char *label,
                       float progress, bool ready, float idleAlpha)
{
    float visualScale = MOBILE_ACTION_VISUAL_SCALE;
    float radius = touchRadius*visualScale;
    Vector2 base = home;
    Vector2 knob = KnobPosition(stick, home, radius);
    unsigned char alpha = stick->active ? 255 : (unsigned char)idleAlpha;
    Color fill = ready ? accent : (Color){ 41, 55, 68, 255 };
    float clampedProgress = Clamp(progress, 0.0f, 1.0f);
    float ringInner = radius + 5.0f*visualScale;
    float ringOuter = radius + 10.0f*visualScale;

    DrawCircleV(Vector2Add(
                    base, (Vector2){ 4.0f*visualScale, 5.0f*visualScale }),
                radius + 3.0f*visualScale,
                Alpha(BLACK, (unsigned char)(alpha*0.60f)));
    DrawRing(Vector2Add(
                 base, (Vector2){ 3.0f*visualScale, 4.0f*visualScale }),
             ringInner, ringOuter + 2.0f*visualScale, 0.0f, 360.0f, 64,
             Alpha(BLACK, (unsigned char)(alpha*0.45f)));
    DrawRing(base, ringInner, ringOuter, 0.0f, 360.0f, 64,
             Alpha((Color){ 255, 247, 218, 255 },
                   (unsigned char)(alpha*0.30f)));
    if (clampedProgress > 0.001f)
        DrawRing(base, ringInner, ringOuter, -90.0f,
                 -90.0f + 360.0f*clampedProgress, 64,
                 Alpha(accent, (unsigned char)(alpha*0.96f)));
    if (ready)
        DrawRing(base, ringOuter + 1.5f*visualScale,
                 ringOuter + 4.0f*visualScale,
                 0.0f, 360.0f, 64,
                 Alpha(accent, (unsigned char)(alpha*0.68f)));

    DrawCircleV(base, radius, Alpha((Color){ 3, 15, 27, 255 },
                                    (unsigned char)(alpha*0.88f)));
    DrawRing(base, radius - 1.5f*visualScale,
             radius + 1.5f*visualScale, 0.0f, 360.0f, 48,
             Alpha((Color){ 255, 247, 218, 255 },
                   (unsigned char)(alpha*0.85f)));
    if (ready)
        DrawCircleV(base, radius - 5.0f*visualScale,
                    Alpha(accent, (unsigned char)(alpha*0.34f)));
    DrawCircleV(knob, radius*0.48f, Alpha(fill, (unsigned char)(alpha*0.94f)));

    int fontSize = (int)Clamp(radius*0.29f, 10.0f, 15.0f);
    int width = MeasureText(label, fontSize);
    DrawText(label, (int)(knob.x - width*0.5f),
             (int)(knob.y - fontSize*0.5f), fontSize,
             Alpha((Color){ 255, 247, 218, 255 }, alpha));
}

static void DrawPause(Rectangle bounds, float idleAlpha)
{
    Color ink = Alpha((Color){ 3, 15, 27, 255 },
                      (unsigned char)(idleAlpha*0.92f));
    Color paper = Alpha((Color){ 255, 247, 218, 255 },
                        (unsigned char)idleAlpha);
    DrawRectangleRounded(
        (Rectangle){ bounds.x + 3, bounds.y + 4, bounds.width, bounds.height },
        0.28f, 8, Alpha(BLACK, (unsigned char)(idleAlpha*0.52f)));
    DrawRectangleRounded(bounds, 0.28f, 8, ink);
    DrawRectangleRoundedLinesEx(bounds, 0.28f, 8, 2.0f, paper);
    float barWidth = 5.0f;
    float barHeight = 19.0f;
    DrawRectangleRounded(
        (Rectangle){ bounds.x + bounds.width*0.5f - 8.0f,
                     bounds.y + (bounds.height - barHeight)*0.5f,
                     barWidth, barHeight }, 0.5f, 4, paper);
    DrawRectangleRounded(
        (Rectangle){ bounds.x + bounds.width*0.5f + 3.0f,
                     bounds.y + (bounds.height - barHeight)*0.5f,
                     barWidth, barHeight }, 0.5f, 4, paper);
}
#endif

void MobileControlsDraw(const App *app)
{
#if defined(BRAWL_MOBILE)
    if (!app || app->session.brawlerCount <= 0 ||
        app->session.match.phase == MATCH_OVER)
        return;

    const MobileControlsState *controls = &app->mobileControls;
    const Brawler *player = &app->session.brawlers[app->session.playerIdx];
    MobileControlLayout layout = PlayerTouchLayout(
        GetScreenWidth(), GetScreenHeight(), AppPlatformSafeInsets());
    const UiTheme *theme = UiSystemActive()->theme;

    float idle = controls->idleAge < 1.4f
        ? 150.0f : fmaxf(82.0f, 150.0f - (controls->idleAge - 1.4f)*48.0f);
    if (!player->alive) idle *= 0.55f;
    DrawStick(&controls->move, layout.moveHome, layout.mainRadius,
              theme->blue, "MOVE");
    DrawStick(&controls->attack, layout.attackHome, layout.mainRadius,
              theme->enemy, "ATTACK");

    const AbilityDefinition *secondary =
        ContentSecondaryAbility(&app->content, player->cls);
    float secondaryProgress = 0.0f;
    bool secondaryReady = false;
    if (secondary)
    {
        if (secondary->behavior == ABILITY_BEHAVIOR_SHIELD)
        {
            secondaryProgress = secondary->data.shield.capacity > 0.0f
                ? Clamp(player->shieldCharge/
                        secondary->data.shield.capacity, 0.0f, 1.0f) : 0.0f;
            secondaryReady = player->shieldActive ||
                (player->shieldBrokenTimer <= 0.0f &&
                 !player->shieldRearmRequired && player->shieldCharge > 0.0f);
        }
        else
        {
            secondaryProgress = player->mobilityCooldown <= 0.0f ? 1.0f :
                Clamp(1.0f - player->mobilityCooldown/secondary->cooldown,
                      0.0f, 1.0f);
            secondaryReady = player->mobilityCooldown <= 0.0f;
        }
    }

    DrawAction(&controls->secondary, layout.secondaryHome, layout.actionRadius,
               theme->blue, "SKILL", secondaryProgress,
               secondaryReady, idle);
    DrawAction(&controls->superAbility, layout.superHome, layout.actionRadius,
               theme->gold, "SUPER", Clamp(player->superCharge, 0.0f, 1.0f),
               player->superCharge >= 1.0f, idle);
    DrawPause(layout.pause, idle);
#else
    (void)app;
#endif
}
