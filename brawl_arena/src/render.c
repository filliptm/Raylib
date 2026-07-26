#include "render.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "effects.h"
#include "assets.h"
#include "rlgl.h"
#include <stddef.h>
#include "raymath.h"
#include <math.h>

//------------------------------------------------------------------------------------
// Palette. Textures carry the detail, these tint them.
//------------------------------------------------------------------------------------
static const Color FLOOR_TINT  = { 150, 163, 190, 255 };
static const Color WALL_TINT   = { 132, 146, 176, 255 };
static const Color WALL_CAP    = { 176, 190, 218, 255 };
static const Color CRATE_TINT  = { 205, 168, 122, 255 };
static const Color BUSH_TINT   = { 128, 205, 132, 255 };
static const Color SKIN_TINT   = { 232, 190, 158, 255 };

// Camera framing: pulled back and tilted, the way the mobile game reads.
static const Vector3 CAM_OFFSET = { 0.0f, 31.0f, -22.0f };

static Assets *g_assets = NULL;

void RenderSetAssets(Assets *a) { g_assets = a; }

//------------------------------------------------------------------------------------
static Matrix TRS(Vector3 scale, float yawRadians, Vector3 position)
{
    Matrix m = MatrixScale(scale.x, scale.y, scale.z);
    if (yawRadians != 0.0f) m = MatrixMultiply(m, MatrixRotateY(yawRadians));
    return MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
}

void CameraInit(World *w)
{
    w->camFocus = w->arena.playerSpawn;
    w->camera.position = Vector3Add(w->camFocus, CAM_OFFSET);
    w->camera.target = w->camFocus;
    w->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    w->camera.fovy = 46.0f;
    w->camera.projection = CAMERA_PERSPECTIVE;
}

void CameraUpdate(World *w, float dt)
{
    Brawler *p = &w->brawlers[w->playerIdx];

    Vector3 desired = p->alive ? p->position : w->camFocus;
    if (p->alive)
    {
        Vector3 lead = Vector3Subtract(w->aimPoint, p->position);
        lead.y = 0.0f;
        float len = Vector3Length(lead);
        if (len > 0.001f)
        {
            float clamped = fminf(len, 8.0f)*0.28f;
            lead = Vector3Scale(Vector3Normalize(lead), clamped);
            desired = Vector3Add(desired, lead);
        }
    }

    w->camFocus = Vector3Lerp(w->camFocus, desired, 6.0f*dt);

    Vector3 shake = { 0 };
    if (w->shake > 0.0f)
    {
        float s = w->shake*0.22f;
        shake.x = (GetRandomValue(-100, 100)/100.0f)*s;
        shake.y = (GetRandomValue(-100, 100)/100.0f)*s;
        shake.z = (GetRandomValue(-100, 100)/100.0f)*s;
    }

    w->camera.position = Vector3Add(Vector3Add(w->camFocus, CAM_OFFSET), shake);
    w->camera.target = Vector3Add(w->camFocus, shake);
}

