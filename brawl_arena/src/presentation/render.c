#include "render.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "effects.h"
#include "gems.h"
#include "assets.h"
#include "character_animation.h"
#include "environment.h"
#include "ability_visuals.h"
#include "render_state.h"
#include "vfx.h"
#include "content_catalog.h"
#include "rlgl.h"
#include <stddef.h>
#include "raymath.h"
#include <math.h>

//------------------------------------------------------------------------------------
// Palette for the procedural fallback brawlers. Environment colors live with the
// station renderer.
//------------------------------------------------------------------------------------
static const Color SKIN_TINT   = { 232, 190, 158, 255 };

static Assets *g_assets = NULL;

void RenderSetAssets(Assets *a) { g_assets = a; }
int RenderLoadedVfxAtlasCount(void) { return VfxLoadedAtlasCount(g_assets); }

// Grass instances are static geometry: positions are baked once per arena and all the
// motion happens in the vertex shader.
static Matrix g_grassXforms[MAX_GRASS_INSTANCES];
static int g_grassCount = 0;

// Lights collected this frame, shared between the scene and grass shaders.
static Vector3 g_lightPos[MAX_SHADER_LIGHTS];
static Vector3 g_lightCol[MAX_SHADER_LIGHTS];
static int g_lightCount = 0;

// Deterministic jitter, so an arena rebuild reproduces exactly the same field.
static float Scatter(int a, int b, int c)
{
    int h = a*374761393 + b*668265263 + c*1442695040;
    h = (h ^ (h >> 13))*1274126177;
    return (float)((h ^ (h >> 16)) & 0xFFFFFF)/(float)0xFFFFFF;
}

//------------------------------------------------------------------------------------
static Matrix TRS(Vector3 scale, float yawRadians, Vector3 position)
{
    Matrix m = MatrixScale(scale.x, scale.y, scale.z);
    if (yawRadians != 0.0f) m = MatrixMultiply(m, MatrixRotateY(yawRadians));
    return MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
}

void RenderBuildGrass(App *w)
{
    g_grassCount = 0;
    const Arena *arena = &w->session.arena;

    for (int tz = 0; tz < arena->height; tz++)
    {
        for (int tx = 0; tx < arena->width; tx++)
        {
            if (arena->tiles[tz][tx].type != TILE_BUSH) continue;

            Vector3 c = ArenaTileCenter(arena, tx, tz);

            for (int i = 0; i < GRASS_PER_TILE; i++)
            {
                if (g_grassCount >= MAX_GRASS_INSTANCES) return;

                float r = Scatter(tx, tz, i*3 + 2);

                // Work out the blade's footprint first, then allow only enough jitter for
                // the whole quad to stay inside its own tile. The field then lines up with
                // the floor grid instead of bleeding over the edges onto bare ground.
                // Narrow enough that roots can sit close to the tile edge without the
                // quad overhanging it; density comes from GRASS_PER_TILE instead.
                float width = arena->tileSize*(0.21f + r*0.13f);
                float room = arena->tileSize*0.5f - width*0.5f;
                if (room < 0.0f) room = 0.0f;

                float jx = (Scatter(tx, tz, i*3 + 0) - 0.5f)*2.0f*room;
                float jz = (Scatter(tx, tz, i*3 + 1) - 0.5f)*2.0f*room;

                Vector3 pos = { c.x + jx, 0.0f, c.z + jz };
                float height = 0.78f + r*0.46f;     // multiplies the grassHeight uniform

                g_grassXforms[g_grassCount++] =
                    TRS((Vector3){ width, height, width }, r*PI*2.0f, pos);
            }
        }
    }
}

// A thick ground segment, shared by the shockwave rings and the aim-preview outlines.
static void DrawGroundEdge(Vector3 a, Vector3 b, float thickness, Color color)
{
    DrawCylinderEx((Vector3){ a.x, ARENA_PREVIEW_Y, a.z },
                   (Vector3){ b.x, ARENA_PREVIEW_Y, b.z },
                   thickness, thickness, 6, color);
}

// Scale + free rotation + translate, for objects that tumble rather than just yaw.
static Matrix TumbleTRS(float scale, Vector3 euler, Vector3 position)
{
    Matrix m = MatrixScale(scale, scale, scale);
    m = MatrixMultiply(m, MatrixRotateXYZ(euler));
    return MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
}

