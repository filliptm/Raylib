#include "hud.h"

#include "command_center.h"
#include "config.h"
#include "content_catalog.h"
#include "brawler.h"
#include "phone_layout.h"
#include "weapons.h"
#include "ui_system.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>

enum {
    TUTORIAL_MOVE = 1 << 0,
    TUTORIAL_AIM = 1 << 1,
    TUTORIAL_FIRE = 1 << 2,
    TUTORIAL_QUICK = 1 << 3,
    TUTORIAL_MOBILITY = 1 << 4,
    TUTORIAL_SUPER = 1 << 5,
    TUTORIAL_COMMAND = 1 << 6
};

static HudResultAction g_resultAction;

static Color CueColor(const App *w, Color base)
{
    return UiThemeHighContrast(base, w->uiPreferences.highContrast);
}

static void DrawBarText(Rectangle bounds, const char *text, Color color)
{
    Rectangle shadow = bounds;
    shadow.x += UiScale(1);
    shadow.y += UiScale(1);
    UiDrawTextFit(UI_TEXT_CAPTION, text, shadow, UI_ALIGN_CENTER,
                  UiSystemActive()->theme->shadow);
    UiDrawTextFit(UI_TEXT_CAPTION, text, bounds, UI_ALIGN_CENTER, color);
}

static void DrawAmmo(const App *w, Rectangle bounds, float ammo, int maxAmmo)
{
    if (maxAmmo < 1) return;
    const UiTheme *t = UiSystemActive()->theme;
    float shell = UiScale(2.0f);
    float gap = UiScale(2.0f);
    float radius = 0.34f;
    Color loaded = CueColor(w, t->yellow);
    Color empty = CueColor(w, t->surfaceMuted);

    Rectangle shadow = {
        bounds.x + UiScale(2.0f), bounds.y + UiScale(2.0f),
        bounds.width, bounds.height
    };
    DrawRectangleRounded(shadow, radius, 6, t->shadow);
    DrawRectangleRounded(bounds, radius, 6, t->paper);

    Rectangle rail = {
        bounds.x + shell, bounds.y + shell,
        bounds.width - shell*2.0f, bounds.height - shell*2.0f
    };
    DrawRectangleRounded(rail, radius, 6, t->ink);

    float cellWidth = (rail.width - gap*(maxAmmo - 1))/maxAmmo;
    for (int i = 0; i < maxAmmo; i++)
    {
        Rectangle cell = {
            rail.x + i*(cellWidth + gap), rail.y, cellWidth, rail.height
        };
        DrawRectangleRounded(cell, 0.28f, 5, empty);

        float amount = Clamp(ammo - i, 0.0f, 1.0f);
        if (amount > 0.001f)
        {
            Rectangle fill = cell;
            fill.width *= amount;
            DrawRectangleRounded(fill, 0.28f, 5, loaded);
        }
    }
}

void HudObservePlayerInput(App *w, const PlayerInput *input)
{
    if (!w || !input || input->actionsBlocked) return;
    int before = w->uiPreferences.tutorialFlags;
    if (Vector3Length(input->moveIntent) > 0.1f)
        w->uiPreferences.tutorialFlags |= TUTORIAL_MOVE;
    if (input->attackPressed) w->uiPreferences.tutorialFlags |= TUTORIAL_AIM;
    if (input->attackReleased) w->uiPreferences.tutorialFlags |= TUTORIAL_FIRE;
    if (input->autoAttackPressed) w->uiPreferences.tutorialFlags |= TUTORIAL_QUICK;
    if (input->secondaryPressed || input->mobilityPressed)
        w->uiPreferences.tutorialFlags |= TUTORIAL_MOBILITY;
    if (input->superHeld) w->uiPreferences.tutorialFlags |= TUTORIAL_SUPER;
    if (w->session.sandbox && CommandCenterIsOpen())
        w->uiPreferences.tutorialFlags |= TUTORIAL_COMMAND;
    if (before != w->uiPreferences.tutorialFlags) ConfigMarkDirty();
}

HudResultAction HudConsumeResultAction(void)
{
    HudResultAction action = g_resultAction;
    g_resultAction = HUD_RESULT_NONE;
    return action;
}

void HudResetFeedback(void)
{
    g_resultAction = HUD_RESULT_NONE;
}

static void TriggerStamp(App *w, PresentationUiStamp stamp, float duration)
{
    UiFeedbackState *feedback = &w->presentation.uiFeedback;
    feedback->stamp = stamp;
    feedback->stampAge = 0.0f;
    feedback->stampDuration = duration;
}