//------------------------------------------------------------------------------------
// Light collection: rank every candidate emitter and hand the best few to the shader.
//------------------------------------------------------------------------------------
static void SubmitLights(World *w, Assets *a)
{
    Vector3 positions[MAX_SHADER_LIGHTS];
    Vector3 colors[MAX_SHADER_LIGHTS];
    float scores[MAX_SHADER_LIGHTS];
    int count = 0;

    // Insert into a fixed-size list, the weakest entry falling off once it is full.
    #define TRY_LIGHT(POS, COL, SCORE)                                          \
        do {                                                                    \
            Vector3 _p = (POS); Vector3 _c = (COL); float _s = (SCORE);         \
            int _slot = count;                                                  \
            if (count == MAX_SHADER_LIGHTS) {                                   \
                int _worst = 0;                                                 \
                for (int _i = 1; _i < count; _i++)                              \
                    if (scores[_i] < scores[_worst]) _worst = _i;               \
                if (scores[_worst] >= _s) break;                                \
                _slot = _worst;                                                 \
            } else count++;                                                     \
            positions[_slot] = _p; colors[_slot] = _c; scores[_slot] = _s;      \
        } while (0)

    // Effect lights: explosions, muzzle flashes, dashes.
    for (int i = 0; i < MAX_FX_LIGHTS; i++)
    {
        FxLight *l = &w->lights[i];
        if (!l->active) continue;

        float fade = l->life/l->maxLife;
        float intensity = l->radius*fade;
        Vector3 color = {
            l->color.r/255.0f*intensity,
            l->color.g/255.0f*intensity,
            l->color.b/255.0f*intensity
        };
        TRY_LIGHT(l->position, color, intensity);
    }

    // Projectiles glow faintly, supers much more.
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
        if (!p->active) continue;

        float intensity = p->isSuper ? 1.5f : 0.5f;
        Vector3 color = {
            p->color.r/255.0f*intensity,
            p->color.g/255.0f*intensity,
            p->color.b/255.0f*intensity
        };
        TRY_LIGHT(p->position, color, intensity*0.6f);
    }

    // A charged brawler carries its own glow.
    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (!b->alive || b->superCharge < 1.0f || !b->visible) continue;

        float pulse = 0.7f + 0.3f*sinf(w->time*6.0f);
        Vector3 color = { 1.0f*pulse, 0.78f*pulse, 0.28f*pulse };
        TRY_LIGHT(((Vector3){ b->position.x, 0.7f, b->position.z }), color, 0.9f);
    }

    #undef TRY_LIGHT

    AssetsSetLights(a, positions, colors, count);
}

//------------------------------------------------------------------------------------
// Ground decals and glows, drawn unlit with additive or alpha blending.
//------------------------------------------------------------------------------------
static void DrawGroundGlow(Assets *a, Vector3 pos, float radius, Color tint)
{
    rlDisableDepthMask();
    Matrix m = TRS((Vector3){ radius*2.0f, 1.0f, radius*2.0f }, 0.0f,
                   (Vector3){ pos.x, 0.03f, pos.z });
    DrawLit(a, a->plane, m, a->texGlow, tint, (Vector2){ 1.0f, 1.0f }, 1.0f);
    rlEnableDepthMask();
}

static void DrawShadow(Assets *a, Vector3 pos, float radius)
{
    DrawGroundGlow(a, pos, radius, (Color){ 0, 0, 0, 120 });
}