//------------------------------------------------------------------------------------
// Light collection: rank every candidate emitter and hand the best few to the shader.
//------------------------------------------------------------------------------------
static void SubmitLights(App *w, Assets *a)
{
    Vector3 *positions = g_lightPos;
    Vector3 *colors = g_lightCol;
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
        FxLight *l = &w->presentation.lights[i];
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
        Projectile *p = &w->session.projectiles[i];
        if (!p->active) continue;

        float intensity = p->isSuper ? 0.75f : 0.25f;   // halved: the glow was lighting the whole lane
        Vector3 color = {
            p->color.r/255.0f*intensity,
            p->color.g/255.0f*intensity,
            p->color.b/255.0f*intensity
        };
        TRY_LIGHT(p->position, color, intensity*0.6f);
    }

    // A charged brawler carries its own glow.
    for (int i = 0; i < w->session.brawlerCount; i++)
    {
        Brawler *b = &w->session.brawlers[i];
        if (!b->alive || b->superCharge < 1.0f || !b->visible) continue;

        float pulse = 0.7f + 0.3f*sinf(w->session.time*6.0f);
        Vector3 color = { 1.0f*pulse, 0.78f*pulse, 0.28f*pulse };
        TRY_LIGHT(((Vector3){ b->position.x, 0.7f, b->position.z }), color, 0.9f);
    }

    #undef TRY_LIGHT

    g_lightCount = count;
    AssetsSetLights(a, positions, colors, count);
}

//------------------------------------------------------------------------------------
// Ground decals and glows, drawn unlit with additive or alpha blending. The raw helper
// deliberately leaves depth-mask ownership with its caller so nested additive passes
// cannot accidentally turn depth writes back on halfway through.
//------------------------------------------------------------------------------------
static void DrawGroundGlowRaw(Assets *a, Vector3 pos, float radius, Color tint)
{
    Matrix m = TRS((Vector3){ radius*2.0f, 1.0f, radius*2.0f }, 0.0f,
                   (Vector3){ pos.x, ARENA_DECAL_Y, pos.z });
    DrawLit(a, a->plane, m, a->texGlow, tint, (Vector2){ 1.0f, 1.0f }, 1.0f);
}

static void DrawGroundGlow(Assets *a, Vector3 pos, float radius, Color tint)
{
    RenderBeginNoDepthWrite();
    DrawGroundGlowRaw(a, pos, radius, tint);
    RenderEndNoDepthWrite();
}

static void DrawShadow(Assets *a, Vector3 pos, float radius)
{
    DrawGroundGlow(a, pos, radius, (Color){ 0, 0, 0, 120 });
}

//------------------------------------------------------------------------------------
static void DrawArenaGeometry(App *w, Assets *a)
{
    EnvironmentDraw(w, a);
}