void HudUpdateFeedback(App *w, float dt)
{
    if (!w || w->session.brawlerCount <= 0) return;
    UiFeedbackState *feedback = &w->presentation.uiFeedback;
    Brawler *player = &w->session.brawlers[w->session.playerIdx];
    bool superReady = player->superCharge >= 1.0f;
    int phase = (int)w->session.match.phase;

    feedback->stampAge += fmaxf(0.0f, dt);
    feedback->objectivePulse =
        fmaxf(0.0f, feedback->objectivePulse - fmaxf(0.0f, dt)*2.8f);

    if (!feedback->initialized)
    {
        feedback->initialized = true;
        feedback->wasSuperReady = superReady;
        feedback->wasAlive = player->alive;
        feedback->previousShieldBrokenTimer = player->shieldBrokenTimer;
        feedback->previousMatchPhase = phase;
        feedback->previousCountdownTeam = w->session.match.countdownTeam;
        feedback->previousKills = w->session.kills;
    }
    else
    {
        if (phase == MATCH_COUNTDOWN &&
            (feedback->previousMatchPhase != MATCH_COUNTDOWN ||
             feedback->previousCountdownTeam != w->session.match.countdownTeam))
        {
            TriggerStamp(w, PRESENTATION_UI_STAMP_TEAM_LOCK, 1.15f);
            feedback->objectivePulse = 1.0f;
        }
        else if (!feedback->wasSuperReady && superReady)
        {
            TriggerStamp(w, PRESENTATION_UI_STAMP_SUPER_READY, 0.95f);
        }
        else if (w->session.kills > feedback->previousKills)
        {
            TriggerStamp(w, PRESENTATION_UI_STAMP_KNOCKOUT, 0.78f);
        }
        else if (feedback->previousShieldBrokenTimer <= 0.0f &&
                 player->shieldBrokenTimer > 0.0f)
        {
            TriggerStamp(w, PRESENTATION_UI_STAMP_SHIELD_BROKEN, 0.90f);
        }
        else if (feedback->wasAlive && !player->alive)
        {
            TriggerStamp(w, PRESENTATION_UI_STAMP_DOWNED, 0.90f);
        }
    }

    if (phase == MATCH_OVER && feedback->previousMatchPhase != MATCH_OVER)
    {
        UiFocus(UiHash("result.continue"));
    }

    feedback->wasSuperReady = superReady;
    feedback->wasAlive = player->alive;
    feedback->previousShieldBrokenTimer = player->shieldBrokenTimer;
    feedback->previousMatchPhase = phase;
    feedback->previousCountdownTeam = w->session.match.countdownTeam;
    feedback->previousKills = w->session.kills;
}

void HudDrawBars(App *w)
{
    const UiTheme *t = UiSystemActive()->theme;
    for (int i = 0; i < w->session.brawlerCount; i++)
    {
        Brawler *b = &w->session.brawlers[i];
        if (!b->alive || !b->visible) continue;
        Vector3 head = b->position;
        head.y = b->isPlayer ? 4.05f : 3.60f;
        Vector2 screen = GetWorldToScreen(head, w->presentation.camera);
        if (screen.x < -80 || screen.x > GetScreenWidth() + 80 ||
            screen.y < -80 || screen.y > GetScreenHeight() + 80) continue;

        bool mine = b->isPlayer;
        float width = UiScale(mine ? 76.0f : 62.0f);
        float height = UiScale(mine ? 12.0f : 10.5f);
        Rectangle bar = { screen.x - width*0.5f, screen.y, width, height };
        float health = b->maxHealth > 0 ? (float)b->health/b->maxHealth : 0.0f;
        const AbilityDefinition *shield =
            ContentSecondaryAbility(&w->content, b->cls);
        bool hasShield =
            shield && shield->behavior == ABILITY_BEHAVIOR_SHIELD;
        // Health color communicates allegiance, never remaining-health danger.
        Color team = HudHealthBarColor(
            t, b->team, w->uiPreferences.highContrast);

        if (hasShield)
        {
            float shieldRatio = shield->data.shield.capacity > 0
                              ? b->shieldCharge/
                                shield->data.shield.capacity : 0.0f;
            float shellHeight = UiScale(mine ? 14.0f : 12.0f);
            Rectangle shellBar = {
                bar.x, bar.y - shellHeight - UiScale(4),
                bar.width, shellHeight
            };
            UiDrawProgress(shellBar, shieldRatio,
                           b->shieldBrokenTimer > 0.0f
                               ? t->enemy : CueColor(w, t->blue),
                           false, 0);
            char shieldPoints[32];
            if (b->shieldBrokenTimer > 0.0f)
            {
                snprintf(shieldPoints, sizeof(shieldPoints), "0 // %.1fs",
                         b->shieldBrokenTimer);
            }
            else
                snprintf(shieldPoints, sizeof(shieldPoints), "%.0f",
                         b->shieldCharge);
            DrawBarText(shellBar, shieldPoints, t->paper);
        }

        UiDrawProgress(bar, health, team, false, 0);
        char healthPoints[24];
        snprintf(healthPoints, sizeof(healthPoints), "%d", b->health);
        DrawBarText(bar, healthPoints, t->paper);

        // Shape markers make team identity readable without relying on hue.
        if (b->team == TEAM_PLAYER)
            UiIconDraw(UI_ICON_ALLY,
                       (Vector2){ bar.x - UiScale(10), bar.y + bar.height*0.5f },
                       UiScale(9), team);
        else
            UiIconDraw(UI_ICON_ENEMY,
                       (Vector2){ bar.x + bar.width + UiScale(10),
                                  bar.y + bar.height*0.5f },
                       UiScale(9), team);

        if (mine)
        {
            DrawAmmo(w,
                     (Rectangle){ bar.x, bar.y + bar.height + UiScale(5),
                                  bar.width, UiScale(11) },
                     b->ammo,
                     ContentCharacter(&w->content, b->cls)->maxAmmo);
        }

        if (b->gems > 0)
        {
            char gems[16];
            snprintf(gems, sizeof(gems), "%d", b->gems);
            UiIconDraw(UI_ICON_GEM,
                       (Vector2){ screen.x - UiScale(12),
                                  bar.y + bar.height + UiScale(mine ? 25 : 15) },
                       UiScale(11), t->purple);
            UiDrawTextShadow(UI_TEXT_CAPTION, gems,
                             (Vector2){ screen.x, bar.y + bar.height +
                                        UiScale(mine ? 18 : 8) }, t->paper);
        }
    }
}

