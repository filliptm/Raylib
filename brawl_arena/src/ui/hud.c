#include "hud.h"

#include "command_center.h"
#include "config.h"
#include "content_catalog.h"
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

static bool g_continueRequested;

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

static void DrawAmmo(Rectangle bounds, float ammo, int maxAmmo, Color fill)
{
    if (maxAmmo < 1) return;
    UiDrawProgress(bounds, Clamp(ammo/maxAmmo, 0.0f, 1.0f), fill, true, maxAmmo);
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

bool HudConsumeContinue(void)
{
    bool requested = g_continueRequested;
    g_continueRequested = false;
    return requested;
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
            DrawRectangleRec(
                (Rectangle){ shellBar.x - UiScale(1),
                             shellBar.y - UiScale(1),
                             shellBar.width + UiScale(2),
                             shellBar.height + UiScale(2) },
                t->shadow);
            DrawRectangleRec(shellBar, t->hull);
            Rectangle shellFill = shellBar;
            shellFill.width *= Clamp(shieldRatio, 0.0f, 1.0f);
            DrawRectangleRec(
                shellFill,
                b->shieldBrokenTimer > 0.0f
                    ? t->enemy : CueColor(w, t->ion));
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

        DrawRectangleRec((Rectangle){ bar.x - UiScale(2), bar.y - UiScale(2),
                                      bar.width + UiScale(4), bar.height + UiScale(4) },
                         t->shadow);
        DrawRectangleRec(bar, t->hull);
        Rectangle healthFill = bar;
        healthFill.width *= Clamp(health, 0.0f, 1.0f);
        DrawRectangleRec(healthFill, team);
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
            DrawAmmo((Rectangle){ bar.x, bar.y + bar.height + UiScale(6),
                                  bar.width, UiScale(5) },
                     b->ammo, ContentCharacter(&w->content, b->cls)->maxAmmo, t->ion);
        }

        if (b->gems > 0)
        {
            char gems[16];
            snprintf(gems, sizeof(gems), "%d", b->gems);
            UiIconDraw(UI_ICON_GEM,
                       (Vector2){ screen.x - UiScale(12),
                                  bar.y + bar.height + UiScale(mine ? 25 : 15) },
                       UiScale(11), t->reactor);
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
    UiDrawFeaturePanel(bounds, t->deck, t->line, true);
    UiDrawDecoration(UI_DECORATION_RADAR_DISC, UiRefRect(594, 25, 84, 84),
                     t->reactor, 0.055f);
    UiDrawSignalRail(bounds, t->reactor, false);
    UiDrawTextAligned(UI_TEXT_CAPTION,
                      w->session.sandbox ? "PRACTICE TELEMETRY" :
                      (w->tune.gemGrab ? "GEM GRAB // FIRST TO TARGET" : "SKIRMISH"),
                      UiRefRect(454, 28, 372, 22), UI_ALIGN_CENTER, t->muted);

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
                          UI_ALIGN_CENTER, t->ready);
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

static void DrawAbilityTile(Rectangle bounds, const char *eyebrow, const char *name,
                            const char *binding, float progress, bool ready,
                            Color accent)
{
    const UiTheme *t = UiSystemActive()->theme;
    UiDrawPanel(bounds, ready ? t->deckRaised : t->deck,
                ready ? accent : t->line, true);
    if (ready) UiDrawSignalRail(bounds, accent, true);
    UiDrawText(UI_TEXT_CAPTION, eyebrow,
               (Vector2){ bounds.x + UiScale(14), bounds.y + UiScale(9) },
               ready ? accent : t->muted);
    Rectangle nameBounds = { bounds.x + UiScale(14), bounds.y + UiScale(25),
                             bounds.width - UiScale(28), UiScale(28) };
    UiDrawTextFit(UI_TEXT_EMPHASIS, name, nameBounds, UI_ALIGN_LEFT,
                  ready ? t->paper : t->mist);
    UiDrawKeycap((Rectangle){ bounds.x + UiScale(14),
                              bounds.y + bounds.height - UiScale(34),
                              UiScale(86), UiScale(24) }, binding, ready);
    UiDrawProgress((Rectangle){ bounds.x + UiScale(112),
                                bounds.y + bounds.height - UiScale(28),
                                bounds.width - UiScale(126), UiScale(8) },
                   progress, ready ? accent : t->hullBright, false, 0);
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
                    UiBindingLabel("RMB", "RB"), superProgress, superReady,
                    CueColor(w, t->ready));

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
        }
        DrawAbilityTile(UiRefRect(920, 548, 336, 104), "SECONDARY",
                        secondaryName,
                        UiBindingLabel("SHIFT", "LB"), progress,
                        ready,
                        CueColor(w, t->ion));
    }
}