//------------------------------------------------------------------------------------
// Draws a brawler from its own fields only. No App, so the menu can stand one on a
// podium without a match existing.
void RenderBrawlerModel(Assets *a, Brawler *b, float time, float dither, const Color *bodyTint)
{

    // Everything below is expressed in units of `s`, so one factor scales the whole
    // character. Roughly one tile tall, which is the proportion the genre uses.
    float s = b->spawnScale*1.32f;
    if (s <= 0.03f) return;

    // In a match the body is team coloured, because telling friend from foe matters more
    // than telling kits apart. The menu overrides it to distinguish the kits instead.
    Color body = bodyTint ? *bodyTint : TEAM_COLORS[b->team];
    Color dark = bodyTint ? ColorLerpC(*bodyTint, BLACK, 0.42f) : TEAM_DARK[b->team];
    Color skin = SKIN_TINT;
    Color helmet = b->isPlayer ? (Color){ 236, 242, 252, 255 } : dark;

    if (b->hitFlash > 0.0f)
    {
        body = ColorLerpC(body, WHITE, b->hitFlash);
        dark = ColorLerpC(dark, WHITE, b->hitFlash);
        skin = ColorLerpC(skin, WHITE, b->hitFlash);
        helmet = ColorLerpC(helmet, WHITE, b->hitFlash);
    }

    Vector3 pos = b->position;
    float bob = sinf(b->bobPhase)*0.06f*s;
    float yaw = b->renderYaw;

    DrawShadow(a, pos, 0.52f*s);

    // Concealed in grass: dissolve the body on a Bayer pattern. Screen-door beats real
    // transparency here because it needs no blending and so cannot sort wrongly against
    // the grass drawn on top of it. A green cast sells "in the bushes" as well.
    bool concealed = (dither > 0.001f);
    if (concealed)
    {
        AssetsSetDither(a, dither);
        Color moss = { 108, 190, 120, 255 };
        body = ColorLerpC(body, moss, 0.28f);
        dark = ColorLerpC(dark, moss, 0.28f);
        skin = ColorLerpC(skin, moss, 0.28f);
        helmet = ColorLerpC(helmet, moss, 0.28f);
    }

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

    // Weapon, shaped per kit: a long thin barrel for the sniper, a stubby wide one for
    // the lobber, a broad slab for the bruiser. Kit is then readable from the silhouette.
    float gunLen = 0.72f, gunGirth = 0.11f, gunReach = 0.62f;
    switch (b->cls)
    {
        case CLASS_SNIPER:  gunLen = 1.15f; gunGirth = 0.085f; gunReach = 0.82f; break;
        case CLASS_LOBBER:  gunLen = 0.46f; gunGirth = 0.19f;  gunReach = 0.50f; break;
        case CLASS_BRUISER: gunLen = 0.58f; gunGirth = 0.22f;  gunReach = 0.54f; break;
        case CLASS_HEALER:  gunLen = 0.68f; gunGirth = 0.14f;  gunReach = 0.60f; break;
        default: break;
    }

    float ax = sinf(yaw), az = cosf(yaw);
    Vector3 gunMid = { pos.x + ax*gunReach*s, 0.72f*s + bob, pos.z + az*gunReach*s };
    Matrix gun = TRS((Vector3){ gunGirth*s, gunGirth*s, gunLen*s }, yaw, gunMid);
    DrawLit(a, a->cube, gun, a->texMetal, (Color){ 78, 84, 100, body.a }, (Vector2){ 1.0f, 1.0f }, 0.0f);

    // The lobber carries a drum on top of its launcher.
    if (b->cls == CLASS_LOBBER)
    {
        Matrix drum = TRS((Vector3){ 0.20f*s, 0.16f*s, 0.20f*s }, yaw,
                          (Vector3){ pos.x + ax*0.44f*s, 0.86f*s + bob, pos.z + az*0.44f*s });
        DrawLit(a, a->sphere, drum, a->texMetal, (Color){ 96, 102, 122, body.a }, (Vector2){ 1.0f, 1.0f }, 0.0f);
    }
    else if (b->cls == CLASS_HEALER)
    {
        Vector3 focus = { pos.x + ax*0.86f*s, 0.72f*s + bob, pos.z + az*0.86f*s };
        Matrix orb = TRS((Vector3){ 0.16f*s, 0.16f*s, 0.16f*s }, yaw, focus);
        DrawLit(a, a->sphere, orb, a->texGlow, (Color){ 70, 244, 166, body.a },
                (Vector2){ 1.0f, 1.0f }, 0.85f);
    }

    Matrix grip = TRS((Vector3){ 0.10f*s, 0.18f*s, 0.10f*s }, yaw,
                      (Vector3){ pos.x + ax*0.34f*s, 0.60f*s + bob, pos.z + az*0.34f*s });
    DrawLit(a, a->cube, grip, a->texMetal, (Color){ 56, 60, 74, body.a }, (Vector2){ 1.0f, 1.0f }, 0.0f);

    if (concealed) AssetsSetDither(a, 0.0f);

    // Charged-super ring on the floor
    if (b->superCharge >= 1.0f)
    {
        float pulse = 0.5f + 0.5f*sinf(time*6.0f);
        DrawGroundGlow(a, pos, 1.05f, (Color){ 255, 210, 90, (unsigned char)(70 + pulse*90) });
    }

    // Dash streak
    if (b->dashTimer > 0.0f)
        DrawGroundGlow(a, pos, 1.3f, (Color){ 255, 190, 100, 150 });
}

