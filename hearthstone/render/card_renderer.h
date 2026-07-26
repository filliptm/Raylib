#ifndef CARD_RENDERER_H
#define CARD_RENDERER_H

#include "../common.h"
#include "../types.h"
#include "raylib.h"

// Card rendering functions
void DrawCard3D(Card* card, Camera3D camera);
void DrawCardStatsOnCard(Card* card, Camera3D camera);
void DrawCardEffects(Card* card);
void DrawCardHighlight(Card* card, Color highlightColor);
void DrawCardKeywords(Card* card, Vector2 screenPos);
void DrawAllCardStats(GameState* game);

// Helper functions
Vector2 GetCardScreenPosition(Card* card, Camera3D camera);

#endif