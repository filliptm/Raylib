#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "../common.h"
#include "../types.h"
#include "raylib.h"

// UI rendering functions
void DrawGameUI(GameState* game);
void DrawTurnIndicator(GameState* game);
void DrawPlayerInfo(Player* player, int yPosition, bool isActive);
void DrawControls(void);
void DrawSelectedCardInfo(Card* card);
void DrawGameEndScreen(GameState* game);

// UI components
void DrawManaBar(int current, int max, int x, int y);
void DrawHealthBar(int current, int max, int x, int y);

#endif