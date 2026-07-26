#include "render.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "effects.h"
#include "gems.h"
#include "assets.h"
#include "rlgl.h"
#include <stddef.h>
#include "raymath.h"
#include <math.h>

//------------------------------------------------------------------------------------
// Palette. Textures carry the detail, these tint them.
//------------------------------------------------------------------------------------
static const Color FLOOR_TINT  = { 150, 163, 190, 255 };
static const Color WALL_TINT   = { 120, 134, 166, 255 };   // cold steel: permanent cover
static const Color WALL_CAP    = { 126, 140, 172, 255 };   // top plate, kept off the floor's value
static const Color WALL_BASE   = {  74,  84, 108, 255 };   // skirt where it meets the floor
static const Color CRATE_TINT  = { 205, 168, 122, 255 };
static const Color SKIN_TINT   = { 232, 190, 158, 255 };

// Camera framing: pulled back and tilted, the way the mobile game reads.
static const Vector3 CAM_OFFSET = { 0.0f, 31.0f, -22.0f };

static Assets *g_assets = NULL;

void RenderSetAssets(Assets *a) { g_assets = a; }

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

void RenderBuildGrass(World *w)
{
    g_grassCount = 0;

    for (int tz = 0; tz < ARENA_H; tz++)
    {
        for (int tx = 0; tx < ARENA_W; tx++)
        {
            if (w->arena.tiles[tz][tx].type != TILE_BUSH) continue;

            Vector3 c = ArenaTileCenter(tx, tz);

            for (int i = 0; i < GRASS_PER_TILE; i++)
            {
                if (g_grassCount >= MAX_GRASS_INSTANCES) return;

                float r = Scatter(tx, tz, i*3 + 2);

                // Work out the blade's footprint first, then allow only enough jitter for
                // the whole quad to stay inside its own tile. The field then lines up with
                // the floor grid instead of bleeding over the edges onto bare ground.
                // Narrow enough that roots can sit close to the tile edge without the
                // quad overhanging it; density comes from GRASS_PER_TILE instead.
                float width = TILE_SIZE*(0.21f + r*0.13f);
                float room = TILE_SIZE*0.5f - width*0.5f;
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

#define AIM_Y 0.08f     // sits just clear of the floor and its grid lines

// A thick ground segment, shared by the shockwave rings and the aim-preview outlines.
static void DrawGroundEdge(Vector3 a, Vector3 b, float thickness, Color color)
{
    DrawCylinderEx((Vector3){ a.x, AIM_Y, a.z }, (Vector3){ b.x, AIM_Y, b.z },
                   thickness, thickness, 6, color);
}

// Scale + free rotation + translate, for objects that tumble rather than just yaw.
static Matrix TumbleTRS(float scale, Vector3 euler, Vector3 position)
{
    Matrix m = MatrixScale(scale, scale, scale);
    m = MatrixMultiply(m, MatrixRotateXYZ(euler));
    return MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
}

void CameraInit(World *w)
{
    w->camFocus = ArenaSpawnFor(&w->arena, TEAM_PLAYER, 0);
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

    g_lightCount = count;
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

                // From this camera the cap is most of what you see, so it carries the
                // same bolted plating rather than a blank brushed-metal lid.
                Matrix cap = TRS((Vector3){ TILE_SIZE*1.03f, 0.20f, TILE_SIZE*1.03f }, 0.0f,
                                 (Vector3){ c.x, WALL_HEIGHT + 0.06f, c.z });
                DrawLit(a, a->cube, cap, a->texWall, WALL_CAP, (Vector2){ 1.0f, 1.0f }, 0.0f);

                // A wider skirt at the floor. Crates sit flush on the ground, so this
                // silhouette alone tells you which cover you can shoot through.
                Matrix plinth = TRS((Vector3){ TILE_SIZE*1.06f, 0.30f, TILE_SIZE*1.06f }, 0.0f,
                                    (Vector3){ c.x, 0.15f, c.z });
                DrawLit(a, a->cube, plinth, a->texMetal, WALL_BASE, (Vector2){ 1.0f, 1.0f }, 0.0f);
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
                // Darken the soil so the grass field has something to sit in. The blades
                // themselves are instanced separately in DrawGrass().
                DrawGroundGlow(a, c, TILE_SIZE*0.66f, (Color){ 12, 32, 16, 130 });
            }
        }
    }
}

//------------------------------------------------------------------------------------
// Draws a brawler from its own fields only. No World, so the menu can stand one on a
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
static void DrawSolidEffects(World *w, Assets *a)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
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

    // Debris chunks from a blast, tumbling on their own axes.
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->particles[i];
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
static void DrawGems(World *w, Assets *a)
{
    if (!w->tune.gemGrab) return;

    // The vent: a lit pad marking where the next gem will surface.
    float pulse = 0.5f + 0.5f*sinf(w->time*2.4f);
    Vector3 vent = w->arena.gemVent;
    Matrix pad = TRS((Vector3){ 1.5f, 0.14f, 1.5f }, 0.0f, (Vector3){ vent.x, 0.07f, vent.z });
    DrawLit(a, a->cylinder, pad, a->texMetal,
            (Color){ 92, 62, 132, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
    DrawGroundGlow(a, vent, 1.35f,
                   (Color){ GEM_TINT.r, GEM_TINT.g, GEM_TINT.b, (unsigned char)(55 + pulse*70) });

    for (int i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &w->gems[i];
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
static void DrawShockwaves(World *w)
{
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    for (int i = 0; i < MAX_SHOCKWAVES; i++)
    {
        Shockwave *sw = &w->waves[i];
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
        Vector3 prev = { sw->position.x, AIM_Y, sw->position.z + radius };
        for (int k = 1; k <= SEG; k++)
        {
            float ang = (k/(float)SEG)*PI*2.0f;
            Vector3 pt = { sw->position.x + sinf(ang)*radius, AIM_Y,
                           sw->position.z + cosf(ang)*radius };
            DrawGroundEdge(prev, pt, thickness, c);
            prev = pt;
        }
    }

    rlEnableDepthMask();
    EndBlendMode();
}

static void DrawProjectiles(World *w, Assets *a)
{
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
        if (!p->active) continue;

        Color glow = p->color;
        float coreSize = p->arcing ? 0.30f : fmaxf(p->radius, 0.16f)*2.6f;

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

        // The shell already has a solid body, so it only needs a fuse glow, not a core.
        if (p->arcing)
        {
            float flicker = 0.72f + 0.28f*sinf(w->time*34.0f + i);
            DrawBillboard(w->camera, a->texGlow, p->position, coreSize*2.1f*flicker,
                          (Color){ glow.r, glow.g, glow.b, 70 });
            continue;
        }

        Color hot = { 255, 255, 255, 165 };
        DrawBillboard(w->camera, a->texGlow, p->position, coreSize*1.45f, (Color){ glow.r, glow.g, glow.b, 88 });
        DrawBillboard(w->camera, a->texGlow, p->position, coreSize*0.58f, hot);
    }

    if (w->tune.gemGrab)
    {
        for (int i = 0; i < MAX_GEMS; i++)
        {
            Gem *g = &w->gems[i];
            if (!g->active) continue;

            Vector3 pos = g->position;
            pos.y += sinf(g->bobPhase)*0.12f;
            float twinkle = 0.75f + 0.25f*sinf(w->time*5.0f + g->bobPhase);
            DrawBillboard(w->camera, a->texGlow, pos, 1.05f*twinkle,
                          (Color){ GEM_TINT.r, GEM_TINT.g, GEM_TINT.b, 92 });
        }
    }

    // Particles ride in the same additive pass so sparks actually glow.
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pa = &w->particles[i];
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
            DrawBillboard(w->camera, a->texGlow, pa->position, pa->size*1.5f, c);
            continue;
        }

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

// Filled cone for spread weapons. Every rib is raycast separately, so the shape is
// clipped by whatever wall it runs into rather than passing through it.
static void DrawAimCone(World *w, Vector3 origin, float centerAngle, float halfSpread,
                        float range, Color fill, Color edge)
{
    const int SEG = 26;
    Vector3 pts[SEG + 1];

    for (int i = 0; i <= SEG; i++)
    {
        float angle = centerAngle - halfSpread + (i/(float)SEG)*halfSpread*2.0f;
        float d = RayGroundDistance(w, origin, angle, range);
        pts[i] = (Vector3){ origin.x + sinf(angle)*d, AIM_Y, origin.z + cosf(angle)*d };
    }

    Vector3 apex = { origin.x, AIM_Y, origin.z };

    // Culling is off because the winding flips depending on which way you are facing.
    rlDisableBackfaceCulling();
    for (int i = 0; i < SEG; i++) DrawTriangle3D(apex, pts[i], pts[i + 1], fill);
    rlEnableBackfaceCulling();

    for (int i = 0; i < SEG; i++) DrawGroundEdge(pts[i], pts[i + 1], 0.055f, edge);
    DrawGroundEdge(apex, pts[0], 0.05f, edge);
    DrawGroundEdge(apex, pts[SEG], 0.05f, edge);
}

// Single thick beam, for weapons that fire one shot down a line.
static void DrawAimBeam(World *w, Vector3 origin, float angle, float range,
                        float halfWidth, Color fill, Color edge)
{
    float d = RayGroundDistance(w, origin, angle, range);
    float fx = sinf(angle), fz = cosf(angle);
    float px = cosf(angle), pz = -sinf(angle);      // perpendicular in the ground plane

    Vector3 nearL = { origin.x + px*halfWidth, AIM_Y, origin.z + pz*halfWidth };
    Vector3 nearR = { origin.x - px*halfWidth, AIM_Y, origin.z - pz*halfWidth };
    Vector3 farL  = { nearL.x + fx*d, AIM_Y, nearL.z + fz*d };
    Vector3 farR  = { nearR.x + fx*d, AIM_Y, nearR.z + fz*d };

    rlDisableBackfaceCulling();
    DrawTriangle3D(nearL, nearR, farR, fill);
    DrawTriangle3D(nearL, farR, farL, fill);
    rlEnableBackfaceCulling();

    DrawGroundEdge(nearL, farL, 0.055f, edge);
    DrawGroundEdge(nearR, farR, 0.055f, edge);
    DrawGroundEdge(farL, farR, 0.055f, edge);
}

// Filled disc, for the splash of a lobbed shot.
static void DrawAimDisc(Vector3 center, float radius, Color fill, Color edge)
{
    const int SEG = 32;
    Vector3 middle = { center.x, AIM_Y, center.z };
    Vector3 prev = { center.x, AIM_Y, center.z + radius };

    rlDisableBackfaceCulling();
    for (int i = 1; i <= SEG; i++)
    {
        float a = (i/(float)SEG)*PI*2.0f;
        Vector3 p = { center.x + sinf(a)*radius, AIM_Y, center.z + cosf(a)*radius };
        DrawTriangle3D(middle, prev, p, fill);
        DrawGroundEdge(prev, p, 0.06f, edge);
        prev = p;
    }
    rlEnableBackfaceCulling();
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
    float radius    = super ? def->sProjRadius : def->projRadius;

    // Supers read gold, ordinary shots read cool blue.
    Color fill = super ? (Color){ 255, 206, 92, 62 } : (Color){ 96, 178, 255, 58 };
    Color edge = super ? (Color){ 255, 238, 170, 210 } : (Color){ 176, 224, 255, 205 };

    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    // The dash charge previews as the lane it will carve.
    if (super && def->sDash)
    {
        DrawAimBeam(w, b->position, b->aimAngle, w->tune.dashSpeed*0.45f,
                    BRAWLER_RADIUS*1.1f, fill, edge);
        rlEnableDepthMask();
        EndBlendMode();
        return;
    }

    // Lobbed shots: the splash disc where each shell lands, plus its flight path.
    if (def->arcing)
    {
        float aimDist = Clamp(w->aimDist, 1.5f, range);
        float half = (spreadDeg*DEG2RAD)*0.5f;

        for (int i = 0; i < pellets; i++)
        {
            // Mirror how WeaponsFire fans multiple shells, so the preview matches.
            float t = (pellets == 1) ? 0.5f : (i/(float)(pellets - 1));
            float angle = b->aimAngle + (t - 0.5f)*half*2.0f;

            Vector3 land = { b->position.x + sinf(angle)*aimDist, 0.0f,
                             b->position.z + cosf(angle)*aimDist };

            DrawAimDisc(land, radius, fill, edge);

            if (i == pellets/2)
            {
                Vector3 start = { b->position.x, 0.8f, b->position.z };
                float height = Vector3Distance(start, land)*0.42f + 1.0f;
                for (int k = 1; k <= 16; k++)
                {
                    float p = k/16.0f;
                    Vector3 pt = Vector3Lerp(start, land, p);
                    pt.y = sinf(p*PI)*height;
                    if (k % 2 == 0) DrawBillboard(w->camera, a->texGlow, pt, 0.32f, edge);
                }
            }
        }

        rlEnableDepthMask();
        EndBlendMode();
        return;
    }

    // Everything else is either a spread cone or, for single-shot weapons, one beam.
    if (pellets > 1 && spreadDeg > 0.5f)
        DrawAimCone(w, b->position, b->aimAngle, (spreadDeg*DEG2RAD)*0.5f, range, fill, edge);
    else
        DrawAimBeam(w, b->position, b->aimAngle, range, fmaxf(radius*2.2f, 0.30f), fill, edge);

    rlEnableDepthMask();
    EndBlendMode();
}

static void DrawBrawler(World *w, Assets *a, Brawler *b)
{
    if (!b->alive || !b->visible) return;
    RenderBrawlerModel(a, b, w->time, b->inBush ? w->tune.concealDither : 0.0f, NULL);
}

//------------------------------------------------------------------------------------
// Instanced grass. Alpha cutout with depth writes, so it needs no sorting and can be
// drawn after the brawlers it partially hides.
//------------------------------------------------------------------------------------
static void DrawGrass(World *w, Assets *a)
{
    if (!a->grassOk || g_grassCount == 0) return;

    Vector3 actorPos[MAX_SHADER_LIGHTS];
    Vector2 actorVel[MAX_SHADER_LIGHTS];
    int actorCount = 0;

    for (int i = 0; i < w->brawlerCount && actorCount < MAX_SHADER_LIGHTS; i++)
    {
        Brawler *b = &w->brawlers[i];
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

    AssetsGrassFrame(a, &w->tune, w->time, w->camera.position,
                     actorPos, actorVel, actorCount,
                     g_lightPos, g_lightCol, g_lightCount);

    DrawMeshInstanced(a->grassBlade, a->grassMat, g_grassXforms, g_grassCount);
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

        // Grass goes down after the brawlers so it can cover them, and before the
        // additive effects pass so muzzle flashes still read through the blades.
        DrawGrass(w, a);

        // Concealed-player locator. Drawn after the grass with depth testing off, so
        // however deep in the field you are you can still tell where you are standing.
        {
            Brawler *me = &w->brawlers[w->playerIdx];
            if (me->alive && me->inBush)
            {
                float pulse = 0.5f + 0.5f*sinf(w->time*3.6f);
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
        DrawAimPreview(w, a);
        DrawShockwaves(w);
        DrawProjectiles(w, a);

        if (w->tune.showDebug) DrawDebugOverlay(w);

    EndMode3D();
}
