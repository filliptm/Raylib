#include "attack_effects.h"

#include "render_state.h"
#include "vfx_catalog.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

// Presentation-only randomness: deterministic sim state is never touched.
static unsigned int g_seed = 0x9E3779B9u;
static float Rand01(void)
{
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return (float)(g_seed & 0xFFFFFF)/(float)0xFFFFFF;
}

static AttackParticle *AllocParticle(PresentationState *p)
{
    for (int i = 0; i < MAX_ATTACK_PARTICLES; i++)
        if (!p->attackParticles[i].active) return &p->attackParticles[i];
    return NULL;
}

static void SpawnFromLayer(PresentationState *p, const AttackEffectLayer *layer,
                           Vector3 origin, float yaw, int followBrawler)
{
    Vector3 forward = { sinf(yaw), 0.0f, cosf(yaw) };
    Vector3 side = { cosf(yaw), 0.0f, -sinf(yaw) };
    Vector3 base = Vector3Add(origin,
        Vector3Add(Vector3Scale(forward, layer->forward),
        Vector3Add((Vector3){ 0.0f, layer->up, 0.0f },
                   Vector3Scale(side, layer->side))));

    int count = layer->pattern == ATTACK_PATTERN_SINGLE ? 1 : layer->count;
    float spread = layer->spreadDeg*DEG2RAD;

    for (int i = 0; i < count; i++)
    {
        AttackParticle *particle = AllocParticle(p);
        if (!particle) return;

        float direction = yaw;
        float speed = layer->speed;
        float lift = 0.0f;
        switch (layer->pattern)
        {
            case ATTACK_PATTERN_BURST:
                direction = yaw + (Rand01() - 0.5f)*spread;
                speed = layer->speed*(0.55f + 0.45f*Rand01());
                lift = fabsf(layer->speed)*0.30f*Rand01();
                break;
            case ATTACK_PATTERN_RING:
                direction = (i/(float)count)*PI*2.0f;
                break;
            case ATTACK_PATTERN_CONE:
                direction = yaw + ((count == 1) ? 0.0f
                          : ((i/(float)(count - 1)) - 0.5f))*spread;
                break;
            default: break;
        }

        *particle = (AttackParticle){
            .active = true,
            .atlas = layer->atlas,
            .frame = layer->frame,
            .frameCount = layer->frameCount,
            .fps = layer->fps,
            .position = base,
            .velocity = { sinf(direction)*speed, lift, cosf(direction)*speed },
            .gravity = layer->gravity,
            .drag = layer->drag,
            .delay = layer->delay,
            .age = 0.0f,
            .duration = layer->duration,
            .scaleStart = layer->scaleStart,
            .scaleEnd = layer->scaleEnd,
            .colorStart = layer->colorStart,
            .colorEnd = layer->colorEnd,
            .blend = layer->blend,
            .rotation = layer->rotateSpeed != 0.0f ? Rand01()*360.0f : 0.0f,
            .rotateSpeed = layer->rotateSpeed,
            .ground = layer->ground,
            .follow = followBrawler,
            .followOffset = Vector3Subtract(base, origin)
        };
        if (followBrawler >= 0)
            particle->followOffset = Vector3Subtract(base, origin);
    }
}

void AttackFxSpawn(App *w, const AttackPresentation *doc, int anchor,
                   Vector3 origin, float yaw, int followBrawler)
{
    if (!doc || !doc->authored) return;
    for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
    {
        const AttackEffectLayer *layer = &doc->layers[i];
        if (!layer->used || layer->anchor != anchor) continue;
        SpawnFromLayer(&w->presentation, layer, origin, yaw,
                       layer->anchor == ATTACK_ANCHOR_SELF ? followBrawler : -1);
    }
}

void AttackFxUpdate(App *w, float dt)
{
    for (int i = 0; i < MAX_ATTACK_PARTICLES; i++)
    {
        AttackParticle *particle = &w->presentation.attackParticles[i];
        if (!particle->active) continue;

        particle->age += dt;
        if (particle->age >= particle->delay + particle->duration)
        {
            particle->active = false;
            continue;
        }
        if (particle->age < particle->delay) continue;

        particle->velocity.y += particle->gravity*dt;
        float damp = 1.0f - particle->drag*dt;
        if (damp < 0.0f) damp = 0.0f;
        particle->velocity = Vector3Scale(particle->velocity, damp);
        particle->rotation += particle->rotateSpeed*dt;

        if (particle->follow >= 0 && particle->follow < w->session.brawlerCount)
        {
            // Caster-anchored: physics run in the caster's local frame.
            particle->followOffset = Vector3Add(particle->followOffset,
                                                Vector3Scale(particle->velocity, dt));
            particle->position = Vector3Add(
                w->session.brawlers[particle->follow].position,
                particle->followOffset);
        }
        else
            particle->position = Vector3Add(particle->position,
                                            Vector3Scale(particle->velocity, dt));
    }
}

