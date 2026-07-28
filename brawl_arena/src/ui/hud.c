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
static void CenteredWorldText(const char *text, int centerX, int y,
                              UiTextRole role, Color color)
{
    Vector2 size = UiMeasureText(role, text);
    UiDrawTextShadow(role, text, (Vector2){ centerX - size.x*0.5f, (float)y }, color);
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
    if (input->mobilityPressed) w->uiPreferences.tutorialFlags |= TUTORIAL_MOBILITY;
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
        float width = UiScale(mine ? 68.0f : 50.0f);
        float height = UiScale(mine ? 8.0f : 6.0f);
        Rectangle bar = { screen.x - width*0.5f, screen.y, width, height };
        float health = b->maxHealth > 0 ? (float)b->health/b->maxHealth : 0.0f;
        Color team = b->team == TEAM_PLAYER ? CueColor(w, t->ally) : CueColor(w, t->enemy);
        if (health < 0.30f) team = t->enemy;

        char number[24];
        snprintf(number, sizeof(number), "%d", b->health);
        CenteredWorldText(number, (int)screen.x, (int)(screen.y - UiScale(20)),
                          UI_TEXT_CAPTION, mine ? t->paper : t->mist);

        DrawRectangleRec((Rectangle){ bar.x - UiScale(2), bar.y - UiScale(2),
                                      bar.width + UiScale(4), bar.height + UiScale(4) },
                         t->shadow);
        DrawRectangleRec(bar, t->hull);
        Rectangle healthFill = bar;
        healthFill.width *= Clamp(health, 0.0f, 1.0f);
        DrawRectangleRec(healthFill, team);

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
    UiDrawPanel(bounds, t->deck, t->line, true);
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

static void DrawVitals(App *w, const Brawler *player)
{
    const UiTheme *t = UiSystemActive()->theme;
    const CharacterDefinition *character = ContentCharacter(&w->content, player->cls);
    Rectangle panel = UiRefRect(24, 642, 360, 134);
    UiDrawPanel(panel, t->deck, t->line, true);
    UiDrawSignalRail(panel, CueColor(w, t->ally), false);
    UiDrawText(UI_TEXT_CAPTION, character->displayName, UiRefPoint(48, 654), t->ally);
    char health[64];
    snprintf(health, sizeof(health), "%d / %d", player->health, player->maxHealth);
    UiDrawText(UI_TEXT_HEADING, health, UiRefPoint(48, 676), t->paper);
    UiDrawProgress(UiRefRect(48, 716, 310, 14),
                   player->maxHealth > 0 ? (float)player->health/player->maxHealth : 0,
                   CueColor(w, t->ally), false, 0);
    DrawAmmo(UiRefRect(48, 744, 310, 10), player->ammo, character->maxAmmo, t->ion);
    UiDrawText(UI_TEXT_CAPTION, "INTEGRITY", UiRefPoint(278, 682), t->muted);
    UiDrawText(UI_TEXT_CAPTION, "AMMO", UiRefPoint(305, 758), t->muted);
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
    const AbilityDefinition *mobility = ContentMobilityAbility(&w->content, player->cls);
    float superProgress = Clamp(player->superCharge, 0.0f, 1.0f);
    bool superReady = superProgress >= 1.0f;
    DrawAbilityTile(UiRefRect(920, 666, 336, 110), "ULTIMATE", super->name,
                    UiBindingLabel("RMB", "RB"), superProgress, superReady,
                    CueColor(w, t->ready));

    if (mobility)
    {
        float progress = player->mobilityCooldown <= 0.0f ? 1.0f :
            Clamp(1.0f - player->mobilityCooldown/mobility->cooldown, 0.0f, 1.0f);
        DrawAbilityTile(UiRefRect(920, 548, 336, 104), "BRAWLER ABILITY", mobility->name,
                        UiBindingLabel("SHIFT", "LB"), progress,
                        player->mobilityCooldown <= 0.0f, CueColor(w, t->ion));
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
    if (ContentMobilityAbility(&w->content, player->cls) &&
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

static void DrawDowned(const Brawler *player)
{
    const UiTheme *t = UiSystemActive()->theme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 30, 5, 10, 110 });
    Rectangle panel = UiRefRect(410, 296, 460, 190);
    UiDrawPanel(panel, t->deckRaised, t->enemy, true);
    UiDrawSignalRail(panel, t->enemy, false);
    UiDrawTextAligned(UI_TEXT_TITLE, "BRAWLER DOWN", UiRefRect(438, 320, 404, 60),
                      UI_ALIGN_CENTER, t->enemy);
    char timer[64];
    snprintf(timer, sizeof(timer), "REDEPLOY IN %.1f SECONDS", player->respawnTimer);
    UiDrawTextAligned(UI_TEXT_DATA, timer, UiRefRect(438, 396, 404, 42),
                      UI_ALIGN_CENTER, t->paper);
    UiDrawProgress(UiRefRect(474, 454, 332, 10),
                   1.0f - Clamp(player->respawnTimer/5.0f, 0.0f, 1.0f),
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
    UiDrawPanel(panel, t->deckRaised, outcome, true);
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
    DrawVitals(w, player);
    DrawAbilities(w, player);
    DrawTutorial(w, player);

    if (!player->alive) DrawDowned(player);
    if (w->tune.gemGrab && !w->session.sandbox &&
        w->session.match.phase == MATCH_OVER)
        DrawResult(w);
}
