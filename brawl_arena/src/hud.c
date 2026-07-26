#include "hud.h"
#include "weapons.h"
#include "effects.h"
#include "raymath.h"
#include <math.h>

static const Color PANEL_BG = { 12, 16, 26, 205 };
static const Color SUPER_GOLD = { 255, 206, 74, 255 };
static const Color HEALTH_GREEN = { 78, 216, 108, 255 };
static const Color AMMO_FULL = { 122, 206, 255, 255 };
static const Color AMMO_PART = { 62, 122, 176, 255 };
static const Color SLOT_EMPTY = { 40, 46, 58, 255 };

// Text with a hard drop shadow, so readouts stay legible over any part of the arena.
static void DrawCenteredLabel(const char *text, int centerX, int y, int size, Color color)
{
    int tw = MeasureText(text, size);
    DrawText(text, centerX - tw / 2 + 1, y + 1, size, (Color){ 0, 0, 0, 200 });
    DrawText(text, centerX - tw / 2, y, size, color);
}

// Reload state as segmented tabs, one per ammo pip. Ammo is fractional, so a partly
// refilled pip shows how far along the reload is.
static void DrawAmmoTabs(int x, int y, int width, float ammo)
{
    const int gap = 3;
    const int h = 5;
    int tabW = (width - gap * (MAX_AMMO - 1)) / MAX_AMMO;
    if (tabW < 1) return;

    for (int i = 0; i < MAX_AMMO; i++)
    {
        int tx = x + i * (tabW + gap);
        float fill = Clamp(ammo - i, 0.0f, 1.0f);

        DrawRectangle(tx - 1, y - 1, tabW + 2, h + 2, (Color){ 0, 0, 0, 180 });
        DrawRectangle(tx, y, tabW, h, SLOT_EMPTY);

        if (fill > 0.0f)
            DrawRectangle(tx, y, (int)(tabW * fill), h, (fill >= 1.0f) ? AMMO_FULL : AMMO_PART);
    }
}

//------------------------------------------------------------------------------------
void HudDrawBars(World *w)
{
    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (!b->alive || !b->visible) continue;

        Vector3 head = b->position;
        // Brawlers stand about one tile tall. Your own cluster is taller than an
        // enemy's - number above, ammo tabs below - so it needs extra clearance or the
        // tabs sit on the helmet.
        head.y = b->isPlayer ? 3.05f : 2.60f;
        Vector2 sp = GetWorldToScreen(head, w->camera);

        if (sp.x < -80 || sp.x > GetScreenWidth() + 80) continue;
        if (sp.y < -80 || sp.y > GetScreenHeight() + 80) continue;

        bool mine = b->isPlayer;
        int bw = mine ? 62 : 46;
        int bh = mine ? 9 : 6;
        int x = (int)sp.x - bw / 2;
        int y = (int)sp.y;

        float ratio = (float)b->health / (float)b->maxHealth;
        if (ratio < 0.0f) ratio = 0.0f;

        // Health reads as a number above every bar. Yours is larger and cooler-toned;
        // enemies get a smaller, warmer figure so the two never get confused.
        DrawCenteredLabel(TextFormat("%d", b->health), (int)sp.x,
                          y - (mine ? 18 : 15), mine ? 16 : 13,
                          mine ? (Color){ 236, 248, 238, 255 } : (Color){ 255, 208, 208, 255 });

        DrawRectangle(x - 2, y - 2, bw + 4, bh + 4, (Color){ 0, 0, 0, 180 });
        DrawRectangle(x, y, bw, bh, SLOT_EMPTY);

        // Green for you, team colour for everyone else, reddening as they get low.
        Color fill = HEALTH_GREEN;
        if (!mine)
        {
            fill = TEAM_COLORS[b->team];
            if (ratio < 0.3f) fill = ColorLerpC(fill, (Color){ 255, 90, 90, 255 }, 0.6f);
        }
        DrawRectangle(x, y, (int)(bw * ratio), bh, fill);

        if (mine) DrawAmmoTabs(x, y + bh + 5, bw, b->ammo);

        // Super-ready pip beside the bar.
        if (b->superCharge >= 1.0f)
        {
            float pulse = 0.5f + 0.5f * sinf(w->time * 8.0f);
            Color c = SUPER_GOLD;
            c.a = (unsigned char)(160 + pulse * 95);
            DrawCircle(x + bw + 9, y + bh / 2, 5.0f, c);
        }

        if (b->inBush)
            DrawText("~", x - 14, y - 4, 16, (Color){ 130, 235, 150, 220 });
    }
}

