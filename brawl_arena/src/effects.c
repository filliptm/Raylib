#include "effects.h"
#include "raymath.h"
#include <stdio.h>
#include <math.h>

Color ColorLerpC(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

float EaseOutBack(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

void FxSpawnParticle(World *w, Vector3 pos, Vector3 vel, Color color, float life, float size, ParticleType type)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &w->particles[i];
        if (p->active) continue;

        p->position = pos;
        p->velocity = vel;
        p->color = color;
        p->life = p->maxLife = life;
        p->size = size;
        p->type = type;
        p->active = true;
        return;
    }
}

void FxMuzzleFlash(World *w, Vector3 pos, float angle, Color color)
{
    // Kept small and short: these are drawn additively, so a big flash washes the
    // shooter out completely.
    for (int i = 0; i < 4; i++)
    {
        float spread = (GetRandomValue(-30, 30) / 100.0f);
        float a = angle + spread;
        float speed = 2.6f + GetRandomValue(0, 160) / 100.0f;
        Vector3 vel = { sinf(a) * speed, GetRandomValue(0, 100) / 100.0f, cosf(a) * speed };
        FxSpawnParticle(w, pos, vel, color, 0.09f, 0.12f, PARTICLE_MUZZLE);
    }
    FxSpawnLight(w, pos, color, 1.3f, 0.10f);
}

void FxImpact(World *w, Vector3 pos, Color color, int count)
{
    for (int i = 0; i < count; i++)
    {
        Vector3 vel = {
            GetRandomValue(-350, 350) / 100.0f,
            GetRandomValue(80, 420) / 100.0f,
            GetRandomValue(-350, 350) / 100.0f
        };
        FxSpawnParticle(w, pos, vel, color, 0.28f, 0.11f, PARTICLE_SPARK);
    }
}

void FxExplosion(World *w, Vector3 pos, float radius, Color color)
{
    int count = 18 + (int)(radius * 4.0f);
    for (int i = 0; i < count; i++)
    {
        float a = (i / (float)count) * PI * 2.0f + GetRandomValue(-20, 20) / 100.0f;
        float speed = radius * (1.5f + GetRandomValue(0, 120) / 100.0f);
        Vector3 vel = { sinf(a) * speed, GetRandomValue(40, 320) / 100.0f, cosf(a) * speed };
        Color c = ColorLerpC(color, WHITE, GetRandomValue(0, 45) / 100.0f);
        FxSpawnParticle(w, pos, vel, c, 0.40f + GetRandomValue(0, 25) / 100.0f, 0.16f, PARTICLE_SPARK);
    }
    for (int i = 0; i < 8; i++)
    {
        Vector3 vel = { GetRandomValue(-90, 90) / 100.0f, GetRandomValue(60, 200) / 100.0f, GetRandomValue(-90, 90) / 100.0f };
        FxSpawnParticle(w, pos, vel, (Color){ 190, 190, 200, 160 }, 0.7f, 0.35f, PARTICLE_SMOKE);
    }

    // A hot white core that fades into the blast colour reads as a real detonation.
    FxSpawnLight(w, (Vector3){ pos.x, pos.y + 0.9f, pos.z }, (Color){ 255, 240, 200, 255 }, radius * 1.1f, 0.16f);
    FxSpawnLight(w, pos, color, radius * 1.6f, 0.38f);

    FxShake(w, radius * 0.9f);
}

void FxDeathBurst(World *w, Vector3 pos, Color color)
{
    for (int i = 0; i < 22; i++)
    {
        Vector3 vel = {
            GetRandomValue(-420, 420) / 100.0f,
            GetRandomValue(200, 640) / 100.0f,
            GetRandomValue(-420, 420) / 100.0f
        };
        Color c = ColorLerpC(color, WHITE, GetRandomValue(0, 55) / 100.0f);
        FxSpawnParticle(w, pos, vel, c, 0.55f + GetRandomValue(0, 30) / 100.0f, 0.17f, PARTICLE_SPARK);
    }
    FxSpawnLight(w, pos, color, 2.6f, 0.3f);
    FxShake(w, 1.6f);
}

