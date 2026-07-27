#ifndef EFFECTS_H
#define EFFECTS_H

#include "app_types.h"

void FxSpawnParticle(App *w, Vector3 pos, Vector3 vel, Color color, float life, float size, ParticleType type);
void FxMuzzleFlash(App *w, Vector3 pos, float angle, Color color);
void FxImpact(App *w, Vector3 pos, Color color, int count);
void FxExplosion(App *w, Vector3 pos, float radius, Color color);
void FxDeathBurst(App *w, Vector3 pos, Color color);
void FxCrateBreak(App *w, Vector3 pos);
void FxFloatText(App *w, Vector3 pos, const char *text, Color color);
// Reserved for non-combat match-state emphasis. Attacks and impact helpers never call it.
void FxMatchShake(App *w, float amount);

// Converts simulation events into presentation-owned particles, lights, text, and
// non-combat match camera feedback, then clears the session queue.
void FxConsumeGameEvents(App *w);
void FxSpawnLight(App *w, Vector3 pos, Color color, float radius, float life);
void FxShockwave(App *w, Vector3 pos, float maxRadius, float life, Color color);

void FxUpdate(App *w, float dt);
void FxDrawScreen(App *w);     // called after EndMode3D

Color ColorLerpC(Color a, Color b, float t);
float EaseOutBack(float t);

#endif // EFFECTS_H