//------------------------------------------------------------------------------------
// Projectiles: a lit core plus additive glow billboards, with a short trail.
//------------------------------------------------------------------------------------
// Anything with real geometry goes down first, lit and depth-written, so the additive
// glow layer has something solid to sit on top of.
static void DrawSolidEffects(App *w, Assets *a)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->session.projectiles[i];
        if (!p->active || !p->arcing) continue;

        // Tumble the shell as it flies: a spinning object reads as thrown, a static
        // billboard reads as a light.
        float spin = p->arcT*14.0f;
        Vector3 euler = { spin*1.7f, spin*1.15f, spin*0.6f };

        float body = 0.34f;
        DrawLit(a, a->sphere, TumbleTRS(body, euler, p->position), a->texMetal,
                (Color){ 96, 104, 126, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);

        // A wide emissive band around the casing: it tells you whose shell it is, and
        // keeps the silhouette readable against the dark floor as it tumbles.
        Matrix band = MatrixMultiply(MatrixScale(body*1.18f, body*0.46f, body*1.18f),
                                     MatrixRotateXYZ(euler));
        band = MatrixMultiply(band, MatrixTranslate(p->position.x, p->position.y, p->position.z));
        DrawLit(a, a->sphere, band, a->texFlat, p->color, (Vector2){ 1.0f, 1.0f }, 0.55f);
    }

    // Bullet bodies: solid emissive rounds in the projectile's colour. Solid geometry
    // keeps them crisp - stacked additive sprites washed out to white blobs - and the
    // depth they write means the toon ink pass outlines them too.
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->session.projectiles[i];
        if (!p->active || p->arcing) continue;

        float r = fmaxf(p->radius, 0.16f)*2.0f;
        Matrix m = MatrixMultiply(MatrixScale(r, r, r),
                                  MatrixTranslate(p->position.x, p->position.y, p->position.z));
        DrawLit(a, a->sphere, m, a->texFlat, p->color, (Vector2){ 1.0f, 1.0f }, 0.85f);
    }

    // Debris chunks from a blast, tumbling on their own axes.
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->presentation.particles[i];
        if (!pa->active || pa->type != PARTICLE_DEBRIS) continue;

        float t = pa->life/pa->maxLife;
        float spin = (1.0f - t)*11.0f;
        Vector3 euler = { spin*1.4f, spin, spin*0.7f };

        DrawLit(a, a->cube, TumbleTRS(pa->size*(0.7f + t*0.5f), euler, pa->position),
                a->texMetal, pa->color, (Vector2){ 1.0f, 1.0f }, 0.0f);
    }
}

static const Color GEM_TINT = { 176, 104, 248, 255 };

