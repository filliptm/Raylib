#ifndef EFFECT_RENDERER_H
#define EFFECT_RENDERER_H

#include "../common.h"
#include "../types.h"
#include "raylib.h"

// Effect rendering functions
void DrawVisualEffects(GameState* game);
void DrawEffect(VisualEffect* effect, Camera3D camera);

#endif