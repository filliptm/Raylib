#ifndef RENDER_H
#define RENDER_H

#include "common.h"
#include "types.h"
#include "raylib.h"

// Include all render sub-modules
#include "render/board_renderer.h"
#include "render/card_renderer.h"
#include "render/ui_renderer.h"
#include "render/effect_renderer.h"

// Main rendering function
void DrawGame(GameState* game);

// Camera management
void UpdateGameCamera(GameState* game, float deltaTime);

#endif // RENDER_H