// Gems and the vent they come from. Solid faceted geometry, because a gem has to be
// legible at a glance across the arena.
static void DrawGems(App *w, Assets *a)
{
    if (!w->tune.gemGrab) return;

    // The vent: a lit pad marking where the next gem will surface.
    float pulse = 0.5f + 0.5f*sinf(w->session.time*2.4f);
    Vector3 vent = w->session.arena.gemVent;
    Matrix pad = TRS((Vector3){ 1.5f, 0.14f, 1.5f }, 0.0f, (Vector3){ vent.x, 0.07f, vent.z });
    DrawLit(a, a->cylinder, pad, a->texMetal,
            (Color){ 92, 62, 132, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
    DrawGroundGlow(a, vent, 1.35f,
                   (Color){ GEM_TINT.r, GEM_TINT.g, GEM_TINT.b, (unsigned char)(55 + pulse*70) });

    for (int i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &w->session.gems[i];
        if (!g->active) continue;

        Vector3 pos = g->position;
        pos.y += sinf(g->bobPhase)*0.12f;

        // A cube tipped onto its corner reads as a cut crystal from this camera.
        Vector3 euler = { 0.62f, g->spin, 0.62f };
        DrawLit(a, a->cube, TumbleTRS(0.42f, euler, pos), a->texFlat,
                GEM_TINT, (Vector2){ 1.0f, 1.0f }, 0.55f);

        DrawShadow(a, (Vector3){ g->position.x, 0.0f, g->position.z }, 0.26f);
    }
}

// Expanding ground rings. Drawn additively so they read as light rather than paint.
static void DrawShockwaves(App *w)
{
    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    for (int i = 0; i < MAX_SHOCKWAVES; i++)
    {
        Shockwave *sw = &w->presentation.waves[i];
        if (!sw->active) continue;

        float t = 1.0f - sw->life/sw->maxLife;          // 0 at birth, 1 at death
        float eased = 1.0f - powf(1.0f - t, 3.0f);      // fast out, then settles
        float radius = sw->maxRadius*eased;
        if (radius < 0.05f) continue;

        float fade = 1.0f - t;
        Color c = sw->color;
        c.a = (unsigned char)(225*fade*fade);
        float thickness = 0.07f + 0.20f*fade;

        const int SEG = 30;
        Vector3 prev = { sw->position.x, ARENA_PREVIEW_Y, sw->position.z + radius };
        for (int k = 1; k <= SEG; k++)
        {
            float ang = (k/(float)SEG)*PI*2.0f;
            Vector3 pt = { sw->position.x + sinf(ang)*radius, ARENA_PREVIEW_Y,
                           sw->position.z + cosf(ang)*radius };
            DrawGroundEdge(prev, pt, thickness, c);
            prev = pt;
        }
    }

    RenderEndNoDepthWrite();
    EndBlendMode();
}

static void DrawProjectiles(App *w, Assets *a)
{
    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->session.projectiles[i];
        if (!p->active) continue;

        Color glow = p->color;
        // Visual size only: p->radius stays the gameplay hitbox. The solid body is
        // drawn in the lit pass; this pass only adds a restrained halo and trail.
        float coreSize = p->arcing ? 0.55f : fmaxf(p->radius, 0.16f)*2.6f;

        if (!p->arcing)
        {
            // Trail: a few fading billboards strung out behind the shot.
            Vector3 dir = Vector3Normalize(p->velocity);
            int steps = p->isSuper ? 7 : 5;
            float spacing = p->isSuper ? 0.42f : 0.3f;
            float trailScale = 1.0f;
            BrawlerClass ownerClass = CLASS_SHOTGUNNER;
            if (p->owner >= 0 && p->owner < w->session.brawlerCount)
                ownerClass = w->session.brawlers[p->owner].cls;
            if (!p->isSuper)
            {
                if (ownerClass == CLASS_SNIPER)
                {
                    steps = 8;
                    spacing = 0.52f;
                    trailScale = 0.68f;
                    Vector3 lineEnd =
                        Vector3Subtract(p->position, Vector3Scale(dir, 3.4f));
                    DrawLine3D(p->position, lineEnd,
                               (Color){ glow.r, glow.g, glow.b, 160 });
                }
                else if (ownerClass == CLASS_BRUISER)
                {
                    steps = 6;
                    spacing = 0.36f;
                    trailScale = 1.32f;
                }
                else if (ownerClass == CLASS_SHOTGUNNER)
                {
                    steps = 4;
                    spacing = 0.24f;
                    trailScale = 0.92f;
                }
            }

            for (int s = steps; s >= 1; s--)
            {
                float t = (float)s/steps;
                Vector3 tp = Vector3Subtract(p->position, Vector3Scale(dir, spacing*s));
                Color c = glow;
                c.a = (unsigned char)(78*(1.0f - t));
                DrawBillboard(w->presentation.camera, a->texGlow, tp,
                              coreSize*trailScale*(1.0f - t*0.55f), c);
            }
        }

        // The shell already has a solid body, so it only needs a fuse glow, not a core.
        if (p->arcing)
        {
            float flicker = 0.72f + 0.28f*sinf(w->session.time*34.0f + i);
            DrawBillboard(w->presentation.camera, a->texGlow, p->position, coreSize*2.1f*flicker,
                          (Color){ glow.r, glow.g, glow.b, 70 });
            continue;
        }

        // One colored halo; the white-hot centre is gone - the solid body is the core.
        DrawBillboard(w->presentation.camera, a->texGlow, p->position, coreSize*1.5f, (Color){ glow.r, glow.g, glow.b, 70 });
    }

    if (w->tune.gemGrab)
    {
        for (int i = 0; i < MAX_GEMS; i++)
        {
            Gem *g = &w->session.gems[i];
            if (!g->active) continue;

            Vector3 pos = g->position;
            pos.y += sinf(g->bobPhase)*0.12f;
            float twinkle = 0.75f + 0.25f*sinf(w->session.time*5.0f + g->bobPhase);
            DrawBillboard(w->presentation.camera, a->texGlow, pos, 1.05f*twinkle,
                          (Color){ GEM_TINT.r, GEM_TINT.g, GEM_TINT.b, 92 });
        }
    }

    // Particles ride in the same additive pass so sparks actually glow.
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->presentation.particles[i];
        if (!pa->active) continue;

        float t = pa->life/pa->maxLife;
        Color c = pa->color;

        // Smoke is soft and dark; debris is solid. Both are handled elsewhere.
        if (pa->type == PARTICLE_SMOKE || pa->type == PARTICLE_DEBRIS) continue;

        c.a = (unsigned char)(c.a*(t > 1.0f ? 1.0f : t));

        if (pa->type == PARTICLE_SPARK)
        {
            // Stretched along travel, so a blast throws streaks instead of dots.
            Vector3 tail = Vector3Subtract(pa->position, Vector3Scale(pa->velocity, 0.035f));
            DrawCylinderEx(tail, pa->position, pa->size*0.35f, pa->size*1.15f, 5, c);
            DrawBillboard(w->presentation.camera, a->texGlow, pa->position, pa->size*1.5f, c);
            continue;
        }

        float size = pa->size*(0.6f + t*0.8f)*1.9f;
        DrawBillboard(w->presentation.camera, a->texGlow, pa->position, size, c);
    }

    RenderEndNoDepthWrite();
    EndBlendMode();

    // Smoke, alpha blended
    RenderBeginNoDepthWrite();
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->presentation.particles[i];
        if (!pa->active || pa->type != PARTICLE_SMOKE) continue;

        float t = pa->life/pa->maxLife;
        Color c = pa->color;
        c.a = (unsigned char)(c.a*t*0.7f);

        DrawBillboard(w->presentation.camera, a->texGlow, pa->position, pa->size*(2.2f - t)*3.0f, c);
    }
    RenderEndNoDepthWrite();

    // Landing markers for lobbed shots, drawn flat on the ground.
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->session.projectiles[i];
        if (!p->active || !p->arcing) continue;

        Color ring = p->color;
        ring.a = 110;
        DrawGroundGlow(a, p->arcEnd, p->radius, ring);
    }
}