static void DrawObjective(App *w)
{
    const UiTheme *t = UiSystemActive()->theme;
    Rectangle bounds = UiRefRect(434, 20, 412, 94);
    float pulse = w->uiPreferences.reducedMotion
                ? 0.0f : w->presentation.uiFeedback.objectivePulse;
    if (pulse > 0.0f)
    {
        float grow = UiScale(8.0f*pulse);
        bounds = (Rectangle){ bounds.x - grow, bounds.y - grow*0.45f,
                              bounds.width + grow*2.0f,
                              bounds.height + grow*0.90f };
    }
    UiDrawFeaturePanel(bounds, t->ink, t->paper, true);
    UiDrawDecoration(UI_DECORATION_HALFTONE, UiRefRect(594, 25, 84, 84),
                     t->purple, 0.22f);
    UiDrawSignalRail(bounds, t->purple, false);
    UiDrawTextAligned(UI_TEXT_CAPTION,
                      w->session.sandbox ? "PRACTICE TELEMETRY" :
                      (w->tune.gemGrab ? "GEM GRAB // FIRST TO TARGET" : "SKIRMISH"),
                      UiRefRect(454, 28, 372, 22), UI_ALIGN_CENTER, t->textMuted);

    if (w->tune.gemGrab && !w->session.sandbox)
    {
        const Match *match = &w->session.match;
        char ally[16], enemy[16], target[32];
        snprintf(ally, sizeof(ally), "%d", match->teamGems[TEAM_PLAYER]);
        snprintf(enemy, sizeof(enemy), "%d", match->teamGems[TEAM_ENEMY]);
        snprintf(target, sizeof(target), "TARGET %d", w->tune.gemsToWin);
        UiIconDraw(UI_ICON_ALLY, UiRefPoint(492, 73), UiScale(20), CueColor(w, t->ally));
        UiDrawText(UI_TEXT_HEADING, ally, UiRefPoint(516, 53), t->paper);
        UiDrawTextAligned(UI_TEXT_DATA, target, UiRefRect(558, 52, 164, 40),
                          UI_ALIGN_CENTER, t->gold);
        UiDrawTextAligned(UI_TEXT_HEADING, enemy, UiRefRect(728, 52, 44, 40),
                          UI_ALIGN_RIGHT, t->paper);
        UiIconDraw(UI_ICON_ENEMY, UiRefPoint(792, 73), UiScale(20), CueColor(w, t->enemy));

        if (match->phase == MATCH_COUNTDOWN)
        {
            char countdown[80];
            snprintf(countdown, sizeof(countdown), "%s LOCK // %.1f",
                     match->countdownTeam == TEAM_PLAYER ? "ALLY" : "ENEMY",
                     match->countdown);
            UiDrawTextAligned(UI_TEXT_CAPTION, countdown, UiRefRect(508, 91, 264, 18),
                              UI_ALIGN_CENTER,
                              match->countdownTeam == TEAM_PLAYER ? t->ally : t->enemy);
        }
    }
    else
    {
        char state[96];
        int alive = 0;
        for (int i = 0; i < w->session.brawlerCount; i++)
            if (w->session.brawlers[i].team == TEAM_ENEMY &&
                w->session.brawlers[i].alive) alive++;
        snprintf(state, sizeof(state), "%d KO  /  %d DOWN  /  %d HOSTILES",
                 w->session.kills, w->session.deaths, alive);
        UiDrawTextAligned(UI_TEXT_DATA, state, UiRefRect(468, 56, 344, 42),
                          UI_ALIGN_CENTER, t->paper);
    }
}