//------------------------------------------------------------------------------------
static void DrawArenaGeometry(World *w, Assets *a)
{
    float worldW = ARENA_W*TILE_SIZE;
    float worldH = ARENA_H*TILE_SIZE;

    // Floor, one quad with the tile texture repeated once per game tile.
    Matrix floorM = TRS((Vector3){ worldW, 1.0f, worldH }, 0.0f, (Vector3){ 0.0f, 0.0f, 0.0f });
    DrawLit(a, a->plane, floorM, a->texFloor, FLOOR_TINT,
            (Vector2){ (float)ARENA_W, (float)ARENA_H }, 0.0f);

    for (int tz = 0; tz < ARENA_H; tz++)
    {
        for (int tx = 0; tx < ARENA_W; tx++)
        {
            const Tile *t = &w->arena.tiles[tz][tx];
            if (t->type == TILE_FLOOR) continue;

            Vector3 c = ArenaTileCenter(tx, tz);

            if (t->type == TILE_WALL)
            {
                Matrix body = TRS((Vector3){ TILE_SIZE, WALL_HEIGHT, TILE_SIZE }, 0.0f,
                                  (Vector3){ c.x, WALL_HEIGHT*0.5f, c.z });
                DrawLit(a, a->cube, body, a->texWall, WALL_TINT, (Vector2){ 1.0f, 1.0f }, 0.0f);

                // Bright cap so the top edge catches the key light.
                Matrix cap = TRS((Vector3){ TILE_SIZE*1.01f, 0.16f, TILE_SIZE*1.01f }, 0.0f,
                                 (Vector3){ c.x, WALL_HEIGHT + 0.04f, c.z });
                DrawLit(a, a->cube, cap, a->texWall, WALL_CAP, (Vector2){ 1.0f, 1.0f }, 0.0f);
            }
            else if (t->type == TILE_CRATE)
            {
                float hp = (float)t->health/(float)CRATE_HEALTH;
                Color tint = CRATE_TINT;
                if (t->hitFlash > 0.0f) tint = ColorLerpC(tint, WHITE, t->hitFlash);
                tint = ColorLerpC((Color){ 128, 96, 66, 255 }, tint, 0.4f + hp*0.6f);

                float size = TILE_SIZE*0.9f;
                Matrix body = TRS((Vector3){ size, CRATE_HEIGHT, size }, 0.0f,
                                  (Vector3){ c.x, CRATE_HEIGHT*0.5f, c.z });
                DrawLit(a, a->cube, body, a->texCrate, tint, (Vector2){ 1.0f, 1.0f }, 0.0f);

                DrawShadow(a, c, size*0.62f);
            }
            else if (t->type == TILE_BUSH)
            {
                // A clump of overlapping spheres reads as foliage from this angle.
                const float ox[4] = { -0.36f, 0.38f, 0.02f, 0.18f };
                const float oz[4] = { 0.30f, -0.28f, 0.38f, -0.36f };
                const float sc[4] = { 0.52f, 0.48f, 0.44f, 0.40f };
                const float hy[4] = { 0.30f, 0.27f, 0.36f, 0.22f };

                for (int i = 0; i < 4; i++)
                {
                    float r = TILE_SIZE*sc[i];
                    // Squashed hard on Y: a brawler standing behind a bush must stay visible.
                    Matrix m = TRS((Vector3){ r, r*0.50f, r }, 0.0f,
                                   (Vector3){ c.x + ox[i], BUSH_HEIGHT*hy[i], c.z + oz[i] });
                    Color tint = ColorLerpC(BUSH_TINT, (Color){ 74, 150, 84, 255 }, i*0.22f);
                    DrawLit(a, a->sphere, m, a->texBush, tint, (Vector2){ 1.6f, 1.6f }, 0.0f);
                }
                DrawShadow(a, c, TILE_SIZE*0.6f);
            }
        }
    }
}

