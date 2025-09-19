#ifndef BOARD_RENDERER_H
#define BOARD_RENDERER_H

#include "../common.h"
#include "../types.h"
#include "raylib.h"

// Board rendering functions
void DrawGameBoard(void);
void DrawValidDropZones(GameState* game, Card* draggedCard);
void DrawDragFeedback(GameState* game);
void DrawTargetingLine(Vector3 from, Vector3 to, Color color);

// Player portrait/hero rendering
void DrawPlayerPortrait(Player* player, Camera3D camera);
void DrawPlayerBoard(Player* player, Camera3D camera);
void DrawPlayerHand(Player* player, Camera3D camera);

#endif