static void DrawImpactStamp(const App *w)
{
    const UiFeedbackState *feedback = &w->presentation.uiFeedback;
    if (feedback->stamp == PRESENTATION_UI_STAMP_NONE ||
        feedback->stampAge >= feedback->stampDuration)
        return;

    const UiTheme *t = UiSystemActive()->theme;
    const char *label = "";
    Color fill = t->yellow;
    Color text = t->ink;
    switch (feedback->stamp)
    {
        case PRESENTATION_UI_STAMP_SUPER_READY:
            label = "ULTIMATE READY!";
            fill = t->purple;
            text = t->paper;
            break;
        case PRESENTATION_UI_STAMP_SHIELD_BROKEN:
            label = "SHELL BROKEN!";
            fill = t->enemy;
            text = t->paper;
            break;
        case PRESENTATION_UI_STAMP_TEAM_LOCK:
            label = w->session.match.countdownTeam == TEAM_PLAYER
                  ? "ALLY TEAM LOCK!" : "ENEMY TEAM LOCK!";
            fill = w->session.match.countdownTeam == TEAM_PLAYER
                 ? t->ally : t->enemy;
            text = t->paper;
            break;
        case PRESENTATION_UI_STAMP_DOWNED:
            label = "KNOCKED OUT!";
            fill = t->enemy;
            text = t->paper;
            break;
        case PRESENTATION_UI_STAMP_KNOCKOUT:
        {
            const CharacterUiStyle *style = ContentCharacterUiStyle(
                w->session.brawlers[w->session.playerIdx].cls);
            label = style ? style->impactLabel : "K.O.!";
            fill = style ? style->primary : t->yellow;
            text = UiThemeContrastRatio(t->ink, fill) >=
                   UiThemeContrastRatio(t->paper, fill) ? t->ink : t->paper;
            break;
        }
        default:
            return;
    }

    float normalized = Clamp(feedback->stampAge/feedback->stampDuration, 0.0f, 1.0f);
    float scale = w->uiPreferences.reducedMotion
                ? 1.0f : 0.82f + 0.18f*UiEaseOutBack(
                    UiMotionProgress(normalized, 0.0f, 0.28f, false));
    float alpha = 1.0f - Clamp((normalized - 0.72f)/0.28f, 0.0f, 1.0f);
    Rectangle base = UiRefRect(452, 126, 376, 64);
    Rectangle panel = {
        base.x + base.width*(1.0f - scale)*0.5f,
        base.y + base.height*(1.0f - scale)*0.5f,
        base.width*scale, base.height*scale
    };
    fill.a = (unsigned char)(255.0f*alpha);
    text.a = fill.a;
    UiDrawControlSurface(panel, fill, t->ink, true);
    UiDrawTextFit(UI_TEXT_HEADING, label, panel, UI_ALIGN_CENTER, text);
}

#if !defined(BRAWL_MOBILE)
static void DrawAbilityTile(Rectangle bounds, const char *eyebrow, const char *name,
                            const char *binding, float progress, bool ready,
                            Color accent)
{
    const UiTheme *t = UiSystemActive()->theme;
    UiDrawPanel(bounds, ready ? t->surfaceRaised : t->inkSoft,
                ready ? accent : t->paper, true);
    if (ready) UiDrawSignalRail(bounds, accent, true);
    UiDrawText(UI_TEXT_CAPTION, eyebrow,
               (Vector2){ bounds.x + UiScale(14), bounds.y + UiScale(9) },
               ready ? accent : t->textMuted);
    Rectangle nameBounds = { bounds.x + UiScale(14), bounds.y + UiScale(25),
                             bounds.width - UiScale(28), UiScale(28) };
    UiDrawTextFit(UI_TEXT_EMPHASIS, name, nameBounds, UI_ALIGN_LEFT,
                  ready ? t->paper : t->textSecondary);
    UiDrawKeycap((Rectangle){ bounds.x + UiScale(14),
                              bounds.y + bounds.height - UiScale(34),
                              UiScale(86), UiScale(24) }, binding, ready);
    UiDrawProgress((Rectangle){ bounds.x + UiScale(112),
                                bounds.y + bounds.height - UiScale(28),
                                bounds.width - UiScale(126), UiScale(8) },
                   progress, ready ? accent : t->surfaceStrong, false, 0);
    if (ready)
        UiDrawText(UI_TEXT_CAPTION, "READY",
                   (Vector2){ bounds.x + bounds.width - UiScale(58),
                              bounds.y + UiScale(10) }, accent);
}