//------------------------------------------------------------------------------------
static void DrawBrawler(World *w, Assets *a, Brawler *b)
{
    if (!b->alive || !b->visible) return;

    // Everything below is expressed in units of `s`, so one factor scales the whole
    // character. Roughly one tile tall, which is the proportion the genre uses.
    float s = b->spawnScale*1.32f;
    if (s <= 0.03f) return;

    Color body = TEAM_COLORS[b->team];
    Color dark = TEAM_DARK[b->team];
    Color skin = SKIN_TINT;
    Color helmet = b->isPlayer ? (Color){ 236, 242, 252, 255 } : dark;

    if (b->hitFlash > 0.0f)
    {
        body = ColorLerpC(body, WHITE, b->hitFlash);
        dark = ColorLerpC(dark, WHITE, b->hitFlash);
        skin = ColorLerpC(skin, WHITE, b->hitFlash);
        helmet = ColorLerpC(helmet, WHITE, b->hitFlash);
    }

    // Visible-but-hidden brawlers render translucent.
    if (b->inBush)
    {
        body.a = 170; dark.a = 170; skin.a = 170; helmet.a = 170;
    }

    Vector3 pos = b->position;
    float bob = sinf(b->bobPhase)*0.06f*s;
    float yaw = b->renderYaw;

    DrawShadow(a, pos, 0.52f*s);

    // Legs: two stubby cylinders that swing with the walk cycle.
    float stride = sinf(b->bobPhase)*0.16f*s;
    for (int i = 0; i < 2; i++)
    {
        float side = (i == 0) ? -1.0f : 1.0f;
        float fwd = (i == 0) ? stride : -stride;
        Vector3 legPos = {
            pos.x + cosf(yaw)*0.17f*s*side + sinf(yaw)*fwd,
            0.0f,
            pos.z - sinf(yaw)*0.17f*s*side + cosf(yaw)*fwd
        };
        Matrix m = TRS((Vector3){ 0.13f*s, 0.36f*s, 0.13f*s }, yaw, legPos);
        DrawLit(a, a->cylinder, m, a->texCloth, dark, (Vector2){ 1.0f, 1.0f }, 0.0f);
    }

    // Torso
    Matrix torso = TRS((Vector3){ 0.34f*s, 0.52f*s, 0.30f*s }, yaw,
                       (Vector3){ pos.x, 0.34f*s + bob, pos.z });
    DrawLit(a, a->cylinder, torso, a->texCloth, body, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Chest webbing, a darker band that breaks up the silhouette
    Matrix vest = TRS((Vector3){ 0.36f*s, 0.18f*s, 0.32f*s }, yaw,
                      (Vector3){ pos.x, 0.52f*s + bob, pos.z });
    DrawLit(a, a->cylinder, vest, a->texCloth, dark, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Shoulders
    Matrix shoulders = TRS((Vector3){ 0.38f*s, 0.20f*s, 0.34f*s }, yaw,
                           (Vector3){ pos.x, 0.86f*s + bob, pos.z });
    DrawLit(a, a->sphere, shoulders, a->texCloth, body, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Head
    Matrix head = TRS((Vector3){ 0.24f*s, 0.26f*s, 0.24f*s }, yaw,
                      (Vector3){ pos.x, 1.14f*s + bob, pos.z });
    DrawLit(a, a->sphere, head, a->texCloth, skin, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Helmet: dome plus a forward brim
    Matrix dome = TRS((Vector3){ 0.27f*s, 0.22f*s, 0.27f*s }, yaw,
                      (Vector3){ pos.x, 1.24f*s + bob, pos.z });
    DrawLit(a, a->sphere, dome, a->texCloth, helmet, (Vector2){ 1.0f, 1.0f }, 0.0f);

    Matrix brim = TRS((Vector3){ 0.30f*s, 0.06f*s, 0.20f*s }, yaw,
                      (Vector3){ pos.x + sinf(yaw)*0.14f*s, 1.20f*s + bob, pos.z + cosf(yaw)*0.14f*s });
    DrawLit(a, a->cube, brim, a->texCloth, helmet, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Backpack
    Matrix pack = TRS((Vector3){ 0.30f*s, 0.30f*s, 0.20f*s }, yaw,
                      (Vector3){ pos.x - sinf(yaw)*0.26f*s, 0.68f*s + bob, pos.z - cosf(yaw)*0.26f*s });
    DrawLit(a, a->cube, pack, a->texMetal, dark, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Weapon
    float ax = sinf(yaw), az = cosf(yaw);
    Vector3 gunMid = { pos.x + ax*0.62f*s, 0.72f*s + bob, pos.z + az*0.62f*s };
    Matrix gun = TRS((Vector3){ 0.11f*s, 0.11f*s, 0.72f*s }, yaw, gunMid);
    DrawLit(a, a->cube, gun, a->texMetal, (Color){ 78, 84, 100, body.a }, (Vector2){ 1.0f, 1.0f }, 0.0f);

    Matrix grip = TRS((Vector3){ 0.10f*s, 0.18f*s, 0.10f*s }, yaw,
                      (Vector3){ pos.x + ax*0.34f*s, 0.60f*s + bob, pos.z + az*0.34f*s });
    DrawLit(a, a->cube, grip, a->texMetal, (Color){ 56, 60, 74, body.a }, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Charged-super ring on the floor
    if (b->superCharge >= 1.0f)
    {
        float pulse = 0.5f + 0.5f*sinf(w->time*6.0f);
        DrawGroundGlow(a, pos, 1.05f, (Color){ 255, 210, 90, (unsigned char)(70 + pulse*90) });
    }

    // Dash streak
    if (b->dashTimer > 0.0f)
        DrawGroundGlow(a, pos, 1.3f, (Color){ 255, 190, 100, 150 });
}

//------------------------------------------------------------------------------------
// Projectiles: a lit core plus additive glow billboards, with a short trail.
//------------------------------------------------------------------------------------
static void DrawProjectiles(World *w, Assets *a)
{
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
        if (!p->active) continue;

        Color glow = p->color;
        float coreSize = p->arcing ? 0.55f : fmaxf(p->radius, 0.16f)*2.6f;

        if (!p->arcing)
        {
            // Trail: a few fading billboards strung out behind the shot.
            Vector3 dir = Vector3Normalize(p->velocity);
            int steps = p->isSuper ? 7 : 5;
            float spacing = p->isSuper ? 0.42f : 0.3f;

            for (int s = steps; s >= 1; s--)
            {
                float t = (float)s/steps;
                Vector3 tp = Vector3Subtract(p->position, Vector3Scale(dir, spacing*s));
                Color c = glow;
                c.a = (unsigned char)(105*(1.0f - t));
                DrawBillboard(w->camera, a->texGlow, tp, coreSize*(1.0f - t*0.55f), c);
            }
        }

        Color hot = { 255, 255, 255, 165 };
        DrawBillboard(w->camera, a->texGlow, p->position, coreSize*1.45f, (Color){ glow.r, glow.g, glow.b, 88 });
        DrawBillboard(w->camera, a->texGlow, p->position, coreSize*0.58f, hot);
    }

    // Particles ride in the same additive pass so sparks actually glow.
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->particles[i];
        if (!pa->active) continue;

        float t = pa->life/pa->maxLife;
        Color c = pa->color;

        if (pa->type == PARTICLE_SMOKE)
        {
            // Smoke is soft and dark rather than emissive, so it goes in the alpha pass.
            continue;
        }

        c.a = (unsigned char)(c.a*(t > 1.0f ? 1.0f : t));
        float size = pa->size*(0.6f + t*0.8f)*1.9f;
        DrawBillboard(w->camera, a->texGlow, pa->position, size, c);
    }

    rlEnableDepthMask();
    EndBlendMode();

    // Smoke, alpha blended
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->particles[i];
        if (!pa->active || pa->type != PARTICLE_SMOKE) continue;

        float t = pa->life/pa->maxLife;
        Color c = pa->color;
        c.a = (unsigned char)(c.a*t*0.7f);

        rlDisableDepthMask();
        DrawBillboard(w->camera, a->texGlow, pa->position, pa->size*(2.2f - t)*3.0f, c);
        rlEnableDepthMask();
    }

    // Landing markers for lobbed shots, drawn flat on the ground.
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
        if (!p->active || !p->arcing) continue;

        Color ring = p->color;
        ring.a = 110;
        DrawGroundGlow(a, p->arcEnd, p->radius, ring);
    }
}

//------------------------------------------------------------------------------------
static float RayGroundDistance(World *w, Vector3 from, float angle, float maxDist)
{
    for (float d = 0.6f; d < maxDist; d += 0.25f)
    {
        float x = from.x + sinf(angle)*d;
        float z = from.z + cosf(angle)*d;
        if (ArenaSolidAt(&w->arena, x, z)) return d;
    }
    return maxDist;
}

static void DrawGroundLine(Vector3 from, float angle, float dist, float thickness, Color color)
{
    Vector3 a = { from.x, 0.07f, from.z };
    Vector3 b = { from.x + sinf(angle)*dist, 0.07f, from.z + cosf(angle)*dist };
    DrawCylinderEx(a, b, thickness, thickness, 6, color);
}

static void DrawAimPreview(World *w, Assets *a)
{
    Brawler *b = &w->brawlers[w->playerIdx];
    if (!b->alive || b->dashTimer > 0.0f) return;
    if (!w->charging && !w->aimingSuper) return;

    bool super = w->aimingSuper;
    const WeaponDef *def = &WEAPONS[b->cls];

    float range     = super ? def->sRange : def->range;
    float spreadDeg = super ? def->sSpreadDeg : def->spreadDeg;
    int pellets     = super ? def->sPellets : def->pellets;

    Color tint = super ? (Color){ 255, 214, 92, 190 } : (Color){ 120, 200, 255, 170 };
    Color edge = super ? (Color){ 255, 240, 170, 230 } : (Color){ 190, 230, 255, 230 };

    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    if (super && def->sDash)
    {
        float d = RayGroundDistance(w, b->position, b->aimAngle, w->tune.dashSpeed*0.45f);
        DrawGroundLine(b->position, b->aimAngle, d, 0.45f, tint);
        Vector3 tip = { b->position.x + sinf(b->aimAngle)*d, 0.0f, b->position.z + cosf(b->aimAngle)*d };
        rlEnableDepthMask();
        EndBlendMode();
        DrawGroundGlow(a, tip, 1.1f, edge);
        return;
    }

    if (def->arcing)
    {
        float aimDist = Clamp(w->aimDist, 1.5f, range);
        Vector3 land = {
            b->position.x + sinf(b->aimAngle)*aimDist, 0.0f,
            b->position.z + cosf(b->aimAngle)*aimDist
        };
        float radius = super ? def->sProjRadius : def->projRadius;

        Vector3 start = { b->position.x, 0.8f, b->position.z };
        float height = Vector3Distance(start, land)*0.42f + 1.0f;
        for (int i = 1; i <= 16; i++)
        {
            float t = i/16.0f;
            Vector3 pt = Vector3Lerp(start, land, t);
            pt.y = sinf(t*PI)*height;
            if (i % 2 == 0) DrawBillboard(w->camera, a->texGlow, pt, 0.34f, edge);
        }

        rlEnableDepthMask();
        EndBlendMode();
        DrawGroundGlow(a, land, radius, tint);
        return;
    }

    float half = (spreadDeg*DEG2RAD)*0.5f;
    float center = RayGroundDistance(w, b->position, b->aimAngle, range);
    DrawGroundLine(b->position, b->aimAngle, center, 0.075f, edge);

    if (pellets > 1 && half > 0.001f)
    {
        float dl = RayGroundDistance(w, b->position, b->aimAngle - half, range);
        float dr = RayGroundDistance(w, b->position, b->aimAngle + half, range);
        DrawGroundLine(b->position, b->aimAngle - half, dl, 0.06f, tint);
        DrawGroundLine(b->position, b->aimAngle + half, dr, 0.06f, tint);

        for (int i = 1; i < 5; i++)
        {
            float t = (i/5.0f)*2.0f - 1.0f;
            float ang = b->aimAngle + t*half;
            float d = RayGroundDistance(w, b->position, ang, range);
            Color faint = tint;
            faint.a = 55;
            DrawGroundLine(b->position, ang, d, 0.04f, faint);
        }
    }

    rlEnableDepthMask();
    EndBlendMode();

    Vector3 tip = { b->position.x + sinf(b->aimAngle)*center, 0.0f, b->position.z + cosf(b->aimAngle)*center };
    DrawGroundGlow(a, tip, 0.55f, edge);
}

//------------------------------------------------------------------------------------
static void DrawDebugOverlay(World *w)
{
    Brawler *p = &w->brawlers[w->playerIdx];

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (!b->alive) continue;

        const WeaponDef *def = &WEAPONS[b->cls];

        Color ring = TEAM_COLORS[b->team];
        ring.a = 70;
        DrawCylinderWires((Vector3){ b->position.x, 0.08f, b->position.z },
                          def->range, def->range, 0.01f, 40, ring);

        DrawCylinderWires((Vector3){ b->position.x, 0.09f, b->position.z },
                          BRAWLER_RADIUS, BRAWLER_RADIUS, 0.01f, 16, (Color){ 255, 255, 255, 60 });

        if (b->isPlayer) continue;

        bool sees = BrawlerCanSee(w, i, w->playerIdx);
        Color los = sees ? (Color){ 120, 255, 140, 180 } : (Color){ 255, 110, 110, 90 };
        if (p->alive)
            DrawLine3D((Vector3){ b->position.x, 1.0f, b->position.z },
                       (Vector3){ p->position.x, 1.0f, p->position.z }, los);
    }
}

//------------------------------------------------------------------------------------
void RenderWorld(World *w)
{
    Assets *a = g_assets;
    if (!a) return;

    AssetsSetCamera(a, w->camera.position);
    SubmitLights(w, a);

    BeginMode3D(w->camera);

        DrawArenaGeometry(w, a);

        for (int i = 0; i < w->brawlerCount; i++)
            DrawBrawler(w, a, &w->brawlers[i]);

        DrawAimPreview(w, a);
        DrawProjectiles(w, a);

        if (w->tune.showDebug) DrawDebugOverlay(w);

    EndMode3D();
}