//------------------------------------------------------------------------------------
// In-match draw for any kit with an imported rigged character.
//
// The clip is chosen from the movement direction RELATIVE TO FACING, which is what
// stops the moonwalk: a brawler aims one way and moves another, so backpedaling picks
// the backward clip and circling picks a diagonal. Facing +Z, the character's left is
// +X, so a positive relative angle selects the left-side clips.
//
// There is no crossfade in raylib, so a clip change restarts its cycle - starting at
// frame zero pops less than landing mid-stride.
static void StoreFallbackSockets(PresentationState *presentation, int index,
                                 const Brawler *brawler)
{
    if (!presentation || !brawler || index < 0 || index >= MAX_BRAWLERS) return;
    CharacterSocketPose *pose = &presentation->sockets[index];
    *pose = (CharacterSocketPose){ 0 };

    Vector3 forward = { sinf(brawler->renderYaw), 0.0f,
                        cosf(brawler->renderYaw) };
    Vector3 right = { forward.z, 0.0f, -forward.x };
    Vector3 center = Vector3Add(brawler->position, (Vector3){ 0, 0.92f, 0 });
    Vector3 chest = Vector3Add(brawler->position, (Vector3){ 0, 1.48f, 0 });
    Vector3 shoulderBase =
        Vector3Add(Vector3Add(chest, Vector3Scale(forward, 0.03f)),
                   (Vector3){ 0, 0.24f, 0 });
    Vector3 handBase =
        Vector3Add(Vector3Add(chest, Vector3Scale(forward, 0.52f)),
                   (Vector3){ 0, -0.16f, 0 });

    pose->positions[VFX_SOCKET_CENTER] = center;
    pose->positions[VFX_SOCKET_CHEST] = chest;
    pose->positions[VFX_SOCKET_LEFT_SHOULDER] =
        Vector3Subtract(shoulderBase, Vector3Scale(right, 0.42f));
    pose->positions[VFX_SOCKET_RIGHT_SHOULDER] =
        Vector3Add(shoulderBase, Vector3Scale(right, 0.42f));
    pose->positions[VFX_SOCKET_LEFT_HAND] =
        Vector3Subtract(handBase, Vector3Scale(right, 0.30f));
    pose->positions[VFX_SOCKET_RIGHT_HAND] =
        Vector3Add(handBase, Vector3Scale(right, 0.30f));
    pose->positions[VFX_SOCKET_LEFT_FOOT] =
        Vector3Subtract(Vector3Add(brawler->position, (Vector3){ 0, 0.10f, 0 }),
                        Vector3Scale(right, 0.22f));
    pose->positions[VFX_SOCKET_RIGHT_FOOT] =
        Vector3Add(Vector3Add(brawler->position, (Vector3){ 0, 0.10f, 0 }),
                   Vector3Scale(right, 0.22f));
    for (int socket = VFX_SOCKET_CENTER; socket < VFX_SOCKET_COUNT; socket++)
        pose->valid[socket] = true;
}