static void DrawAbilities(App *w, const Brawler *player)
{
    const UiTheme *t = UiSystemActive()->theme;
    const AbilityDefinition *super = ContentSuperAbility(&w->content, player->cls);
    const AbilityDefinition *secondary =
        ContentSecondaryAbility(&w->content, player->cls);
    float superProgress = Clamp(player->superCharge, 0.0f, 1.0f);
    bool superReady = superProgress >= 1.0f;
    DrawAbilityTile(UiRefRect(920, 666, 336, 110), "ULTIMATE", super->name,
                    UiBindingLabel("RMB", "RB", "SUPER"), superProgress, superReady,
                    CueColor(w, t->gold));

    if (secondary)
    {
        float progress;
        bool ready;
        char secondaryName[96];
        snprintf(secondaryName, sizeof(secondaryName), "%s",
                 secondary->name);
        if (secondary->behavior == ABILITY_BEHAVIOR_SHIELD)
        {
            if (player->shieldBrokenTimer > 0.0f)
            {
                progress = Clamp(
                    1.0f - player->shieldBrokenTimer/
                           secondary->data.shield.breakLockout,
                    0.0f, 1.0f);
                snprintf(secondaryName, sizeof(secondaryName),
                         "%s // BROKEN %.1fs", secondary->name,
                         player->shieldBrokenTimer);
                ready = false;
            }
            else
            {
                progress = Clamp(
                    player->shieldCharge/
                    secondary->data.shield.capacity, 0.0f, 1.0f);
                snprintf(secondaryName, sizeof(secondaryName),
                         "%s // %.0f", secondary->name,
                         player->shieldCharge);
                ready = player->shieldActive ||
                        (player->shieldCharge > 0.0f &&
                         !player->shieldRearmRequired);
            }
        }
        else
        {
            progress = player->mobilityCooldown <= 0.0f ? 1.0f :
                Clamp(1.0f - player->mobilityCooldown/secondary->cooldown,
                      0.0f, 1.0f);
            ready = player->mobilityCooldown <= 0.0f;
            if (secondary->behavior == ABILITY_BEHAVIOR_GRAPPLE &&
                w->controller.aimingSecondary)
                snprintf(secondaryName, sizeof(secondaryName),
                         "%s // AIMING", secondary->name);
            else if (secondary->behavior == ABILITY_BEHAVIOR_GRAPPLE &&
                     BrawlerIsGrappling(player))
                snprintf(secondaryName, sizeof(secondaryName),
                         "%s // PULLING", secondary->name);
            else if (secondary->behavior == ABILITY_BEHAVIOR_MINE)
            {
                bool armed = false;
                if (WeaponsMineActive(AppGameContext(w),
                                      w->session.playerIdx, &armed))
                    snprintf(secondaryName, sizeof(secondaryName),
                             "%s // %s", secondary->name,
                             armed ? "ACTIVE" : "ARMING");
            }
        }
        DrawAbilityTile(UiRefRect(920, 548, 336, 104), "SECONDARY",
                        secondaryName,
                        UiBindingLabel("SHIFT", "LB", "SKILL"), progress,
                        ready,
                        CueColor(w, t->blue));
    }
}

static const char *NextTutorial(const App *w, const Brawler *player,
                                const char **binding)
{
    int flags = w->uiPreferences.tutorialFlags;
    if (!(flags & TUTORIAL_MOVE))
    {
        *binding = UiBindingLabel("WASD", "LEFT STICK", "LEFT STICK");
        return "Move through the arena";
    }
    if (!(flags & TUTORIAL_AIM))
    {
        *binding = UiBindingLabel("HOLD LMB", "HOLD RT", "DRAG ATTACK");
        return "Aim your main attack";
    }
    if (!(flags & TUTORIAL_FIRE))
    {
        *binding = UiBindingLabel("RELEASE LMB", "RELEASE RT", "RELEASE");
        return "Release to fire";
    }
    if (!(flags & TUTORIAL_QUICK))
    {
        *binding = UiBindingLabel("SPACE", "A", "TAP ATTACK");
        return "Try a quick auto-aim shot";
    }
    if (ContentSecondaryAbility(&w->content, player->cls) &&
        !(flags & TUTORIAL_MOBILITY))
    {
        *binding = UiBindingLabel("SHIFT", "LB", "SKILL");
        return "Use your brawler ability";
    }
    if (player->superCharge >= 1.0f && !(flags & TUTORIAL_SUPER))
    {
        *binding = UiBindingLabel("RMB", "RB", "SUPER");
        return "Your ultimate is ready";
    }
    if (w->session.sandbox && !(flags & TUTORIAL_COMMAND))
    {
        *binding = UiBindingLabel("TAB", "VIEW", "PRACTICE TOOLS");
        return "Open the command center";
    }
    return NULL;
}