void FxCrateBreak(World *w, Vector3 pos)
{
    for (int i = 0; i < 14; i++)
    {
        Vector3 p = { pos.x + GetRandomValue(-60, 60) / 100.0f, 0.4f + GetRandomValue(0, 130) / 100.0f, pos.z + GetRandomValue(-60, 60) / 100.0f };
        Vector3 vel = { GetRandomValue(-260, 260) / 100.0f, GetRandomValue(150, 480) / 100.0f, GetRandomValue(-260, 260) / 100.0f };
        Color c = (Color){ 165, 118, 66, 255 };
        c = ColorLerpC(c, WHITE, GetRandomValue(0, 30) / 100.0f);
        FxSpawnParticle(w, p, vel, c, 0.6f, 0.2f, PARTICLE_DEBRIS);
    }
    FxSpawnLight(w, pos, (Color){ 255, 210, 150, 255 }, 2.0f, 0.25f);
    FxShake(w, 1.1f);
}

void FxFloatText(World *w, Vector3 pos, const char *text, Color color)
{
    for (int i = 0; i < MAX_FLOATTEXTS; i++)
    {
        FloatText *f = &w->texts[i];
        if (f->active) continue;

        f->world = pos;
        snprintf(f->text, sizeof(f->text), "%s", text);
        f->color = color;
        f->life = f->maxLife = 0.85f;
        f->rise = 0.0f;
        f->scale = 0.0f;
        f->active = true;
        return;
    }
}

void FxSpawnLight(World *w, Vector3 pos, Color color, float radius, float life)
{
    for (int i = 0; i < MAX_FX_LIGHTS; i++)
    {
        FxLight *l = &w->lights[i];
        if (l->active) continue;

        l->position = pos;
        l->color = color;
        l->radius = radius;
        l->life = l->maxLife = life;
        l->active = true;
        return;
    }
}

void FxShake(World *w, float amount)
{
    if (amount > w->shake) w->shake = amount;
    if (w->shake > 4.0f) w->shake = 4.0f;
}

void FxUpdate(World *w, float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &w->particles[i];
        if (!p->active) continue;

        p->life -= dt;
        if (p->life <= 0.0f) { p->active = false; continue; }

        if (p->type == PARTICLE_SPARK || p->type == PARTICLE_DEBRIS)
            p->velocity.y -= 16.0f * dt;
        else if (p->type == PARTICLE_SMOKE)
            p->velocity = Vector3Scale(p->velocity, 1.0f - 1.6f * dt);

        p->position = Vector3Add(p->position, Vector3Scale(p->velocity, dt));

        // Bounce debris off the floor once it lands.
        if (p->position.y < 0.05f && (p->type == PARTICLE_SPARK || p->type == PARTICLE_DEBRIS))
        {
            p->position.y = 0.05f;
            p->velocity.y *= -0.35f;
            p->velocity.x *= 0.7f;
            p->velocity.z *= 0.7f;
        }
    }

    for (int i = 0; i < MAX_FLOATTEXTS; i++)
    {
        FloatText *f = &w->texts[i];
        if (!f->active) continue;

        f->life -= dt;
        if (f->life <= 0.0f) { f->active = false; continue; }

        f->rise += dt * 2.6f;

        float age = 1.0f - (f->life / f->maxLife);
        f->scale = (age < 0.25f) ? EaseOutBack(age / 0.25f) : 1.0f;
    }

    for (int i = 0; i < MAX_FX_LIGHTS; i++)
    {
        FxLight *l = &w->lights[i];
        if (!l->active) continue;

        l->life -= dt;
        if (l->life <= 0.0f) l->active = false;
    }

    if (w->shake > 0.0f)
    {
        w->shake -= dt * 6.0f;
        if (w->shake < 0.0f) w->shake = 0.0f;
    }
}

void FxDrawScreen(World *w)
{
    for (int i = 0; i < MAX_FLOATTEXTS; i++)
    {
        FloatText *f = &w->texts[i];
        if (!f->active) continue;

        Vector3 world = f->world;
        world.y += 1.6f + f->rise;

        Vector2 sp = GetWorldToScreen(world, w->camera);
        if (sp.x < -100 || sp.x > GetScreenWidth() + 100) continue;
        if (sp.y < -100 || sp.y > GetScreenHeight() + 100) continue;

        int fontSize = (int)(26 * f->scale);
        if (fontSize < 1) continue;

        Color c = f->color;
        float fade = f->life / f->maxLife;
        c.a = (unsigned char)(255 * (fade > 0.5f ? 1.0f : fade * 2.0f));

        int tw = MeasureText(f->text, fontSize);
        DrawText(f->text, (int)(sp.x - tw / 2) + 2, (int)(sp.y - fontSize / 2) + 2, fontSize, (Color){ 0, 0, 0, c.a / 2 });
        DrawText(f->text, (int)(sp.x - tw / 2), (int)(sp.y - fontSize / 2), fontSize, c);
    }
}