static void DrawBrawlerCharacterModel(App *w, Assets *a, Brawler *b)
{
    int idx = (int)(b - w->session.brawlers);
    if (idx < 0 || idx >= MAX_BRAWLERS) return;
    RiggedCharacter *character = &a->characters[b->cls];

    // Clip choice and time are advanced by CharacterAnimationsUpdate on the clamped
    // gameplay clock, for drawn and concealed brawlers alike. Drawing only reads.
    const CharacterAnimState *anim = &w->presentation.anim[idx];
    int clip = anim->valid ? anim->clip : character->clipIdle;
    float time = anim->valid ? anim->time : 0.0f;
    bool loop = anim->valid ? anim->loop : true;

    DrawShadow(a, b->position, 0.52f*b->spawnScale);

    // A grey model must still read friend-or-foe at a glance: enemies get a red cast,
    // allied bots a blue one, and your own character stays clean.
    Color tint = WHITE;
    if (!b->isPlayer)
        tint = (b->team == TEAM_PLAYER)
             ? ColorLerpC(WHITE, (Color){ 168, 202, 255, 255 }, 0.45f)
             : ColorLerpC(WHITE, (Color){ 255, 118, 118, 255 }, 0.50f);

    float dither = 0.0f;
    float emissive = 0.0f;

    if (b->alive)
    {
        if (b->hitFlash > 0.0f)
        {
            tint = ColorLerpC(tint, WHITE, b->hitFlash);
            emissive = b->hitFlash*0.85f;
        }
        if (b->inBush)
        {
            dither = w->tune.concealDither;
            tint = ColorLerpC(tint, (Color){ 108, 190, 120, 255 }, 0.30f);
        }
    }

    AssetsDrawCharacter(a, b->cls, b->position, b->renderYaw, b->spawnScale,
                        clip, time*CHARACTER_CLIP_FPS, loop,
                        tint, dither, emissive,
                        w->presentation.actions[idx].action,
                        CharacterActionProgress(&w->presentation.actions[idx]),
                        CharacterActionBlendWeight(&w->presentation.actions[idx]),
                        &w->presentation.sockets[idx],
                        g_lightPos, g_lightCol, g_lightCount, w->presentation.camera.position);

    if (b->alive && b->superCharge >= 1.0f)
    {
        float pulse = 0.5f + 0.5f*sinf(w->session.time*6.0f);
        DrawGroundGlow(a, b->position, 1.05f,
                       (Color){ 255, 210, 90, (unsigned char)(70 + pulse*90) });
    }
    if (b->alive && b->dashTimer > 0.0f)
    {
        const AbilityDefinition *dash =
            ContentAbility(&w->content, b->dashAbility);
        Color dashColor = (dash && dash->damage <= 0)
                        ? (Color){ 92, 220, 255, 150 }
                        : (Color){ 255, 190, 100, 150 };
        DrawGroundGlow(a, b->position, 1.3f, dashColor);
    }
}

static void DrawBrawler(App *w, Assets *a, Brawler *b)
{
    RiggedCharacter *character = &a->characters[b->cls];
    bool modelKit = character->ok && w->tune.modelCharacter;

    // A downed model plays its death clip and holds the final pose until close to the
    // respawn; primitives keep vanishing into their particle burst as before.
    if (!b->alive)
    {
        if (modelKit && character->clipDeath >= 0 && b->respawnTimer > 0.5f)
            DrawBrawlerCharacterModel(w, a, b);
        return;
    }

    if (!b->visible) return;

    if (modelKit) DrawBrawlerCharacterModel(w, a, b);
    else RenderBrawlerModel(a, b, w->session.time, b->inBush ? w->tune.concealDither : 0.0f, NULL);
}