static const char *NextTutorial(const App *w, const Brawler *player,
                                const char **binding)
{
    int flags = w->uiPreferences.tutorialFlags;
    if (!(flags & TUTORIAL_MOVE))
    {
        *binding = UiBindingLabel("WASD", "LEFT STICK");
        return "Move through the arena";
    }
    if (!(flags & TUTORIAL_AIM))
    {
        *binding = UiBindingLabel("HOLD LMB", "HOLD RT");
        return "Aim your main attack";
    }
    if (!(flags & TUTORIAL_FIRE))
    {
        *binding = UiBindingLabel("RELEASE LMB", "RELEASE RT");
        return "Release to fire";
    }
    if (!(flags & TUTORIAL_QUICK))
    {
        *binding = UiBindingLabel("SPACE", "A");
        return "Try a quick auto-aim shot";
    }
    if (ContentSecondaryAbility(&w->content, player->cls) &&
        !(flags & TUTORIAL_MOBILITY))
    {
        *binding = UiBindingLabel("SHIFT", "LB");
        return "Use your brawler ability";
    }
    if (player->superCharge >= 1.0f && !(flags & TUTORIAL_SUPER))
    {
        *binding = UiBindingLabel("RMB", "RB");
        return "Your ultimate is ready";
    }
    if (w->session.sandbox && !(flags & TUTORIAL_COMMAND))
    {
        *binding = UiBindingLabel("TAB", "VIEW");
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
    UiDrawPanel(chip, t->deck, t->hullBright, true);
    UiDrawKeycap(UiRefRect(444, 711, 104, 36), binding, true);
    UiDrawTextFit(UI_TEXT_BODY, prompt, UiRefRect(566, 710, 266, 38),
                  UI_ALIGN_LEFT, t->paper);
}

static void DrawDowned(const Brawler *player, float respawnTotal)
{
    if (respawnTotal < 0.1f) respawnTotal = 0.1f;
    const UiTheme *t = UiSystemActive()->theme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 30, 5, 10, 110 });
    Rectangle panel = UiRefRect(410, 296, 460, 190);
    UiDrawFeaturePanel(panel, t->deckRaised, t->enemy, true);
    UiDrawSignalRail(panel, t->enemy, false);
    UiDrawTextAligned(UI_TEXT_TITLE, "BRAWLER DOWN", UiRefRect(438, 320, 404, 60),
                      UI_ALIGN_CENTER, t->enemy);
    char timer[64];
    snprintf(timer, sizeof(timer), "REDEPLOY IN %.1f SECONDS", player->respawnTimer);
    UiDrawTextAligned(UI_TEXT_DATA, timer, UiRefRect(438, 396, 404, 42),
                      UI_ALIGN_CENTER, t->paper);
    UiDrawProgress(UiRefRect(474, 454, 332, 10),
                   1.0f - Clamp(player->respawnTimer/respawnTotal, 0.0f, 1.0f),
                   t->safety, false, 0);
}

static void DrawResult(App *w)
{
    const UiTheme *t = UiSystemActive()->theme;
    bool won = w->session.match.winner == TEAM_PLAYER;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 2, 6, 12, 225 });
    Rectangle panel = UiRefRect(330, 142, 620, 516);
    Color outcome = won ? t->ally : t->enemy;
    UiDrawFeaturePanel(panel, t->deckRaised, outcome, true);
    UiDrawDecoration(UI_DECORATION_ORBITAL_RING, UiRefRect(442, 188, 396, 396),
                     outcome, 0.045f);
    UiDrawSignalRail(panel, outcome, false);
    UiDrawTextAligned(UI_TEXT_RESULT, won ? "VICTORY" : "DEFEAT",
                      UiRefRect(372, 174, 536, 108), UI_ALIGN_CENTER, outcome);
    UiDrawTextAligned(UI_TEXT_CAPTION, "FINAL BROADCAST",
                      UiRefRect(372, 278, 536, 24), UI_ALIGN_CENTER, t->muted);

    char score[48];
    snprintf(score, sizeof(score), "%d  —  %d",
             w->session.match.teamGems[TEAM_PLAYER],
             w->session.match.teamGems[TEAM_ENEMY]);
    UiDrawTextAligned(UI_TEXT_DISPLAY, score, UiRefRect(394, 310, 492, 80),
                      UI_ALIGN_CENTER, t->paper);
    char summary[96];
    snprintf(summary, sizeof(summary), "%d KOs  /  %d downs",
             w->session.kills, w->session.deaths);
    UiDrawTextAligned(UI_TEXT_DATA, summary, UiRefRect(394, 398, 492, 38),
                      UI_ALIGN_CENTER, t->mist);

    UiResponse continueButton =
        UiButton(UiHash("result.continue"), UiRefRect(484, 490, 312, 78),
                 "CONTINUE", UI_BUTTON_PRIMARY, UI_ICON_NEXT);
    if (continueButton.activated) g_continueRequested = true;

    int remaining = (int)ceilf(w->tune.matchResultHold - w->session.match.overTimer);
    if (remaining < 0) remaining = 0;
    char fallback[80];
    snprintf(fallback, sizeof(fallback), "AUTO RETURN IN %d", remaining);
    UiDrawTextAligned(UI_TEXT_CAPTION, fallback, UiRefRect(486, 584, 308, 24),
                      UI_ALIGN_CENTER, t->muted);
}

void HudDrawPanel(App *w)
{
    if (w->session.brawlerCount <= 0) return;
    Brawler *player = &w->session.brawlers[w->session.playerIdx];
    DrawObjective(w);
    DrawAbilities(w, player);
    DrawTutorial(w, player);

    if (!player->alive) DrawDowned(player, w->tune.playerRespawn);
    if (w->tune.gemGrab && !w->session.sandbox &&
        w->session.match.phase == MATCH_OVER)
        DrawResult(w);
}