static void DrawTutorial(const App *w, const Brawler *player)
{
    if (!w->uiPreferences.showTutorialHints) return;
    const char *binding = NULL;
    const char *prompt = NextTutorial(w, player, &binding);
    if (!prompt) return;
    const UiTheme *t = UiSystemActive()->theme;
    Rectangle chip = UiRefRect(430, 700, 420, 58);
    UiDrawPanel(chip, t->ink, t->paper, true);
    UiDrawSignalRail(chip, t->yellow, false);
    UiDrawKeycap(UiRefRect(444, 711, 104, 36), binding, true);
    UiDrawTextFit(UI_TEXT_BODY, prompt, UiRefRect(566, 710, 266, 38),
                  UI_ALIGN_LEFT, t->paper);
}
#endif

static void DrawDowned(const Brawler *player, float respawnTotal)
{
    if (respawnTotal < 0.1f) respawnTotal = 0.1f;
    const UiTheme *t = UiSystemActive()->theme;
#if defined(BRAWL_MOBILE)
    UiSystem *ui = UiSystemActive();
    UiPhoneFrame frame = UiPhoneFrameForViewport(
        GetScreenWidth(), GetScreenHeight(), ui->insets);
    UiFrameLayout previous = UiPhoneApplyFrame(ui, frame);
    float width = frame.referenceWidth;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 30, 5, 10, 110 });
    Rectangle panel = UiRefRect(width*0.20f, 148, width*0.60f, 210);
    UiDrawFeaturePanel(panel, t->ink, t->enemy, true);
    UiDrawDecoration(UI_DECORATION_SPEED_LINES, panel, t->enemy, 0.13f);
    UiDrawSignalRail(panel, t->enemy, false);
    UiDrawTextAligned(
        UI_TEXT_TITLE, "BRAWLER DOWN",
        UiRefRect(width*0.20f + 28, 172, width*0.60f - 56, 64),
        UI_ALIGN_CENTER, t->enemy);
    char timer[64];
    snprintf(timer, sizeof(timer), "REDEPLOY IN %.1f SECONDS", player->respawnTimer);
    UiDrawTextAligned(
        UI_TEXT_DATA, timer,
        UiRefRect(width*0.20f + 28, 246, width*0.60f - 56, 46),
        UI_ALIGN_CENTER, t->paper);
    UiDrawProgress(
        UiRefRect(width*0.20f + 68, 318, width*0.60f - 136, 12),
        1.0f - Clamp(player->respawnTimer/respawnTotal, 0.0f, 1.0f),
        t->yellow, false, 0);
    UiPhoneRestoreFrame(ui, previous);
#else
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 30, 5, 10, 110 });
    Rectangle panel = UiRefRect(410, 296, 460, 190);
    UiDrawFeaturePanel(panel, t->ink, t->enemy, true);
    UiDrawDecoration(UI_DECORATION_SPEED_LINES, panel, t->enemy, 0.13f);
    UiDrawSignalRail(panel, t->enemy, false);
    UiDrawTextAligned(UI_TEXT_TITLE, "BRAWLER DOWN", UiRefRect(438, 320, 404, 60),
                      UI_ALIGN_CENTER, t->enemy);
    char timer[64];
    snprintf(timer, sizeof(timer), "REDEPLOY IN %.1f SECONDS", player->respawnTimer);
    UiDrawTextAligned(UI_TEXT_DATA, timer, UiRefRect(438, 396, 404, 42),
                      UI_ALIGN_CENTER, t->paper);
    UiDrawProgress(UiRefRect(474, 454, 332, 10),
                   1.0f - Clamp(player->respawnTimer/respawnTotal, 0.0f, 1.0f),
                   t->yellow, false, 0);
#endif
}