static Rectangle AtlasFrame(const Assets *a, int atlas, int frame, Texture2D *texture)
{
    if (atlas < 0) atlas = 0;
    if (atlas >= VFX_ATLAS_COUNT) atlas = VFX_ATLAS_COUNT - 1;
    *texture = a->vfxAtlases[atlas];

    int columns = 1, rows = 1, frames = 1;
    VfxAtlasGrid((VfxAtlasId)atlas, &columns, &rows, &frames);
    if (frame < 0) frame = 0;
    if (frame >= frames) frame = frames - 1;
    float width = texture->width/(float)columns;
    float height = texture->height/(float)rows;
    const float inset = 0.5f;
    return (Rectangle){
        (frame%columns)*width + inset,
        (frame/columns)*height + inset,
        width - inset*2.0f,
        height - inset*2.0f
    };
}

static void DrawGroundQuad(Texture2D texture, Rectangle source, Vector3 position,
                           float rotationDeg, float size, Color color)
{
    float angle = rotationDeg*DEG2RAD;
    float half = size*0.5f;
    Vector3 right = { cosf(angle)*half, 0.0f, -sinf(angle)*half };
    Vector3 forward = { sinf(angle)*half, 0.0f, cosf(angle)*half };
    Vector3 y = { 0.0f, fmaxf(position.y, 0.03f), 0.0f };

    float u0 = source.x/texture.width;
    float v0 = source.y/texture.height;
    float u1 = (source.x + source.width)/texture.width;
    float v1 = (source.y + source.height)/texture.height;

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(u0, v0);
    rlVertex3f(position.x - right.x - forward.x, y.y,
               position.z - right.z - forward.z);
    rlTexCoord2f(u0, v1);
    rlVertex3f(position.x - right.x + forward.x, y.y,
               position.z - right.z + forward.z);
    rlTexCoord2f(u1, v1);
    rlVertex3f(position.x + right.x + forward.x, y.y,
               position.z + right.z + forward.z);
    rlTexCoord2f(u1, v0);
    rlVertex3f(position.x + right.x - forward.x, y.y,
               position.z + right.z - forward.z);
    rlEnd();
    rlSetTexture(0);
}

static Color LerpColor(Color a, Color b, float t)
{
    return (Color){
        (unsigned char)(a.r + (b.r - a.r)*t),
        (unsigned char)(a.g + (b.g - a.g)*t),
        (unsigned char)(a.b + (b.b - a.b)*t),
        (unsigned char)(a.a + (b.a - a.a)*t)
    };
}

static void DrawBlendGroup(App *w, Assets *a, int blend)
{
    Camera3D camera = w->presentation.camera;
    for (int i = 0; i < MAX_ATTACK_PARTICLES; i++)
    {
        const AttackParticle *particle = &w->presentation.attackParticles[i];
        if (!particle->active || particle->blend != blend) continue;
        if (particle->age < particle->delay) continue;

        float t = (particle->age - particle->delay)/particle->duration;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        int frame = particle->frame;
        if (particle->frameCount > 1 && particle->fps > 0.0f)
            frame += ((int)((particle->age - particle->delay)*particle->fps))
                     %particle->frameCount;

        Texture2D texture;
        Rectangle source = AtlasFrame(a, particle->atlas, frame, &texture);
        if (texture.id == 0)
        {
            texture = a->texGlow;
            source = (Rectangle){ 0, 0, (float)texture.width, (float)texture.height };
        }

        float size = particle->scaleStart +
                     (particle->scaleEnd - particle->scaleStart)*t;
        Color color = LerpColor(particle->colorStart, particle->colorEnd, t);

        if (particle->ground)
            DrawGroundQuad(texture, source, particle->position,
                           particle->rotation, size, color);
        else
            DrawBillboardPro(camera, texture, source, particle->position,
                             (Vector3){ 0.0f, 1.0f, 0.0f },
                             (Vector2){ size, size },
                             (Vector2){ size*0.5f, size*0.5f },
                             particle->rotation, color);
    }
}

void AttackFxDraw(App *w, Assets *a)
{
    RenderBeginNoDepthWrite();
    DrawBlendGroup(w, a, ATTACK_BLEND_ALPHA);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawBlendGroup(w, a, ATTACK_BLEND_ADDITIVE);
    EndBlendMode();
    RenderEndNoDepthWrite();
}