//------------------------------------------------------------------------------------
// Instanced grass. Alpha cutout with depth writes, so it needs no sorting and can be
// drawn after the brawlers it partially hides.
//------------------------------------------------------------------------------------
static void DrawGrass(App *w, Assets *a)
{
    if (!a->grassOk || g_grassCount == 0) return;

    Vector3 actorPos[MAX_SHADER_LIGHTS];
    Vector2 actorVel[MAX_SHADER_LIGHTS];
    int actorCount = 0;

    for (int i = 0; i < w->session.brawlerCount && actorCount < MAX_SHADER_LIGHTS; i++)
    {
        Brawler *b = &w->session.brawlers[i];
        if (!b->alive) continue;

        // Only brawlers you are allowed to see bend the grass. Otherwise a hidden enemy
        // would part the blades as they moved and their own concealment would betray
        // them. `visible` already encodes the bush rules, including firing and proximity
        // reveals, so a revealed enemy starts disturbing the field again straight away.
        if (!b->visible) continue;

        actorPos[actorCount] = b->position;

        // Normalised travel direction, so blades lean the way someone is running rather
        // than only pushing straight out from them.
        float speed = sqrtf(b->velocity.x*b->velocity.x + b->velocity.z*b->velocity.z);
        float scale = (speed > 0.2f) ? fminf(speed/w->tune.moveSpeed, 1.0f)/speed : 0.0f;
        actorVel[actorCount] = (Vector2){ b->velocity.x*scale, b->velocity.z*scale };

        actorCount++;
    }

    AssetsGrassFrame(a, &w->tune, w->session.time, w->presentation.camera.position,
                     actorPos, actorVel, actorCount,
                     g_lightPos, g_lightCol, g_lightCount);

    DrawMeshInstanced(a->grassBlade, a->grassMat, g_grassXforms, g_grassCount);
}

//------------------------------------------------------------------------------------
static void DrawDebugOverlay(App *w)
{
    Brawler *p = &w->session.brawlers[w->session.playerIdx];

    for (int i = 0; i < w->session.brawlerCount; i++)
    {
        Brawler *b = &w->session.brawlers[i];
        if (!b->alive) continue;

        const AbilityDefinition *ability = ContentMainAbility(&w->content, b->cls);

        Color ring = TEAM_COLORS[b->team];
        ring.a = 70;
        DrawCylinderWires((Vector3){ b->position.x, ARENA_PREVIEW_Y, b->position.z },
                          ability->range, ability->range, 0.01f, 40, ring);

        DrawCylinderWires((Vector3){ b->position.x, ARENA_PREVIEW_Y + 0.01f, b->position.z },
                          BRAWLER_RADIUS, BRAWLER_RADIUS, 0.01f, 16, (Color){ 255, 255, 255, 60 });

        if (b->isPlayer) continue;

        bool sees = BrawlerCanSee(AppGameContext(w), i, w->session.playerIdx);
        Color los = sees ? (Color){ 120, 255, 140, 180 } : (Color){ 255, 110, 110, 90 };
        if (p->alive)
            DrawLine3D((Vector3){ b->position.x, 1.0f, b->position.z },
                       (Vector3){ p->position.x, 1.0f, p->position.z }, los);
    }
}

//------------------------------------------------------------------------------------
void RenderWorld(App *w)
{
    Assets *a = g_assets;
    if (!a) return;

    AssetsSetCamera(a, w->presentation.camera.position);
    AssetsSetToon(a, w->tune.toon, w->tune.toonBands);
    SubmitLights(w, a);

    BeginMode3D(w->presentation.camera);

        DrawArenaGeometry(w, a);

        for (int i = 0; i < w->session.brawlerCount; i++)
            StoreFallbackSockets(&w->presentation, i, &w->session.brawlers[i]);

        for (int i = 0; i < w->session.brawlerCount; i++)
            DrawBrawler(w, a, &w->session.brawlers[i]);

        // Grass goes down after the brawlers so it can cover them, and before the
        // additive effects pass so muzzle flashes still read through the blades.
        DrawGrass(w, a);

        // Concealed-player locator. Drawn after the grass with depth testing off, so
        // however deep in the field you are you can still tell where you are standing.
        {
            Brawler *me = &w->session.brawlers[w->session.playerIdx];
            if (me->alive && me->inBush)
            {
                float pulse = 0.5f + 0.5f*sinf(w->session.time*3.6f);
                rlDisableDepthTest();
                BeginBlendMode(BLEND_ADDITIVE);
                DrawGroundGlow(a, me->position, 0.95f,
                               (Color){ 80, 210, 120, (unsigned char)(48 + pulse*54) });
                EndBlendMode();
                rlEnableDepthTest();
            }
        }

        DrawGems(w, a);
        DrawSolidEffects(w, a);
        AbilityVisualsDraw(w, a);
        DrawShockwaves(w);
        VfxDraw(&w->presentation, a, w->presentation.camera,
                w->uiPreferences.reducedMotion);
        DrawProjectiles(w, a);

        if (w->tune.showDebug) DrawDebugOverlay(w);

    EndMode3D();
}