#if defined(BRAWL_MOBILE)
static void DrawResultMobile(App *w)
{
    UiSystem *ui = UiSystemActive();
    const UiTheme *t = ui->theme;
    bool won = w->session.match.winner == TEAM_PLAYER;
    UiPhoneFrame frame = UiPhoneFrameForViewport(
        GetScreenWidth(), GetScreenHeight(), ui->insets);
    UiFrameLayout previous = UiPhoneApplyFrame(ui, frame);
    UiPhoneResultLayout layout = UiPhoneResultLayoutForFrame(frame);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 2, 6, 12, 236 });
    Vector2 left[4] = {
        { layout.canvas.x, layout.canvas.y },
        { layout.canvas.x, layout.canvas.y + layout.canvas.height },
        { layout.canvas.x + layout.canvas.width*0.43f,
          layout.canvas.y + layout.canvas.height },
        { layout.canvas.x + layout.canvas.width*0.61f, layout.canvas.y }
    };
    Vector2 right[4] = {
        { layout.canvas.x + layout.canvas.width*0.61f, layout.canvas.y },
        { layout.canvas.x + layout.canvas.width*0.43f,
          layout.canvas.y + layout.canvas.height },
        { layout.canvas.x + layout.canvas.width,
          layout.canvas.y + layout.canvas.height },
        { layout.canvas.x + layout.canvas.width, layout.canvas.y }
    };
    DrawTriangleFan(left, 4, won ? t->blue : t->enemy);
    DrawTriangleFan(right, 4, won ? t->yellow : t->surfaceRaised);
    UiDrawDecoration(UI_DECORATION_HALFTONE, layout.canvas, t->ink, 0.18f);

    Color outcome = won ? t->ally : t->enemy;
    UiDrawFeaturePanel(layout.panel, t->ink, t->paper, true);
    UiDrawDecoration(UI_DECORATION_BURST, layout.motif,
                     won ? t->yellow : t->enemy, 0.24f);
    UiDrawSignalRail(layout.panel, outcome, false);
    UiDrawTextAligned(UI_TEXT_RESULT, won ? "VICTORY" : "DEFEAT",
                      layout.title, UI_ALIGN_CENTER, outcome);

    const Brawler *player = &w->session.brawlers[w->session.playerIdx];
    const CharacterDefinition *character =
        ContentCharacter(&w->content, player->cls);
    if (character)
    {
        const CharacterUiStyle *style = ContentCharacterUiStyle(player->cls);
        UiDrawCharacterMotif(style->motif, layout.motif,
                             style->primary, style->secondary, 0.30f);
        UiDrawTextAligned(UI_TEXT_HEADING, character->displayName,
                          layout.character, UI_ALIGN_CENTER, t->paper);
        UiDrawTextAligned(UI_TEXT_CAPTION, style->impactLabel,
                          layout.impact, UI_ALIGN_CENTER, style->secondary);
    }

    char score[48];
    snprintf(score, sizeof(score), "%d  —  %d",
             w->session.match.teamGems[TEAM_PLAYER],
             w->session.match.teamGems[TEAM_ENEMY]);
    UiDrawTextAligned(UI_TEXT_DISPLAY, score, layout.score,
                      UI_ALIGN_CENTER, t->paper);
    char summary[96];
    snprintf(summary, sizeof(summary), "%d KOs  //  %d DOWNS  //  %s",
             w->session.kills, w->session.deaths,
             won ? "TEAM SECURED" : "TEAM OVERRUN");
    UiDrawTextAligned(UI_TEXT_DATA, summary, layout.summary,
                      UI_ALIGN_CENTER, t->textSecondary);

    UiResponse continueButton = UiButton(
        UiHash("result.continue"), layout.actions[0],
        "CONTINUE", UI_BUTTON_YELLOW, UI_ICON_NEXT);
    UiResponse rematchButton = UiButton(
        UiHash("result.rematch"), layout.actions[1],
        "REMATCH", UI_BUTTON_PRIMARY, UI_ICON_PRACTICE);
    UiResponse changeButton = UiButton(
        UiHash("result.change"), layout.actions[2],
        "CHANGE BRAWLER", UI_BUTTON_BLUE, UI_ICON_CONTROLS);
    if (continueButton.activated) g_resultAction = HUD_RESULT_CONTINUE;
    else if (rematchButton.activated)
        g_resultAction = HUD_RESULT_REMATCH;
    else if (changeButton.activated)
        g_resultAction = HUD_RESULT_CHANGE_BRAWLER;

    int remaining =
        (int)ceilf(w->tune.matchResultHold - w->session.match.overTimer);
    if (remaining < 0) remaining = 0;
    char fallback[80];
    snprintf(fallback, sizeof(fallback), "AUTO RETURN IN %d", remaining);
    UiDrawTextAligned(UI_TEXT_CAPTION, fallback, layout.fallback,
                      UI_ALIGN_CENTER, t->textMuted);
    UiPhoneRestoreFrame(ui, previous);
}
#endif

