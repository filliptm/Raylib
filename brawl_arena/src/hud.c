#include "hud.h"
#include "command_center.h"
#include "weapons.h"
#include "effects.h"
#include "raymath.h"
#include <math.h>

static const Color PANEL_BG = { 12, 16, 26, 205 };
static const Color SUPER_GOLD = { 255, 206, 74, 255 };

//------------------------------------------------------------------------------------
void HudDrawBars(World *w)
{
    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (!b->alive || !b->visible) continue;

        Vector3 head = b->position;
        head.y = 2.60f;      // clears the helmet; brawlers stand about one tile tall
        Vector2 sp = GetWorldToScreen(head, w->camera);

        if (sp.x < -80 || sp.x > GetScreenWidth() + 80) continue;
        if (sp.y < -80 || sp.y > GetScreenHeight() + 80) continue;

        int bw = b->isPlayer ? 56 : 46;
        int bh = b->isPlayer ? 8 : 6;
        int x = (int)sp.x - bw / 2;
        int y = (int)sp.y;

        float ratio = (float)b->health / (float)b->maxHealth;
        if (ratio < 0.0f) ratio = 0.0f;

        DrawRectangle(x - 2, y - 2, bw + 4, bh + 4, (Color){ 0, 0, 0, 170 });
        DrawRectangle(x, y, bw, bh, (Color){ 40, 44, 54, 255 });

        Color fill = TEAM_COLORS[b->team];
        if (ratio < 0.3f) fill = ColorLerpC(fill, (Color){ 255, 90, 90, 255 }, 0.6f);
        DrawRectangle(x, y, (int)(bw * ratio), bh, fill);

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

//------------------------------------------------------------------------------------
static void DrawAmmoPips(int x, int y, float ammo)
{
    const int pipW = 42, pipH = 10, gap = 6;

    for (int i = 0; i < MAX_AMMO; i++)
    {
        float fill = Clamp(ammo - i, 0.0f, 1.0f);
        int px = x + i * (pipW + gap);

        DrawRectangleRounded((Rectangle){ px - 1, y - 1, pipW + 2, pipH + 2 }, 0.5f, 6, (Color){ 0, 0, 0, 150 });
        DrawRectangleRounded((Rectangle){ px, y, pipW, pipH }, 0.5f, 6, (Color){ 44, 50, 62, 255 });

        if (fill > 0.0f)
        {
            Color c = (fill >= 1.0f) ? (Color){ 120, 205, 255, 255 } : (Color){ 70, 130, 180, 255 };
            DrawRectangleRounded((Rectangle){ px, y, pipW * fill, pipH }, 0.5f, 6, c);
        }
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

    //--- Bottom-left kit panel --------------------------------------------------
    // Slide clear of the command center rather than hiding behind it.
    int px = CommandCenterIsOpen() ? 404 : 20;
    int py = sh - 132, pw = 330, ph = 112;
    DrawRectangleRounded((Rectangle){ px, py, pw, ph }, 0.09f, 8, PANEL_BG);

    DrawText(def->name, px + 16, py + 12, 24, WHITE);
    DrawText(def->flavor, px + 16, py + 38, 13, (Color){ 150, 162, 180, 255 });

    // Health
    float hpRatio = p->alive ? (float)p->health / (float)p->maxHealth : 0.0f;
    int hbX = px + 16, hbY = py + 60, hbW = pw - 32, hbH = 14;

    DrawRectangleRounded((Rectangle){ hbX, hbY, hbW, hbH }, 0.5f, 6, (Color){ 40, 44, 54, 255 });
    Color hpColor = (hpRatio < 0.3f) ? (Color){ 235, 90, 90, 255 } : (Color){ 90, 210, 120, 255 };
    if (hpRatio > 0.0f)
        DrawRectangleRounded((Rectangle){ hbX, hbY, hbW * hpRatio, hbH }, 0.5f, 6, hpColor);

    const char *hpText = TextFormat("%d / %d", p->health, p->maxHealth);
    int htw = MeasureText(hpText, 12);
    DrawText(hpText, hbX + hbW / 2 - htw / 2, hbY + 1, 12, WHITE);

    // Ammo
    DrawAmmoPips(px + 16, py + 84, p->ammo);

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
