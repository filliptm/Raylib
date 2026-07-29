#include "effects.h"
#include "attack_effects.h"
#include "vfx.h"
#include "character_animation.h"
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

void FxSpawnParticle(App *w, Vector3 pos, Vector3 vel, Color color, float life, float size, ParticleType type)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &w->presentation.particles[i];
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

void FxMuzzleFlash(App *w, Vector3 pos, float angle, Color color)
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

void FxImpact(App *w, Vector3 pos, Color color, int count)
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

void FxExplosion(App *w, Vector3 pos, float radius, Color color)
{
    Vector3 ground = { pos.x, 0.12f, pos.z };

    // 1. A hard white core for two frames, so the blast has an instant.
    FxSpawnParticle(w, (Vector3){ pos.x, pos.y + 0.4f, pos.z }, (Vector3){ 0, 0.4f, 0 },
                    (Color){ 255, 250, 235, 255 }, 0.09f, radius*0.42f, PARTICLE_MUZZLE);

    // 2. An expanding ring on the floor that reads the blast radius exactly.
    FxShockwave(w, ground, radius, 0.42f, color);
    FxShockwave(w, ground, radius*0.62f, 0.26f, (Color){ 255, 244, 214, 255 });

    // 3. Sparks thrown along the ground rather than up, so the ring stays legible.
    int count = 16 + (int)(radius*5.0f);
    for (int i = 0; i < count; i++)
    {
        float a = (i/(float)count)*PI*2.0f + GetRandomValue(-24, 24)/100.0f;
        float speed = radius*(2.2f + GetRandomValue(0, 150)/100.0f);
        Vector3 vel = { sinf(a)*speed, GetRandomValue(30, 190)/100.0f, cosf(a)*speed };
        Color c = ColorLerpC(color, WHITE, GetRandomValue(20, 70)/100.0f);
        FxSpawnParticle(w, ground, vel, c, 0.34f + GetRandomValue(0, 22)/100.0f, 0.15f, PARTICLE_SPARK);
    }

    // 4. Solid tumbling chunks give the blast something with edges in it.
    for (int i = 0; i < 9; i++)
    {
        float a = GetRandomValue(0, 628)/100.0f;
        float speed = radius*(1.0f + GetRandomValue(0, 110)/100.0f);
        Vector3 vel = { sinf(a)*speed, GetRandomValue(240, 520)/100.0f, cosf(a)*speed };
        FxSpawnParticle(w, ground, vel, ColorLerpC(color, (Color){ 60, 62, 72, 255 }, 0.55f),
                        0.7f, 0.17f, PARTICLE_DEBRIS);
    }

    // 5. Smoke lingers after the light has gone.
    for (int i = 0; i < 9; i++)
    {
        Vector3 p = { pos.x + GetRandomValue(-70, 70)/100.0f, 0.35f + GetRandomValue(0, 90)/100.0f,
                      pos.z + GetRandomValue(-70, 70)/100.0f };
        Vector3 vel = { GetRandomValue(-80, 80)/100.0f, GetRandomValue(70, 210)/100.0f,
                        GetRandomValue(-80, 80)/100.0f };
        FxSpawnParticle(w, p, vel, (Color){ 176, 178, 190, 165 }, 0.85f, 0.38f, PARTICLE_SMOKE);
    }

    // A hot white core fading into the blast colour.
    FxSpawnLight(w, (Vector3){ pos.x, pos.y + 0.9f, pos.z }, (Color){ 255, 240, 200, 255 }, radius*1.1f, 0.16f);
    FxSpawnLight(w, pos, color, radius*1.6f, 0.38f);

}

void FxDeathBurst(App *w, Vector3 pos, Color color)
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
}

void FxCrateBreak(App *w, Vector3 pos)
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
}