static void DrawResult(App *w)
{
#if defined(BRAWL_MOBILE)
    DrawResultMobile(w);
    return;
#endif
    const UiTheme *t = UiSystemActive()->theme;
    bool won = w->session.match.winner == TEAM_PLAYER;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 2, 6, 12, 236 });
    Rectangle canvas = UiRefRect(76, 56, 1128, 688);
    Vector2 left[4] = {
        { canvas.x, canvas.y },
        { canvas.x, canvas.y + canvas.height },
        { canvas.x + canvas.width*0.43f, canvas.y + canvas.height },
        { canvas.x + canvas.width*0.61f, canvas.y }
    };
    Vector2 right[4] = {
        { canvas.x + canvas.width*0.61f, canvas.y },
        { canvas.x + canvas.width*0.43f, canvas.y + canvas.height },
        { canvas.x + canvas.width, canvas.y + canvas.height },
        { canvas.x + canvas.width, canvas.y }
    };
    DrawTriangleFan(left, 4, won ? t->blue : t->enemy);
    DrawTriangleFan(right, 4, won ? t->yellow : t->surfaceRaised);
    UiDrawDecoration(UI_DECORATION_HALFTONE, canvas, t->ink, 0.18f);

    Rectangle panel = UiRefRect(124, 84, 1032, 624);
    Color outcome = won ? t->ally : t->enemy;
    UiDrawFeaturePanel(panel, t->ink, t->paper, true);
    UiDrawDecoration(UI_DECORATION_BURST, UiRefRect(414, 148, 452, 452),
                     won ? t->yellow : t->enemy, 0.24f);
    UiDrawSignalRail(panel, outcome, false);
    UiDrawTextAligned(UI_TEXT_RESULT, won ? "VICTORY" : "DEFEAT",
                      UiRefRect(176, 104, 928, 104), UI_ALIGN_CENTER, outcome);
    const CharacterDefinition *character = ContentCharacter(
        &w->content, w->session.brawlers[w->session.playerIdx].cls);
    if (character)
    {
        const CharacterUiStyle *style = ContentCharacterUiStyle(
            w->session.brawlers[w->session.playerIdx].cls);
        UiDrawCharacterMotif(style->motif,
                             UiRefRect(430, 190, 420, 310),
                             style->primary,
                             style->secondary, 0.32f);
        UiDrawTextAligned(UI_TEXT_HEADING, character->displayName,
                          UiRefRect(428, 216, 424, 44),
                          UI_ALIGN_CENTER, t->paper);
        UiDrawTextAligned(UI_TEXT_CAPTION, style->impactLabel,
                          UiRefRect(428, 258, 424, 28),
                          UI_ALIGN_CENTER, style->secondary);
    }

    char score[48];
    snprintf(score, sizeof(score), "%d  —  %d",
             w->session.match.teamGems[TEAM_PLAYER],
             w->session.match.teamGems[TEAM_ENEMY]);
    UiDrawTextAligned(UI_TEXT_DISPLAY, score, UiRefRect(390, 316, 500, 84),
                      UI_ALIGN_CENTER, t->paper);
    char summary[96];
    snprintf(summary, sizeof(summary), "%d KOs  //  %d DOWNS  //  %s",
             w->session.kills, w->session.deaths,
             won ? "TEAM SECURED" : "TEAM OVERRUN");
    UiDrawTextAligned(UI_TEXT_DATA, summary, UiRefRect(280, 430, 720, 38),
                      UI_ALIGN_CENTER, t->textSecondary);

    UiResponse continueButton =
        UiButton(UiHash("result.continue"), UiRefRect(158, 586, 292, 72),
                 "CONTINUE", UI_BUTTON_YELLOW, UI_ICON_NEXT);
    UiResponse rematchButton =
        UiButton(UiHash("result.rematch"), UiRefRect(494, 586, 292, 72),
                 "REMATCH", UI_BUTTON_PRIMARY, UI_ICON_PRACTICE);
    UiResponse changeButton =
        UiButton(UiHash("result.change"), UiRefRect(830, 586, 292, 72),
                 "CHANGE BRAWLER", UI_BUTTON_BLUE, UI_ICON_CONTROLS);
    if (continueButton.activated) g_resultAction = HUD_RESULT_CONTINUE;
    else if (rematchButton.activated)
        g_resultAction = HUD_RESULT_REMATCH;
    else if (changeButton.activated)
        g_resultAction = HUD_RESULT_CHANGE_BRAWLER;

    int remaining = (int)ceilf(w->tune.matchResultHold - w->session.match.overTimer);
    if (remaining < 0) remaining = 0;
    char fallback[80];
    snprintf(fallback, sizeof(fallback), "AUTO RETURN IN %d", remaining);
    UiDrawTextAligned(UI_TEXT_CAPTION, fallback, UiRefRect(486, 670, 308, 24),
                      UI_ALIGN_CENTER, t->textMuted);
}

void HudDrawPanel(App *w)
{
    if (w->session.brawlerCount <= 0) return;
    Brawler *player = &w->session.brawlers[w->session.playerIdx];
    DrawObjective(w);
    DrawImpactStamp(w);
#if !defined(BRAWL_MOBILE)
    DrawAbilities(w, player);
    DrawTutorial(w, player);
#endif

    if (!player->alive) DrawDowned(player, w->tune.playerRespawn);
    if (w->tune.gemGrab && !w->session.sandbox &&
        w->session.match.phase == MATCH_OVER)
        DrawResult(w);
}