void HudDrawPanel(World *w)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Brawler *p = &w->brawlers[w->playerIdx];
    const WeaponDef *def = &WEAPONS[p->cls];

    //--- Top bar: score ---------------------------------------------------------
    DrawRectangleGradientV(0, 0, sw, 64, (Color){ 0, 0, 0, 190 }, (Color){ 0, 0, 0, 0 });

    DrawText("KOs", 22, 10, 13, (Color){ 150, 160, 175, 255 });
    DrawText(TextFormat("%d", w->kills), 22, 25, 30, WHITE);

    DrawText("DOWNS", 100, 10, 13, (Color){ 150, 160, 175, 255 });
    DrawText(TextFormat("%d", w->deaths), 100, 25, 30, (Color){ 235, 130, 130, 255 });

    int alive = 0;
    for (int i = 0; i < w->brawlerCount; i++)
        if (w->brawlers[i].team == TEAM_ENEMY && w->brawlers[i].alive) alive++;

    const char *enemyText = TextFormat("%d ENEMIES", alive);
    int etw = MeasureText(enemyText, 20);
    DrawText(enemyText, sw - etw - 22, 26, 20, (Color){ 235, 150, 150, 255 });

    //--- Super meter, bottom-right ----------------------------------------------
    int sx = sw - 190, sy = sh - 122;
    bool ready = p->superCharge >= 1.0f;

    DrawRectangleRounded((Rectangle){ sx - 12, sy - 12, 172, 92 }, 0.12f, 8, PANEL_BG);

    if (ready)
    {
        float pulse = 0.5f + 0.5f * sinf(w->time * 7.0f);
        Color glow = SUPER_GOLD;
        glow.a = (unsigned char)(80 + pulse * 120);
        DrawRectangleRoundedLines((Rectangle){ sx - 12, sy - 12, 172, 92 }, 0.12f, 8, glow);
        DrawText(def->superName, sx, sy + 4, 20, SUPER_GOLD);
        DrawText("RIGHT-CLICK", sx, sy + 30, 13, (Color){ 220, 200, 150, 255 });
    }
    else
    {
        DrawText("SUPER", sx, sy + 4, 20, (Color){ 150, 160, 175, 255 });
        DrawText(TextFormat("%d%%", (int)(p->superCharge * 100)), sx, sy + 30, 13, (Color){ 150, 160, 175, 255 });
    }

    int mbY = sy + 54;
    DrawRectangleRounded((Rectangle){ sx, mbY, 148, 14 }, 0.5f, 6, (Color){ 40, 44, 54, 255 });
    if (p->superCharge > 0.0f)
    {
        Color fill = ready ? SUPER_GOLD : (Color){ 190, 150, 60, 255 };
        DrawRectangleRounded((Rectangle){ sx, mbY, 148 * p->superCharge, 14 }, 0.5f, 6, fill);
    }

    //--- Respawn overlay --------------------------------------------------------
    if (!p->alive)
    {
        DrawRectangle(0, 0, sw, sh, (Color){ 120, 20, 20, 60 });

        const char *downText = "DOWNED";
        int dtw = MeasureText(downText, 56);
        DrawText(downText, sw / 2 - dtw / 2, sh / 2 - 60, 56, (Color){ 255, 110, 110, 255 });

        const char *timer = TextFormat("Respawning in %.1f", p->respawnTimer);
        int ttw = MeasureText(timer, 24);
        DrawText(timer, sw / 2 - ttw / 2, sh / 2 + 4, 24, WHITE);
    }

    //--- Controls, fading out once you have found your feet ----------------------
    if (w->time < 22.0f)
    {
        int alpha = (int)(220 * Clamp((22.0f - w->time) / 6.0f, 0.0f, 1.0f));
        Color c = { 220, 228, 240, (unsigned char)alpha };
        int cy = 78;

        DrawText("WASD  move", sw - 250, cy, 15, c);
        DrawText("HOLD LMB  aim, release to fire", sw - 250, cy + 20, 15, c);
        DrawText("TAP LMB / SPACE  auto-aim shot", sw - 250, cy + 40, 15, c);
        DrawText("RMB  super (when charged)", sw - 250, cy + 60, 15, c);
        DrawText("1-4  swap kit     R  reset", sw - 250, cy + 80, 15, c);
        DrawText("TAB  command center", sw - 250, cy + 100, 15, (Color){ 92, 178, 255, (unsigned char)alpha });
    }
}
