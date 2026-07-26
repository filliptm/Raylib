#ifndef EFFECTS_H
#define EFFECTS_H

#include "types.h"

void FxSpawnParticle(World *w, Vector3 pos, Vector3 vel, Color color, float life, float size, ParticleType type);
void FxMuzzleFlash(World *w, Vector3 pos, float angle, Color color);
void FxImpact(World *w, Vector3 pos, Color color, int count);
void FxExplosion(World *w, Vector3 pos, float radius, Color color);
void FxDeathBurst(World *w, Vector3 pos, Color color);
void FxCrateBreak(World *w, Vector3 pos);
void FxFloatText(World *w, Vector3 pos, const char *text, Color color);
void FxShake(World *w, float amount);
void FxSpawnLight(World *w, Vector3 pos, Color color, float radius, float life);

void FxUpdate(World *w, float dt);
void FxDrawScreen(World *w);     // called after EndMode3D

Color ColorLerpC(Color a, Color b, float t);
float EaseOutBack(float t);

#endif // EFFECTS_H