void FxFloatText(App *w, Vector3 pos, const char *text, Color color)
{
    for (int i = 0; i < MAX_FLOATTEXTS; i++)
    {
        FloatText *f = &w->presentation.texts[i];
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

void FxSpawnLight(App *w, Vector3 pos, Color color, float radius, float life)
{
    for (int i = 0; i < MAX_FX_LIGHTS; i++)
    {
        FxLight *l = &w->presentation.lights[i];
        if (l->active) continue;

        l->position = pos;
        l->color = color;
        l->radius = radius;
        l->life = l->maxLife = life;
        l->active = true;
        return;
    }
}

void FxShockwave(App *w, Vector3 pos, float maxRadius, float life, Color color)
{
    for (int i = 0; i < MAX_SHOCKWAVES; i++)
    {
        Shockwave *s = &w->presentation.waves[i];
        if (s->active) continue;

        s->position = pos;
        s->maxRadius = maxRadius;
        s->life = s->maxLife = life;
        s->color = color;
        s->active = true;
        return;
    }
}

void FxMatchShake(App *w, float amount)
{
    if (amount > w->presentation.shake) w->presentation.shake = amount;
    if (w->presentation.shake > 4.0f) w->presentation.shake = 4.0f;
}

void FxConsumeGameEvents(App *w)
{
    GameEventQueue *queue = &w->session.events;
    for (int i = 0; i < queue->count; i++)
    {
        const GameEvent *event = &queue->items[i];
        switch (event->type)
        {
            case GAME_EVENT_MUZZLE:
                FxMuzzleFlash(w, event->position, event->angle, event->color);
                break;
            case GAME_EVENT_IMPACT:
                FxImpact(w, event->position, event->color, event->count);
                break;
            case GAME_EVENT_EXPLOSION:
                FxExplosion(w, event->position, event->radius, event->color);
                break;
            case GAME_EVENT_DEATH:
                FxDeathBurst(w, event->position, event->color);
                break;
            case GAME_EVENT_CRATE_BREAK:
                FxCrateBreak(w, event->position);
                break;
            case GAME_EVENT_FLOAT_TEXT:
                FxFloatText(w, event->position, event->text, event->color);
                break;
            case GAME_EVENT_LIGHT:
                FxSpawnLight(w, event->position, event->color, event->radius, event->life);
                break;
            case GAME_EVENT_SHOCKWAVE:
                FxShockwave(w, event->position, event->radius, event->life, event->color);
                break;
            case GAME_EVENT_PARTICLE:
                FxSpawnParticle(w, event->position, event->velocity, event->color,
                                event->life, event->size, event->particleType);
                break;
            case GAME_EVENT_VFX:
                w->presentation.vfxEventsConsumed++;
                w->presentation.lastVfxEffect = event->vfxId;
                w->presentation.vfxLayersSpawned +=
                    VfxSpawnEvent(&w->presentation, event);
                break;
            case GAME_EVENT_CHARACTER_ACTION:
                CharacterActionStartTimed(
                    &w->presentation, event->sourceBrawler,
                    event->characterAction, event->life);
                break;
            case GAME_EVENT_MATCH_SHAKE:
                FxMatchShake(w, event->size);
                break;
            case GAME_EVENT_ATTACK_CAST:
                if (event->abilityIndex >= 0 &&
                    event->abilityIndex < w->content.abilityCount)
                {
                    const AttackPresentation *doc =
                        &w->content.attacks[event->abilityIndex];
                    AttackFxSpawn(w, doc, ATTACK_ANCHOR_CAST, event->position,
                                  event->angle, event->sourceBrawler);
                    AttackFxSpawn(w, doc, ATTACK_ANCHOR_SELF, event->position,
                                  event->angle, event->sourceBrawler);
                }
                break;
            case GAME_EVENT_ATTACK_IMPACT:
                if (event->abilityIndex >= 0 &&
                    event->abilityIndex < w->content.abilityCount)
                    AttackFxSpawn(w, &w->content.attacks[event->abilityIndex],
                                  ATTACK_ANCHOR_IMPACT, event->position,
                                  event->angle, -1);
                break;
        }
    }
    queue->count = 0;
}

void FxUpdate(App *w, float dt)
{
    AttackFxUpdate(w, dt);
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &w->presentation.particles[i];
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
        FloatText *f = &w->presentation.texts[i];
        if (!f->active) continue;

        f->life -= dt;
        if (f->life <= 0.0f) { f->active = false; continue; }

        f->rise += dt * 2.6f;

        float age = 1.0f - (f->life / f->maxLife);
        f->scale = (age < 0.25f) ? EaseOutBack(age / 0.25f) : 1.0f;
    }

    for (int i = 0; i < MAX_FX_LIGHTS; i++)
    {
        FxLight *l = &w->presentation.lights[i];
        if (!l->active) continue;

        l->life -= dt;
        if (l->life <= 0.0f) l->active = false;
    }

    for (int i = 0; i < MAX_SHOCKWAVES; i++)
    {
        Shockwave *s = &w->presentation.waves[i];
        if (!s->active) continue;

        s->life -= dt;
        if (s->life <= 0.0f) s->active = false;
    }

    VfxUpdate(&w->presentation, dt);
    CharacterActionsUpdate(&w->presentation, dt);

    if (w->presentation.shake > 0.0f)
    {
        w->presentation.shake -= dt * 6.0f;
        if (w->presentation.shake < 0.0f) w->presentation.shake = 0.0f;
    }
}

void FxDrawScreen(App *w)
{
    for (int i = 0; i < MAX_FLOATTEXTS; i++)
    {
        FloatText *f = &w->presentation.texts[i];
        if (!f->active) continue;

        Vector3 world = f->world;
        world.y += 1.6f + f->rise;

        Vector2 sp = GetWorldToScreen(world, w->presentation.camera);
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
